// NumPad Hotkeys — tray icon public API
// Copyright (c) 2026 rainyApps.com — MIT License
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace Tray {

// Initialise the tray icon. Must be called after window creation.
bool Init(HWND hWnd, HINSTANCE hInst);

// Remove the tray icon (call before exit).
void Destroy();

// Rebuild the context menu and display it at the current cursor position.
// profileNames: list of profile display names.
// activeProfile: index of currently active profile.
// hookEnabled: whether the hook is currently on.
// autostart: whether autostart is checked.
void ShowContextMenu(HWND hWnd,
                     const std::vector<std::wstring>& profileNames,
                     int  activeProfile,
                     bool hookEnabled,
                     bool autostart);

// Switch between the normal and greyed-out icon.
void SetHookEnabled(bool enabled);

} // namespace Tray
