// NumPad Hotkeys — WH_KEYBOARD_LL hook engine
// Copyright (c) 2026 rainyApps.com — MIT License
#include <windows.h>
#include <atomic>
#include <mutex>
#include <vector>
#include "hook.h"
#include "config.h"
#include "resource.h"

// Verify KBDLLHOOKSTRUCT is the expected layout.
static_assert(sizeof(KBDLLHOOKSTRUCT) == 24,
    "KBDLLHOOKSTRUCT size mismatch — check Windows SDK version");

// ---------------------------------------------------------------------------
// Module-level state — only accessed on the hook thread (same as main thread)
// ---------------------------------------------------------------------------

static HHOOK  s_hHook    = nullptr;
static HWND   s_hWnd     = nullptr;
static bool   s_enabled  = true;

// The binding map is replaced atomically. We use a pointer + mutex so the
// hook callback never blocks (it reads a local copy of the pointer).
static std::mutex   s_bindMtx;
static BindingMap*  s_bindings = nullptr;

// ---------------------------------------------------------------------------
// Helper: is this vkCode+extended a numpad key we may want to intercept?
// ---------------------------------------------------------------------------
static bool IsNumpadKey(WORD vk, bool extended)
{
    // Numpad digits (NumLock ON)
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return true;
    // Decimal point (NumLock ON) — NOT extended
    if (vk == VK_DECIMAL  && !extended) return true;
    // Arithmetic operators
    if (vk == VK_ADD)      return true;   // NOT extended
    if (vk == VK_SUBTRACT) return true;   // NOT extended
    if (vk == VK_MULTIPLY) return true;   // NOT extended
    if (vk == VK_DIVIDE    &&  extended)  return true;  // IS extended
    // Numpad Enter — IS extended; main Enter is NOT
    if (vk == VK_RETURN    &&  extended)  return true;
    // NumLock-OFF navigation keys — NOT extended (numpad), extended = main cluster
    // We intercept the NOT-extended variants for numpad.
    if (!extended) {
        switch (vk) {
        case VK_INSERT: case VK_DELETE:
        case VK_HOME:   case VK_END:
        case VK_PRIOR:  case VK_NEXT:  // Page Up / Down
        case VK_UP:     case VK_DOWN:
        case VK_LEFT:   case VK_RIGHT:
        case VK_CLEAR:  // Numpad 5 with NumLock OFF
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// SendCombo — press all keys in order, release in reverse.
// Uses KEYEVENTF_UNICODE=0 so vk codes are used directly.
// ---------------------------------------------------------------------------
void HookEngine::SendCombo(std::initializer_list<WORD> keys)
{
    // Stack-allocate; no heap in hot path.
    const int kMaxKeys = 16;
    WORD  buf[kMaxKeys];
    int   count = 0;
    for (WORD k : keys) {
        if (count < kMaxKeys) buf[count++] = k;
    }

    // Build INPUT array: press all, release all reversed
    INPUT inp[kMaxKeys * 2] = {};
    for (int i = 0; i < count; ++i) {
        inp[i].type       = INPUT_KEYBOARD;
        inp[i].ki.wVk     = buf[i];
        inp[i].ki.dwFlags = 0;  // key down
    }
    for (int i = 0; i < count; ++i) {
        inp[count + i].type       = INPUT_KEYBOARD;
        inp[count + i].ki.wVk     = buf[count - 1 - i];
        inp[count + i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(count * 2, inp, sizeof(INPUT));
}

// ---------------------------------------------------------------------------
// SendText — types a string character by character via WM_CHAR simulation
// ---------------------------------------------------------------------------
static void SendText(const std::wstring& text)
{
    if (text.empty()) return;
    std::vector<INPUT> inp;
    inp.reserve(text.size() * 2);
    for (wchar_t ch : text) {
        INPUT down = {}, up = {};
        down.type        = INPUT_KEYBOARD;
        down.ki.wVk      = 0;
        down.ki.wScan    = ch;
        down.ki.dwFlags  = KEYEVENTF_UNICODE;
        up               = down;
        up.ki.dwFlags   |= KEYEVENTF_KEYUP;
        inp.push_back(down);
        inp.push_back(up);
    }
    SendInput(static_cast<UINT>(inp.size()), inp.data(), sizeof(INPUT));
}

// ---------------------------------------------------------------------------
// SendMediaKey — single VK_MEDIA_* key
// ---------------------------------------------------------------------------
static void SendMediaKey(WORD vk)
{
    INPUT inp[2] = {};
    inp[0].type       = INPUT_KEYBOARD;
    inp[0].ki.wVk     = vk;
    inp[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    inp[1]            = inp[0];
    inp[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
    SendInput(2, inp, sizeof(INPUT));
}

// ---------------------------------------------------------------------------
// Hook callback
// ---------------------------------------------------------------------------
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0) return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    // Only act on key-down events
    if (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN)
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

    // Injection guard — never intercept our own SendInput events
    if (kb->flags & LLKHF_INJECTED)
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    if (!s_enabled)
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    WORD vk       = static_cast<WORD>(kb->vkCode);
    bool extended = (kb->flags & LLKHF_EXTENDED) != 0;

    if (!IsNumpadKey(vk, extended))
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    // Map NumLock-OFF navigation VKs back to their numpad canonical VK
    // so the binding map can use a single key regardless of NumLock state.
    // We store bindings under the NumLock-ON VK code.
    WORD lookupVk = vk;
    if (!extended) {
        // These are sent by the numpad when NumLock is OFF
        switch (vk) {
        case VK_INSERT: lookupVk = VK_NUMPAD0; break;
        case VK_END:    lookupVk = VK_NUMPAD1; break;
        case VK_DOWN:   lookupVk = VK_NUMPAD2; break;
        case VK_NEXT:   lookupVk = VK_NUMPAD3; break;   // Page Down
        case VK_LEFT:   lookupVk = VK_NUMPAD4; break;
        case VK_CLEAR:  lookupVk = VK_NUMPAD5; break;   // centre key
        case VK_RIGHT:  lookupVk = VK_NUMPAD6; break;
        case VK_HOME:   lookupVk = VK_NUMPAD7; break;
        case VK_UP:     lookupVk = VK_NUMPAD8; break;
        case VK_PRIOR:  lookupVk = VK_NUMPAD9; break;   // Page Up
        case VK_DELETE: lookupVk = VK_DECIMAL; break;
        }
    }

    // Lookup in binding map
    DWORD mapKey = MakeBindingKey(lookupVk, extended);

    // Take a snapshot pointer (no allocation)
    const BindingMap* bindings = nullptr;
    {
        // Use try_lock to avoid blocking inside the hook callback.
        // If the lock is held (config is being updated), pass through.
        if (!s_bindMtx.try_lock()) {
            return CallNextHookEx(s_hHook, nCode, wParam, lParam);
        }
        bindings = s_bindings;
        s_bindMtx.unlock();
    }

    if (!bindings) return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    auto it = bindings->find(mapKey);
    if (it == bindings->end())
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);

    const KeyBinding& bind = it->second;

    switch (bind.action) {
    case ActionType::Keystroke:
        if (!bind.keys.empty()) {
            // Build initializer-list dynamically via INPUT array
            const int kMax = 16;
            WORD  buf[kMax];
            int   cnt = 0;
            for (WORD k : bind.keys) if (cnt < kMax) buf[cnt++] = k;
            INPUT inp[kMax * 2] = {};
            for (int i = 0; i < cnt; ++i) {
                inp[i].type   = INPUT_KEYBOARD;
                inp[i].ki.wVk = buf[i];
            }
            for (int i = 0; i < cnt; ++i) {
                inp[cnt + i].type       = INPUT_KEYBOARD;
                inp[cnt + i].ki.wVk     = buf[cnt - 1 - i];
                inp[cnt + i].ki.dwFlags = KEYEVENTF_KEYUP;
            }
            SendInput(static_cast<UINT>(cnt * 2), inp, sizeof(INPUT));
        }
        break;

    case ActionType::Text:
        SendText(bind.textOrPath);
        break;

    case ActionType::LaunchApp:
        // Post to the message window so ShellExecute is on the main thread
        if (s_hWnd) PostMessageW(s_hWnd, WM_APP_HOOK_KEY,
                                  static_cast<WPARAM>(mapKey), 0);
        // Fall through to suppress the key
        break;

    case ActionType::MediaControl:
        if (!bind.keys.empty()) SendMediaKey(bind.keys[0]);
        break;

    case ActionType::Disabled:
        // Suppress key — return 1 below
        break;
    }

    return 1;   // suppress original key
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool HookEngine::Install(HWND hWnd)
{
    if (s_hHook) return true;   // already installed
    s_hWnd = hWnd;
    // WH_KEYBOARD_LL does NOT need to be in a DLL; it runs in the calling thread.
    s_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                  nullptr, 0);
    return s_hHook != nullptr;
}

void HookEngine::Uninstall()
{
    if (s_hHook) {
        UnhookWindowsHookEx(s_hHook);
        s_hHook = nullptr;
    }
}

void HookEngine::SetBindings(const BindingMap& bindings, bool enabled)
{
    BindingMap* fresh = new BindingMap(bindings);
    BindingMap* old   = nullptr;
    {
        std::lock_guard<std::mutex> lk(s_bindMtx);
        old       = s_bindings;
        s_bindings = fresh;
        s_enabled  = enabled;
    }
    delete old;
}

bool HookEngine::IsInstalled()
{
    return s_hHook != nullptr;
}
