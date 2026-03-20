// NumPad Hotkeys — configuration data model and JSON persistence
// Copyright (c) 2026 rainyApps.com — MIT License
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "config.h"

// ---------------------------------------------------------------------------
// Default binding set (Phase 7)
// ---------------------------------------------------------------------------

AppConfig GetDefaultConfig()
{
    AppConfig cfg;
    cfg.activeProfile    = 0;
    cfg.startWithWindows = false;
    cfg.enableHook       = true;

    Profile p;
    p.name = L"Default";

    auto Add = [&](WORD vk, bool ext, const wchar_t* label,
                   std::vector<WORD> keys)
    {
        KeyBinding b;
        b.label  = label;
        b.action = ActionType::Keystroke;
        b.keys   = std::move(keys);
        p.bindings[MakeBindingKey(vk, ext)] = std::move(b);
    };

    // Numpad 0 → Ctrl+C  (Copy)
    Add(VK_NUMPAD0,  false, L"Copy",       {VK_CONTROL, 'C'});
    // Numpad Enter → Ctrl+V  (Paste)
    Add(VK_RETURN,   true,  L"Paste",      {VK_CONTROL, 'V'});
    // Numpad . → Print Screen  (Screenshot)
    Add(VK_DECIMAL,  false, L"Screenshot", {VK_SNAPSHOT});
    // Numpad + → Ctrl+Alt+G
    Add(VK_ADD,      false, L"Custom",     {VK_CONTROL, VK_MENU, 'G'});
    // Numpad - → Ctrl+Z  (Undo)
    Add(VK_SUBTRACT, false, L"Undo",       {VK_CONTROL, 'Z'});
    // Numpad * → Ctrl+Y  (Redo)
    Add(VK_MULTIPLY, false, L"Redo",       {VK_CONTROL, 'Y'});
    // Numpad / → Ctrl+S  (Save)
    Add(VK_DIVIDE,   true,  L"Save",       {VK_CONTROL, 'S'});
    // Numpad 1 → Ctrl+A  (Select All)
    Add(VK_NUMPAD1,  false, L"Select All", {VK_CONTROL, 'A'});
    // Numpad 2 → Ctrl+F  (Find)
    Add(VK_NUMPAD2,  false, L"Find",       {VK_CONTROL, 'F'});
    // Numpad 3 → Ctrl+N  (New Window)
    Add(VK_NUMPAD3,  false, L"New Window", {VK_CONTROL, 'N'});
    // Numpad 4-9 unbound (pass through)

    cfg.profiles.push_back(std::move(p));
    return cfg;
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::wstring GetConfigPath()
{
    WCHAR appData[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData);
    std::wstring dir = std::wstring(appData) + L"\\NumPadHotkeys";
    CreateDirectoryW(dir.c_str(), nullptr);   // no-op if exists
    return dir + L"\\hotkeys.json";
}

// ---------------------------------------------------------------------------
// Minimal hand-rolled JSON helpers
// ---------------------------------------------------------------------------

// Escape a wstring for JSON output (UTF-16 → UTF-8 not needed here;
// we write UTF-16 wchar_t to a wofstream opened in UTF-8 mode below)
static std::wstring JsonEscape(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + 4);
    for (wchar_t c : s) {
        switch (c) {
        case L'"':  out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n";  break;
        case L'\r': out += L"\\r";  break;
        case L'\t': out += L"\\t";  break;
        default:    out += c;       break;
        }
    }
    return out;
}

static const wchar_t* ActionTypeName(ActionType a)
{
    switch (a) {
    case ActionType::Keystroke:    return L"keystroke";
    case ActionType::Text:         return L"text";
    case ActionType::LaunchApp:    return L"launchapp";
    case ActionType::MediaControl: return L"media";
    case ActionType::Disabled:     return L"disabled";
    }
    return L"disabled";
}

static ActionType ActionTypeFromName(const std::wstring& s)
{
    if (s == L"keystroke")  return ActionType::Keystroke;
    if (s == L"text")       return ActionType::Text;
    if (s == L"launchapp")  return ActionType::LaunchApp;
    if (s == L"media")      return ActionType::MediaControl;
    return ActionType::Disabled;
}

// ---------------------------------------------------------------------------
// SaveConfig — hand-rolled JSON serialiser
// ---------------------------------------------------------------------------

void SaveConfig(const AppConfig& cfg)
{
    std::wstring path = GetConfigPath();
    // Open as binary; write UTF-8 BOM + content manually using narrow stream
    // Simpler: write ANSI-safe JSON with wofstream (all our strings are ASCII-safe labels)
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"w,ccs=UTF-8");
    if (!f) return;

    auto W = [&](const wchar_t* s) { fwprintf(f, L"%s", s); };
    auto Ws = [&](const std::wstring& s) { fwprintf(f, L"%s", s.c_str()); };
    auto Wi = [&](int v) { fwprintf(f, L"%d", v); };

    W(L"{\n");
    W(L"  \"activeProfile\": "); Wi(cfg.activeProfile); W(L",\n");
    W(L"  \"startWithWindows\": "); W(cfg.startWithWindows ? L"true" : L"false"); W(L",\n");
    W(L"  \"enableHook\": "); W(cfg.enableHook ? L"true" : L"false"); W(L",\n");
    W(L"  \"profiles\": [\n");

    for (size_t pi = 0; pi < cfg.profiles.size(); ++pi) {
        const Profile& p = cfg.profiles[pi];
        W(L"    {\n");
        W(L"      \"name\": \""); Ws(JsonEscape(p.name)); W(L"\",\n");
        W(L"      \"bindings\": [\n");

        size_t bi = 0;
        for (auto& [key, bind] : p.bindings) {
            WORD  vkCode   = static_cast<WORD>(key & 0xFFFF);
            bool  extended = (key & 0x10000) != 0;

            W(L"        { \"vkCode\": "); Wi(vkCode);
            W(L", \"extended\": "); W(extended ? L"true" : L"false");
            W(L", \"label\": \""); Ws(JsonEscape(bind.label)); W(L"\"");
            W(L", \"action\": \""); W(ActionTypeName(bind.action)); W(L"\"");

            // keys array
            W(L", \"keys\": [");
            for (size_t ki = 0; ki < bind.keys.size(); ++ki) {
                Wi(bind.keys[ki]);
                if (ki + 1 < bind.keys.size()) W(L", ");
            }
            W(L"]");

            if (!bind.textOrPath.empty()) {
                W(L", \"textOrPath\": \""); Ws(JsonEscape(bind.textOrPath)); W(L"\"");
            }

            W(L" }");
            if (bi + 1 < p.bindings.size()) W(L",");
            W(L"\n");
            ++bi;
        }

        W(L"      ]\n");
        W(L"    }");
        if (pi + 1 < cfg.profiles.size()) W(L",");
        W(L"\n");
    }

    W(L"  ]\n");
    W(L"}\n");
    fclose(f);
}

// ---------------------------------------------------------------------------
// Minimal JSON parser (hand-rolled, handles our own output format only)
// ---------------------------------------------------------------------------

struct JsonParser {
    const std::wstring& src;
    size_t pos = 0;

    void SkipWs() {
        while (pos < src.size() && iswspace(src[pos])) ++pos;
    }
    bool Expect(wchar_t c) {
        SkipWs();
        if (pos < src.size() && src[pos] == c) { ++pos; return true; }
        return false;
    }
    std::wstring ParseString() {
        SkipWs();
        if (pos >= src.size() || src[pos] != L'"') return {};
        ++pos;
        std::wstring out;
        while (pos < src.size()) {
            wchar_t c = src[pos++];
            if (c == L'"') break;
            if (c == L'\\' && pos < src.size()) {
                wchar_t e = src[pos++];
                switch (e) {
                case L'"':  out += L'"';  break;
                case L'\\': out += L'\\'; break;
                case L'n':  out += L'\n'; break;
                case L'r':  out += L'\r'; break;
                case L't':  out += L'\t'; break;
                default:    out += e;     break;
                }
            } else {
                out += c;
            }
        }
        return out;
    }
    long long ParseInt() {
        SkipWs();
        bool neg = false;
        if (pos < src.size() && src[pos] == L'-') { neg = true; ++pos; }
        long long v = 0;
        while (pos < src.size() && iswdigit(src[pos]))
            v = v * 10 + (src[pos++] - L'0');
        return neg ? -v : v;
    }
    bool ParseBool() {
        SkipWs();
        if (src.compare(pos, 4, L"true") == 0)  { pos += 4; return true; }
        if (src.compare(pos, 5, L"false") == 0) { pos += 5; return false; }
        return false;
    }
    // parse key: value pair key (advances past the colon)
    std::wstring ParseKey() {
        SkipWs();
        std::wstring k = ParseString();
        Expect(L':');
        return k;
    }
    std::vector<long long> ParseIntArray() {
        std::vector<long long> arr;
        if (!Expect(L'[')) return arr;
        SkipWs();
        if (pos < src.size() && src[pos] == L']') { ++pos; return arr; }
        while (true) {
            arr.push_back(ParseInt());
            SkipWs();
            if (pos < src.size() && src[pos] == L',') { ++pos; continue; }
            break;
        }
        Expect(L']');
        return arr;
    }
};

AppConfig LoadConfig()
{
    std::wstring path = GetConfigPath();

    // Read file into wstring
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"r,ccs=UTF-8");
    if (!f) return GetDefaultConfig();

    std::wstring src;
    wchar_t buf[1024];
    while (fgetws(buf, 1024, f)) src += buf;
    fclose(f);

    if (src.empty()) return GetDefaultConfig();

    // Strip UTF-8 BOM if present (shows as 0xFEFF after conversion)
    if (!src.empty() && src[0] == 0xFEFF) src.erase(0, 1);

    AppConfig cfg;

    try {
        JsonParser p{src};
        if (!p.Expect(L'{')) return GetDefaultConfig();

        while (true) {
            p.SkipWs();
            if (p.pos >= src.size() || src[p.pos] == L'}') break;
            std::wstring key = p.ParseKey();

            if (key == L"activeProfile") {
                cfg.activeProfile = static_cast<int>(p.ParseInt());
            } else if (key == L"startWithWindows") {
                cfg.startWithWindows = p.ParseBool();
            } else if (key == L"enableHook") {
                cfg.enableHook = p.ParseBool();
            } else if (key == L"profiles") {
                if (!p.Expect(L'[')) return GetDefaultConfig();
                p.SkipWs();
                while (p.pos < src.size() && src[p.pos] != L']') {
                    if (!p.Expect(L'{')) break;
                    Profile prof;
                    while (true) {
                        p.SkipWs();
                        if (p.pos >= src.size() || src[p.pos] == L'}') break;
                        std::wstring pk = p.ParseKey();
                        if (pk == L"name") {
                            prof.name = p.ParseString();
                        } else if (pk == L"bindings") {
                            if (!p.Expect(L'[')) break;
                            p.SkipWs();
                            while (p.pos < src.size() && src[p.pos] != L']') {
                                if (!p.Expect(L'{')) break;
                                WORD  vkCode   = 0;
                                bool  extended = false;
                                KeyBinding bind;
                                while (true) {
                                    p.SkipWs();
                                    if (p.pos >= src.size() || src[p.pos] == L'}') break;
                                    std::wstring bk = p.ParseKey();
                                    if (bk == L"vkCode")     vkCode = static_cast<WORD>(p.ParseInt());
                                    else if (bk == L"extended")   extended = p.ParseBool();
                                    else if (bk == L"label")      bind.label = p.ParseString();
                                    else if (bk == L"action")     bind.action = ActionTypeFromName(p.ParseString());
                                    else if (bk == L"keys") {
                                        auto arr = p.ParseIntArray();
                                        for (auto v : arr) bind.keys.push_back(static_cast<WORD>(v));
                                    }
                                    else if (bk == L"textOrPath") bind.textOrPath = p.ParseString();
                                    p.SkipWs();
                                    if (p.pos < src.size() && src[p.pos] == L',') { ++p.pos; continue; }
                                    break;
                                }
                                p.Expect(L'}');
                                prof.bindings[MakeBindingKey(vkCode, extended)] = std::move(bind);
                                p.SkipWs();
                                if (p.pos < src.size() && src[p.pos] == L',') { ++p.pos; }
                                p.SkipWs();
                            }
                            p.Expect(L']');
                        } else {
                            // skip unknown value (just advance past comma)
                        }
                        p.SkipWs();
                        if (p.pos < src.size() && src[p.pos] == L',') { ++p.pos; continue; }
                        break;
                    }
                    p.Expect(L'}');
                    cfg.profiles.push_back(std::move(prof));
                    p.SkipWs();
                    if (p.pos < src.size() && src[p.pos] == L',') { ++p.pos; }
                    p.SkipWs();
                }
                p.Expect(L']');
            }
            p.SkipWs();
            if (p.pos < src.size() && src[p.pos] == L',') { ++p.pos; continue; }
            break;
        }
    } catch (...) {
        return GetDefaultConfig();
    }

    if (cfg.profiles.empty()) return GetDefaultConfig();
    if (cfg.activeProfile < 0 || cfg.activeProfile >= (int)cfg.profiles.size())
        cfg.activeProfile = 0;

    return cfg;
}

// ---------------------------------------------------------------------------
// Autostart
// ---------------------------------------------------------------------------

static const wchar_t* kRunKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kRunValue = L"NumPadHotkeys";

void SetAutostart(bool enable, const std::wstring& exePath)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;

    if (enable) {
        std::wstring quoted = L"\"" + exePath + L"\"";
        RegSetValueExW(hKey, kRunValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(quoted.c_str()),
            static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, kRunValue);
    }
    RegCloseKey(hKey);
}

bool GetAutostart()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return false;
    DWORD type = 0;
    DWORD size = 0;
    bool  found = (RegQueryValueExW(hKey, kRunValue, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return found;
}
