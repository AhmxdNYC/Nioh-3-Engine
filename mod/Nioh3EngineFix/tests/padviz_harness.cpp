// Offline check of padviz.h (live controller input display): feed rules, scale, layout bounds, lit vs unlit, Xbox vs PlayStation.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../src/padviz.h"

namespace osd {
struct Rect { float x, y, w, h; uint32_t c; };
std::vector<Rect> rects;
std::vector<std::string> texts;
void DrawRect(float x, float y, float w, float h, uint32_t c) { rects.push_back({x, y, w, h, c}); }
float DrawText(float x, float, const char* t, uint32_t, float) { texts.emplace_back(t); return x; }
}

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static size_t Count(uint32_t color) { size_t n = 0; for (auto& r : osd::rects) if (r.c == color) ++n; return n; }
static void Clear() { osd::rects.clear(); osd::texts.clear(); }
static bool Has(const char* t) { for (auto& s : osd::texts) if (s == t) return true; return false; }

int main() {
    using namespace padviz;
    // scale: 1080p size 1 -> 1, 2160p -> 2, size 2 -> 2, clamps
    g_size100 = 100; CHECK(Scale(1080) == 1.0f); CHECK(Scale(2160) == 2.0f); CHECK(Scale(800) == 0.5f);
    g_size100 = 200; CHECK(Scale(1080) == 2.0f); g_size100 = 400; CHECK(Scale(2160) == 8.0f);
    g_size100 = 100;
    g_opacity100 = 80; CHECK(Opacity() > 0.79f && Opacity() < 0.81f);

    // feed: off = ignored; live window; lowest slot wins while fresh
    g_on = 0; Feed(0, kA, 0, 0, 0, 0, 0, 0, 1000); CHECK(!Live(1000));
    g_on = 1; g_at = 0; g_slot = 0;
    Feed(1, kA, 1, 2, 3, 4, 5, 6, 1000); CHECK(Live(1000) && g_slot == 1 && g_buttons == kA && g_rt == 6);
    Feed(2, kB, 0, 0, 0, 0, 0, 0, 1100); CHECK(g_buttons == kA);          // higher slot while slot 1 is fresh: ignored
    Feed(0, kX, 0, 0, 0, 0, 0, 0, 1200); CHECK(g_slot == 0 && g_buttons == kX);
    Feed(1, kY, 0, 0, 0, 0, 0, 0, 1300); CHECK(g_buttons == kX);
    Feed(1, kY, 0, 0, 0, 0, 0, 0, 1800); CHECK(g_slot == 1 && g_buttons == kY);  // slot 0 silent > 500 ms: slot 1 takes over
    CHECK(Live(3000) && !Live(3400));                                       // stale after 1.5 s: hidden
    CHECK(!Draw(0, 0, 1080, 3400) && osd::rects.empty());
    g_on = 0; CHECK(!Draw(0, 0, 1080, 1800));

    // drawing: Xbox style letters, bounds, lit differs from unlit
    State idle; Clear();
    DrawState(idle, 100, 200, 2.0f, 1.0f, 0);
    const size_t nIdle = osd::rects.size();
    CHECK(Has("A") && Has("B") && Has("X") && Has("Y") && Has("LB") && Has("RB"));
    for (auto& r : osd::rects) CHECK(r.w > 0 && r.h > 0 && r.x >= 100 - 0.01f && r.y >= 200 - 0.01f &&
                                     r.x + r.w <= 100 + kW * 2.0f + 0.01f && r.y + r.h <= 200 + kH * 2.0f + 0.01f);
    CHECK(Count(menu_style::kAccent) == 0 && Count(menu_style::kBtnA) == 0);
    State p; p.buttons = kA | kUp | kRB | kStart | kL3; p.lx = 30000; p.lt = 255; p.rt = 128;
    Clear(); DrawState(p, 100, 200, 2.0f, 1.0f, 0);
    CHECK(Count(menu_style::kBtnA) > 0);                 // A lit green
    CHECK(Count(menu_style::kBtnB) == 0 && Count(menu_style::kBtnX) == 0 && Count(menu_style::kBtnY) == 0);
    CHECK(Count(menu_style::kAccent) >= 4);               // trigger fills, D-pad up, Start, stick knob/well
    CHECK(Count(kLitCap) > 0);                            // RB lit
    // trigger fill is proportional: full LT bar is 34 units, RT at 128 about half
    float ltW = 0, rtW = 0;
    for (auto& r : osd::rects) if (r.c == menu_style::kAccent && r.h == 16.0f) { if (r.x < 200) ltW = r.w; else rtW = r.w; }
    CHECK(ltW == 68.0f && rtW > 33.0f && rtW < 36.0f);
    // knob moves right with +lx and up with +ly
    State up; up.ly = 32767; Clear(); DrawState(up, 0, 0, 1.0f, 1.0f, 0);
    State rest; std::vector<osd::Rect> a = osd::rects; Clear(); DrawState(rest, 0, 0, 1.0f, 1.0f, 0);
    bool moved = a.size() != osd::rects.size();
    for (size_t i = 0; !moved && i < a.size(); ++i) moved = a[i].y != osd::rects[i].y || a[i].x != osd::rects[i].x;
    CHECK(moved);

    // PlayStation style: no Xbox letters, L1/R1 labels, symbols drawn from rects
    Clear(); DrawState(idle, 0, 0, 1.0f, 1.0f, 1);
    CHECK(!Has("A") && !Has("X") && !Has("Y") && !Has("B") && Has("L1") && Has("R1") && !Has("LB"));
    CHECK(osd::rects.size() > nIdle / 2);
    State face; face.buttons = kB; Clear(); DrawState(face, 0, 0, 1.0f, 1.0f, 1);
    CHECK(Count(padviz::kPsFillLit) > 0);

    // opacity scales every alpha
    Clear(); DrawState(p, 0, 0, 1.0f, 0.5f, 0);
    for (auto& r : osd::rects) CHECK((r.c >> 24) <= 0x80u);

    // all lit at once stays inside the quad budget and the box
    State all; all.buttons = 0xF3FF; all.lx = all.ly = all.rx = all.ry = 32767; all.lt = all.rt = 255;
    for (int style = 0; style < 2; ++style) {
        Clear(); DrawState(all, 0, 0, 1.0f, 1.0f, style);
        CHECK(osd::rects.size() < 1200);
        for (auto& r : osd::rects) CHECK(r.x >= 0 && r.y >= 0 && r.x + r.w <= kW + 0.01f && r.y + r.h <= kH + 0.01f);
    }
    printf("%s (%d failures), idle quads %zu\n", fails ? "FAILED" : "ALL PASSED", fails, nIdle);
    return fails ? 1 : 0;
}
