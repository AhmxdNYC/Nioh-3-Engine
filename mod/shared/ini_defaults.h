/* ini_defaults.h - carry shipped ini default changes into an existing player ini (TASK #142228, human #142225).
 *
 * The updater never overwrites an ini (tools/updater/updater.py: update() keeps every existing .ini) and the update zip carries no
 * Nioh3BossRematch.ini / Nioh3Training.ini. So a player who updates keeps every value the old ini had and never gets a key added later.
 * v1.8.49 -> v1.8.50 kept SealExitAfterShotMs = 700 (tested and shipped 0) and lacked FormCoreSpeed / FormCoresMove (Enenra 1.3x +
 * steer); v1.8.47-1.8.49 left the style meter rebalance (DisplacePts 80 -> 100, ...) out of every updated Nioh3Training.ini.
 *
 * Rule, one for every key and every mod. The table (ini_defaults_gen.h, written by tools/gen_ini_defaults.py from the inis every public
 * release shipped, release/ini_defaults_history.json, plus the source ini that ships next) lists each key whose shipped value changed or
 * that some release did not ship, with the value history. [IniDefaults] Rev in the player's ini is the table revision applied last.
 *  - Rev present (applied before at rev M): a key moves to the new default only if the player's value is still the one shipped at M
 *    (they never changed it) and that default changed since; a key missing then and missing now is added. A value the player picked
 *    stays, even one equal to an older default.
 *  - No Rev (first run of this code; the ini can be from any release): a missing key is added; a value equal to a default that some
 *    release shipped, and not the current one, moves to the current default. Any other value is the player's and stays.
 * Then Rev = the table revision is written, so the same table never runs twice. Plain C (Nioh3Training is C), Win32 profile API only.
 */
#pragma once
#include <windows.h>
#include <wchar.h>

typedef struct IniDefHist { int ver; const wchar_t* val; } IniDefHist;  /* val 0 = that release did not ship the key */
typedef struct IniDefKey { const wchar_t* sec; const wchar_t* key; const wchar_t* now; const IniDefHist* hist; int n; } IniDefKey;
typedef void (*IniDefLogFn)(const wchar_t* sec, const wchar_t* key, const wchar_t* from, const wchar_t* to, const char* why);

#define INIDEF_MISSING L"\x01<none>"

/* the value the release at version ver shipped (0 = not shipped); hist is in version order, one entry per change */
static inline const wchar_t* IniDefAt(const IniDefKey* k, int ver, int* found) {
    const wchar_t* v = 0; int i; *found = 0;
    for (i = 0; i < k->n; ++i) { if (k->hist[i].ver > ver) break; v = k->hist[i].val; *found = 1; }
    return v;
}
static inline int IniDefSame(const wchar_t* a, const wchar_t* b) { return a && b && wcscmp(a, b) == 0; }

/* Returns the number of keys written (-1: the ini is not there). rev = the table revision (INI_DEFAULTS_REV). */
static inline int IniDefaultsApply(const wchar_t* ini, const IniDefKey* t, int n, int rev, IniDefLogFn log) {
    wchar_t cur[512];
    int i, wrote = 0, applied;
    if (!ini || GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) return -1;
    applied = (int)GetPrivateProfileIntW(L"IniDefaults", L"Rev", 0, ini);
    if (applied >= rev) return 0;
    for (i = 0; i < n; ++i) {
        const IniDefKey* k = &t[i];
        int missing, change = 0, j;
        const char* why = "";
        GetPrivateProfileStringW(k->sec, k->key, INIDEF_MISSING, cur, (DWORD)(sizeof cur / sizeof cur[0]), ini);
        missing = wcscmp(cur, INIDEF_MISSING) == 0;
        if (!missing && IniDefSame(cur, k->now)) continue;
        if (applied > 0) {
            int found = 0;
            const wchar_t* then = IniDefAt(k, applied, &found);
            if (!found) then = 0;  /* older than the history: the key did not exist yet */
            if (missing && !then) { change = 1; why = "added (not in the ini, new since the last update)"; }
            else if (!missing && then && IniDefSame(cur, then) && !IniDefSame(then, k->now)) { change = 1; why = "default changed since the last update"; }
        } else if (missing) {
            change = 1; why = "added (not in the ini)";
        } else {
            for (j = 0; j < k->n; ++j)
                if (IniDefSame(cur, k->hist[j].val)) { change = 1; why = "old shipped default"; break; }
        }
        if (!change) continue;
        if (WritePrivateProfileStringW(k->sec, k->key, k->now, ini)) {
            ++wrote;
            if (log) log(k->sec, k->key, missing ? L"(missing)" : cur, k->now, why);
        }
    }
    {
        wchar_t r[16], tmp[16];
        int a = rev > 0 ? rev : 0, m = 0, q = 0;
        do { tmp[m++] = (wchar_t)(L'0' + a % 10); a /= 10; } while (a && m < 15);  /* no swprintf: MSVC and mingw differ */
        while (m) r[q++] = tmp[--m];
        r[q] = 0;
        WritePrivateProfileStringW(L"IniDefaults", L"Rev", r, ini);
    }
    return wrote;
}
