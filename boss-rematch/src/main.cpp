// main.cpp - Nioh3BossRematch.asi entry: log, ini, coexistence, decrypt wait, site resolution, feature start.
// Coexistence: a named mutex (Local\Nioh3BossRematch.<pid>) keeps a second copy from installing anything; the perf ASI
// (Nioh3EngineFix >= 1.7.41) hooks no boss site, and the only site both may touch (the per-frame call, used by its debug Tour) is
// chained by hooks.cpp whichever loads first (the Tour refuses to install over us; logged by both).
#include "addon.h"
#include "sites_gen.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

extern "C" __declspec(dllexport) const char* Nioh3BossRematch_Version() { return ADDON_VERSION; }

namespace addon {
bool CodeReady();
namespace {
wchar_t g_dir[MAX_PATH] = L"", g_ini[MAX_PATH] = L"", g_logPath[MAX_PATH] = L"";
SRWLOCK g_logLock = SRWLOCK_INIT;
HANDLE g_logFile = INVALID_HANDLE_VALUE;
HMODULE g_self = nullptr;
}  // namespace

void Log(const char* fmt, ...) {
    char b[2048];
    SYSTEMTIME st;
    GetLocalTime(&st);
    int n = snprintf(b, sizeof(b), "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list a;
    va_start(a, fmt);
    const int m = vsnprintf(b + n, sizeof(b) - (size_t)n - 3, fmt, a);
    va_end(a);
    n = m < 0 ? n : n + m;
    if (n > (int)sizeof(b) - 3) n = (int)sizeof(b) - 3;
    b[n++] = '\r';
    b[n++] = '\n';
    AcquireSRWLockExclusive(&g_logLock);
    if (g_logFile != INVALID_HANDLE_VALUE) {
        DWORD w = 0;
        WriteFile(g_logFile, b, (DWORD)n, &w, nullptr);
    }
    ReleaseSRWLockExclusive(&g_logLock);
}
const wchar_t* IniPath() { return g_ini; }
const wchar_t* ModDir() { return g_dir; }
int IniInt(const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, g_ini); }

bool Copy(void* dst, uint64_t src, size_t n) {
    if (src < 0x10000) return false;
    __try {
        memcpy(dst, (const void*)src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool WrU32(uint64_t a, uint32_t v) { return WrBytes(a, &v, 4); }
bool WrRaw(uint64_t a, const void* src, size_t n) {
    __try {
        memcpy((void*)a, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool WrBytes(uint64_t a, const void* src, size_t n) {
    if (a < 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi;
    const DWORD rw = PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    DWORD old = 0;
    bool prot = false;
    if (VirtualQuery((void*)a, &mbi, sizeof(mbi)) && !(mbi.Protect & rw)) {
        if (!VirtualProtect((void*)a, n, PAGE_READWRITE, &old)) return false;
        prot = true;
    }
    const bool ok = WrRaw(a, src, n);
    if (prot) VirtualProtect((void*)a, n, old, &old);
    return ok;
}
void Hex(char* out, size_t cap, uint64_t addr, size_t len) {
    uint8_t tmp[0x100];
    if (len > sizeof(tmp)) len = sizeof(tmp);
    if (!Copy(tmp, addr, len)) {
        snprintf(out, cap, "<unreadable>");
        return;
    }
    size_t o = 0;
    for (size_t i = 0; i < len && o + 4 < cap; ++i) o += (size_t)snprintf(out + o, cap - o, i ? " %02X" : "%02X", tmp[i]);
    out[o < cap ? o : cap - 1] = 0;
}

namespace {

void Feature(const char* name, const char* group) {
    const char* why = nullptr;
    if (InstallGroup(group, &why)) Log("%s: group %s installed", name, group);
    else Log("%s: group %s OFF - %s", name, group, why ? why : "?");
}

DWORD WINAPI Init(void*) {
    const DWORD t0 = GetTickCount();
    const int waitSec = IniInt(L"General", L"DecryptWaitSeconds", 120);
    while (!CodeReady()) {
        if (GetTickCount() - t0 > (DWORD)waitSec * 1000) {
            Log("code did not reach a known state within %d s - resolving anyway (every site is still pattern-checked)", waitSec);
            break;
        }
        Sleep(g_img.knownBuild ? 20 : 250);
    }
    Sleep(200);
    Log("code ready %lu ms after start", GetTickCount() - t0);
    ResolveAll();
    const bool boss = IniInt(L"Debug", L"BossLog", 0) != 0, rm = IniInt(L"Debug", L"RematchLog", 0) != 0, inv = IniInt(L"Debug", L"InvLog", 0) != 0;
    bosslog::Register(boss, rm, inv);
    if (boss || rm) {
        Feature("BossLog", "FIGHT");
        Feature("BossLog", "BOSSLOG");
    }
    if (rm) Feature("RematchLog", "RLOG");
    if (inv) Feature("InvLog", "INV");
    const bool live = rematch::Start();
    if (live) ui::Start();
    remote::Start();
    Log("ready (%lu ms): BossLog %d, RematchLog %d, InvLog %d, Rematch %s", GetTickCount() - t0, boss, rm, inv, live ? "live" : "off");
    return 0;
}

void Boot() {
    GetModuleFileNameW(g_self, g_dir, MAX_PATH);
    if (wchar_t* s = wcsrchr(g_dir, L'\\')) s[1] = 0;
    swprintf_s(g_ini, L"%sNioh3BossRematch.ini", g_dir);
    swprintf_s(g_logPath, L"%sNioh3BossRematch.log", g_dir);
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* en = wcsrchr(exe, L'\\');
    en = en ? en + 1 : exe;
    if (_wcsnicmp(en, L"nioh3", 5) != 0) return;  // ASI loaders can load us into helper processes: do nothing there
    if (!GetPrivateProfileIntW(L"General", L"Enabled", 1, g_ini)) return;
    wchar_t mx[64];
    swprintf_s(mx, L"Local\\Nioh3BossRematch.%lu", GetCurrentProcessId());
    HANDLE m = CreateMutexW(nullptr, TRUE, mx);
    if (!m || GetLastError() == ERROR_ALREADY_EXISTS) return;  // a second copy (renamed ASI): the first one owns every site
    g_logFile = CreateFileW(g_logPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    g_img.base = (uintptr_t)GetModuleHandleW(nullptr);
    const auto* dos = (const IMAGE_DOS_HEADER*)g_img.base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_img.base + dos->e_lfanew);
    g_img.end = g_img.base + nt->OptionalHeader.SizeOfImage;
    g_img.timestamp = nt->FileHeader.TimeDateStamp;
    g_img.sizeOfImage = nt->OptionalHeader.SizeOfImage;
    g_img.knownBuild = g_img.timestamp == sites::kKnownTimestamp && g_img.sizeOfImage == sites::kKnownSizeOfImage;
    const HMODULE perf = GetModuleHandleW(L"Nioh3EngineFix.asi");
    Log("Nioh3BossRematch %s (separate add-on; boss logger + rematch). Game %ls, TimeDateStamp 0x%08X, SizeOfImage 0x%X: %s. Nioh3EngineFix.asi %s.",
        ADDON_VERSION, en, g_img.timestamp, g_img.sizeOfImage, g_img.knownBuild ? "known build v2.02 (fast path)" : "UNKNOWN build - every site by pattern scan",
        perf ? "loaded (no shared boss sites)" : "not loaded (fine)");
    if (HANDLE t = CreateThread(nullptr, 0, Init, nullptr, 0, nullptr)) CloseHandle(t);
}

}  // namespace
}  // namespace addon

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        addon::g_self = h;
        addon::Boot();
    }
    return TRUE;
}
