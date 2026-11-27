/* TASK #142228: offline check of mod/shared/ini_defaults.h with the real generated tables.
 * Build twice (C, asserts on):  zig cc -O0 -UNDEBUG -DMOD_BR ...  /  -DMOD_TR ...   Run: ini_defaults_test.exe <history dir>
 *  1. every public release's shipped ini (history dir/<tag>/<Mod>.ini), untouched by the player, ends with every table key at the value
 *     a fresh install of the source ini has; a second run writes nothing.
 *  2. player edits stay: a value that never was a shipped default is kept; with a Rev marker, a value equal to an OLDER default (picked
 *     by the player after that update) is kept; a value still at the marker release's default moves.
 */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#ifdef MOD_BR
#include "../../Nioh3BossRematch/src/ini_defaults_gen.h"
#define MODNAME L"Nioh3BossRematch"
#else
#include "../../Nioh3Training/src/ini_defaults_gen.h"
#define MODNAME L"Nioh3Training"
#endif

static int g_logs;
static void L(const wchar_t* s, const wchar_t* k, const wchar_t* f, const wchar_t* t, const char* why) { (void)s; (void)k; (void)f; (void)t; (void)why; ++g_logs; }

static void Get(const wchar_t* ini, const wchar_t* s, const wchar_t* k, wchar_t* out, int n) {
    GetPrivateProfileStringW(s, k, INIDEF_MISSING, out, (DWORD)n, ini);
}
static const IniDefKey* Find(const wchar_t* key) {
    int i; for (i = 0; i < INI_DEFAULTS_N; ++i) if (!_wcsicmp(kIniDefaults[i].key, key)) return &kIniDefaults[i];
    return 0;
}
static int AllNow(const wchar_t* ini) {
    int i, bad = 0; wchar_t v[512];
    for (i = 0; i < INI_DEFAULTS_N; ++i) {
        Get(ini, kIniDefaults[i].sec, kIniDefaults[i].key, v, 512);
        if (wcscmp(v, kIniDefaults[i].now)) { ++bad; wprintf(L"  [%ls] %ls = %ls, want %ls\n", kIniDefaults[i].sec, kIniDefaults[i].key, v, kIniDefaults[i].now); }
    }
    return bad;
}

int main(int argc, char** argv) {
    wchar_t hist[MAX_PATH], tmp[MAX_PATH], ini[MAX_PATH], pat[MAX_PATH], src[MAX_PATH], v[512];
    WIN32_FIND_DATAW fd; HANDLE h; int tags = 0, wroteTotal = 0;
    assert(argc > 1 && INI_DEFAULTS_N > 0);
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, hist, MAX_PATH);
    GetTempPathW(MAX_PATH, tmp);
    _snwprintf(ini, MAX_PATH, L"%lsini_defaults_test_%ls.ini", tmp, MODNAME);
    /* 1. every shipped release ini, untouched */
    _snwprintf(pat, MAX_PATH, L"%ls\\v*", hist);
    h = FindFirstFileW(pat, &fd);
    assert(h != INVALID_HANDLE_VALUE);
    do {
        int w, w2;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        _snwprintf(src, MAX_PATH, L"%ls\\%ls\\%ls.ini", hist, fd.cFileName, MODNAME);
        if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) continue;
        assert(CopyFileW(src, ini, FALSE));
        w = IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        if (AllNow(ini)) { wprintf(L"FAIL %ls: keys not at the source defaults after the update\n", fd.cFileName); return 1; }
        w2 = IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        assert(w2 == 0);  /* same rev: never runs twice */
        assert(GetPrivateProfileIntW(L"IniDefaults", L"Rev", 0, ini) == INI_DEFAULTS_REV);
        wroteTotal += w; ++tags;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    assert(tags >= 40);
    wprintf(L"%ls: %d shipped release inis -> source defaults (%d keys written in total), rerun writes 0\n", MODNAME, tags, wroteTotal);

    /* 2. player edits */
    {
        const IniDefKey* k = 0; int i, j, w;
        const wchar_t *older = 0, *mid = 0; int midVer = 0;
        for (i = 0; i < INI_DEFAULTS_N && !k; ++i)  /* a key with two shipped values before now: older -> mid -> now */
            for (j = 1; j < kIniDefaults[i].n; ++j)
                if (kIniDefaults[i].hist[j - 1].val && kIniDefaults[i].hist[j].val && wcscmp(kIniDefaults[i].hist[j].val, kIniDefaults[i].now)) {
                    k = &kIniDefaults[i]; older = k->hist[j - 1].val; mid = k->hist[j].val; midVer = k->hist[j].ver; break;
                }
        if (!k) {  /* fall back: any key with one old shipped value */
            for (i = 0; i < INI_DEFAULTS_N && !k; ++i) for (j = 0; j < kIniDefaults[i].n; ++j)
                if (kIniDefaults[i].hist[j].val && wcscmp(kIniDefaults[i].hist[j].val, kIniDefaults[i].now)) { k = &kIniDefaults[i]; mid = k->hist[j].val; midVer = k->hist[j].ver; break; }
        }
        assert(k && mid);
        /* no marker, a player value that no release shipped: kept */
        DeleteFileW(ini);
        WritePrivateProfileStringW(k->sec, k->key, L"424242", ini);
        IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        Get(ini, k->sec, k->key, v, 512); assert(!wcscmp(v, L"424242"));
        /* no marker, the old shipped default: moved */
        DeleteFileW(ini);
        WritePrivateProfileStringW(k->sec, k->key, mid, ini);
        w = IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        Get(ini, k->sec, k->key, v, 512); assert(w >= 1 && !wcscmp(v, k->now));
        /* marker at the release that shipped mid, value still mid: moved */
        DeleteFileW(ini);
        { wchar_t r[16]; _snwprintf(r, 16, L"%d", midVer); WritePrivateProfileStringW(L"IniDefaults", L"Rev", r, ini); }
        WritePrivateProfileStringW(k->sec, k->key, mid, ini);
        IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        Get(ini, k->sec, k->key, v, 512); assert(!wcscmp(v, k->now));
        if (older) {  /* marker at mid's release, the player picked the OLDER default after that update: kept */
            DeleteFileW(ini);
            { wchar_t r[16]; _snwprintf(r, 16, L"%d", midVer); WritePrivateProfileStringW(L"IniDefaults", L"Rev", r, ini); }
            WritePrivateProfileStringW(k->sec, k->key, older, ini);
            IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
            Get(ini, k->sec, k->key, v, 512); assert(!wcscmp(v, older));
        }
        /* marker already at this rev: nothing runs */
        DeleteFileW(ini);
        { wchar_t r[16]; _snwprintf(r, 16, L"%d", INI_DEFAULTS_REV); WritePrivateProfileStringW(L"IniDefaults", L"Rev", r, ini); }
        WritePrivateProfileStringW(k->sec, k->key, mid, ini);
        assert(IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L) == 0);
        Get(ini, k->sec, k->key, v, 512); assert(!wcscmp(v, mid));
        /* no ini file: -1, nothing created */
        DeleteFileW(ini);
        assert(IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L) == -1);
        assert(GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES);
        wprintf(L"%ls: player edits kept, key [%ls] %ls (old %ls -> now %ls)\n", MODNAME, k->sec, k->key, mid, k->now);
    }
#ifdef MOD_BR
    {   /* the reported gap: an untouched v1.8.49 ini gets the v1.8.50 soul core settings */
        _snwprintf(src, MAX_PATH, L"%ls\\v1.8.49\\Nioh3BossRematch.ini", hist);
        assert(CopyFileW(src, ini, FALSE));
        Get(ini, L"Spirit", L"SealExitAfterShotMs", v, 512); assert(!wcscmp(v, L"700"));
        IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        Get(ini, L"Spirit", L"SealExitAfterShotMs", v, 512); assert(!wcscmp(v, L"0"));
        Get(ini, L"Spirit", L"FormCoreSpeed", v, 512); assert(!wcscmp(v, Find(L"FormCoreSpeed")->now));
        Get(ini, L"Spirit", L"FormCoresMove", v, 512); assert(!wcscmp(v, Find(L"FormCoresMove")->now));
        wprintf(L"v1.8.49 ini: SealExitAfterShotMs 700 -> 0, FormCoreSpeed %ls, FormCoresMove %ls\n", Find(L"FormCoreSpeed")->now, Find(L"FormCoresMove")->now);
    }
#else
    {
        _snwprintf(src, MAX_PATH, L"%ls\\v1.8.47\\Nioh3Training.ini", hist);
        assert(CopyFileW(src, ini, FALSE));
        Get(ini, L"StyleMeter", L"DisplacePts", v, 512); assert(!wcscmp(v, L"80"));
        IniDefaultsApply(ini, kIniDefaults, INI_DEFAULTS_N, INI_DEFAULTS_REV, L);
        Get(ini, L"StyleMeter", L"DisplacePts", v, 512); assert(!wcscmp(v, L"100"));
        Get(ini, L"StyleMeter", L"DecayLowPct", v, 512); assert(!wcscmp(v, L"78"));
        wprintf(L"v1.8.47 ini: DisplacePts 80 -> 100, DecayLowPct 60 -> 78\n");
    }
#endif
    DeleteFileW(ini);
    wprintf(L"ini_defaults_test PASS\n");
    return 0;
}
