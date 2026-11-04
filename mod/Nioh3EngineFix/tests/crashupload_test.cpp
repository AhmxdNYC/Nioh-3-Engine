// Offline check of the automatic crash upload (crashupload.cpp, bridge #64881). No game.
// usage: crashupload_test <ini> record   -> fake access violation -> one pending record, scrubbed, printed path
//        crashupload_test <ini> send     -> SendPendingNow; prints "sent N pending M"
// The ini should point [Report] CrashUrl at a local `py api/crash.py 8792` and set CrashSendDelayMs = 600000.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/crashupload.h"

static int fails = 0;
#define CHECK(x) do { if (!(x)) { ++fails; fprintf(stderr, "FAIL: %s\n", #x); } } while (0)

static EXCEPTION_RECORD g_er;
static CONTEXT g_ctx;
static int Capture(EXCEPTION_POINTERS* ep) {
    g_er = *ep->ExceptionRecord;
    g_ctx = *ep->ContextRecord;
    return EXCEPTION_EXECUTE_HANDLER;
}
static void __declspec(noinline) Fault() {
    __try {
        *(volatile int*)nullptr = 1;
    } __except (Capture(GetExceptionInformation())) {
    }
}

static void Dummy(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) { fprintf(stderr, "usage: crashupload_test <ini> record|send\n"); return 2; }
    crashupload::StartInfo si;
    si.log = &Dummy;
    si.iniPath = argv[1];
    si.selfModule = GetModuleHandleW(nullptr);
    si.base = (uintptr_t)GetModuleHandleW(nullptr);
    si.modVersion = "test";
    si.testMode = 1;
    crashupload::Start(si);
    CHECK(strlen(crashupload::InstallId()) == 32);
    if (wcscmp(argv[2], L"send") == 0) {
        const int sent = crashupload::SendPendingNow();
        printf("sent %d pending %d\n", sent, crashupload::PendingCount());
        return 0;
    }
    Sleep(3500);  // info thread (os / game / mods)
    char prof[300] = {}, user[100] = {}, pc[100] = {};
    GetEnvironmentVariableA("USERPROFILE", prof, sizeof prof);
    GetEnvironmentVariableA("USERNAME", user, sizeof user);
    GetEnvironmentVariableA("COMPUTERNAME", pc, sizeof pc);
    char l1[600], l2[200], l3[200], l4[200];
    snprintf(l1, sizeof l1, "[12:00:00.000] loaded %s\\AppData\\Local\\KoeiTecmo\\NIOH3\\save", prof);
    snprintf(l2, sizeof l2, "[12:00:01.000] user %s on %s steam 76561198012345678", user, pc);
    snprintf(l3, sizeof l3, "[12:00:02.000] other path D:\\Users\\Someone Else\\x.txt and /home/deck/.steam");
    snprintf(l4, sizeof l4, "[12:00:03.000] quote \" backslash \\ tab \t bad utf8 \xff\xfe end");
    const char* ring[6] = {"", l1, l2, l3, l4, nullptr};
    const int before = crashupload::PendingCount();
    Fault();
    CHECK(g_er.ExceptionCode == 0xC0000005);
    crashupload::OnCrash("unhandled exception 0xC0000005 ACCESS_VIOLATION", &g_er, &g_ctx, ring, 5, 0);
    CHECK(crashupload::PendingCount() == before + 1);
    // newest record
    wchar_t pat[MAX_PATH * 2];
    swprintf(pat, MAX_PATH * 2, L"%scrash-*.json", crashupload::PendingDir());
    WIN32_FIND_DATAW fd;
    std::wstring newest;
    FILETIME best{};
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (CompareFileTime(&fd.ftLastWriteTime, &best) >= 0) { best = fd.ftLastWriteTime; newest = fd.cFileName; } } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::wstring full = std::wstring(crashupload::PendingDir()) + newest;
    FILE* f = _wfopen(full.c_str(), L"rb");
    std::string t;
    if (f) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) t.append(b, n); fclose(f); }
    printf("record %ls (%zu bytes)\n", full.c_str(), t.size());
    for (const char* need : {"\"event\":\"crash\"", "\"code\":\"0xC0000005\"", "\"module\":\"crashupload_test.exe\"", "ACCESS_VIOLATION",
                             "\"access\":\"write 0x0\"", "\"mods\":[", "\"os\":\"", "\"test\":1", "<user>\\\\AppData", "<steamid>", "<pc>",
                             "other path <user>\\\\x.txt", "and <user>/.steam", "\\\"", "\\u0009","bad utf8 ?? end"})
        if (t.find(need) == std::string::npos) { ++fails; fprintf(stderr, "missing: %s\n", need); }
    CHECK(t.find("/home/deck") == std::string::npos);
    CHECK(t.find("Someone Else") == std::string::npos);
    CHECK(t.find("76561198012345678") == std::string::npos);
    if (strlen(user) >= 3) CHECK(t.find(user) == std::string::npos);
    if (strlen(pc) >= 3) CHECK(t.find(pc) == std::string::npos);
    CHECK(t.size() <= 64 * 1024);
    printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
