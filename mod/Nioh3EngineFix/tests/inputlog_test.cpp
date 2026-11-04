// Offline input ring (inputlog.cpp, human #64888). No game.
#include <cstdio>
#include <cstring>
#include "../src/inputlog.h"

static int fails = 0;
#define CHECK(x) do { if (!(x)) { ++fails; fprintf(stderr, "FAIL %s\n", #x); } } while (0)

int main() {
    inputlog::Reset();
    inputlog::ObservePad(0, 0, 0, 0, 0, 0, 0, 0);
    inputlog::ObservePad(0, 0, 0, 0, 1000, 1000, 0, 0);
    CHECK(inputlog::Count() == 0);
    inputlog::ObservePad(0, 0x1000, 0, 0, 1000, 1000, 0, 0);
    inputlog::ObservePad(0, 0x1000, 0, 0, 1000, 1000, 0, 0);
    inputlog::ObservePad(0, 0x1000, 0, 0, 1000, 1000, 0, 0);
    CHECK(inputlog::Count() == 1);
    inputlog::ObservePad(0, 0, 0, 0, 1000, 1000, 0, 0);
    CHECK(inputlog::Count() == 2);
    inputlog::ObservePad(0, 0, 0, 0, 0, 25000, 0, 0);
    inputlog::ObservePad(0, 0, 0, 0, 0, 26000, 0, 0);
    inputlog::ObservePad(0, 0, 0, 0, 0, 27000, 0, 0);
    CHECK(inputlog::Count() == 3);
    inputlog::ObserveKey("a", 1, 0);
    inputlog::NoteTyped();
    CHECK(inputlog::Count() == 3);
    inputlog::ObserveKey("esc", 1, 1);
    inputlog::ObserveKey("esc", 1, 1);
    inputlog::ObserveKey("esc", 0, 1);
    CHECK(inputlog::Count() == 5);
    inputlog::NoteMenu("open", "", "", "", "");
    inputlog::NoteMenu("page", "Camera settings", "", "", "");
    inputlog::NoteMenu("row", "Camera settings", "Distance", "", "");
    inputlog::NoteMenu("value", "Camera settings", "Distance", "0", "60");
    inputlog::NoteMenu("page", "C:\\Users\\Someone\\Nioh3", "", "", "");
    CHECK(inputlog::Count() == 10);
    char buf[8192];
    inputlog::WriteTail(buf, (int)sizeof buf, inputlog::kCrashTail);
    CHECK(strstr(buf, "\"what\":\"a\",\"edge\":\"down\"") != nullptr);
    CHECK(strstr(buf, "\"what\":\"a\",\"edge\":\"up\"") != nullptr);
    CHECK(strstr(buf, "\"what\":\"lx\",\"edge\":\"u\"") != nullptr);
    CHECK(strstr(buf, "\"dev\":\"key\",\"what\":\"esc\"") != nullptr);
    CHECK(strstr(buf, "\"what\":\"open\"") != nullptr);
    CHECK(strstr(buf, "\"what\":\"page\",\"page\":\"Camera settings\"") != nullptr);
    CHECK(strstr(buf, "\"what\":\"row\"") != nullptr);
    CHECK(strstr(buf, "\"what\":\"value\"") != nullptr);
    CHECK(strstr(buf, "\"old\":\"0\",\"new\":\"60\"") != nullptr);
    CHECK(strstr(buf, "<redacted>") != nullptr);
    CHECK(strstr(buf, ":\\") == nullptr);
    CHECK(strstr(buf, "Users") == nullptr);
    printf("SAMPLE\n%s\n", buf);
    if (fails) { fprintf(stderr, "%d failed\n", fails); return 1; }
    printf("inputlog_test ok\n");
    return 0;
}
