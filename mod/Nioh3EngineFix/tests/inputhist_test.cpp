// inputhist_test.cpp - offline check of the input history (inputhist.h data + inputhist_draw.h layout) with the OSD calls stubbed.
// zig c++ -std=c++17 -Isrc tests/inputhist_test.cpp -o inputhist_test.exe   (or cl /EHsc /std:c++17 /Isrc)
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "osd.h"
namespace osd {
static int g_rect = 0, g_text = 0, g_sprite = 0;
void DrawRect(float, float, float, float, uint32_t) { ++g_rect; }
float DrawText(float x, float, const char* t, uint32_t, float) { ++g_text; return x + 1; }
void DrawSprite(float, float, float, float, float, float, float, float, uint32_t) { ++g_sprite; }
}  // namespace osd
#include "inputhist_draw.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    using namespace inputhist;
    // direction: d-pad, opposing cancel, stick octants
    CHECK(DirOf(0, 0, 0) == N);
    CHECK(DirOf(0x0001, 0, 0) == U);
    CHECK(DirOf(0x0001 | 0x0008, 0, 0) == UR);
    CHECK(DirOf(0x0004 | 0x0008, 0, 0) == N);
    CHECK(DirOf(0x0002 | 0x0004, 0, 0) == DL);
    CHECK(DirOf(0, 30000, 0) == R);
    CHECK(DirOf(0, -30000, 0) == L);
    CHECK(DirOf(0, 0, 30000) == U);
    CHECK(DirOf(0, 0, -30000) == D);
    CHECK(DirOf(0, 20000, 20000) == UR);
    CHECK(DirOf(0, -20000, 20000) == UL);
    CHECK(DirOf(0, 20000, -20000) == DR);
    CHECK(DirOf(0, -20000, -20000) == DL);
    CHECK(DirOf(0, 5000, 5000) == N);  // inside the deadzone
    CHECK(BtnOf(0x1000 | 0x0200, 255, 0) == (bA | bRB | bLT));
    // history: dedupe, new entry on change, newest first, ring wrap
    Push(0, 0, 0, 0, 0, 1000);
    Push(0, 0, 0, 0, 0, 1016);  // same state: no new entry
    Push(0x0001, 0, 0, 0, 0, 1500);
    Push(0x0001 | 0x4000, 0, 0, 0, 0, 1800);
    Entry e[kCap];
    int n = Snapshot(e, 8);
    CHECK(n == 3);
    CHECK(e[0].dir == N && e[0].dpad == 1 && e[0].btn == bX && e[0].t0 == 1800);
    CHECK(e[1].dir == N && e[1].dpad == 1 && e[1].btn == 0 && e[1].t0 == 1500);
    CHECK(e[2].dir == N && e[2].t0 == 1000);
    CHECK(Frames(1500, 1800) == 18);
    CHECK(Frames(0, 100000) == 99);
    CHECK(Frames(5, 5) == 1);
    for (int i = 0; i < 100; ++i) Push((i & 1) ? 0x0008 : 0x0004, 0, 0, 0, 0, 2000 + 20 * i);
    CHECK(Snapshot(e, kCap) == kCap);
    CHECK(e[0].t0 == 2000 + 20 * 99 && e[1].t0 == 2000 + 20 * 98);
    g_n=0;g_next=0;
    Push(1,30000,0,0,0,5000);Snapshot(e,8);
    CHECK(e[0].dir==R && e[0].dpad==1);
    Push(8,30000,0,0,0,5016);CHECK(Snapshot(e,8)==2);
    CHECK(e[0].dir==R && e[0].dpad==8);
    for(int skin=0;skin<2;++skin) {
        CHECK(inputhist_dpad::Sample(skin,1,17.5f,7)==0xfff5f2f1);
        CHECK(inputhist_dpad::Sample(skin,1,28,17.5f)==0xff413733);
        CHECK(inputhist_dpad::Sample(skin,8,28,17.5f)==0xfff5f2f1);
        CHECK(inputhist_dpad::Sample(skin,8,17.5f,7)==0xff413733);
    }
    CHECK(inputhist_dpad::Sample(0,1,17.5f,17.5f)!=0);
    CHECK(inputhist_dpad::Sample(1,1,17.5f,17.5f)==0);
    // layout: all rows get a number; arrows / glyphs produce quads; every arrow bitmap has ink
    for (int d = 1; d <= 8; ++d) { int ink = 0; for (int r = 0; r < 20; ++r) for (int c = 0; c < 20; ++c) ink += inputhist_draw::ArrowSd(((float)c + 0.5f) / 20.0f - 0.5f, ((float)r + 0.5f) / 20.0f - 0.5f) < 0.0f; CHECK(ink >= 40); }
    g_rows = 6;
    g_n = 0; g_next = 0;
    Push(0x0001 | 0x1000 | 0x0100, 0, 0, 0, 0, 100);
    Push(0, 0, 0, 0, 0, 400);
    osd::g_rect = osd::g_text = osd::g_sprite = 0;
    const float sc = inputhist_draw::Draw(1920, 1080, 16, 200, false, 900, 1);
    CHECK(sc > 0.0f);
    CHECK(osd::g_rect > 50);    // 2 numbers + N
    CHECK(osd::g_sprite >= 1);  // the A glyph + the LB shoulder shape
    CHECK(inputhist_draw::PanelHeight(1080) > 0);
    printf("%s (rect %d text %d sprite %d)\n", fails ? "FAILED" : "PASS", osd::g_rect, osd::g_text, osd::g_sprite);
    fflush(stdout);
    return fails ? 1 : 0;
}
