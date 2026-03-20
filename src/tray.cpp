// NumPad Hotkeys — system tray wrapper
// Copyright (c) 2026 rainyApps.com — MIT License
#include <windows.h>
#include <shellapi.h>
#include "tray.h"
#include "resource.h"

static NOTIFYICONDATAW s_nid = {};
static HINSTANCE       s_hInst = nullptr;
static HICON           s_iconEnabled  = nullptr;
static HICON           s_iconDisabled = nullptr;

bool Tray::Init(HWND hWnd, HINSTANCE hInst)
{
    s_hInst = hInst;

    // Load icons from resources (fall back to system default if missing)
    s_iconEnabled  = static_cast<HICON>(LoadImageW(hInst,
        MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    s_iconDisabled = static_cast<HICON>(LoadImageW(hInst,
        MAKEINTRESOURCEW(IDI_DISABLED), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    if (!s_iconEnabled)
        s_iconEnabled = static_cast<HICON>(
            LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON, 16, 16, LR_SHARED));
    if (!s_iconDisabled)
        s_iconDisabled = s_iconEnabled;

    s_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    s_nid.hWnd             = hWnd;
    s_nid.uID              = 1;
    s_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_APP_TRAY;
    s_nid.hIcon            = s_iconEnabled;
    lstrcpynW(s_nid.szTip, L"NumPad Hotkeys", 128);

    if (!Shell_NotifyIconW(NIM_ADD, &s_nid)) return false;

    // Use NOTIFYICON_VERSION_4 for modern behaviour (WM_LBUTTONDBLCLK etc.)
    s_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &s_nid);
    return true;
}

void Tray::Destroy()
{
    Shell_NotifyIconW(NIM_DELETE, &s_nid);
}

void Tray::SetHookEnabled(bool enabled)
{
    s_nid.hIcon = enabled ? s_iconEnabled : s_iconDisabled;
    s_nid.uFlags = NIF_ICON | NIF_TIP;
    lstrcpynW(s_nid.szTip,
        enabled ? L"NumPad Hotkeys (active)"
                : L"NumPad Hotkeys (disabled)", 128);
    Shell_NotifyIconW(NIM_MODIFY, &s_nid);
}

void Tray::ShowContextMenu(HWND hWnd,
                            const std::vector<std::wstring>& profileNames,
                            int  activeProfile,
                            bool hookEnabled,
                            bool autostart)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // "Hook enabled" toggle (checkmark)
    AppendMenuW(hMenu, MF_STRING | (hookEnabled ? MF_CHECKED : 0),
                IDM_TOGGLE_HOOK, L"Hook enabled");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // "Configure…"
    AppendMenuW(hMenu, MF_STRING, IDM_CONFIGURE, L"Configure\u2026");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // "Active profile ▶" submenu
    HMENU hProfiles = CreatePopupMenu();
    for (int i = 0; i < static_cast<int>(profileNames.size()); ++i) {
        UINT flags = MF_STRING | (i == activeProfile ? MF_CHECKED : 0);
        AppendMenuW(hProfiles, flags,
                    IDM_PROFILE_BASE + i, profileNames[i].c_str());
    }
    AppendMenuW(hMenu, MF_POPUP,
                reinterpret_cast<UINT_PTR>(hProfiles), L"Active profile \u25BA");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // "Start with Windows" toggle
    AppendMenuW(hMenu, MF_STRING | (autostart ? MF_CHECKED : 0),
                IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // "Exit"
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    // Required so the menu dismisses when the user clicks away
    SetForegroundWindow(hWnd);

    POINT pt = {};
    GetCursorPos(&pt);
    // TPM_RETURNCMD: TrackPopupMenu returns the selected command ID
    TrackPopupMenuEx(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_NONOTIFY,
                     pt.x, pt.y, hWnd, nullptr);

    // Post a dummy message so the menu disappears when the user clicks away
    PostMessageW(hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);  // also destroys hProfiles (it's a child)
}
