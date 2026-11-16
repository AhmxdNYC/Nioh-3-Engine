// btnstyle_migrate.h - one-time [Menu] ButtonStyle migration (#106253). Builds before Auto existed shipped ButtonStyle = Xbox, so a
// kept ini pins Xbox glyphs on PlayStation / Nintendo pads. On the first load of an ini with no ButtonStyleMigrated key:
// ButtonStyle = Xbox -> Auto (Auto still picks Xbox for Xbox / unknown pads); any other value (Auto, PlayStation, Nintendo, junk) is
// kept. ButtonStyleMigrated = 1 is then written in every case, so an Xbox the player picks later is never migrated again.
// A missing ini file is never created here. Returns true when ButtonStyle was rewritten.
// #106435: the file is edited line by line in place (not WritePrivateProfileString, which drops the ini's 'Key = Value' spacing):
// only the ButtonStyle value is replaced, and the marker goes on the line after it with the same spacing around '=' and the same
// line ending. A UTF-16 or BOM ini or a read / write failure falls back to WritePrivateProfileString.
#pragma once
#include <windows.h>
#include <cwchar>
#include <string>
#include <vector>

namespace btnstyle_migrate {

namespace detail {
inline bool IsWs(char c) { return c == ' ' || c == '\t'; }
inline std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && IsWs(s[a])) ++a;
    while (b > a && IsWs(s[b - 1])) --b;
    return s.substr(a, b - a);
}
inline bool IEq(const std::string& a, const char* b) { return _stricmp(a.c_str(), b) == 0; }
inline bool ReadAll(const wchar_t* p, std::string& out) {
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (4 << 20);
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD n = 0;
        ok = out.empty() || (ReadFile(h, &out[0], (DWORD)out.size(), &n, nullptr) && n == out.size());
    }
    CloseHandle(h);
    return ok;
}
inline bool WriteAll(const wchar_t* p, const std::string& s) {
    std::wstring tmp = std::wstring(p) + L".migtmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD n = 0;
    bool ok = s.empty() || (WriteFile(h, s.data(), (DWORD)s.size(), &n, nullptr) && n == s.size());
    CloseHandle(h);
    if (ok) ok = MoveFileExW(tmp.c_str(), p, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(tmp.c_str());
    return ok;
}
struct Line { std::string body, eol; };
// Rewrites the text: returns false when the text cannot be handled (UTF-16, UTF-8 BOM), so the caller falls back to the profile API.
inline bool Rewrite(std::string& text, bool xbox) {
    if (text.size() >= 2 && (((unsigned char)text[0] == 0xFF && (unsigned char)text[1] == 0xFE) ||
                             ((unsigned char)text[0] == 0xFE && (unsigned char)text[1] == 0xFF))) return false;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) return false;  // the profile API reads a BOM ini differently: let it write
    std::vector<Line> v;  // split, keeping each line's own terminator
    std::string defEol, out;
    const std::string& t = text;
    for (size_t i = 0; i < t.size();) {
        size_t e = t.find_first_of("\r\n", i);
        if (e == std::string::npos) { v.push_back({t.substr(i), ""}); break; }
        size_t el = (t[e] == '\r' && e + 1 < t.size() && t[e + 1] == '\n') ? 2 : 1;
        v.push_back({t.substr(i, e - i), t.substr(e, el)});
        if (defEol.empty()) defEol = t.substr(e, el);
        i = e + el;
    }
    if (defEol.empty()) defEol = "\r\n";
    const size_t cnt = v.size();
    // find [Menu], its ButtonStyle line, its last key line, and a spacing sample
    long menu = -1, menuEnd = -1, bs = -1, lastKey = -1;
    std::string sep, anySep;
    for (size_t k = 0; k < cnt; ++k) {
        std::string tr = Trim(v[k].body);
        if (!tr.empty() && tr[0] == '[') {
            if (menu >= 0 && menuEnd < 0) menuEnd = (long)k;
            size_t c = tr.find(']');
            if (menu < 0 && c != std::string::npos && IEq(Trim(tr.substr(1, c - 1)), "Menu")) menu = (long)k;
            continue;
        }
        size_t eq = v[k].body.find('=');
        if (tr.empty() || tr[0] == ';' || tr[0] == '#' || eq == std::string::npos) continue;
        size_t a = eq, b = eq + 1;
        while (a > 0 && IsWs(v[k].body[a - 1])) --a;
        while (b < v[k].body.size() && IsWs(v[k].body[b])) ++b;
        std::string s = v[k].body.substr(a, b - a);
        if (anySep.empty()) anySep = s;
        if (menu >= 0 && menuEnd < 0) {
            lastKey = (long)k;
            if (sep.empty()) sep = s;
            if (bs < 0 && IEq(Trim(v[k].body.substr(0, eq)), "ButtonStyle")) { bs = (long)k; sep = s; }
        }
    }
    if (menu >= 0 && menuEnd < 0) menuEnd = (long)cnt;
    if (sep.empty()) sep = anySep.empty() ? " = " : anySep;
    if (xbox && bs >= 0) {
        std::string& b = v[bs].body;
        size_t p = b.find('=') + 1;
        while (p < b.size() && IsWs(b[p])) ++p;
        size_t q = b.size();
        while (q > p && IsWs(b[q - 1])) --q;
        b = b.substr(0, p) + "Auto" + b.substr(q);
    }
    const std::string marker = "ButtonStyleMigrated" + sep + "1";
    long after = bs >= 0 ? bs : (lastKey >= 0 ? lastKey : menu);  // insert after this line
    for (size_t k = 0; k < cnt; ++k) {
        out += v[k].body;
        bool last = k + 1 == cnt;
        if ((long)k == after) {
            out += v[k].eol.empty() ? defEol : v[k].eol;
            out += marker;
            out += last ? v[k].eol : defEol;  // no final newline before: none after
        } else {
            out += v[k].eol;
        }
    }
    if (menu < 0) {  // no [Menu] section: append one
        if (!out.empty() && out.back() != '\n' && out.back() != '\r') out += defEol;
        out += "[Menu]" + defEol + marker + defEol;
    }
    text = out;
    return true;
}
}  // namespace detail

inline bool Run(const wchar_t* ini) {
    if (!ini || !*ini || GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) return false;
    wchar_t m[16];
    GetPrivateProfileStringW(L"Menu", L"ButtonStyleMigrated", L"", m, 16, ini);
    if (*m) return false;  // already migrated (any value): leave the player's choice alone
    wchar_t b[96];
    GetPrivateProfileStringW(L"Menu", L"ButtonStyle", L"", b, 96, ini);
    const bool xbox = _wcsicmp(b, L"Xbox") == 0;
    std::string text;
    if (detail::ReadAll(ini, text) && detail::Rewrite(text, xbox) && detail::WriteAll(ini, text)) {
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini);  // drop any cached copy of the old file
        return xbox;
    }
    if (xbox) WritePrivateProfileStringW(L"Menu", L"ButtonStyle", L"Auto", ini);
    WritePrivateProfileStringW(L"Menu", L"ButtonStyleMigrated", L"1", ini);
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini);  // flush the profile cache
    return xbox;
}

}  // namespace btnstyle_migrate
