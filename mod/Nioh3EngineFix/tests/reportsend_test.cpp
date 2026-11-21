// reportsend_test.cpp - offline test of the Report box sender (#64882): scrubber, JSON escaping, 256 KB cap, and (when
// REPORT_TEST_URL is set) one real POST to a local api/report server. Build: tests/run_reportsend_test.py.
#define _CRT_SECURE_NO_WARNINGS
#define REPORTSEND_NO_CRASHREPORT
#include "../src/reportsend.cpp"
#include <cstdio>
#include <cstdlib>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
    printf("\n");
}

int main() {
    using namespace reportsend;
    SetScrubNamesForTest("Jane Doe", "JANES-PC", "C:\\Users\\Jane Doe");
    std::string s = Scrub("path C:\\Users\\Jane Doe\\AppData\\x.txt and c:/users/Jane Doe/y, /home/deck/.steam, Z:\\home\\deck\\g");
    CHECK(s.find("Jane") == std::string::npos && s.find("deck") == std::string::npos);
    CHECK(s.find("C:\\Users\\<user>\\AppData") != std::string::npos && s.find("/home/<user>/.steam") != std::string::npos);
    s = Scrub("user jane doe on janes-pc, steam 76561198012345678 and 1234567890123456789");
    CHECK(s.find("<user>") != std::string::npos && s.find("<pc>") != std::string::npos && s.find("<steamid>") != std::string::npos);
    CHECK(s.find("1234567890123456789") != std::string::npos);  // not a Steam id shape: kept
    CHECK(JsonString("a\"b\\c\nd\x01\xc3\xa9") == "\"a\\\"b\\\\c\\nd\\u0001??\"");
    // cap: a 300 KB snapshot is cut so the body fits 256 KB
    std::string big(300 * 1024, 'x');
    std::string body = BuildBody("0123456789abcdef0123456789abcdef", "1.8.5", "pad", "hello", big, "", 0, "", 0, 256 * 1024);
    CHECK(!body.empty() && body.size() <= 256 * 1024 && body.find("cut to fit") != std::string::npos);
    body = BuildBody("0123456789abcdef0123456789abcdef", "1.8.5", "pad", std::string(1500, 'a'), "snap", "", 0, "", 0, 256 * 1024);
    CHECK(body.find(std::string(1000, 'a')) != std::string::npos && body.find(std::string(1001, 'a')) == std::string::npos);
    // Actual crash writer + typed-report reader share one persistent ini identity.
    wchar_t temp[MAX_PATH], identityIni[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    GetTempFileNameW(temp, L"n3r", 0, identityIni);
    const char* shared = crashupload::EnsureInstallId(identityIni);
    CHECK(strlen(shared) == 32);
    LoadInstallId(identityIni);
    CHECK(!strcmp(g_id, shared));
    wchar_t stored[64] = {};
    GetPrivateProfileStringW(L"Report", L"InstallId", L"", stored, 64, identityIni);
    CHECK(Utf8(stored) == g_id);
    LoadInstallId(identityIni);
    CHECK(!strcmp(g_id, shared));
    DeleteFileW(identityIni);
    // human #128257: the Player log rides in the snapshot, before the settings sections, whole lines only
    {
        const std::string snap = "head\r\n-- last log lines --\r\n  x\r\n\r\n-- mod settings (Nioh3EngineFix.ini), comments removed --\r\n  A=1\r\n";
        std::string r = AttachPlayerLog(snap, "", false, "t");
        CHECK(r == snap);
        r = AttachPlayerLog(snap, "12:00:00.000  +0  CLK  ratio 0.80\n", false, "2026-11-21 23:45:00");
        CHECK(r.find("-- player log (Nioh3PlayerLog.txt, 34 bytes") != std::string::npos);
        CHECK(r.find("CLK  ratio 0.80") < r.find("-- mod settings ("));
        CHECK(r.find("head") == 0 && r.find("A=1") != std::string::npos);
        r = AttachPlayerLog(snap, "tial line\nPAD down Y\n", true, "t");
        CHECK(r.find("tial") == std::string::npos && r.find("newest 11 bytes") != std::string::npos && r.find("PAD down Y") != std::string::npos);
        r = AttachPlayerLog("no settings", "ACT 0x1\n", false, "t");
        CHECK(r.find("no settings") == 0 && r.find("ACT 0x1") > r.find("-- player log"));
        // file path: a 100 KB log keeps only the newest 64 KB, whole lines
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        std::wstring d = std::wstring(dir) + L"n3plog128257\\";
        CreateDirectoryW(d.c_str(), nullptr);
        std::string plog;
        for (int i = 0; plog.size() < 100 * 1024; ++i) plog += "12:00:00.000  +" + std::to_string(i) + "  CLK  ratio 1.00\n";
        CHECK(WriteAll((d + L"Nioh3PlayerLog.txt").c_str(), plog, false));
        r = WithPlayerLog(snap, d);
        const size_t at = r.find("-- player log (Nioh3PlayerLog.txt, newest ");
        CHECK(at != std::string::npos && r.find("+0  CLK") == std::string::npos && r.find(plog.substr(plog.size() - 40)) != std::string::npos);
        const size_t lb = r.find("--\r\n", at) + 4;
        CHECK(r.compare(lb, 13, "12:00:00.000 ") == 0);  // starts on a whole line
        CHECK(r.find("-- mod settings (") - lb <= kPlayerLogCap + 2);
        DeleteFileW((d + L"Nioh3PlayerLog.txt").c_str());
        CHECK(WithPlayerLog(snap, d) == snap);  // Player log never on: unchanged
        RemoveDirectoryW(d.c_str());
        printf("player log attach checks passed (#128257)\n");
    }
    printf("unit checks passed (shared ini install id verified)\n");

    // Optional live POST (local server.py-style api/report). Writes Nioh3Reports\ beside this exe.
    if (const char* url = getenv("REPORT_TEST_URL")) {
        wchar_t ini[MAX_PATH];
        GetFullPathNameW(L"reportsend_test.ini", MAX_PATH, ini, nullptr);
        wchar_t wurl[512];
        MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 512);
        WritePrivateProfileStringW(L"Report", L"ReportUrl", wurl, ini);
        StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.modVersion = "test";
        Start(si);
        char msg[300];
        CHECK(!Send("   ", "keyboard", msg, sizeof msg) && strstr(msg, "Type"));
        NoteInput("pad", "ok");
        CHECK(Send(getenv("REPORT_TEST_TEXT") ? getenv("REPORT_TEST_TEXT") : "unit test report from C:\\Users\\Jane Doe\\x", "keyboard", msg, sizeof msg));
        State st = State::Sending;
        for (int i = 0; i < 300 && (st = Status(msg, sizeof msg)) == State::Sending; ++i) Sleep(100);
        printf("live: %s\n", msg);
        CHECK(st == State::Sent);
    }
    printf("ALL PASSED: reportsend\n");
    return 0;
}
