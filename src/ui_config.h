// NumPad Hotkeys — configuration UI public API
// Copyright (c) 2026 rainyApps.com — MIT License
#pragma once
#include <windows.h>
#include "config.h"

namespace ConfigUI {

// Open the configuration dialog (modal).
// cfg: the current config; modified in-place if user saves.
// Returns true if the user saved changes.
bool Show(HWND hParent, HINSTANCE hInst, AppConfig& cfg);

// Show a modal "Press your shortcut..." dialog.
// Returns true if a combo was captured; keys filled with the vk codes.
bool RecordShortcut(HWND hParent, HINSTANCE hInst, std::vector<WORD>& keys);

} // namespace ConfigUI
