// resinfoguard_harness.cpp - ResInfoGuard offline test (src/resinfoguard.cpp, GitHub #145). Not a game test.
// Copies the v2.02 header check (2E0384) and its cold block from the decrypted dump into a fake image and
// checks the branch rewrite. resinfoguard_harness.exe <Nioh3.dump.exe> prints ALL OK, exit 0.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/resinfoguard.h"

namespace {

int g_fail = 0;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            ++g_fail;                                  \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
        }                                              \
    } while (0)

char g_logBuf[8192];
size_t g_logLen = 0;
void Log(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    printf("  log: %s\n", line);
    const size_t n = strlen(line);
    if (g_logLen + n + 2 < sizeof g_logBuf) {
        memcpy(g_logBuf + g_logLen, line, n);
        g_logLen += n;
        g_logBuf[g_logLen++] = '\n';
        g_logBuf[g_logLen] = 0;
    }
}
bool LogHas(const char* s) { return strstr(g_logBuf, s) != nullptr; }

constexpr uint32_t kSpan = 0x3BB0000;
constexpr uint32_t kPages[] = {0x2E0000, 0x1912000, 0x3BA6000};
uint8_t* g_img = nullptr;

bool LoadPage(FILE* f, uint32_t page) {
    uint8_t* dst = static_cast<uint8_t*>(VirtualAlloc(g_img + page, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!dst) return false;
    if (fseek(f, static_cast<long>(page), SEEK_SET) != 0) return false;
    return fread(dst, 1, 0x1000, f) == 0x1000;
}

std::wstring WriteIni(const wchar_t* name, const char* body) {
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    std::wstring p = std::wstring(dir) + name;
    FILE* fp = _wfopen(p.c_str(), L"wb");
    if (!fp) return L"";
    fputs(body, fp);
    fclose(fp);
    return p;
}

int32_t Rel(const uint8_t* site) {
    int32_t r = 0;
    memcpy(&r, site + 2, 4);
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* dump = argc > 1 ? argv[1] : "C:\\nioh3-engine\\re\\Nioh3.dump.exe";
    FILE* f = fopen(dump, "rb");
    if (!f) {
        printf("FAIL cannot open dump %s\n", dump);
        return 2;
    }
    g_img = static_cast<uint8_t*>(VirtualAlloc(nullptr, kSpan, MEM_RESERVE, PAGE_NOACCESS));
    CHECK(g_img != nullptr, "reserve");
    for (uint32_t pg : kPages) CHECK(LoadPage(f, pg), "page %X", pg);
    fclose(f);
    if (g_fail) return 1;

    uint8_t* site = g_img + resinfoguard::kJne;
    const int32_t stock = Rel(site);
    const uint8_t* cold = site + 6 + stock;
    CHECK(site[0] == 0x0F && site[1] == 0x85 && site[6] == 0xB0 && site[7] == 0x01, "stock jne shape");
    CHECK(cold[0x1C] == 0x32 && cold[0x1D] == 0xC0, "false return is xor al, al");
    CHECK(static_cast<uint32_t>(site + 6 + stock - g_img) == 0x191235C, "stock target %X",
          static_cast<unsigned>(site + 6 + stock - g_img));

    const std::wstring off = WriteIni(L"rig_off.ini", "[General]\r\nResInfoGuard = 0\r\n");
    resinfoguard::StartInfo si{};
    si.log = &Log;
    si.iniPath = off.c_str();
    si.base = reinterpret_cast<uintptr_t>(g_img);
    si.knownBuild = true;
    resinfoguard::Start(si);
    CHECK(!resinfoguard::Installed(), "ini 0 installed");
    CHECK(Rel(site) == stock, "ini 0 changed the branch");
    CHECK(LogHas("RESINFOGUARD: off"), "missing off log");

    site[0] = 0x90;
    g_logLen = 0;
    g_logBuf[0] = 0;
    const std::wstring on = WriteIni(L"rig_on.ini", "[General]\r\nResInfoGuard = 1\r\n");
    si.iniPath = on.c_str();
    resinfoguard::Start(si);
    CHECK(!resinfoguard::Installed(), "installed over changed bytes");
    CHECK(LogHas("bytes differ"), "missing refuse log");
    site[0] = 0x0F;

    g_logLen = 0;
    g_logBuf[0] = 0;
    resinfoguard::Start(si);
    CHECK(resinfoguard::Installed(), "default not installed");
    CHECK(LogHas("RESINFOGUARD: installed"), "missing install log");
    const int32_t neu = Rel(site);
    CHECK(neu == stock + 0x1C, "rel %d want stock+0x1C", neu);
    const uint8_t* landed = site + 6 + neu;
    CHECK(landed[0] == 0x32 && landed[1] == 0xC0 && landed[2] == 0xE9, "lands on xor al, al; jmp");
    CHECK(static_cast<uint32_t>(landed - g_img) == 0x1912378, "target %X", static_cast<unsigned>(landed - g_img));
    CHECK(site[6] == 0xB0 && site[7] == 0x01, "success path mov al, 1 moved");
    CHECK(cold[23] == 0xE8, "assert call removed");

    resinfoguard::Start(si);
    CHECK(Rel(site) == neu, "second start patched again");

    if (g_fail) {
        printf("%d FAILED\n", g_fail);
        return 1;
    }
    printf("ALL OK\n");
    return 0;
}
