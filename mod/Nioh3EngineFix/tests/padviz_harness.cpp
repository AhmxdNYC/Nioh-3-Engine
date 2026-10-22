// Offline check of padviz.h (live controller input display, Gamepad Viewer skins): feed rules, scale, the layers per state (body,
// lit sprites, stick travel + tilt, trigger opacity / meter), bounds, opacity, Xbox vs DualSense, and the atlas PNG decoder.
// With an output directory argument it also composites preview images (TGA, straight alpha over a dark backdrop) on the CPU from the
// same sprite list the overlay draws: padviz_<skin>.tga, three panels each (idle / several pressed / others pressed).
#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../src/padviz.h"
#include "../src/padviz_atlas_data.h"
#include "../src/padviz_png.h"

namespace osd {
struct Rect { float x, y, w, h; uint32_t c; };
struct Spr { float x, y, w, h, u0, v0, u1, v1; uint32_t c; };
std::vector<Rect> rects;
std::vector<Spr> sprites;
std::vector<std::string> texts;
void DrawRect(float x, float y, float w, float h, uint32_t c) { rects.push_back({x, y, w, h, c}); }
float DrawText(float x, float, const char* t, uint32_t, float) { texts.emplace_back(t); return x; }
void DrawSprite(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t c) {
    sprites.push_back({x, y, w, h, u0, v0, u1, v1, c});
}
}  // namespace osd

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static void Clear() { osd::rects.clear(); osd::texts.clear(); osd::sprites.clear(); }
static bool Near(float a, float b, float e = 0.01f) { return std::fabs(a - b) <= e; }
// the drawn sprite whose atlas rect is skin.s[id] (nullptr = not drawn)
static const osd::Spr* Find(const padviz::Skin& k, int id) {
    const padviz::SpriteRect& r = k.s[id];
    const float u = (float)r.x / k.atlasW, u1 = (float)(r.x + r.w) / k.atlasW, v1 = (float)(r.y + r.h) / k.atlasH;
    for (auto& s : osd::sprites)
        if (Near(s.u0, u, 1e-5f) && Near(s.u1, u1, 1e-5f) && Near(s.v1, v1, 1e-5f)) return &s;
    return nullptr;
}

// ---- CPU compositor for the previews ----
struct Img {
    int w, h;
    std::vector<float> px;  // straight RGB + A, 0..1
};
static void Sample(const padviz_png::Atlas& A, int lvl, float u, float v, float out[4]) {
    const int w = std::max(1, A.w >> lvl), h = std::max(1, A.h >> lvl);
    const uint8_t* p = &A.px[A.off[lvl]];
    float x = u * w - 0.5f, y = v * h - 0.5f;
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    const float fx = x - x0, fy = y - y0;
    for (int c = 0; c < 4; ++c) out[c] = 0;
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            const int xx = std::clamp(x0 + i, 0, w - 1), yy = std::clamp(y0 + j, 0, h - 1);
            const float wt = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            for (int c = 0; c < 4; ++c) out[c] += wt * p[((size_t)yy * w + xx) * 4 + c] / 255.0f;
        }
}
static void Composite(Img& img, const padviz_png::Atlas& A) {
    for (auto& s : osd::sprites) {
        const float texW = std::fabs(s.u1 - s.u0) * A.w, texH = std::fabs(s.v1 - s.v0) * A.h;
        const float ratio = std::max(texW / std::max(s.w, 0.01f), texH / std::max(s.h, 0.01f));
        const int lvl = std::clamp((int)std::floor(std::log2(std::max(ratio, 1.0f)) + 0.5f), 0, A.levels - 1);
        const float ca = (float)(s.c >> 24) / 255.0f;
        for (int y = (int)std::floor(s.y); y < (int)std::ceil(s.y + s.h); ++y)
            for (int x = (int)std::floor(s.x); x < (int)std::ceil(s.x + s.w); ++x) {
                if (x < 0 || y < 0 || x >= img.w || y >= img.h) continue;
                const float cx = x + 0.5f, cy = y + 0.5f;
                if (cx < s.x || cy < s.y || cx >= s.x + s.w || cy >= s.y + s.h) continue;
                const float tx = (cx - s.x) / s.w, ty = (cy - s.y) / s.h;
                float t[4];
                Sample(A, lvl, s.u0 + (s.u1 - s.u0) * tx, s.v0 + (s.v1 - s.v0) * ty, t);
                const float a = t[3] * ca;
                if (a <= 0.002f) continue;
                float* d = &img.px[((size_t)y * img.w + x) * 4];
                for (int c = 0; c < 3; ++c) d[c] = (t[c] / t[3]) * a + d[c] * (1 - a);  // straight over (as the PSO blends)
                d[3] = a + d[3] * (1 - a);
            }
    }
}
static bool WriteTga(const char* path, const Img& img) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    uint8_t hd[18] = {0, 0, 2};
    hd[12] = (uint8_t)(img.w & 255); hd[13] = (uint8_t)(img.w >> 8);
    hd[14] = (uint8_t)(img.h & 255); hd[15] = (uint8_t)(img.h >> 8);
    hd[16] = 32; hd[17] = 0x28;  // top-left origin, 8 alpha bits
    fwrite(hd, 1, 18, f);
    for (size_t i = 0; i < (size_t)img.w * img.h; ++i) {
        const float* p = &img.px[i * 4];
        const uint8_t b[4] = {(uint8_t)(std::clamp(p[2], 0.f, 1.f) * 255 + .5f), (uint8_t)(std::clamp(p[1], 0.f, 1.f) * 255 + .5f),
                              (uint8_t)(std::clamp(p[0], 0.f, 1.f) * 255 + .5f), (uint8_t)(std::clamp(p[3], 0.f, 1.f) * 255 + .5f)};
        fwrite(b, 1, 4, f);
    }
    fclose(f);
    return true;
}

int main(int argc, char** argv) {
    using namespace padviz;
    // scale: both skins kBaseWidth (280 px) wide at 1080p size 1; 4K twice that; size 2 twice; Xbox canvas narrower -> larger s
    g_size100 = 100;
    CHECK(Near(Scale(1080, 1) * kSkin_dualsense.canvasW, 280.0f, 0.1f));
    CHECK(Near(Scale(1080, 0) * kSkin_xbox.canvasW, 280.0f, 0.1f));
    CHECK(Near(Scale(2160, 1), 2 * Scale(1080, 1)));
    g_size100 = 200; CHECK(Near(Scale(1080, 1) * kSkin_dualsense.canvasW, 560.0f, 0.1f));
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
    Clear(); CHECK(!Draw(0, 0, 1080, 3400, 1) && osd::sprites.empty());
    g_on = 0; CHECK(!Draw(0, 0, 1080, 1800, 1));

    // generated tables: every present sprite lies inside its atlas and (at rest) inside the canvas
    for (const Skin* k : {&kSkin_dualsense, &kSkin_xbox})
        for (int i = 0; i < S_COUNT; ++i) {
            const SpriteRect& r = k->s[i];
            if (!r.w) continue;
            CHECK(r.x + r.w <= k->atlasW && r.y + r.h <= k->atlasH);
            CHECK(r.cx >= -1.01f && r.cy >= -0.01f && r.cx + r.w <= k->canvasW + 1.01f && r.cy + r.h <= k->canvasH + 0.5f);
        }
    CHECK(kSkin_dualsense.s[S_TOUCH].w && kSkin_dualsense.s[S_A].w && !kSkin_xbox.s[S_TOUCH].w && !kSkin_xbox.s[S_A].w);

    // DualSense idle: body first (whole canvas), unlit A/B/X/Y and both sticks; nothing lit
    const State idle;
    const Skin& ds = kSkin_dualsense;
    const Skin& xb = kSkin_xbox;
    Clear(); DrawState(idle, 100, 200, 0.5f, 1.0f, 1);
    CHECK(osd::sprites.size() == 7);
    CHECK(!osd::sprites.empty() && Near(osd::sprites[0].x, 100) && Near(osd::sprites[0].w, 807 * 0.5f) && Near(osd::sprites[0].h, 651 * 0.5f));
    CHECK(Find(ds, S_A) && Find(ds, S_STICK) && Find(ds, S_STICK_R) && !Find(ds, S_A_LIT) && !Find(ds, S_LT) && !Find(ds, S_UP));
    CHECK(osd::rects.empty() && osd::texts.empty());
    // Xbox idle: body + sticks (its buttons are body art)
    Clear(); DrawState(idle, 0, 0, 1.0f, 1.0f, 0);
    CHECK(osd::sprites.size() == 3 && Find(xb, S_BODY) && Find(xb, S_STICK));

    // pressed: lit sprite replaces the unlit one; d-pad / bumpers / back / start / touchpad / PS appear
    State p; p.buttons = kA | kUp | kRB | kStart | kL3 | kTouch | kGuide; p.lx = 32767; p.lt = 255; p.rt = 128;
    Clear(); DrawState(p, 0, 0, 1.0f, 1.0f, 1);
    CHECK(Find(ds, S_A_LIT) && !Find(ds, S_A) && Find(ds, S_B) && Find(ds, S_UP) && Find(ds, S_RB) && !Find(ds, S_LB));
    CHECK(Find(ds, S_START) && !Find(ds, S_BACK) && Find(ds, S_TOUCH) && Find(ds, S_META) && Find(ds, S_STICK_P) && !Find(ds, S_STICK));
    // DualSense triggers: opacity = value (Gamepad Viewer default), full size
    const osd::Spr* lt = Find(ds, S_LT);
    const osd::Spr* rt = Find(ds, S_RT);
    CHECK(lt && rt && (lt->c >> 24) == 255 && (rt->c >> 24) >= 127 && (rt->c >> 24) <= 129 && Near(rt->h, 108));
    // left stick: +22 px right, narrowed by cos(22 deg) around its centre (rotateY), height unchanged
    const osd::Spr* st = Find(ds, S_STICK_P);
    const float sw = 100.0f * std::cos(22.0f * 3.14159265f / 180.0f);
    CHECK(st && Near(st->w, sw, 0.05f) && Near(st->h, 100) && Near(st->x, 220 + 22 + (100 - sw) / 2, 0.05f) && Near(st->y, 333));
    // stick inside the radial threshold rests; +y (XInput up) moves the cap up
    State dz; dz.lx = 6000; dz.ry = 32767;
    Clear(); DrawState(dz, 0, 0, 1.0f, 1.0f, 1);
    const osd::Spr* l = Find(ds, S_STICK);
    const osd::Spr* r = Find(ds, S_STICK_R);
    CHECK(l && Near(l->x, 220) && Near(l->w, 100) && r && r->y < 333 - 15 && Near(r->x, 487));
    // Xbox triggers: fill meter from the bottom (clip from the top), opacity 1
    Clear(); DrawState(p, 0, 0, 1.0f, 1.0f, 0);
    const osd::Spr* xl = Find(xb, S_LT);
    const osd::Spr* xr = Find(xb, S_RT);
    CHECK(xl && xr && Near(xl->h, 103) && Near(xr->h, 103 * 128 / 255.0f, 0.05f) && Near(xr->y, 103 - xr->h, 0.05f) && (xr->c >> 24) == 255);
    CHECK(xr && Near(xr->v0, (xb.s[S_RT].y + 103 * (1 - 128 / 255.0f)) / xb.atlasH, 1e-4f));
    CHECK(Find(xb, S_A_LIT) && Find(xb, S_UP) && Find(xb, S_RB) && Find(xb, S_META) && Find(xb, S_STICK_P));
    // Xbox stick: 25 px travel, 30 deg tilt
    const osd::Spr* xs = Find(xb, S_STICK_P);
    const float xw = 72.0f * std::cos(30.0f * 3.14159265f / 180.0f);
    CHECK(xs && Near(xs->x, 141.5f + 25 + (72 - xw) / 2, 0.05f));

    // opacity scales every alpha; zero trigger draws nothing
    Clear(); DrawState(p, 0, 0, 1.0f, 0.5f, 1);
    { bool anyDim = false; for (auto& s : osd::sprites) anyDim |= (s.c >> 24) <= 0x80u; CHECK(anyDim); }  // pressed art stays solid
    // everything at once stays inside the canvas box for both skins
    State all; all.buttons = 0xFFFF; all.lx = all.ly = all.rx = all.ry = -32768; all.lt = all.rt = 255;
    for (int style = 0; style < 2; ++style) {
        const Skin& k = SkinFor(style);
        for (int corner = 0; corner < 2; ++corner) {
            all.lx = all.ly = all.rx = all.ry = corner ? 32767 : -32768;
            Clear(); DrawState(all, 10, 20, 0.5f, 1.0f, style);
            CHECK(osd::sprites.size() < 40);
            for (auto& s : osd::sprites)
                CHECK(s.w > 0 && s.h > 0 && s.x >= 10 - 1.01f && s.y >= 20 - 0.01f && s.x + s.w <= 10 + k.canvasW * 0.5f + 1.01f &&
                      s.y + s.h <= 20 + k.canvasH * 0.5f + 0.5f);
        }
    }

    // atlas PNGs decode to the generated sizes; the body is opaque in the middle and transparent in the corner
    padviz_png::Atlas atl[2];
    CHECK(padviz_png::DecodeAtlas(kAtlasPng_xbox, sizeof(kAtlasPng_xbox), atl[0]));
    CHECK(padviz_png::DecodeAtlas(kAtlasPng_dualsense, sizeof(kAtlasPng_dualsense), atl[1]));
    for (int i = 0; i < 2; ++i) {
        const Skin& k = SkinFor(i);
        CHECK(atl[i].w == k.atlasW && atl[i].h == k.atlasH && atl[i].levels >= 6);
        if (atl[i].w != k.atlasW) continue;
        const SpriteRect& b = k.s[S_BODY];
        const size_t mid = ((size_t)(b.y + b.h / 2) * k.atlasW + b.x + b.w / 2) * 4 + 3, corner = ((size_t)b.y * k.atlasW + b.x) * 4 + 3;
        CHECK(atl[i].px[mid] == 255 && atl[i].px[corner] == 0);
    }
    unsigned char junk[64] = {137, 80, 78, 71, 13, 10, 26, 10};
    padviz_png::Atlas bad;
    CHECK(!padviz_png::DecodeAtlas(junk, sizeof(junk), bad));
    std::vector<unsigned char> cut(kAtlasPng_xbox, kAtlasPng_xbox + sizeof(kAtlasPng_xbox) / 2);
    CHECK(!padviz_png::DecodeAtlas(cut.data(), cut.size(), bad));

    // previews: idle | A + RB + d-pad up + Start + LT full + RT half + left stick up-right + R3 | Y + B + LB + down-left + back +
    // touchpad + PS + RT full + right stick left
    if (argc > 1 && !fails) {
        for (int style = 0; style < 2; ++style) {
            const Skin& k = SkinFor(style);
            const float s = 420.0f / k.canvasW, pw = k.canvasW * s, ph = k.canvasH * s, gap = 24;
            Img img{(int)(3 * pw + 4 * gap), (int)(ph + 2 * gap), {}};
            img.px.assign((size_t)img.w * img.h * 4, 0);
            for (size_t i = 0; i < (size_t)img.w * img.h; ++i) {  // dark, slightly bluish "game" backdrop
                img.px[i * 4 + 0] = 0.11f; img.px[i * 4 + 1] = 0.12f; img.px[i * 4 + 2] = 0.15f; img.px[i * 4 + 3] = 1;
            }
            State a; a.buttons = kA | kRB | kUp | kStart | kR3; a.lt = 255; a.rt = 128; a.lx = 23000; a.ly = 23000;
            State b; b.buttons = kY | kB | kLB | kDown | kLeft | kBack | kTouch | kGuide; b.rt = 255; b.rx = -32768; b.ry = -8000;
            const State* st3[3] = {&idle, &a, &b};
            Clear();
            for (int i = 0; i < 3; ++i) DrawState(*st3[i], gap + i * (pw + gap), gap, s, 0.9f, style);
            Composite(img, atl[style]);
            const std::string path = std::string(argv[1]) + (style ? "\\padviz_dualsense.tga" : "\\padviz_xbox.tga");
            CHECK(WriteTga(path.c_str(), img));
            printf("preview %s (%dx%d, %zu sprites)\n", path.c_str(), img.w, img.h, osd::sprites.size());
        }
    }
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
