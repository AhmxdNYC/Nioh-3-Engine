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

// #97638: a fixed perfsnap-shaped snapshot; the record must carry it as "perf" and the site must accept it
static size_t FakePerf(char* out, size_t cap, bool crashPath) {
    const char* j = crashPath ? "{\"window_s\":10.0,\"frames\":601,\"avg_fps\":60.0,\"low1_fps\":55.2,\"bound\":\"gpu\",\"bound_why\":\"test\","
                                "\"gpu\":\"Test GPU\",\"vram_mb\":{\"total\":16304,\"used\":9120,\"budget\":15480},\"parts\":[{\"name\":\"GPU fence wait (GF)\",\"ms\":6.67}]}"
                              : "{}";
    return (size_t)snprintf(out, cap, "%s", j);
}

// #97734: a real-size perf block (~4 KB: 5 threads, parts, settings) inside the 64 KB record, with the training log at its cap
static bool g_bigPerf = false;
static size_t BigPerf(char* out, size_t cap, bool crashPath) {
    if (!crashPath) return (size_t)snprintf(out, cap, "{}");
    std::string j = "{\"window_s\":10.0,\"frames\":601,\"avg_fps\":60.0,\"low1_fps\":55.2,\"bound\":\"cpu\",\"bound_why\":\"game thread 14.2 ms per frame\","
                    "\"gpu\":\"AMD Radeon RX 9070\",\"cpu\":\"AMD Ryzen 7 7800X3D 8-Core Processor\",\"vram_mb\":{\"total\":16304,\"used\":9120,\"budget\":15480},"
                    "\"threads_window_s\":30.0,\"thread_count\":148,\"busiest_thread\":{\"name\":\"Nioh3.exe+0x1a2b30 / present\",\"tid\":4242,\"core_pct\":91.3},\"threads\":[";
    for (int i = 0; i < 5; ++i) {
        char b[160];
        snprintf(b, sizeof b, "%s{\"name\":\"amdxc64.dll+0x%x00 worker thread name padding\",\"tid\":%d,\"core_pct\":%.1f}", i ? "," : "", 0x1a2b + i, 4242 + i, 91.3 - i * 11.1);
        j += b;
    }
    j += "],\"game_settings\":{";
    for (int i = 0; i < 40; ++i) { char b[96]; snprintf(b, sizeof b, "%s\"REN_%02d\":\"%d\"", i ? "," : "", i, i * 7); j += b; }
    j += "},\"parts\":[";
    for (int i = 0; i < 20; ++i) { char b[120]; snprintf(b, sizeof b, "%s{\"name\":\"engine part number %d with a long descriptive name\",\"ms\":%.2f}", i ? "," : "", i, 0.5 + i); j += b; }
    j += "]}";
    return (size_t)snprintf(out, cap, "%s", j.c_str());
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
    // Fixtures live beside this isolated test executable, never in the game directory.
    wchar_t root[MAX_PATH]; GetModuleFileNameW(nullptr, root, MAX_PATH);
    wchar_t* slash = wcsrchr(root, L'\\'); if (slash) slash[1] = 0;
    auto fixture = [&](const wchar_t* name, const std::string& data) {
        std::wstring path = std::wstring(root) + name;
        FILE* out = _wfopen(path.c_str(), L"wb"); CHECK(out != nullptr);
        if (out) { fwrite(data.data(), 1, data.size(), out); fclose(out); }
    };
    fixture(L"Nioh3Training.ini", "[Summon]\nKiBreakLog=1\nKiBreakNoFinisher=0\nKiBreakActFix=1\nMoveBlock=0x123,0x456\nPrivateName=DO_NOT_UPLOAD\n[Moves.1234]\n0xABCD=0\nRepeatGapMs=500\n[Training]\nBossAggro=1\n");
    fixture(L"Nioh3BossRematch.ini", "[Spirit]\nSoulCoreForms=1\nUserName=DO_NOT_UPLOAD\n");
    g_bigPerf = wcscmp(argv[2], L"bigperf") == 0;
    const bool stress = wcscmp(argv[2], L"stress") == 0 || g_bigPerf;
    const bool utf16 = wcscmp(argv[2], L"utf16") == 0;
    if (utf16) {
        std::string encoded("\xff\xfe", 2);
        for (char c : std::string("[Spirit]\nSoulCoreForms=1\n")) { encoded += c; encoded += '\0'; }
        fixture(L"Nioh3BossRematch.ini", encoded);
    }
    std::string training;
    for (int i=0; i<25; ++i) training += "[12:00:10.000] EVENT" + std::to_string(i) + " " + std::string(stress ? 1900 : 600, stress ? '\t' : 'z') + " HP=125176/194938 END_EVENT\n";
    fixture(L"Nioh3Training.log", training);
    crashupload::Start(si);
    crashupload::SetPerfJson(g_bigPerf ? &BigPerf : &FakePerf);
    CHECK(strlen(crashupload::InstallId()) == 32);
    if (wcscmp(argv[2], L"send") == 0) {
        const int sent = crashupload::SendPendingNow();
        printf("sent %d pending %d\n", sent, crashupload::PendingCount());
        return 0;
    }
    Sleep(3500);  // info thread (os / game / mods)
    CHECK(LoadLibraryW(L"gameoverlayrenderer64.dll") != nullptr);
    CHECK(LoadLibraryW(L"System32\\excluded-system.dll") != nullptr);
    CHECK(LoadLibraryW(L"WinSxS\\excluded-sxs.dll") != nullptr);
    // DLLs loaded after the info snapshot must appear; enough to exercise truncation.
    for (int i = 0; i < 70; ++i) {
        wchar_t dll[64];
        swprintf(dll, 64, L"inventory-%02d.dll", i);
        CHECK(LoadLibraryW(dll) != nullptr);
    }
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
    g_ctx.Rdx = 0x123456789abcdef0ull;
    static unsigned long long known[12] = {0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0xc0ffee, 0x8888, 0x9999, 0xaaaa, 0xbbbb, 0xcccc};
    g_ctx.Rbx = (DWORD64)(uintptr_t)known;  // #72: readable pointer register -> "MEM rbx" line with its 12 qwords
    g_ctx.Rsi = 0x10;                       // not a pointer: skipped
    const bool nullContext = wcscmp(argv[2], L"null") == 0;
    const bool badStack = wcscmp(argv[2], L"badstack") == 0;
    if (badStack) { g_ctx.Rip = 0; g_ctx.Rsp = 1; }
    crashupload::OnCrash("unhandled exception 0xC0000005 ACCESS_VIOLATION", &g_er, nullContext ? nullptr : &g_ctx, ring, 5, 0);
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
        if (!stress && t.find(need) == std::string::npos) { ++fails; fprintf(stderr, "missing: %s\n", need); }
    CHECK(t.find("/home/deck") == std::string::npos);
    CHECK(t.find("Someone Else") == std::string::npos);
    CHECK(t.find("76561198012345678") == std::string::npos);
    if (strlen(user) >= 3) CHECK(t.find(user) == std::string::npos);
    if (strlen(pc) >= 3) CHECK(t.find(pc) == std::string::npos);
    CHECK(t.find("DO_NOT_UPLOAD") == std::string::npos);
    CHECK(t.find("HP=125176/194938 END_EVENT") != std::string::npos);

    if (!stress) CHECK(t.find("EVENT5 ") != std::string::npos);
    CHECK(t.find("SoulCoreForms") != std::string::npos);
    CHECK(t.find("MoveBlock") != std::string::npos);
    CHECK(t.find("0xABCD") != std::string::npos);
    CHECK(t.find("MEM fault 0x0: free") != std::string::npos);  // #72: fault region state
    if (!nullContext && !stress) CHECK(t.find("MEM rbx+30: c0ffee 8888 9999 aaaa bbbb cccc") != std::string::npos);
    CHECK(t.find("MEM rsi") == std::string::npos);
    if (nullContext) CHECK(t.find("\"registers\":null") != std::string::npos);
    else CHECK(t.find("0x123456789abcdef0") != std::string::npos);
    if (badStack) CHECK(t.find("\"stack_top\":[null,null") != std::string::npos);
    CHECK(t.size() <= 64 * 1024);
    printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
