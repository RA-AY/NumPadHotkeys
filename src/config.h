// NumPad Hotkeys — config data model
// Copyright (c) 2026 rainyApps.com — MIT License
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------

enum class ActionType {
    Keystroke,      // send a key combo via SendInput
    Text,           // type a literal string via SendInput
    LaunchApp,      // ShellExecute a path
    MediaControl,   // VK_MEDIA_* single key
    Disabled        // suppress original key, send nothing
};

struct KeyBinding {
    std::wstring       label;
    ActionType         action  = ActionType::Disabled;
    std::vector<WORD>  keys;         // Keystroke: VK codes (mods first, then vk)
    std::wstring       textOrPath;   // Text / LaunchApp payload
};

// Key in BindingMap: low 16 bits = vkCode; bit 16 set if extended key
inline DWORD MakeBindingKey(WORD vkCode, bool extended)
{
    return static_cast<DWORD>(vkCode) | (extended ? 0x10000u : 0u);
}

using BindingMap = std::map<DWORD, KeyBinding>;

struct Profile {
    std::wstring name;
    BindingMap   bindings;
};

struct AppConfig {
    std::vector<Profile> profiles;
    int  activeProfile     = 0;
    bool startWithWindows  = false;
    bool enableHook        = true;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

AppConfig GetDefaultConfig();

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

std::wstring GetConfigPath();   // %APPDATA%\NumPadHotkeys\hotkeys.json

AppConfig LoadConfig();
void      SaveConfig(const AppConfig& cfg);

void SetAutostart(bool enable, const std::wstring& exePath);
bool GetAutostart();
