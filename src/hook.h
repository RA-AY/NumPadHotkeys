// NumPad Hotkeys — hook engine public API
// Copyright (c) 2026 rainyApps.com — MIT License
#pragma once
#include <windows.h>
#include <initializer_list>
#include "config.h"

namespace HookEngine {

// Install the global WH_KEYBOARD_LL hook.
// hWnd: message-only window to receive WM_APP_HOOK_KEY notifications.
bool Install(HWND hWnd);

// Uninstall the hook.
void Uninstall();

// Replace the active binding map (thread-safe via atomic swap).
void SetBindings(const BindingMap& bindings, bool enabled);

// Send a synthetic key combo (presses all, then releases in reverse).
// Must NOT be called from inside the hook callback.
void SendCombo(std::initializer_list<WORD> keys);

// Returns true if the hook is currently installed.
bool IsInstalled();

} // namespace HookEngine
