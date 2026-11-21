// human #126496 / #126502: style meter, game side. Included by training.c after frame_meter_game.h. Scoring: style_meter.h (tests/style_meter_test.c).
// Runs from the FRAMEPROBE pump (FpTick, game thread, FpLock held): SmCapture before FmFeed, SmFeed after it. Not tied to Training / Practice:
// [StyleMeter] StyleMeter 1 installs the pump on its own (FpTick gate) and the player entry (FpPlayerTick gate) in any mission.
// Inputs (all read only; no game memory is written):
//   hits landed / taken  NfHpDamage (HP damage 787268 in the hit apply 329890) -> SmHpEvent: attacker of the hit (325F40) == the player = a hit landed
//                        on any actor (normal enemies too); victim = the player's HP = damage taken (also counted when Infinite health cuts it to 0).
//                        The hook is the existing one (HpDmgHookInstall, shared with Infinite health / damage multipliers / KiBreakNoFinisher); with
//                        StyleMeter 1 it is installed if nothing else did. Fallback when it cannot be installed: HP drops of the boss slots and of the player.
//   enemy hit window     g_fp[slot].pulse / open g_fpw windows of the boss slots (BvActor: Training bosses AND native bosses with a HUD HP bar,
//                        NativeSlotsTick) = the frame meter's 'active' signal.
//   player i-frames      FmPlayerIfrBits (frame meter I-FR signal); Ki pulse landed FmPlayerKiLanded (frame meter Ki check); action id g_fp[FP_PL].run.act.
// Drawing: own click-through topmost layered window (same approach as the frame meter: GDI into a 32-bit DIB, UpdateLayeredWindow), right side.
// Sprites: style/rank_atlas.png (approved set #126376) embedded with .incbin, decoded once with WIC (ole32 loaded at run time; no new link deps).
#include "style_meter.h"
#include "style_atlas_gen.h"
#define COBJMACROS
#include <wincodec.h>
__asm__(".section .rdata,\"dr\"\n\t.p2align 4\n\t.globl g_sm_png\ng_sm_png:\n\t.incbin \"style/rank_atlas.png\"\n\t.globl g_sm_png_end\ng_sm_png_end:\n\t.byte 0\n\t.text\n");
extern const unsigned char g_sm_png[], g_sm_png_end[];
static SmCfg g_smc; static SmState g_sm; static SRWLOCK g_sm_lock = SRWLOCK_INIT;
static int g_cfg_sm_x = -1, g_cfg_sm_y = -1, g_cfg_sm_scale = 100;
static volatile LONG g_sm_hits, g_sm_taken, g_sm_thread, g_sm_vis, g_sm_rank, g_sm_gauge;  // g_sm_gauge in 1/1000
static int g_sm_enemy; static double g_sm_clock;
static HWND g_sm_game;
static void SmLoadIni(const char* ini) {
    g_cfg_sm = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeter", 0, ini) ? 1 : 0;
    g_cfg_sm_x = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterX", -1, ini); if (g_cfg_sm_x < -1 || g_cfg_sm_x > 1000) g_cfg_sm_x = -1;
    g_cfg_sm_y = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterY", -1, ini); if (g_cfg_sm_y < -1 || g_cfg_sm_y > 1000) g_cfg_sm_y = -1;
    g_cfg_sm_scale = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterScale", 100, ini); if (g_cfg_sm_scale < 25 || g_cfg_sm_scale > 400) g_cfg_sm_scale = 100;
    SmCfgDefaults(&g_smc);
    g_smc.hit = (float)(int)GetPrivateProfileIntA("StyleMeter", "HitPts", SM_HIT_PTS, ini);
    g_smc.dodge = (float)(int)GetPrivateProfileIntA("StyleMeter", "DodgePts", SM_DODGE_PTS, ini);
    g_smc.ki = (float)(int)GetPrivateProfileIntA("StyleMeter", "KiPts", SM_KI_PTS, ini);
    g_smc.repeatPct = (int)GetPrivateProfileIntA("StyleMeter", "RepeatPct", SM_REPEAT_PCT, ini);
    g_smc.repeatFloorPct = (int)GetPrivateProfileIntA("StyleMeter", "RepeatFloorPct", SM_REPEAT_FLOOR_PCT, ini);
    g_smc.multiPct = (int)GetPrivateProfileIntA("StyleMeter", "MultiHitPct", SM_MULTI_PCT, ini);
    g_smc.kiHitMs = (int)GetPrivateProfileIntA("StyleMeter", "KiHitMs", SM_KI_HIT_MS, ini);
    g_smc.decayDelayMs = (int)GetPrivateProfileIntA("StyleMeter", "DecayDelayMs", SM_DECAY_DELAY_MS, ini);
    g_smc.decayPerS = (float)(int)GetPrivateProfileIntA("StyleMeter", "DecayPerSec", SM_DECAY_PER_S, ini);
    g_smc.decayRankPct = (int)GetPrivateProfileIntA("StyleMeter", "DecayRankPct", SM_DECAY_RANK_PCT, ini);
    g_smc.combatMs = (int)GetPrivateProfileIntA("StyleMeter", "CombatMs", SM_COMBAT_MS, ini);
    g_smc.damageGraceMs = (int)GetPrivateProfileIntA("StyleMeter", "DamageGraceMs", SM_DAMAGE_GRACE_MS, ini);
    SmCfgClamp(&g_smc); SmReset(&g_sm);
    Log("STYLE METER: [StyleMeter] StyleMeter %d X %d Y %d Scale %d%% | hit %.0f dodge %.0f ki %.0f repeat %d%% (floor %d%%) multi-hit %d%% decay %.0f/s +%d%%/rank after %d ms, combat %d ms, damage grace %d ms, ki needs a hit within %d ms",
        g_cfg_sm, g_cfg_sm_x, g_cfg_sm_y, g_cfg_sm_scale, g_smc.hit, g_smc.dodge, g_smc.ki, g_smc.repeatPct, g_smc.repeatFloorPct, g_smc.multiPct,
        g_smc.decayPerS, g_smc.decayRankPct, g_smc.decayDelayMs, g_smc.combatMs, g_smc.damageGraceMs, g_smc.kiHitMs);
}
// NfHpDamage (hit apply, game thread): raw = the hit's HP damage before any mod scaling / Infinite-health cut.
static void SmHpEvent(uint64_t hp, int raw, uint64_t hit) {
    if (!g_cfg_sm || raw <= 0 || !hp || !hit || !g_pst_atk_ok) return;
    __try {
        const uint64_t me = PlayerChar(); uint64_t st = 0;
        if (!me) return;
        if (Rd64(me + 0x3A0, &st) && st && st + 0x38 == hp) {
            const uint64_t atk = ((uint64_t (*)(uint64_t))(B + Grva(0x325F40)))(hit);
            if (atk != me) InterlockedIncrement(&g_sm_taken);
            return;
        }
        if (((uint64_t (*)(uint64_t))(B + Grva(0x325F40)))(hit) == me) InterlockedIncrement(&g_sm_hits);
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
}
// FpTick, before FmFeed: an enemy (boss slot) hit window is open this tick (same rule as the frame meter's 'active' cell).
static void SmCapture(void) {
    int on = 0;
    if (!g_cfg_sm) return;
    for (int s = 0; s < N3T_SLOTS && !on; ++s) {
        if (!g_fp[s].valid) continue;
        if (g_fp[s].pulse || g_fp[s].pulseUb || g_fp[s].pulseGrab) on = 1;
        for (int w = 0; w < FP_WIN && !on; ++w) if (g_fpw[w].used && g_fpw[w].slot == s && g_fpw[w].act == g_fp[s].run.act) on = 1;
    }
    g_sm_enemy = on;
}
static __attribute__((noinline)) int SmHpRead(uint64_t a, uint64_t* cur) {
    uint64_t st = 0; *cur = 0;
    if (!a || !Rd64(a + 0x3A0, &st) || !st) return 0;
    __try { *cur = *(volatile uint64_t*)(st + 0x58); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static DWORD WINAPI SmThread(LPVOID p);
// FpTick, after FmFeed (FpLock held).
static void SmFeed(double gdt) {
    if (!g_cfg_sm) return;
    if (!g_hpdmg_hooked) HpDmgHookInstall("style meter (#126502)");
    if (!g_sm_thread && InterlockedExchange(&g_sm_thread, 1) == 0) { HANDLE t = CreateThread(0, 0, SmThread, 0, 0, 0); if (t) CloseHandle(t); else Log("STYLE METER: draw thread failed (%lu)", GetLastError()); }
    double dt = gdt; if (dt < 0) dt = 0; if (dt > 150) dt = 150;
    const uint8_t* gs = GameState();
    if (((g_menu_frozen || g_dbg_frozen) && !g_fz_over) || (gs && gs[2])) dt = 0;  // same clock stops as the frame meter
    g_sm_clock += dt;
    const int pl = g_fp[FP_PL].valid;
    LONG hits = InterlockedExchange(&g_sm_hits, 0), taken = InterlockedExchange(&g_sm_taken, 0);
    if (g_hpdmg_hooked != 1 || !g_pst_atk_ok) {  // fallback: HP drops (boss slots = hits, player = damage; an ally's hit on the boss counts too)
        static uint64_t prevB[N3T_SLOTS], prevBa[N3T_SLOTS], prevP, prevPa;
        for (int s = 0; s < N3T_SLOTS; ++s) {
            uint64_t c = 0; const uint64_t a = g_fp[s].valid ? g_fp[s].a : 0;
            if (!a || !SmHpRead(a, &c)) { prevBa[s] = 0; continue; }
            if (a == prevBa[s] && c < prevB[s]) hits++;
            prevBa[s] = a; prevB[s] = c;
        }
        uint64_t c = 0; const uint64_t a = pl ? g_fp[FP_PL].a : 0;
        if (a && SmHpRead(a, &c)) { if (a == prevPa && c < prevP) taken++; prevPa = a; prevP = c; } else prevPa = 0;
    }
    int ifr = 0, ki = 0;
    if (pl) {
        if (g_fm_pl_stamp == g_fp_frame) { ifr = g_fm_pl_ifr; ki = g_fm_pl_ki; }  // the frame meter's player row already read them this tick
        else {
            static FmiState fs; float a = 0, b = 0, c = 0;
            ifr = (FmPlayerIfrBits(&fs) & FMF_NODMG) != 0;
            g_fm_ki_quiet = 1; ki = FmPlayerKiLanded(g_fp[FP_PL].a, &a, &b, &c); g_fm_ki_quiet = 0;
        }
    }
    const uint32_t act = pl ? g_fp[FP_PL].run.act : SM_ACT_NONE;
    const uint64_t run = pl ? ((uint64_t)g_fp[FP_PL].run.f0 << 32 | act) : 0;
    const int fight = g_pst_live != 0;
    AcquireSRWLockExclusive(&g_sm_lock);
    const int r0 = g_sm.rank, a0 = g_sm.active;
    const int dodge = SmDodgeStep(&g_sm.dodge, g_sm_enemy, ifr, taken > 0);
    float ph = 0.f, pk = -1.f, pd = -1.f;  // human #126744: per-event play log (points each event scored)
    for (LONG i = 0; i < hits && i < 64; ++i) ph += SmOnHit(&g_sm, &g_smc, g_sm_clock, act, run);
    if (ki) pk = SmOnKi(&g_sm, &g_smc, g_sm_clock);
    if (dodge) pd = SmOnDodge(&g_sm, &g_smc, g_sm_clock);
    if (taken > 0) SmOnDamaged(&g_sm, &g_smc, g_sm_clock);
    SmTick(&g_sm, &g_smc, g_sm_clock, dt);
    const int vis = pl && SmVisible(&g_sm, &g_smc, g_sm_clock, fight);
    const int r1 = g_sm.rank, a1 = g_sm.active; const float gg = SmGauge(&g_sm, &g_smc);
    const uint32_t nh = g_sm.hits, nd = g_sm.dodges, nk = g_sm.kis, nt = g_sm.damaged;
    const float gp = g_sm.pts;
    ReleaseSRWLockExclusive(&g_sm_lock);
    if (hits > 0 || ki || dodge || taken > 0) {
        static int evl;
        if (evl++ < 4000) {
            char e[160]; int n = 0; e[0] = 0;
            if (hits > 0) n += snprintf(e + n, sizeof e - n, " hit x%ld +%.1f", (long)hits, ph);
            if (ki && pk > 0) n += snprintf(e + n, sizeof e - n, " ki +%.0f", pk);
            else if (ki) n += snprintf(e + n, sizeof e - n, " ki +0 (no hit in %d ms)", g_smc.kiHitMs);
            if (dodge) n += snprintf(e + n, sizeof e - n, " dodge +%.0f", pd);
            if (taken > 0) n += snprintf(e + n, sizeof e - n, " damaged x%ld", (long)taken);
            Log("STYLE EVENT: t %.0f act %u%s -> %s%s %.0f/%.0f", g_sm_clock, act, e, sm_rank_name[r1], a1 ? "" : " (idle)", gp, g_smc.rankPts[r1]);
        }
    }
    InterlockedExchange(&g_sm_rank, r1); InterlockedExchange(&g_sm_gauge, (LONG)(gg * 1000.f)); InterlockedExchange(&g_sm_vis, vis);
    if (r0 != r1 || a0 != a1) {
        static int lines;
        if (lines++ < 400) Log("STYLE METER: rank %s%s -> %s%s (hits %u dodges %u ki %u damaged %u, fight %d, act %u)", sm_rank_name[r0], a0 ? "" : " (idle)",
            sm_rank_name[r1], a1 ? "" : " (idle)", nh, nd, nk, nt, fight, act);
    }
    // FmFeed's early return (frame meter off) leaves the hook's pulses set; they are consumed here so a stale pulse is not a window next tick
    for (int s = 0; s <= N3T_SLOTS; ++s) g_fp[s].pulse = g_fp[s].pulseUb = g_fp[s].pulseGrab = 0;
}
// ---- drawing (own thread) ----
static const GUID SM_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID SM_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID SM_GUID_PBGRA = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10}};
static HDC g_sm_atlas_dc; static HBITMAP g_sm_atlas_bmp;
// Decode the embedded atlas into a premultiplied BGRA top-down DIB (the AlphaBlend source). Returns 1 on success.
static int SmAtlasLoad(void) {
    typedef HRESULT (WINAPI *CoInitFn)(LPVOID, DWORD);
    typedef HRESULT (WINAPI *CoCreateFn)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
    HMODULE ole = LoadLibraryW(L"ole32.dll");
    CoInitFn coInit = ole ? (CoInitFn)(void*)GetProcAddress(ole, "CoInitializeEx") : 0;
    CoCreateFn coCreate = ole ? (CoCreateFn)(void*)GetProcAddress(ole, "CoCreateInstance") : 0;
    if (!coInit || !coCreate) { Log("STYLE METER: ole32 not available, no sprites"); return 0; }
    coInit(0, COINIT_MULTITHREADED);
    IWICImagingFactory* f = 0; IWICStream* st = 0; IWICBitmapDecoder* dec = 0; IWICBitmapFrameDecode* fr = 0; IWICFormatConverter* cv = 0;
    UINT w = 0, h = 0; int ok = 0; HRESULT hr;
    const DWORD size = (DWORD)(g_sm_png_end - g_sm_png);
    hr = coCreate(&SM_CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, &SM_IID_IWICImagingFactory, (LPVOID*)&f);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateStream(f, &st);
    if (SUCCEEDED(hr)) hr = IWICStream_InitializeFromMemory(st, (BYTE*)g_sm_png, size);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateDecoderFromStream(f, (IStream*)st, 0, WICDecodeMetadataCacheOnDemand, &dec);
    if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(dec, 0, &fr);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateFormatConverter(f, &cv);
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_Initialize(cv, (IWICBitmapSource*)fr, &SM_GUID_PBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_GetSize(cv, &w, &h);
    if (SUCCEEDED(hr) && w == SM_ATLAS_W && h == SM_ATLAS_H) {
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = (LONG)w; bi.bmiHeader.biHeight = -(LONG)h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        void* px = 0; HDC scr = GetDC(0);
        g_sm_atlas_bmp = CreateDIBSection(scr, &bi, DIB_RGB_COLORS, &px, 0, 0);
        g_sm_atlas_dc = CreateCompatibleDC(scr); ReleaseDC(0, scr);
        if (g_sm_atlas_bmp && px && g_sm_atlas_dc) {
            hr = IWICFormatConverter_CopyPixels(cv, 0, w * 4, w * 4 * h, (BYTE*)px);
            if (SUCCEEDED(hr)) { SelectObject(g_sm_atlas_dc, g_sm_atlas_bmp); GdiFlush(); ok = 1; }
        }
    } else if (SUCCEEDED(hr)) hr = E_FAIL;
    if (cv) IWICFormatConverter_Release(cv);
    if (fr) IWICBitmapFrameDecode_Release(fr);
    if (dec) IWICBitmapDecoder_Release(dec);
    if (st) IWICStream_Release(st);
    if (f) IWICImagingFactory_Release(f);
    if (!ok) { if (g_sm_atlas_dc) DeleteDC(g_sm_atlas_dc); if (g_sm_atlas_bmp) DeleteObject(g_sm_atlas_bmp); g_sm_atlas_dc = 0; g_sm_atlas_bmp = 0; }
    Log("STYLE METER: sprite atlas %s (%lu bytes embedded, %ux%u, hr %08lX)", ok ? "decoded" : "NOT decoded, text fallback", size, w, h, (unsigned long)hr);
    return ok;
}
static BOOL CALLBACK SmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_sm_wnd || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_sm_game = h; return FALSE;
}
// Layout in window pixels for client height H (1080 = the sprite design size x SM_LETTER_H_1080P / tallest letter).
typedef struct { float k; int w, h, pad; } SmGeo;
static SmGeo SmGeom(int H) {
    SmGeo g; int lw = 0, lh = 0, ww = 0, wh = 0;
    for (int i = 0; i < SM_RANKS; ++i) { if (sm_atlas_letter[i][2] > lw) lw = sm_atlas_letter[i][2]; if (sm_atlas_letter[i][3] > lh) lh = sm_atlas_letter[i][3];
        if (sm_atlas_word[i][2] > ww) ww = sm_atlas_word[i][2]; if (sm_atlas_word[i][3] > wh) wh = sm_atlas_word[i][3]; }
    g.k = (float)H / 1080.f * (float)g_cfg_sm_scale / 100.f * (float)SM_LETTER_H_1080P / (float)lh;
    g.pad = (int)(8 * g.k) + 2;
    const int gw = sm_atlas_gauge_bg[2] > ww ? sm_atlas_gauge_bg[2] : ww;
    g.w = (int)((lw > gw ? lw : gw) * g.k) + 2 * g.pad;
    g.h = (int)((lh + wh + sm_atlas_gauge_bg[3]) * g.k) + 4 * g.pad;
    return g;
}
static void SmBlit(HDC dst, int x, int y, const int* r, int srcW, float k) {
    const int dw = (int)(srcW * k + 0.5f), dh = (int)(r[3] * k + 0.5f);
    if (dw <= 0 || dh <= 0 || srcW <= 0) return;
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dst, x, y, dw, dh, g_sm_atlas_dc, r[0], r[1], srcW, r[3], bf);
}
// Right-aligned: letter, word under it, thin gauge under the word.
static void SmRender(HWND wnd, const SmGeo* g, int rank, int gaugeMilli) {
    const int w = g->w, h = g->h;
    HDC scr = GetDC(0), mem = CreateCompatibleDC(scr);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = 0;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, 0, 0);
    if (bmp && px) {
        HGDIOBJ ob = SelectObject(mem, bmp);
        memset(px, 0, (size_t)w * h * 4);
        if (rank < 0) rank = 0; if (rank >= SM_RANKS) rank = SM_RANKS - 1;
        const int* L = sm_atlas_letter[rank]; const int* W = sm_atlas_word[rank];
        const int right = w - g->pad; int y = g->pad;
        if (g_sm_atlas_dc) {
            SmBlit(mem, right - (int)(L[2] * g->k + 0.5f), y, L, L[2], g->k); y += (int)(L[3] * g->k) + g->pad;
            SmBlit(mem, right - (int)(W[2] * g->k + 0.5f), y, W, W[2], g->k); y += (int)(W[3] * g->k) + g->pad;
            const int gx = right - (int)(sm_atlas_gauge_bg[2] * g->k + 0.5f);
            SmBlit(mem, gx, y, sm_atlas_gauge_bg, sm_atlas_gauge_bg[2], g->k);
            const int fw = (int)((float)sm_atlas_gauge_fill[2] * (float)gaugeMilli / 1000.f + 0.5f);
            if (fw > 0) SmBlit(mem, gx, y, sm_atlas_gauge_fill, fw, g->k);
        } else {  // no sprites (WIC failed): plain text, so the meter still shows something
            HFONT fn = CreateFontW(-(int)(200 * g->k), 0, 0, 0, FW_BOLD, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Georgia");
            HGDIOBJ of = SelectObject(mem, fn); SetBkMode(mem, TRANSPARENT); SetTextColor(mem, RGB(230, 60, 40));
            RECT tr = {0, y, right, h}; DrawTextA(mem, sm_rank_name[rank], -1, &tr, DT_RIGHT | DT_TOP | DT_SINGLELINE);
            SelectObject(mem, of); DeleteObject(fn); GdiFlush();
            for (int i = 0; i < w * h; ++i) if (px[i] & 0xFFFFFF) px[i] |= 0xFF000000u;  // GDI text leaves alpha 0: opaque where drawn
            const int gy = h - g->pad - (int)(6 * g->k) - 1, gw = (int)(440 * g->k), gx = right - gw, fw = gw * gaugeMilli / 1000;
            for (int yy = gy; yy < gy + (int)(5 * g->k) + 1 && yy < h; ++yy) for (int xx = gx; xx < right; ++xx) if (xx >= 0 && yy >= 0) px[yy * w + xx] = xx < gx + fw ? 0xF5CDB98Bu : 0x50505050u;
        }
        GdiFlush();
        POINT src = {0, 0}; SIZE sz = {w, h};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(wnd, scr, 0, &sz, mem, &src, 0, &bf, ULW_ALPHA);
        SelectObject(mem, ob);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem); ReleaseDC(0, scr);
}
static LRESULT CALLBACK SmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) { PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}
static DWORD WINAPI SmThread(LPVOID p) {
    (void)p;
    __try { SmAtlasLoad(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_sm_atlas_dc = 0; Log("STYLE METER: atlas decode faulted, text fallback"); }
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = SmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingStyleMeter";
    RegisterClassExW(&wc);
    g_sm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_sm_wnd) { Log("STYLE METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    Log("STYLE METER: overlay window ready");
    int shown = 0, lastRank = -1, lastGauge = -1; RECT last = {0};
    for (;;) {
        Sleep(33);
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_sm_game || !IsWindow(g_sm_game)) { g_sm_game = 0; EnumWindows(SmFind, 0); }
        const int gameOk = g_sm_game && !IsIconic(g_sm_game);
        const int fg = gameOk && (GetForegroundWindow() == g_sm_game || g_cfg_fm_unfocused);
        const uint8_t* lgs = GameState(); const int loading = lgs && lgs[2];
        const int want = g_cfg_sm && g_sm_vis && gameOk && fg && !loading && !GateMenuOpen();
        if (!want) { if (shown) { ShowWindow(g_sm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_sm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_sm_game, &o);
        if (cr.right <= 0 || cr.bottom <= 0) continue;
        const SmGeo g = SmGeom(cr.bottom);
        // default anchor from rank_atlas.json: right edge at 1690/1920 of the width, letter centre at 380/1080 of the height
        const int rx = g_cfg_sm_x >= 0 ? cr.right * g_cfg_sm_x / 1000 : cr.right * 1690 / 1920;
        const int cy = g_cfg_sm_y >= 0 ? cr.bottom * g_cfg_sm_y / 1000 : cr.bottom * 380 / 1080;
        const int left = o.x + rx - g.w, top = o.y + cy - (int)(SM_LETTER_H_1080P * (float)cr.bottom / 1080.f * (float)g_cfg_sm_scale / 100.f) / 2 - g.pad;
        RECT want_r = {left, top, left + g.w, top + g.h};
        const int rank = (int)g_sm_rank, gauge = (int)g_sm_gauge;
        const int gpx = (int)(sm_atlas_gauge_fill[2] * g.k) * gauge / 1000;  // redraw only when the gauge moves a pixel
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_sm_wnd, HWND_TOPMOST, want_r.left, want_r.top, g.w, g.h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastRank = -1;
        }
        if (rank != lastRank || gpx != lastGauge) {
            lastRank = rank; lastGauge = gpx;
            __try { SmRender(g_sm_wnd, &g, rank, gauge); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
}
