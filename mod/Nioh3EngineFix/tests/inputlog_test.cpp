// Offline input ring (inputlog.cpp, human #64888). No game.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <windows.h>
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
    CHECK(strstr(buf, "\"what\":\"a\",\"edge\":\"up\",\"ms\":") != nullptr);
    CHECK(strstr(buf, "\"held\":") != nullptr);
    // #118266 gameplay keyboard + mouse
    {
        unsigned char kb[256] = {};
        const int before = inputlog::Count();
        inputlog::ObserveKeyboard(kb, 0);
        CHECK(inputlog::Count() == before);
        kb[0x2F] = 0x80;  // v
        kb[0x2A] = 0x80;  // lshift
        inputlog::ObserveKeyboard(kb, 0);
        inputlog::ObserveKeyboard(kb, 0);
        CHECK(inputlog::Count() == before + 2);
        Sleep(30);
        kb[0x2F] = 0;
        inputlog::ObserveKeyboard(kb, 0);
        CHECK(inputlog::Count() == before + 3);
        // menu open: typing is never recorded, and its release after close adds nothing
        kb[0x1E] = 0x80;  // a
        kb[0xC8] = 0x80;  // up
        inputlog::ObserveKeyboard(kb, 1);
        CHECK(inputlog::Count() == before + 3);
        kb[0x1E] = 0;
        inputlog::ObserveKeyboard(kb, 0);
        CHECK(inputlog::Count() == before + 3);
        kb[0xC8] = 0;
        kb[0x2A] = 0;
        inputlog::ObserveKeyboard(kb, 0);
        CHECK(inputlog::Count() == before + 4);  // lshift up only
        inputlog::ObserveMouse(0x1, 0);
        inputlog::ObserveMouse(0x3, 0);
        inputlog::ObserveMouse(0x0, 0);
        CHECK(inputlog::Count() == before + 8);
        inputlog::ObserveMouse(0x4, 1);
        inputlog::ObserveMouse(0x0, 0);
        CHECK(inputlog::Count() == before + 8);
        char kbuf[8192];
        inputlog::WriteTail(kbuf, (int)sizeof kbuf, 8);
        CHECK(strstr(kbuf, "\"dev\":\"kb\",\"what\":\"v\",\"edge\":\"down\",\"ms\":") != nullptr);
        CHECK(strstr(kbuf, "\"dev\":\"kb\",\"what\":\"v\",\"edge\":\"up\",\"ms\":") != nullptr);
        CHECK(strstr(kbuf, "\"dev\":\"kb\",\"what\":\"lshift\",\"edge\":\"up\"") != nullptr);
        CHECK(strstr(kbuf, "\"what\":\"a\"") == nullptr);
        CHECK(strstr(kbuf, "\"dev\":\"mouse\",\"what\":\"lmb\",\"edge\":\"down\"") != nullptr);
        CHECK(strstr(kbuf, "\"dev\":\"mouse\",\"what\":\"rmb\",\"edge\":\"up\"") != nullptr);
        CHECK(strstr(kbuf, "mmb") == nullptr);
        const char* vu = strstr(kbuf, "\"what\":\"v\",\"edge\":\"up\"");
        const char* h = vu ? strstr(vu, "\"held\":") : nullptr;
        CHECK(h && atoi(h + 7) >= 25);
        printf("KB SAMPLE\n%s\n", kbuf);
    }
    printf("SAMPLE\n%s\n", buf);
    if (fails) { fprintf(stderr, "%d failed\n", fails); return 1; }
    printf("inputlog_test ok\n");
    return 0;
}
