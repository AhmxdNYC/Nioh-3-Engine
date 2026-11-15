// #106253: one-time [Menu] ButtonStyle Xbox -> Auto migration on real ini files (old / fresh / already migrated / other values).
#include "../src/btnstyle_migrate.h"
#include <cstdio>
#include <string>
static int g_fail = 0, g_n = 0;
#define CHECK(x) do { ++g_n; if (!(x)) { ++g_fail; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
static std::wstring g_dir;
static std::wstring Make(const wchar_t* name, const char* body) {
    std::wstring p = g_dir + name;
    FILE* f = _wfopen(p.c_str(), L"wb");
    fputs(body, f);
    fclose(f);
    return p;
}
static std::wstring Get(const std::wstring& p, const wchar_t* k) {
    wchar_t b[96];
    GetPrivateProfileStringW(L"Menu", k, L"<none>", b, 96, p.c_str());
    return b;
}
static std::string Text(const std::wstring& p) {
    std::string s; char b[512]; FILE* f = _wfopen(p.c_str(), L"rb"); size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    fclose(f); return s;
}
int main() {
    wchar_t t[MAX_PATH]; GetTempPathW(MAX_PATH, t);
    g_dir = std::wstring(t) + L"btnstyle_migrate_test\\";
    CreateDirectoryW(g_dir.c_str(), nullptr);
    using btnstyle_migrate::Run;
    // old ini (pre-Auto shipped Xbox): Xbox -> Auto + marker; other keys and comments untouched; second load is a no-op
    {auto p = Make(L"old.ini", "; my settings\r\n[Menu]\r\nKey = F10\r\nButtonStyle = Xbox\r\nControllerDisplay = 1\r\n[General]\r\nX = 5\r\n");
     CHECK(Run(p.c_str()));
     CHECK(Get(p, L"ButtonStyle") == L"Auto"); CHECK(Get(p, L"ButtonStyleMigrated") == L"1");
     CHECK(Get(p, L"Key") == L"F10"); CHECK(Get(p, L"ControllerDisplay") == L"1");
     const std::string s = Text(p); CHECK(s.find("; my settings") == 0 && s.find("X = 5") != std::string::npos);
     CHECK(!Run(p.c_str())); CHECK(Text(p) == s);}
    // lower case xbox counts too
    {auto p = Make(L"lower.ini", "[Menu]\r\nButtonStyle=xbox\r\n"); CHECK(Run(p.c_str())); CHECK(Get(p, L"ButtonStyle") == L"Auto");}
    // fresh ini (shipped Auto / no key): value kept, only the marker added
    {auto p = Make(L"fresh.ini", "[Menu]\r\nButtonStyle = Auto\r\n"); CHECK(!Run(p.c_str()));
     CHECK(Get(p, L"ButtonStyle") == L"Auto"); CHECK(Get(p, L"ButtonStyleMigrated") == L"1");}
    {auto p = Make(L"nokey.ini", "[General]\r\nX = 5\r\n"); CHECK(!Run(p.c_str()));
     CHECK(Get(p, L"ButtonStyle") == L"<none>"); CHECK(Get(p, L"ButtonStyleMigrated") == L"1");}
    // already migrated: an Xbox the player chose later is kept (any marker value counts)
    {auto p = Make(L"done.ini", "[Menu]\r\nButtonStyle = Xbox\r\nButtonStyleMigrated = 1\r\n"); const std::string s = Text(p);
     CHECK(!Run(p.c_str())); CHECK(Get(p, L"ButtonStyle") == L"Xbox"); CHECK(Text(p) == s);}
    {auto p = Make(L"done0.ini", "[Menu]\r\nButtonStyle = Xbox\r\nButtonStyleMigrated = 0\r\n"); CHECK(!Run(p.c_str())); CHECK(Get(p, L"ButtonStyle") == L"Xbox");}
    // other explicit values kept (+ marker)
    for (const char* v : {"PlayStation", "Nintendo", "XboxOne"}) {
        std::string body = std::string("[Menu]\r\nButtonStyle = ") + v + "\r\n";
        auto p = Make(L"other.ini", body.c_str()); CHECK(!Run(p.c_str()));
        wchar_t w[32]; swprintf(w, 32, L"%hs", v); CHECK(Get(p, L"ButtonStyle") == w); CHECK(Get(p, L"ButtonStyleMigrated") == L"1");
    }
    // missing ini is never created
    {std::wstring p = g_dir + L"missing.ini"; DeleteFileW(p.c_str()); CHECK(!Run(p.c_str())); CHECK(GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES);
     CHECK(!Run(nullptr)); CHECK(!Run(L""));}
    printf("btnstyle migrate: %d checks, %d failed\n", g_n, g_fail);
    if (!g_fail) puts("PASS: ButtonStyle migration (old Xbox -> Auto once, fresh / no key, already migrated kept, PlayStation / Nintendo / junk kept, no file created)");
    return g_fail ? 1 : 0;
}
