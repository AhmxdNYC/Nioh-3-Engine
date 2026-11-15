// btnstyle_migrate.h - one-time [Menu] ButtonStyle migration (#106253). Builds before Auto existed shipped ButtonStyle = Xbox, so a
// kept ini pins Xbox glyphs on PlayStation / Nintendo pads. On the first load of an ini with no ButtonStyleMigrated key:
// ButtonStyle = Xbox -> Auto (Auto still picks Xbox for Xbox / unknown pads); any other value (Auto, PlayStation, Nintendo, junk) is
// kept. ButtonStyleMigrated = 1 is then written in every case, so an Xbox the player picks later is never migrated again.
// A missing ini file is never created here. Returns true when ButtonStyle was rewritten.
#pragma once
#include <windows.h>
#include <cwchar>

namespace btnstyle_migrate {

inline bool Run(const wchar_t* ini) {
    if (!ini || !*ini || GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) return false;
    wchar_t m[16];
    GetPrivateProfileStringW(L"Menu", L"ButtonStyleMigrated", L"", m, 16, ini);
    if (*m) return false;  // already migrated (any value): leave the player's choice alone
    wchar_t b[96];
    GetPrivateProfileStringW(L"Menu", L"ButtonStyle", L"", b, 96, ini);
    const bool xbox = _wcsicmp(b, L"Xbox") == 0;
    if (xbox) WritePrivateProfileStringW(L"Menu", L"ButtonStyle", L"Auto", ini);
    WritePrivateProfileStringW(L"Menu", L"ButtonStyleMigrated", L"1", ini);
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini);  // flush the profile cache
    return xbox;
}

}  // namespace btnstyle_migrate
