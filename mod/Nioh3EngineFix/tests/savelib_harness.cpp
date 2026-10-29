// Offline harness for savelib.cpp (native save library). Never touches the installed save root: every tree is a temp copy.
// Build: cl /EHsc /std:c++17 /DUNICODE /D_UNICODE tests\savelib_harness.cpp src\savelib.cpp src\savefiles.cpp bcrypt.lib shell32.lib
// Run:   savelib_harness.exe <copied save root with Savedata\<id>\SAVEDATA00> [<python-made library copy> <slot>] [<out dir for crypto dumps>]
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <string>
#include "../src/savelib.h"

static int checks = 0, fails = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++fails; fprintf(stderr, "FAIL line %d: %s  [last: %s]\n", __LINE__, #x, savelib::LastResult()); } } while (0)

static bool g_gate = true;
static bool Gate() { return g_gate; }

static std::string ReadAll(const std::wstring& p) {
    HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER sz{}; GetFileSizeEx(f, &sz);
    std::string d((size_t)sz.QuadPart, 0); DWORD n = 0;
    ReadFile(f, d.data(), (DWORD)d.size(), &n, nullptr); CloseHandle(f); d.resize(n);
    return d;
}
static void WriteAll(const std::wstring& p, const std::string& d) {
    HANDLE f = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    DWORD n = 0; WriteFile(f, d.data(), (DWORD)d.size(), &n, nullptr); CloseHandle(f);
}
static bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static void CopyTree(const std::wstring& src, const std::wstring& dst) {
    CreateDirectoryW(dst.c_str(), nullptr);
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW((src + L"\\*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) continue;
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) CopyTree(src + L"\\" + d.cFileName, dst + L"\\" + d.cFileName);
        else CopyFileW((src + L"\\" + d.cFileName).c_str(), (dst + L"\\" + d.cFileName).c_str(), TRUE);
    } while (FindNextFileW(h, &d));
    FindClose(h);
}
static std::wstring FirstAccount(const std::wstring& root) {
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW((root + L"\\Savedata\\*").c_str(), &d);
    std::wstring r;
    if (h == INVALID_HANDLE_VALUE) return r;
    do { if ((d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcslen(d.cFileName) == 17) r = d.cFileName; } while (FindNextFileW(h, &d));
    FindClose(h);
    return r;
}
static std::string N(const std::wstring& w) { std::string s; for (wchar_t c : w) s += (char)c; return s; }

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fprintf(stderr, "usage: savelib_harness <save root copy> [<library copy> <slot>] [<dump dir>]\n"); return 2; }
    savelib::SetSettleMs(200);
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    const std::wstring work = std::wstring(tmp) + L"n3savelib-" + std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(work.c_str(), nullptr);
    const std::wstring root = work + L"\\NIOH3", lib = work + L"\\Library\\library";
    CopyTree(argv[1], root);
    const std::wstring acct = FirstAccount(root);
    CHECK(!acct.empty());
    const std::string acctA = N(acct);
    const std::wstring prof = root + L"\\Savedata\\" + acct + L"\\SAVEDATA00\\SAVEDATA.BIN";
    const std::string blob = ReadAll(prof);

    // crypto: validate + restamp round trip
    uint64_t at = 0; std::string err;
    CHECK(savelib::ValidateSave(blob, acctA, &at, &err));
    printf("validate: %s savedAt=%llu\n", err.empty() ? "ok" : err.c_str(), (unsigned long long)at);
    CHECK(!savelib::ValidateSave(blob, "76561190000000000", nullptr, &err));
    std::string bad = blob; bad[0x200] ^= 1;
    CHECK(!savelib::ValidateSave(bad, acctA, nullptr, &err)); printf("tampered: %s\n", err.c_str());
    const std::string re = savelib::Restamp(blob, at + 100);
    CHECK(savelib::SavedAt(re) == at + 101);
    CHECK(savelib::ValidateSave(re, acctA, nullptr, &err));  // the header stamp is outside the body checksum
    CHECK(savelib::Restamp(blob, at - 1) == blob);
    if (argc >= 3 && argc != 4) {  // dumps for the Python cross-check
        const std::wstring dump = argv[argc == 3 ? 2 : 4];
        WriteAll(dump + L"\\native-decrypt.bin", savelib::DecryptForTest(blob));
        WriteAll(dump + L"\\native-restamp.bin", re);
        char b[64]; snprintf(b, sizeof b, "%08x %llu", savelib::ChecksumForTest(savelib::DecryptForTest(blob)), (unsigned long long)at);
        WriteAll(dump + L"\\native-checksum.txt", b);
    }

    // bootstrap: creates the library, real root untouched
    CHECK(savelib::RunBootstrap(lib, root));
    printf("bootstrap: %s\n", savelib::LastResult());
    CHECK(Exists(lib + L"\\sandbox.json") && Exists(lib + L"\\slots\\My current save\\manifest.json"));
    CHECK(Exists(lib + L"\\current\\Savedata\\" + acct + L"\\SAVEDATA00\\BACKUP.BIN"));
    CHECK(savelib::VerifySlot(lib, "My current save"));
    CHECK(savelib::RunBootstrap(lib, root));  // existing library: verified, unchanged
    CHECK(strstr(savelib::LastResult(), "existing") != nullptr);
    CHECK(!savelib::RunBootstrap(root + L"\\inside", root));  // library inside the source: refused
    CHECK(!savelib::RunBootstrap(work + L"\\KoeiTecmo\\lib", root));  // installed-save-like path: refused

    // backup (full-root snapshot at the title)
    g_gate = false;
    CHECK(!savelib::RunBackup(lib, root, "Gate Closed", L"t1", &Gate));
    CHECK(!Exists(lib + L"\\slots\\Gate Closed"));
    g_gate = true;
    CHECK(savelib::RunBackup(lib, root, "Test Backup", L"t2", &Gate));
    printf("backup: %s\n", savelib::LastResult());
    CHECK(savelib::VerifySlot(lib, "Test Backup"));
    CHECK(!savelib::RunBackup(lib, root, "Test Backup", L"t3", &Gate));  // immutable: never overwrite
    CHECK(!savelib::RunBackup(lib, root, "../escape", L"t4", &Gate));

    // live import: new SAVEDATA01 published, newest stamp, existing profile untouched
    const std::string before00 = ReadAll(prof);
    CHECK(savelib::RunLiveImport(lib, root, "My current save", L"live-1", true, &Gate));
    printf("live: %s\n", savelib::LastResult());
    const std::wstring p01 = root + L"\\Savedata\\" + acct + L"\\SAVEDATA01\\SAVEDATA.BIN";
    CHECK(Exists(p01) && Exists(root + L"\\Savedata\\" + acct + L"\\SAVEDATA01\\BACKUP.BIN"));
    const std::string imported = ReadAll(p01);
    CHECK(savelib::ValidateSave(imported, acctA, nullptr, &err));
    CHECK(savelib::SavedAt(imported) > at);
    CHECK(ReadAll(prof) == before00);
    CHECK(Exists(lib + L"\\request-live-1\\receipt.json"));
    CHECK(strstr(ReadAll(lib + L"\\request-live-1\\receipt.json").c_str(), "\"imported\"") != nullptr);
    CHECK(!savelib::RunLiveImport(lib, root, "My current save", L"live-1", true, &Gate));  // token reuse refused
    // the full-root backup slot has two profiles now? (no: made before the import) - import it too: picks its newest valid profile
    CHECK(savelib::RunLiveImport(lib, root, "Test Backup", L"live-2", true, &Gate));
    CHECK(Exists(root + L"\\Savedata\\" + acct + L"\\SAVEDATA02\\SAVEDATA.BIN"));
    // gate closes -> refused, nothing published
    g_gate = false;
    CHECK(!savelib::RunLiveImport(lib, root, "My current save", L"live-3", true, &Gate));
    CHECK(!Exists(root + L"\\Savedata\\" + acct + L"\\SAVEDATA03"));
    g_gate = true;
    // cancel marker -> refused
    WriteAll(lib + L"\\cancel-live-4", "");
    CHECK(!savelib::RunLiveImport(lib, root, "My current save", L"live-4", true, &Gate));
    CHECK(!Exists(root + L"\\Savedata\\" + acct + L"\\SAVEDATA03"));
    // tampered slot -> refused
    {
        const std::wstring f = lib + L"\\slots\\My current save\\data\\Savedata\\" + acct + L"\\SAVEDATA00\\BACKUP.BIN";
        std::string d = ReadAll(f); d[100] ^= 1; WriteAll(f, d);
        CHECK(!savelib::VerifySlot(lib, "My current save"));
        CHECK(!savelib::RunLiveImport(lib, root, "My current save", L"live-5", true, &Gate));
        d[100] ^= 1; WriteAll(f, d);
        CHECK(savelib::VerifySlot(lib, "My current save"));
    }
    // threaded entry point (what savefiles uses)
    HANDLE h = (HANDLE)savelib::StartLiveImport(lib, root, "My current save", L"live-6", true, &Gate);
    CHECK(h != nullptr);
    if (h) { WaitForSingleObject(h, 60000); DWORD c = 9; GetExitCodeThread(h, &c); CloseHandle(h); CHECK(c == 0); }
    CHECK(Exists(root + L"\\Savedata\\" + acct + L"\\SAVEDATA03\\SAVEDATA.BIN"));

    // Python-made library: native verify reads its manifest.json
    if (argc >= 4) {
        CHECK(savelib::VerifySlot(argv[2], N(argv[3])));
        printf("python library slot verify: %s\n", savelib::LastResult()[0] ? savelib::LastResult() : "ok");
    }
    printf("%s: %d checks, %d failed (work dir %ls)\n", fails ? "FAILED" : "ALL PASSED", checks, fails, work.c_str());
    return fails ? 1 : 0;
}
