// NumPad Hotkeys — main entry point
// Copyright (c) 2026 rainyApps.com — MIT License
// Subsystem: WINDOWS (no console window)
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include <vector>

#include "resource.h"
#include "config.h"
#include "hook.h"
#include "tray.h"
#include "ui_config.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static HINSTANCE g_hInst  = nullptr;
static HWND      g_hWnd   = nullptr;   // HWND_MESSAGE window
static AppConfig g_cfg;
static bool      g_configOpen = false; // prevent duplicate config windows

static const wchar_t* kWndClass = L"NumPadHotkeysWndClass";
static const wchar_t* kMutex    = L"NumPadHotkeysMutex_v1";

// ---------------------------------------------------------------------------
// Apply the current active profile's bindings to the hook engine
// ---------------------------------------------------------------------------

static void ApplyBindings()
{
    const BindingMap* bm = nullptr;
    if (g_cfg.activeProfile < (int)g_cfg.profiles.size())
        bm = &g_cfg.profiles[g_cfg.activeProfile].bindings;

    static BindingMap empty;
    HookEngine::SetBindings(bm ? *bm : empty, g_cfg.enableHook);
    Tray::SetHookEnabled(g_cfg.enableHook);
}

// ---------------------------------------------------------------------------
// Open configuration dialog (non-reentrant)
// ---------------------------------------------------------------------------

static void OpenConfigDialog()
{
    if (g_configOpen) return;
    g_configOpen = true;

    AppConfig tmp = g_cfg;   // work on a copy
    if (ConfigUI::Show(g_hWnd, g_hInst, tmp)) {
        g_cfg = tmp;
        SaveConfig(g_cfg);
        ApplyBindings();

        // Sync autostart
        WCHAR exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        SetAutostart(g_cfg.startWithWindows, exePath);
    }

    g_configOpen = false;
}

// ---------------------------------------------------------------------------
// Collect profile name strings for the tray menu
// ---------------------------------------------------------------------------

static std::vector<std::wstring> GetProfileNames()
{
    std::vector<std::wstring> v;
    v.reserve(g_cfg.profiles.size());
    for (auto& p : g_cfg.profiles) v.push_back(p.name);
    return v;
}

// ---------------------------------------------------------------------------
// Message-only window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK MsgWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    // ----- Tray icon callback (NOTIFYICON_VERSION_4) -----
    case WM_APP_TRAY: {
        UINT event = LOWORD(lp);
        switch (event) {
        case WM_LBUTTONDBLCLK:
            OpenConfigDialog();
            break;

        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            bool autostart = GetAutostart();
            Tray::ShowContextMenu(hWnd, GetProfileNames(),
                                  g_cfg.activeProfile,
                                  g_cfg.enableHook,
                                  autostart);
            break;
        }
        }
        return 0;
    }

    // ----- Tray context menu commands -----
    case WM_COMMAND: {
        WORD id = LOWORD(wp);

        if (id == IDM_EXIT) {
            PostQuitMessage(0);
            return 0;
        }

        if (id == IDM_CONFIGURE) {
            OpenConfigDialog();
            return 0;
        }

        if (id == IDM_TOGGLE_HOOK) {
            g_cfg.enableHook = !g_cfg.enableHook;
            ApplyBindings();
            SaveConfig(g_cfg);
            return 0;
        }

        if (id == IDM_AUTOSTART) {
            g_cfg.startWithWindows = !g_cfg.startWithWindows;
            WCHAR exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            SetAutostart(g_cfg.startWithWindows, exePath);
            SaveConfig(g_cfg);
            return 0;
        }

        // Profile submenu items
        if (id >= IDM_PROFILE_BASE && id <= IDM_PROFILE_MAX) {
            int idx = static_cast<int>(id - IDM_PROFILE_BASE);
            if (idx < (int)g_cfg.profiles.size()) {
                g_cfg.activeProfile = idx;
                ApplyBindings();
                SaveConfig(g_cfg);
            }
            return 0;
        }

        return 0;
    }

    // ----- Second instance wants to show config -----
    case WM_APP_SHOW_CONFIG:
        OpenConfigDialog();
        return 0;

    // ----- LaunchApp action from hook -----
    case WM_APP_HOOK_KEY: {
        DWORD mapKey = static_cast<DWORD>(wp);
        if (g_cfg.activeProfile < (int)g_cfg.profiles.size()) {
            auto& bm = g_cfg.profiles[g_cfg.activeProfile].bindings;
            auto it = bm.find(mapKey);
            if (it != bm.end() && it->second.action == ActionType::LaunchApp) {
                ShellExecuteW(nullptr, L"open",
                    it->second.textOrPath.c_str(),
                    nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    g_hInst = hInst;

    // ---- Single-instance guard ----
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, kMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Signal existing instance to show config
        HWND existing = FindWindowW(kWndClass, nullptr);
        if (existing) PostMessageW(existing, WM_APP_SHOW_CONFIG, 0, 0);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ---- Init common controls (for ComboBox etc.) ----
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    // ---- Create hidden message-only window ----
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MsgWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc)) { CloseHandle(hMutex); return 1; }

    g_hWnd = CreateWindowExW(0, kWndClass, L"NumPadHotkeys",
                              0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!g_hWnd) { CloseHandle(hMutex); return 1; }

    // ---- Load configuration (fall back to defaults on parse failure) ----
    g_cfg = LoadConfig();

    // Sync autostart state (might have changed externally)
    g_cfg.startWithWindows = GetAutostart();

    // ---- Install keyboard hook ----
    if (!HookEngine::Install(g_hWnd)) {
        MessageBoxW(nullptr,
            L"Failed to install keyboard hook.\n"
            L"NumPad Hotkeys will run in tray-only mode.",
            L"NumPad Hotkeys", MB_OK | MB_ICONWARNING);
    }

    ApplyBindings();

    // ---- Show tray icon ----
    if (!Tray::Init(g_hWnd, hInst)) {
        MessageBoxW(nullptr, L"Failed to create tray icon.",
                    L"NumPad Hotkeys", MB_OK | MB_ICONERROR);
        HookEngine::Uninstall();
        CloseHandle(hMutex);
        return 1;
    }

    // ---- Message pump ----
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ---- Clean shutdown ----
    HookEngine::Uninstall();
    Tray::Destroy();
    DestroyWindow(g_hWnd);
    CloseHandle(hMutex);

    return static_cast<int>(msg.wParam);
}
