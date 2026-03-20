// NumPad Hotkeys — configuration UI, numpad visualiser, record-shortcut dialog
// Copyright (c) 2026 rainyApps.com — MIT License
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "ui_config.h"
#include "config.h"
#include "hook.h"
#include "resource.h"

// ============================================================
// Numpad layout
// ============================================================

struct NumpadKeyDef {
    WORD           vk;
    bool           extended;
    const wchar_t* label;
    int col, row, colSpan, rowSpan;
};

static const NumpadKeyDef kKeys[] = {
    // row 0
    { VK_NUMLOCK,  false, L"Num",  0, 0, 1, 1 },
    { VK_DIVIDE,   true,  L"/",   1, 0, 1, 1 },
    { VK_MULTIPLY, false, L"*",   2, 0, 1, 1 },
    { VK_SUBTRACT, false, L"\u2212", 3, 0, 1, 1 },
    // row 1
    { VK_NUMPAD7,  false, L"7",   0, 1, 1, 1 },
    { VK_NUMPAD8,  false, L"8",   1, 1, 1, 1 },
    { VK_NUMPAD9,  false, L"9",   2, 1, 1, 1 },
    { VK_ADD,      false, L"+",   3, 1, 1, 2 },
    // row 2
    { VK_NUMPAD4,  false, L"4",   0, 2, 1, 1 },
    { VK_NUMPAD5,  false, L"5",   1, 2, 1, 1 },
    { VK_NUMPAD6,  false, L"6",   2, 2, 1, 1 },
    // row 3
    { VK_NUMPAD1,  false, L"1",   0, 3, 1, 1 },
    { VK_NUMPAD2,  false, L"2",   1, 3, 1, 1 },
    { VK_NUMPAD3,  false, L"3",   2, 3, 1, 1 },
    { VK_RETURN,   true,  L"Ent", 3, 3, 1, 2 },
    // row 4
    { VK_NUMPAD0,  false, L"0",   0, 4, 2, 1 },
    { VK_DECIMAL,  false, L".",   2, 4, 1, 1 },
};
static const int kKeyCount = static_cast<int>(sizeof(kKeys)/sizeof(kKeys[0]));

// ============================================================
// NumPad visualiser — registered window class "NumPadViz"
// ============================================================

struct VizState {
    int               selectedIdx = -1;
    int               hoverIdx    = -1;
    const BindingMap* bindings    = nullptr;
};

static RECT KeyRect(const NumpadKeyDef& k, const RECT& c)
{
    int totalW = c.right  - c.left - 4;
    int totalH = c.bottom - c.top  - 4;
    int cw = totalW / 4, ch = totalH / 5;
    const int m = 2;
    RECT r;
    r.left   = c.left + 2 + k.col * cw + m;
    r.top    = c.top  + 2 + k.row * ch + m;
    r.right  = r.left + k.colSpan * cw - m * 2;
    r.bottom = r.top  + k.rowSpan * ch - m * 2;
    return r;
}

static COLORREF KeyColor(int idx, const BindingMap* bm, int sel)
{
    if (idx == sel) return RGB(0, 120, 215);
    if (!bm) return RGB(200, 200, 200);
    const NumpadKeyDef& k = kKeys[idx];
    auto it = bm->find(MakeBindingKey(k.vk, k.extended));
    if (it == bm->end()) return RGB(210, 210, 210);
    switch (it->second.action) {
    case ActionType::Keystroke:    return RGB(100, 160, 230);
    case ActionType::Text:         return RGB( 80, 180, 100);
    case ActionType::LaunchApp:    return RGB(230, 150,  60);
    case ActionType::MediaControl: return RGB(150,  80, 200);
    case ActionType::Disabled:     return RGB(200,  80,  80);
    }
    return RGB(210, 210, 210);
}

static int HitTest(POINT pt, const RECT& c)
{
    for (int i = 0; i < kKeyCount; ++i) {
        RECT r = KeyRect(kKeys[i], c);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static LRESULT CALLBACK VizWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    VizState* st = reinterpret_cast<VizState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(new VizState()));
        return 0;
    case WM_DESTROY:
        delete st;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT canvas; GetClientRect(hwnd, &canvas);

        HBRUSH bg = CreateSolidBrush(RGB(40,40,40));
        FillRect(hdc, &canvas, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        HFONT hf = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT ho = static_cast<HFONT>(SelectObject(hdc, hf));

        for (int i = 0; i < kKeyCount; ++i) {
            RECT r = KeyRect(kKeys[i], canvas);
            COLORREF col = KeyColor(i, st ? st->bindings : nullptr,
                                    st ? st->selectedIdx : -1);

            HBRUSH hbr = CreateSolidBrush(col);
            SelectObject(hdc, hbr);
            SelectObject(hdc, GetStockObject(NULL_PEN));
            RoundRect(hdc, r.left, r.top, r.right, r.bottom, 6, 6);
            DeleteObject(hbr);

            if (st && i == st->selectedIdx) {
                HPEN hp = CreatePen(PS_SOLID, 3, RGB(255,255,255));
                HPEN hpo = static_cast<HPEN>(SelectObject(hdc, hp));
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                RoundRect(hdc, r.left, r.top, r.right, r.bottom, 6, 6);
                SelectObject(hdc, hpo);
                DeleteObject(hp);
            }

            SetTextColor(hdc, RGB(255,255,255));
            DrawTextW(hdc, kKeys[i].label, -1, &r,
                      DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOCLIP);

            // Small sub-label
            if (st && st->bindings) {
                auto it = st->bindings->find(MakeBindingKey(kKeys[i].vk, kKeys[i].extended));
                if (it != st->bindings->end() && !it->second.label.empty()) {
                    LOGFONTW lf={};
                    GetObjectW(hf, sizeof(lf), &lf);
                    lf.lfHeight = -8;
                    HFONT hs = CreateFontIndirectW(&lf);
                    HFONT hso = static_cast<HFONT>(SelectObject(hdc, hs));
                    RECT sub = r; sub.top = r.top + (r.bottom-r.top)/2;
                    SetTextColor(hdc, RGB(220,220,220));
                    DrawTextW(hdc, it->second.label.c_str(), -1, &sub,
                              DT_CENTER|DT_BOTTOM|DT_SINGLELINE|DT_END_ELLIPSIS);
                    SelectObject(hdc, hso);
                    DeleteObject(hs);
                    SetTextColor(hdc, RGB(255,255,255));
                }
            }
        }
        SelectObject(hdc, ho);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!st) return 0;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        RECT c; GetClientRect(hwnd, &c);
        int idx = HitTest(pt, c);
        if (idx >= 0) {
            st->selectedIdx = idx;
            InvalidateRect(hwnd, nullptr, FALSE);
            NMHDR nm = {};
            nm.hwndFrom = hwnd;
            nm.idFrom   = static_cast<UINT_PTR>(GetDlgCtrlID(hwnd));
            nm.code     = NM_CLICK;
            SendMessageW(GetParent(hwnd), WM_NOTIFY,
                static_cast<WPARAM>(nm.idFrom), reinterpret_cast<LPARAM>(&nm));
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!st) return 0;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        RECT c; GetClientRect(hwnd, &c);
        int h = HitTest(pt, c);
        if (h != st->hoverIdx) {
            st->hoverIdx = h;
            InvalidateRect(hwnd, nullptr, FALSE);
            SetCursor(LoadCursorW(nullptr, h >= 0 ? IDC_HAND : IDC_ARROW));
        }
        return 0;
    }
    case WM_SETCURSOR: return TRUE;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterVizClass(HINSTANCE hInst)
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = VizWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wc.lpszClassName = L"NumPadViz";
    RegisterClassExW(&wc);
    done = true;
}

// ============================================================
// Record-shortcut helper
// ============================================================

struct RecordState {
    HHOOK             hHook    = nullptr;
    std::vector<WORD> keys;
    bool              captured = false;
    bool              cancel   = false;
    HWND              hWnd     = nullptr;
};

static RecordState* g_rec = nullptr;

static LRESULT CALLBACK RecordHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0 || !g_rec)
        return CallNextHookEx(g_rec ? g_rec->hHook : nullptr, nCode, wParam, lParam);

    if (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN)
        return CallNextHookEx(g_rec->hHook, nCode, wParam, lParam);

    const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (kb->flags & LLKHF_INJECTED)
        return CallNextHookEx(g_rec->hHook, nCode, wParam, lParam);

    WORD vk = static_cast<WORD>(kb->vkCode);

    if (vk == VK_ESCAPE) {
        g_rec->cancel = true;
        PostMessageW(g_rec->hWnd, WM_CLOSE, 0, 0);
        return 1;
    }

    // Skip pure modifiers
    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU ||
        vk == VK_LCONTROL || vk == VK_RCONTROL ||
        vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
        vk == VK_LMENU    || vk == VK_RMENU    ||
        vk == VK_LWIN     || vk == VK_RWIN)
        return 1;

    std::vector<WORD> combo;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) combo.push_back(VK_CONTROL);
    if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) combo.push_back(VK_SHIFT);
    if (GetAsyncKeyState(VK_MENU)    & 0x8000) combo.push_back(VK_MENU);
    if (GetAsyncKeyState(VK_LWIN)    & 0x8000) combo.push_back(VK_LWIN);
    combo.push_back(vk);

    g_rec->keys     = combo;
    g_rec->captured = true;
    PostMessageW(g_rec->hWnd, WM_CLOSE, 0, 0);
    return 1;
}

// ============================================================
// Format combo string
// ============================================================

static std::wstring FormatCombo(const std::vector<WORD>& keys)
{
    if (keys.empty()) return L"(none)";
    std::wstring r;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) r += L" + ";
        WORD vk = keys[i];
        switch (vk) {
        case VK_CONTROL: r += L"Ctrl";  break;
        case VK_SHIFT:   r += L"Shift"; break;
        case VK_MENU:    r += L"Alt";   break;
        case VK_LWIN: case VK_RWIN: r += L"Win"; break;
        default: {
            WCHAR buf[32] = {};
            UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
            if (GetKeyNameTextW(static_cast<LONG>(sc << 16), buf, 32) > 0)
                r += buf;
            else { buf[0] = static_cast<WCHAR>(vk); buf[1]=0; r += buf; }
            break;
        }
        }
    }
    return r;
}

// ============================================================
// ParseCombo — convert human text back to VK vector
// Accepts tokens separated by '+', case-insensitive, spaces ignored.
// e.g. "Ctrl + Alt + Shift + PrtSc"  or  "ctrl+c"  or  "Win+R"
// ============================================================

static std::wstring ToLower(std::wstring s)
{
    for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

static WORD TokenToVK(const std::wstring& tok)
{
    std::wstring t = ToLower(tok);
    // Trim whitespace
    size_t a = t.find_first_not_of(L" \t");
    size_t b = t.find_last_not_of(L" \t");
    if (a == std::wstring::npos) return 0;
    t = t.substr(a, b - a + 1);

    // Modifiers
    if (t == L"ctrl"  || t == L"control")        return VK_CONTROL;
    if (t == L"shift")                            return VK_SHIFT;
    if (t == L"alt"   || t == L"menu")            return VK_MENU;
    if (t == L"win"   || t == L"windows" ||
        t == L"lwin"  || t == L"rwin")            return VK_LWIN;

    // Function keys F1–F24
    if (t.size() >= 2 && t[0] == L'f') {
        bool allDigits = true;
        for (size_t i = 1; i < t.size(); ++i)
            if (!iswdigit(t[i])) { allDigits = false; break; }
        if (allDigits) {
            int n = _wtoi(t.c_str() + 1);
            if (n >= 1 && n <= 24) return static_cast<WORD>(VK_F1 + n - 1);
        }
    }

    // Common named keys
    if (t == L"enter"  || t == L"return")         return VK_RETURN;
    if (t == L"esc"    || t == L"escape")          return VK_ESCAPE;
    if (t == L"tab")                               return VK_TAB;
    if (t == L"space"  || t == L"spacebar")        return VK_SPACE;
    if (t == L"backspace" || t == L"bksp" ||
        t == L"back")                              return VK_BACK;
    if (t == L"delete" || t == L"del")             return VK_DELETE;
    if (t == L"insert" || t == L"ins")             return VK_INSERT;
    if (t == L"home")                              return VK_HOME;
    if (t == L"end")                               return VK_END;
    if (t == L"pageup"  || t == L"pgup"  ||
        t == L"page up" || t == L"prior")          return VK_PRIOR;
    if (t == L"pagedown"|| t == L"pgdn"  ||
        t == L"page down"||t == L"next")           return VK_NEXT;
    if (t == L"up")                                return VK_UP;
    if (t == L"down")                              return VK_DOWN;
    if (t == L"left")                              return VK_LEFT;
    if (t == L"right")                             return VK_RIGHT;
    if (t == L"prtsc"  || t == L"printscreen" ||
        t == L"print screen" || t == L"snapshot"||
        t == L"prtscr" || t == L"prtscrn")        return VK_SNAPSHOT;
    if (t == L"scrolllock" || t == L"scroll")      return VK_SCROLL;
    if (t == L"pause"  || t == L"break")           return VK_PAUSE;
    if (t == L"numlock")                           return VK_NUMLOCK;
    if (t == L"capslock" || t == L"caps")          return VK_CAPITAL;
    if (t == L"apps"   || t == L"menu key")        return VK_APPS;

    // Media keys
    if (t == L"medianext"  || t == L"nexttrack")  return VK_MEDIA_NEXT_TRACK;
    if (t == L"mediaprev"  || t == L"prevtrack")  return VK_MEDIA_PREV_TRACK;
    if (t == L"mediastop"  || t == L"stoptrack")  return VK_MEDIA_STOP;
    if (t == L"mediaplay"  || t == L"playpause")  return VK_MEDIA_PLAY_PAUSE;
    if (t == L"volumemute" || t == L"mute")        return VK_VOLUME_MUTE;
    if (t == L"volumeup"   || t == L"volup")       return VK_VOLUME_UP;
    if (t == L"volumedown" || t == L"voldown")     return VK_VOLUME_DOWN;

    // Punctuation / symbols by display name
    if (t == L"minus"  || t == L"-")              return VK_OEM_MINUS;
    if (t == L"plus"   || t == L"=")              return VK_OEM_PLUS;
    if (t == L"comma"  || t == L",")              return VK_OEM_COMMA;
    if (t == L"period" || t == L".")              return VK_OEM_PERIOD;
    if (t == L"/"      || t == L"slash")           return VK_OEM_2;
    if (t == L"`"      || t == L"grave"||
        t == L"backtick")                          return VK_OEM_3;
    if (t == L"["      || t == L"lbracket")        return VK_OEM_4;
    if (t == L"\\"     || t == L"backslash")       return VK_OEM_5;
    if (t == L"]"      || t == L"rbracket")        return VK_OEM_6;
    if (t == L"'"      || t == L"quote"||
        t == L"apostrophe")                        return VK_OEM_7;
    if (t == L";"      || t == L"semicolon")       return VK_OEM_1;

    // Single ASCII letter or digit → VkKeyScanW
    if (t.size() == 1) {
        SHORT vks = VkKeyScanW(t[0]);
        if (vks != -1) return static_cast<WORD>(LOBYTE(vks));
    }

    // Try GetKeyNameText in reverse: brute-force scan VK 1..254
    // (slow but only called once on user input)
    WCHAR kname[64];
    for (int vk = 1; vk < 255; ++vk) {
        UINT sc = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        if (sc == 0) continue;
        kname[0] = 0;
        if (GetKeyNameTextW(static_cast<LONG>(sc << 16), kname, 64) > 0) {
            if (ToLower(kname) == t) return static_cast<WORD>(vk);
        }
    }

    return 0;   // unknown
}

// Parse "Ctrl + Alt + Shift + PrtSc" → {VK_CONTROL, VK_MENU, VK_SHIFT, VK_SNAPSHOT}
// Returns empty vector on complete parse failure.
static std::vector<WORD> ParseCombo(const std::wstring& text)
{
    std::vector<WORD> result;
    if (text.empty()) return result;

    // Split on '+'
    std::vector<std::wstring> tokens;
    std::wstring cur;
    for (wchar_t c : text) {
        if (c == L'+') {
            tokens.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    tokens.push_back(cur);

    for (auto& tok : tokens) {
        WORD vk = TokenToVK(tok);
        if (vk != 0) result.push_back(vk);
    }
    return result;
}

// ============================================================
// Configuration window (registered class "NumPadHotkeysConfig")
// ============================================================

struct ConfigState {
    HINSTANCE  hInst;
    AppConfig* cfg;
    bool       saved    = false;
    int        selKey   = -1;
    HWND       hViz     = nullptr;
    // Pending key data from Record
    std::vector<WORD> pendingKeys;
};

static void UpdateViz(HWND hWnd, ConfigState* st)
{
    if (!st->hViz) return;
    const BindingMap* bm = nullptr;
    if (st->cfg->activeProfile < (int)st->cfg->profiles.size())
        bm = &st->cfg->profiles[st->cfg->activeProfile].bindings;
    VizState* vs = reinterpret_cast<VizState*>(
        GetWindowLongPtrW(st->hViz, GWLP_USERDATA));
    if (vs) { vs->bindings = bm; vs->selectedIdx = st->selKey; }
    InvalidateRect(st->hViz, nullptr, FALSE);
}

static void PopEditor(HWND hWnd, ConfigState* st)
{
    if (st->selKey < 0 || st->selKey >= kKeyCount) {
        SetDlgItemTextW(hWnd, IDC_SELECTED_KEY_LABEL, L"Click a key to edit it");
        SetDlgItemTextW(hWnd, IDC_LABEL_EDIT, L"");
        SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, L"");
        CheckRadioButton(hWnd, IDC_ACTION_KEYSTROKE, IDC_ACTION_DISABLED, IDC_ACTION_DISABLED);
        return;
    }
    const NumpadKeyDef& kd = kKeys[st->selKey];
    std::wstring title = std::wstring(L"Selected: Numpad ") + kd.label;
    SetDlgItemTextW(hWnd, IDC_SELECTED_KEY_LABEL, title.c_str());

    const BindingMap& bm = st->cfg->profiles[st->cfg->activeProfile].bindings;
    auto it = bm.find(MakeBindingKey(kd.vk, kd.extended));
    if (it == bm.end()) {
        SetDlgItemTextW(hWnd, IDC_LABEL_EDIT, L"");
        SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, L"");
        CheckRadioButton(hWnd, IDC_ACTION_KEYSTROKE, IDC_ACTION_DISABLED, IDC_ACTION_DISABLED);
        return;
    }
    const KeyBinding& b = it->second;
    SetDlgItemTextW(hWnd, IDC_LABEL_EDIT, b.label.c_str());

    int rid = IDC_ACTION_DISABLED;
    switch (b.action) {
    case ActionType::Keystroke:    rid = IDC_ACTION_KEYSTROKE; break;
    case ActionType::Text:         rid = IDC_ACTION_TEXT;      break;
    case ActionType::LaunchApp:    rid = IDC_ACTION_APP;       break;
    case ActionType::MediaControl: rid = IDC_ACTION_MEDIA;     break;
    case ActionType::Disabled:     rid = IDC_ACTION_DISABLED;  break;
    }
    CheckRadioButton(hWnd, IDC_ACTION_KEYSTROKE, IDC_ACTION_DISABLED, rid);

    if (b.action == ActionType::Keystroke)
        SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, FormatCombo(b.keys).c_str());
    else if (b.action == ActionType::Text || b.action == ActionType::LaunchApp)
        SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, b.textOrPath.c_str());
    else
        SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, L"");
}

static void CommitEditor(HWND hWnd, ConfigState* st)
{
    if (st->selKey < 0) return;
    const NumpadKeyDef& kd  = kKeys[st->selKey];
    DWORD               key = MakeBindingKey(kd.vk, kd.extended);
    BindingMap& bm = st->cfg->profiles[st->cfg->activeProfile].bindings;

    KeyBinding b;
    WCHAR buf[512] = {};
    GetDlgItemTextW(hWnd, IDC_LABEL_EDIT, buf, 511);
    b.label = buf;

    if (IsDlgButtonChecked(hWnd, IDC_ACTION_KEYSTROKE)) b.action = ActionType::Keystroke;
    else if (IsDlgButtonChecked(hWnd, IDC_ACTION_TEXT)) b.action = ActionType::Text;
    else if (IsDlgButtonChecked(hWnd, IDC_ACTION_APP))  b.action = ActionType::LaunchApp;
    else if (IsDlgButtonChecked(hWnd, IDC_ACTION_MEDIA)) b.action = ActionType::MediaControl;
    else b.action = ActionType::Disabled;

    if (b.action == ActionType::Text || b.action == ActionType::LaunchApp) {
        GetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, buf, 511);
        b.textOrPath = buf;
    } else if (b.action == ActionType::Keystroke || b.action == ActionType::MediaControl) {
        // Priority: (1) pending recorded keys, (2) text typed in the shortcut field,
        // (3) existing binding keys unchanged
        if (!st->pendingKeys.empty()) {
            b.keys = st->pendingKeys;
            st->pendingKeys.clear();
        } else {
            // Try to parse whatever is in the shortcut edit box
            GetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, buf, 511);
            std::vector<WORD> parsed = ParseCombo(buf);
            if (!parsed.empty()) {
                b.keys = parsed;
            } else {
                // Fall back to existing keys
                auto existing = bm.find(key);
                if (existing != bm.end()) b.keys = existing->second.keys;
            }
        }
    } else {
        auto existing = bm.find(key);
        if (existing != bm.end()) b.keys = existing->second.keys;
    }

    if (b.label.empty() && b.action == ActionType::Disabled)
        bm.erase(key);
    else
        bm[key] = std::move(b);
}

static void RefreshProfileCombo(HWND hWnd, const AppConfig& cfg)
{
    HWND hCb = GetDlgItem(hWnd, IDC_PROFILE_COMBO);
    ComboBox_ResetContent(hCb);
    for (auto& p : cfg.profiles) ComboBox_AddString(hCb, p.name.c_str());
    ComboBox_AddString(hCb, L"New profile\u2026");
    ComboBox_SetCurSel(hCb, cfg.activeProfile);
}

static LRESULT CALLBACK ConfigWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ConfigState* st = reinterpret_cast<ConfigState*>(
        GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<ConfigState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

        HFONT hf = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto Mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD style,
                      int x,int y,int w,int h, int id) -> HWND {
            HWND hw = CreateWindowExW(0, cls, txt, WS_CHILD|WS_VISIBLE|style,
                x,y,w,h, hWnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), st->hInst, nullptr);
            SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(hf), TRUE);
            return hw;
        };

        // ── Layout constants ──────────────────────────────────────
        // Client area: 580 × 306
        // Left column  (visualiser): x=8,  w=305
        // Right column (editor):     x=321, w=251  (ends at 572)
        // Top header: y=0..32  Bottom bar: y=274..306
        const int kVizX=8, kVizY=36, kVizW=305, kVizH=224;
        const int kRX=321, kRW=251;   // right column x, width
        const int kBtnY=276;           // bottom buttons y

        // ── Profile row (full width) ──────────────────────────────
        Mk(L"STATIC",    L"Profile:", SS_LEFT|SS_CENTERIMAGE, 8,7,52,22, -1);
        Mk(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST|WS_VSCROLL,   64,7,196,200, IDC_PROFILE_COMBO);
        Mk(L"BUTTON", L"New",    BS_PUSHBUTTON, 266,7,52,22, IDC_NEW_PROFILE_BTN);
        Mk(L"BUTTON", L"Delete", BS_PUSHBUTTON, 322,7,58,22, IDC_DEL_PROFILE_BTN);

        // Horizontal separator
        Mk(L"STATIC", L"", SS_ETCHEDHORZ, 8,33,564,2, -1);

        // ── Left column: NumPad visualiser ───────────────────────
        st->hViz = CreateWindowExW(WS_EX_CLIENTEDGE, L"NumPadViz", L"",
            WS_CHILD|WS_VISIBLE, kVizX,kVizY,kVizW,kVizH,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NUMPAD_VIZ)),
            st->hInst, nullptr);

        // ── Right column: editor ──────────────────────────────────
        // Selected key label
        Mk(L"STATIC", L"Click a key to edit", SS_LEFT,
           kRX, kVizY, kRW, 18, IDC_SELECTED_KEY_LABEL);

        // Label field
        Mk(L"STATIC", L"Label:", SS_LEFT|SS_CENTERIMAGE,
           kRX, kVizY+24, 42, 22, -1);
        Mk(L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER,
           kRX+46, kVizY+24, kRW-46, 22, IDC_LABEL_EDIT);

        // Action group box + radios
        Mk(L"BUTTON", L"Action", BS_GROUPBOX,
           kRX, kVizY+52, kRW, 88, -1);
        Mk(L"BUTTON", L"Keystroke", BS_AUTORADIOBUTTON|WS_GROUP,
           kRX+8, kVizY+68, 90, 18, IDC_ACTION_KEYSTROKE);
        Mk(L"BUTTON", L"Text",      BS_AUTORADIOBUTTON,
           kRX+102, kVizY+68, 55, 18, IDC_ACTION_TEXT);
        Mk(L"BUTTON", L"App",       BS_AUTORADIOBUTTON,
           kRX+161, kVizY+68, 50, 18, IDC_ACTION_APP);
        Mk(L"BUTTON", L"Media",     BS_AUTORADIOBUTTON,
           kRX+8, kVizY+90, 60, 18, IDC_ACTION_MEDIA);
        Mk(L"BUTTON", L"Disabled",  BS_AUTORADIOBUTTON,
           kRX+72, kVizY+90, 75, 18, IDC_ACTION_DISABLED);
        CheckRadioButton(hWnd, IDC_ACTION_KEYSTROKE, IDC_ACTION_DISABLED, IDC_ACTION_DISABLED);

        // Shortcut field
        Mk(L"STATIC", L"Shortcut:", SS_LEFT|SS_CENTERIMAGE,
           kRX, kVizY+148, 55, 22, -1);
        Mk(L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER,
           kRX+58, kVizY+148, kRW-58, 22, IDC_SHORTCUT_EDIT);

        // Record / Browse buttons
        Mk(L"BUTTON", L"Record\u2026", BS_PUSHBUTTON,
           kRX, kVizY+176, 100, 24, IDC_RECORD_BTN);
        Mk(L"BUTTON", L"Browse\u2026", BS_PUSHBUTTON,
           kRX+108, kVizY+176, 100, 24, IDC_BROWSE_BTN);

        // ── Horizontal separator above bottom bar ─────────────────
        Mk(L"STATIC", L"", SS_ETCHEDHORZ, 8, kBtnY-6, 564, 2, -1);

        // ── Bottom buttons ────────────────────────────────────────
        Mk(L"BUTTON", L"Save",             BS_DEFPUSHBUTTON, 8,       kBtnY, 88, 26, IDC_SAVE_BTN);
        Mk(L"BUTTON", L"Cancel",           BS_PUSHBUTTON,   104,      kBtnY, 88, 26, IDC_CANCEL_BTN);
        Mk(L"BUTTON", L"Reset to default", BS_PUSHBUTTON,   200,      kBtnY, 130,26, IDC_RESET_BTN);

        RefreshProfileCombo(hWnd, *st->cfg);
        UpdateViz(hWnd, st);
        PopEditor(hWnd, st);
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nm = reinterpret_cast<NMHDR*>(lp);
        if (nm->code == NM_CLICK && nm->idFrom == IDC_NUMPAD_VIZ) {
            VizState* vs = reinterpret_cast<VizState*>(
                GetWindowLongPtrW(st->hViz, GWLP_USERDATA));
            if (vs) {
                st->selKey = vs->selectedIdx;
                PopEditor(hWnd, st);
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        if (!st) return 0;
        WORD ctrl = LOWORD(wp), notif = HIWORD(wp);

        if (ctrl == IDC_PROFILE_COMBO && notif == CBN_SELCHANGE) {
            HWND hCb = GetDlgItem(hWnd, IDC_PROFILE_COMBO);
            int sel = ComboBox_GetCurSel(hCb);
            int newIdx = (int)st->cfg->profiles.size();  // "New profile…"

            if (sel == newIdx) {
                // Simple input dialog via MessageBox trick — use a custom prompt
                WCHAR name[128] = L"New Profile";
                // Re-use an existing pattern: nested loop + edit in a tiny window
                HWND hInput = CreateWindowExW(
                    WS_EX_DLGMODALFRAME, L"#32770",
                    L"New Profile",
                    WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
                    CW_USEDEFAULT, CW_USEDEFAULT, 300, 110,
                    hWnd, nullptr, st->hInst, nullptr);
                if (hInput) {
                    HFONT hf = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
                    auto Mk2 = [&](const wchar_t* cls, const wchar_t* txt, DWORD sty,
                                   int x,int y,int w,int h,int id)->HWND{
                        HWND hw=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|sty,
                            x,y,w,h,hInput,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                            st->hInst,nullptr);
                        SendMessageW(hw,WM_SETFONT,reinterpret_cast<WPARAM>(hf),TRUE);
                        return hw;
                    };
                    Mk2(L"STATIC",L"Profile name:",SS_LEFT,8,8,100,18,-1);
                    HWND hEdit=Mk2(L"EDIT",L"",ES_AUTOHSCROLL|WS_BORDER,8,28,275,22,IDC_PROFILE_NAME_EDIT);
                    Mk2(L"BUTTON",L"OK",    BS_DEFPUSHBUTTON,8, 60,80,24,IDOK);
                    Mk2(L"BUTTON",L"Cancel",BS_PUSHBUTTON,  96,60,80,24,IDCANCEL);
                    SetFocus(hEdit);

                    bool ok = false;
                    MSG m2;
                    while (IsWindow(hInput) && GetMessageW(&m2,nullptr,0,0)>0) {
                        if (m2.message==WM_COMMAND) {
                            if (LOWORD(m2.wParam)==IDOK) {
                                GetWindowTextW(hEdit,name,127);
                                ok=true; DestroyWindow(hInput); break;
                            } else if (LOWORD(m2.wParam)==IDCANCEL) {
                                DestroyWindow(hInput); break;
                            }
                        }
                        if (m2.message==WM_KEYDOWN && m2.wParam==VK_ESCAPE) {
                            DestroyWindow(hInput); break;
                        }
                        TranslateMessage(&m2); DispatchMessageW(&m2);
                    }

                    if (ok && name[0]) {
                        Profile np; np.name = name;
                        st->cfg->profiles.push_back(std::move(np));
                        st->cfg->activeProfile=(int)st->cfg->profiles.size()-1;
                    } else {
                        ComboBox_SetCurSel(hCb, st->cfg->activeProfile);
                        return 0;
                    }
                }
            } else if (sel >= 0 && sel < (int)st->cfg->profiles.size()) {
                st->cfg->activeProfile = sel;
            }

            RefreshProfileCombo(hWnd, *st->cfg);
            st->selKey = -1;
            UpdateViz(hWnd, st);
            PopEditor(hWnd, st);
            return 0;
        }

        if (ctrl == IDC_DEL_PROFILE_BTN) {
            if (st->cfg->profiles.size() <= 1) {
                MessageBoxW(hWnd, L"Cannot delete the last profile.", L"NumPad Hotkeys", MB_OK|MB_ICONWARNING);
            } else {
                int idx = st->cfg->activeProfile;
                st->cfg->profiles.erase(st->cfg->profiles.begin() + idx);
                if (st->cfg->activeProfile >= (int)st->cfg->profiles.size())
                    st->cfg->activeProfile = (int)st->cfg->profiles.size()-1;
                RefreshProfileCombo(hWnd, *st->cfg);
                st->selKey = -1;
                UpdateViz(hWnd, st);
                PopEditor(hWnd, st);
            }
            return 0;
        }

        if (ctrl == IDC_RECORD_BTN) {
            CommitEditor(hWnd, st);
            std::vector<WORD> newKeys;
            if (ConfigUI::RecordShortcut(hWnd, st->hInst, newKeys)) {
                if (st->selKey >= 0) {
                    const NumpadKeyDef& kd = kKeys[st->selKey];
                    DWORD key = MakeBindingKey(kd.vk, kd.extended);
                    auto& bm = st->cfg->profiles[st->cfg->activeProfile].bindings;
                    bm[key].keys   = newKeys;
                    bm[key].action = ActionType::Keystroke;
                    st->pendingKeys = newKeys;
                    SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, FormatCombo(newKeys).c_str());
                    CheckRadioButton(hWnd, IDC_ACTION_KEYSTROKE, IDC_ACTION_DISABLED,
                                     IDC_ACTION_KEYSTROKE);
                }
            }
            return 0;
        }

        if (ctrl == IDC_BROWSE_BTN) {
            WCHAR path[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hWnd;
            ofn.lpstrFile   = path;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0";
            ofn.Flags       = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                SetDlgItemTextW(hWnd, IDC_SHORTCUT_EDIT, path);
                CheckRadioButton(hWnd, IDC_ACTION_KEYSTROKE, IDC_ACTION_DISABLED, IDC_ACTION_APP);
            }
            return 0;
        }

        if (ctrl == IDC_SAVE_BTN) {
            CommitEditor(hWnd, st);
            st->saved = true;
            DestroyWindow(hWnd);
            return 0;
        }

        if (ctrl == IDC_CANCEL_BTN) {
            DestroyWindow(hWnd);
            return 0;
        }

        if (ctrl == IDC_RESET_BTN) {
            if (st->cfg->activeProfile < (int)st->cfg->profiles.size()) {
                AppConfig def = GetDefaultConfig();
                if (!def.profiles.empty())
                    st->cfg->profiles[st->cfg->activeProfile].bindings = def.profiles[0].bindings;
                st->selKey = -1;
                UpdateViz(hWnd, st);
                PopEditor(hWnd, st);
            }
            return 0;
        }

        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { DestroyWindow(hWnd); return 0; }
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        // Do NOT PostQuitMessage — that would kill the main tray message pump.
        // The nested loop in ConfigUI::Show exits via IsWindow() going false.
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

static void RegisterConfigClass(HINSTANCE hInst)
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = ConfigWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    wc.lpszClassName = L"NumPadHotkeysConfig";
    RegisterClassExW(&wc);
    done = true;
}

bool ConfigUI::Show(HWND hParent, HINSTANCE hInst, AppConfig& cfg)
{
    RegisterVizClass(hInst);
    RegisterConfigClass(hInst);

    ConfigState st;
    st.hInst = hInst;
    st.cfg   = &cfg;

    // Client area: 580 wide × 306 tall
    // Controls end at y=276+26=302; 4px bottom margin → 306.
    const DWORD kStyle   = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    const DWORD kExStyle = WS_EX_DLGMODALFRAME;
    const int   kClientW = 580, kClientH = 306;
    RECT adjRect = { 0, 0, kClientW, kClientH };
    AdjustWindowRectEx(&adjRect, kStyle, FALSE, kExStyle);
    int wndW = adjRect.right  - adjRect.left;
    int wndH = adjRect.bottom - adjRect.top;

    // NOTE: hParent must NOT be a HWND_MESSAGE window — message-only windows
    // cannot own visible windows; CreateWindowExW would silently return NULL.
    // We use nullptr so the config window is a top-level owned by the desktop.
    HWND hWnd = CreateWindowExW(
        kExStyle,
        L"NumPadHotkeysConfig",
        L"NumPad Hotkeys \u2014 Configuration  v1.0.0",
        kStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, wndW, wndH,
        nullptr, nullptr, hInst, &st);

    if (!hWnd) {
        WCHAR buf[128];
        swprintf_s(buf, L"Config window creation failed (error %lu).", GetLastError());
        MessageBoxW(nullptr, buf, L"NumPad Hotkeys", MB_OK|MB_ICONERROR);
        return false;
    }

    // Centre on parent / screen
    RECT rw; GetWindowRect(hWnd, &rw);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int wx = (sw - (rw.right-rw.left)) / 2;
    int wy = (sh - (rw.bottom-rw.top)) / 2;
    SetWindowPos(hWnd, nullptr, wx, wy, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // Enable InitCommonControls for combo box
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // Nested message loop — exits when the config window is destroyed.
    // Uses PeekMessage so a real WM_QUIT (app exit) is re-posted for the
    // main pump rather than swallowed here.
    MSG m;
    while (IsWindow(hWnd)) {
        if (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) {
                // Real shutdown — re-post so main loop also sees it, then stop.
                PostQuitMessage(static_cast<int>(m.wParam));
                break;
            }
            TranslateMessage(&m);
            DispatchMessageW(&m);
        } else {
            WaitMessage();  // sleep until the next message arrives
        }
    }

    return st.saved;
}

bool ConfigUI::RecordShortcut(HWND hParent, HINSTANCE hInst, std::vector<WORD>& keys)
{
    RecordState rec;
    rec.hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME|WS_EX_TOPMOST,
        L"STATIC",
        L"Press your shortcut\u2026  (Esc to cancel)",
        WS_POPUP|WS_CAPTION|WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 70,
        hParent, nullptr, hInst, nullptr);
    if (!rec.hWnd) return false;

    // Centre
    RECT rp={}, rw={};
    if (hParent) GetWindowRect(hParent, &rp);
    else { rp.right=GetSystemMetrics(SM_CXSCREEN); rp.bottom=GetSystemMetrics(SM_CYSCREEN); }
    GetWindowRect(rec.hWnd, &rw);
    SetWindowPos(rec.hWnd, nullptr,
        rp.left+(rp.right-rp.left-(rw.right-rw.left))/2,
        rp.top +(rp.bottom-rp.top-(rw.bottom-rw.top))/2,
        0,0, SWP_NOSIZE|SWP_NOZORDER);

    g_rec = &rec;
    rec.hHook = SetWindowsHookExW(WH_KEYBOARD_LL, RecordHookProc, nullptr, 0);
    if (!rec.hHook) { DestroyWindow(rec.hWnd); g_rec=nullptr; return false; }

    MSG m;
    while (IsWindow(rec.hWnd) && GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
        if (!IsWindow(rec.hWnd)) break;
    }

    UnhookWindowsHookEx(rec.hHook);
    g_rec = nullptr;
    if (IsWindow(rec.hWnd)) DestroyWindow(rec.hWnd);

    if (rec.captured) { keys = rec.keys; return true; }
    return false;
}
