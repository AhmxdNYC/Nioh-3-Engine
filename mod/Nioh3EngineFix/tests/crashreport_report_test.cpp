// Offline check of the menu Report snapshot (crashreport.cpp): the file must carry the system section, the mod / Training
// settings (comments removed, user names redacted) and the Training / summoned-boss section. No game: Training is not loaded,
// so that section must say so. Build next to a copy of Nioh3Training.ini (the report reads the .ini files beside the module).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>
#include "../src/crashreport.h"

static int fails = 0;
#define CHECK(x) do { if (!(x)) { ++fails; fprintf(stderr, "FAIL: %s\n", #x); } } while (0)

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fprintf(stderr, "usage: crashreport_report_test <Nioh3EngineFix.ini>\n"); return 2; }
    crashreport::StartInfo si;
    si.iniPath = argv[1];
    si.selfModule = GetModuleHandleW(nullptr);
    crashreport::Start(si);
    crashreport::SetState("menu", "test");
    char path[600] = {};
    CHECK(crashreport::Snapshot(path, sizeof path));
    printf("report: %s\n", path);
    FILE* f = nullptr;
    fopen_s(&f, path, "rb");
    std::string t;
    if (f) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) t.append(b, n); fclose(f); }
    for (const char* need : {"-- system --", "CPU:", "RAM:", "GPU", "OS family:", "-- Training session / summoned bosses --",
                             "Nioh3Training.asi: not loaded", "-- mod settings (Nioh3EngineFix.ini)", "[SaveFiles]", "Native=1",
                             "-- Training settings (Nioh3Training.ini)", "-- Boss Rematch settings", "-- last mod state --"})
        if (t.find(need) == std::string::npos) { ++fails; fprintf(stderr, "missing: %s\n", need); }
    char user[260] = {};
    if (GetEnvironmentVariableA("USERNAME", user, sizeof user) && strlen(user) >= 3) {
        const size_t s = t.find("-- mod settings"), e = t.find("-- loaded modules --");
        CHECK(s != std::string::npos && e != std::string::npos && t.substr(s, e - s).find(user) == std::string::npos);
    }
    CHECK(t.find("\n  ;") == std::string::npos);  // comment lines removed
    const size_t a = t.find("-- system --");
    if (a != std::string::npos) printf("%s\n", t.substr(a, t.find("-- mod settings") - a).c_str());
    printf("%s (%zu bytes)\n", fails ? "FAILED" : "ALL PASSED", t.size());
    return fails ? 1 : 0;
}
