// spirit_upgrade.h - one-time upgrade of a BossRematch.ini from before soul-core forms (1.7.90 and older).
// Those inis have no [Spirit] SoulCoreForms key. The 1.7.90 [Spirit] values FormKey=0 and AttackCancel=0,
// and a missing FormHide (code default 0), leave the menu "Soul core transformations ON (FormKey off)" and
// never start a form. Write the 1.8 shipped values for those keys once. A present SoulCoreForms key, including 0,
// is the player's choice and is left alone, along with every other key.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace addon {
// Returns how many [Spirit] keys were written. 0 = SoulCoreForms already present, or no ini file.
inline int UpgradeAbsentSoulCoreForms(const wchar_t* ini) {
    if (!ini || !ini[0]) return 0;
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) return 0;
    wchar_t present[12] = {};
    GetPrivateProfileStringW(L"Spirit", L"SoulCoreForms", L"", present, 12, ini);
    if (present[0] != 0) return 0;
    struct Kv { const wchar_t* key; const wchar_t* val; };
    // FormKey=0: hooks never install. AttackCancel=0: AttackCancelInstall returns.
    // FormHide absent: FormHide4Tick returns before a form can start. Shipped 1.8 value is 4.
    static const Kv kOn[] = {
        {L"FormKey", L"1"},
        {L"AttackCancel", L"1"},
        {L"FormHide", L"4"},
        {L"SoulCoreForms", L"1"},
    };
    int n = 0;
    for (const Kv& e : kOn) {
        if (WritePrivateProfileStringW(L"Spirit", e.key, e.val, ini)) ++n;
    }
    // SealIdleLog's code default is 1. Leaving it absent would keep ScLog / FH4perf on after FormHide turns on.
    // Write 0 only when the key is absent, so an explicit 1 stays.
    wchar_t idle[12] = {};
    GetPrivateProfileStringW(L"Spirit", L"SealIdleLog", L"", idle, 12, ini);
    if (idle[0] == 0 && WritePrivateProfileStringW(L"Spirit", L"SealIdleLog", L"0", ini)) ++n;
    return n;
}
}  // namespace addon
