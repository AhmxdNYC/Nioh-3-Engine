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
    printf("unit checks passed\n");

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
