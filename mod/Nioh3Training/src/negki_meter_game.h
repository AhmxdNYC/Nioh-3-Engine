// user #23400: Negative Ki meter, game side. Included by training.c after frame_meter_game.h. Decisions / geometry: negki_meter.h (tests/negki_meter_test.c).
// Sampling: NkmTick from PstTick (game thread, 20 Hz). Per Training boss slot (BvActor): K = [actor+0x3A0]+0x98, reads cur K+0x14, max K+0x18 and the
// second pool K+0x60 (all under __try, read only); when either is below 0 the effective Ki floor (KfEffective(K, KfStock()) = KiFloor 2F89C8 / Ki floor
// depth, practice_stats_game.h) gives the scale. Nothing is written to the game.
// Drawing: like the frame meter (frame_meter_game.h) a click-through topmost layered GDI window over the game, own thread; it only reads the snapshot
// below. The native boss Ki bar is drawn by the game's UI (BossGaugeRowsUpdate 1D21EF8 -> KiRowWidget 235F9A4) in UI-local space (boss-bar quads
// x -440.5..440.5), so there is no cheap screen-position source: the meter sits at [Practice] NegKiMeterY / NegKiMeterW (1/1000 of the game client
// height / width, centred), defaults measured on the 1280 x 720 capture in negki_meter.h. One row per boss with a negative pool, stacked downward.
// Hidden while the mod menu is open, the game is not the foreground window, or no boss has a negative pool.
// Ini: [Practice] NegKiMeter = 0/1 (default 0 since #38077, Boss page row 'Negative Ki meter'), NegKiMeterY (default 89), NegKiMeterW (default 334).
#include "negki_meter.h"
typedef struct { int show; float cur, k60, floorv, max, fCur, f60; } NkmRow;
static NkmRow g_nkm[N3T_SLOTS]; static SRWLOCK g_nkm_lock = SRWLOCK_INIT;
static uint64_t g_nkm_a[N3T_SLOTS]; static DWORD g_nkm_tNeg[N3T_SLOTS]; static int g_nkm_seen[N3T_SLOTS];
static volatile LONG g_nkm_ver, g_nkm_thread, g_nkm_any, g_nkm_logs; static int g_cfg_nkm_y = 89, g_cfg_nkm_w = 334;
static HWND g_nkm_wnd, g_nkm_game;
static DWORD WINAPI NkmThread(LPVOID p);
static void NkmLoadIni(const char* ini) {
    g_cfg_nkm_y = (int)GetPrivateProfileIntA("Practice", "NegKiMeterY", 89, ini); if (g_cfg_nkm_y < 0 || g_cfg_nkm_y > 950) g_cfg_nkm_y = 89;
    g_cfg_nkm_w = (int)GetPrivateProfileIntA("Practice", "NegKiMeterW", 334, ini); if (g_cfg_nkm_w < 50 || g_cfg_nkm_w > 1000) g_cfg_nkm_w = 334;
    Log("NEG KI METER (#23400): [Practice] NegKiMeter %d, NegKiMeterY %d / 1000, NegKiMeterW %d / 1000 (read-only, GDI overlay window)", g_pst.v[PS_NEG_KI_METER], g_cfg_nkm_y, g_cfg_nkm_w);
}
// one boss: 1 = read ok (out filled), 0 = no actor / unreadable
static __attribute__((noinline)) int NkmRead(uint64_t a, float* cur, float* mx, float* k60, int* dead) {
    uint64_t st = 0;
    if (!a || !Rd64(a + 0x3A0, &st) || !st) return 0;
    __try {
        *dead = *(volatile int32_t*)(st + 0x48) == 1 || !*(volatile uint64_t*)(st + 0x50);
        *cur = *(volatile float*)(st + 0x98 + 0x14); *mx = *(volatile float*)(st + 0x98 + 0x18); *k60 = *(volatile float*)(st + 0x98 + 0x60);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static void NkmTick(int live) {
    const int on = g_pst.v[PS_NEG_KI_METER] != 0;
    NkmRow rows[N3T_SLOTS]; memset(rows, 0, sizeof rows); int any = 0;
    const DWORD now = GetTickCount();
    for (int s = 0; s < N3T_SLOTS; ++s) {
        const uint64_t a = on && live ? BvActor(s) : 0;
        if (a != g_nkm_a[s]) { g_nkm_a[s] = a; g_nkm_tNeg[s] = 0; g_nkm_seen[s] = 0; }  // new boss in the slot (retry / swap): fresh
        float cur = 0, mx = 0, k60 = 0; int dead = 0, which = 0;
        if (!a || !NkmRead(a, &cur, &mx, &k60, &dead)) continue;
        const float neg = NkmNeg(cur, k60, &which);
        if (neg < 0.0f) g_nkm_tNeg[s] = now ? now : 1;
        const int32_t since = g_nkm_tNeg[s] ? (int32_t)(now - g_nkm_tNeg[s]) : -1;
        if (!NkmVisible(on, live, dead, neg, since)) continue;
        NkmRow* r = &rows[s];
        r->show = 1; r->cur = cur; r->k60 = k60; r->max = mx;
        { uint64_t st = 0; r->floorv = Rd64(a + 0x3A0, &st) && st ? KfEffective(st + 0x98, KfStock()) : KfStock(); }  // KiFloor 2F89C8 (SEH-guarded, byte-checked)
        r->fCur = NkmFraction(NkmNegPart(cur), r->floorv, mx); r->f60 = NkmFraction(NkmNegPart(k60), r->floorv, mx);
        any = 1;
        if (!g_nkm_seen[s] && neg < 0.0f) { g_nkm_seen[s] = 1;
            if (InterlockedIncrement(&g_nkm_logs) <= 40) Log("NEG KI METER Boss %c %p: negative Ki detected (cur %.1f, K+60 %.2f, max %.1f, floor %.1f) -> meter %.0f %% (%s)", 'A' + s, (void*)a, cur, k60, mx, r->floorv, 100.0f * (r->fCur > r->f60 ? r->fCur : r->f60), which == 2 ? "K+60 deeper" : "Ki"); }
    }
    AcquireSRWLockExclusive(&g_nkm_lock); memcpy(g_nkm, rows, sizeof rows); ReleaseSRWLockExclusive(&g_nkm_lock);
    InterlockedExchange(&g_nkm_any, any); InterlockedIncrement(&g_nkm_ver);
    if (any && !g_nkm_thread && InterlockedExchange(&g_nkm_thread, 1) == 0) { HANDLE t = CreateThread(0, 0, NkmThread, 0, 0, 0); if (t) CloseHandle(t); else Log("NEG KI METER: draw thread failed (%lu)", GetLastError()); }
}
// ---- drawing (own thread) ----
#define NKM_KEY RGB(255, 0, 255)
static int g_nkm_H, g_nkm_W;
static void NkmFill(HDC dc, int x, int y, int w, int h, COLORREF c) { if (w <= 0 || h <= 0) return; RECT r = {x, y, x + w, y + h}; HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
static int NkmCount(const NkmRow* rows) { int n = 0; for (int s = 0; s < N3T_SLOTS; ++s) n += rows[s].show != 0; return n; }
static void NkmPaint(HDC dc, int w, int h) {
    NkmRow rows[N3T_SLOTS]; AcquireSRWLockShared(&g_nkm_lock); memcpy(rows, g_nkm, sizeof rows); ReleaseSRWLockShared(&g_nkm_lock);
    NkmFill(dc, 0, 0, w, h, NKM_KEY);
    const NkmGeo g = NkmGeom(g_nkm_W, g_nkm_H, g_cfg_nkm_y, g_cfg_nkm_w);
    const int multi = NkmCount(rows) > 1; int row = 0;
    for (int s = 0; s < N3T_SLOTS; ++s) {
        const NkmRow* r = &rows[s]; if (!r->show) continue;
        const int y = row * g.pitch, by = y + (g.pitch - 3 - g.barH) / 2;
        NkmFill(dc, g.x - 1, by - 1, g.w + 2, g.barH + 2, RGB(70, 28, 34));  // frame
        NkmFill(dc, g.x, by, g.w, g.barH, RGB(18, 8, 10));                   // empty
        const int pDeep = NkmFillPx(r->fCur > r->f60 ? r->fCur : r->f60, g.w), pCur = NkmFillPx(r->fCur, g.w);
        NkmFill(dc, g.x, by, pDeep, g.barH, RGB(110, 18, 28));               // deepest pool (K+0x60 when it is deeper): dark red
        NkmFill(dc, g.x, by, pCur, g.barH, RGB(205, 38, 52));                // negative Ki (cur): red
        char t[96]; const float sc = NkmScale(r->floorv, r->max);
        const float c = NkmNegPart(r->cur), k = NkmNegPart(r->k60);
        char who[8] = ""; if (multi) snprintf(who, sizeof who, "%c ", 'A' + s);
        int n = snprintf(t, sizeof t, "%s-Ki %.0f", who, c < 0.0f ? -c : 0.0f);
        if (k < c && n > 0 && n < (int)sizeof t) n += snprintf(t + n, sizeof t - n, " res %.0f", -k);
        if (n > 0 && n < (int)sizeof t) snprintf(t + n, sizeof t - n, " / %.0f", sc);
        SetTextColor(dc, RGB(235, 110, 115)); N3StrokeText(dc, g.x + g.w + 6, y, t, (int)strlen(t), g.fontPx, N3StrokeWidth(g.fontPx));
        ++row;
    }
}
static BOOL CALLBACK NkmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_nkm_wnd || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_nkm_game = h; return FALSE;
}
static LRESULT CALLBACK NkmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; BeginPaint(h,&ps);
        __try { N3StrokeWindow(h,NkmPaint); } __except (EXCEPTION_EXECUTE_HANDLER) { n3_stroke_layered=0; }
        EndPaint(h,&ps);return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static DWORD WINAPI NkmThread(LPVOID p) {
    (void)p;
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = NkmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingNegKiMeter";
    RegisterClassExW(&wc);
    g_nkm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 10, 0, 0, wc.hInstance, 0);
    if (!g_nkm_wnd) { Log("NEG KI METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    // Per-pixel alpha is supplied by N3StrokeWindow.
    Log("NEG KI METER: overlay window ready");
    int shown = 0; LONG lastVer = -1; RECT last = {0};
    for (;;) {
        Sleep(33);
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_nkm_game || !IsWindow(g_nkm_game)) { g_nkm_game = 0; EnumWindows(NkmFind, 0); }
        NkmRow rows[N3T_SLOTS]; AcquireSRWLockShared(&g_nkm_lock); memcpy(rows, g_nkm, sizeof rows); ReleaseSRWLockShared(&g_nkm_lock);
        const int n = NkmCount(rows);
        const int want = g_pst.v[PS_NEG_KI_METER] && g_pst_live && g_nkm_any && n && g_nkm_game && !IsIconic(g_nkm_game) && GetForegroundWindow() == g_nkm_game && !GateMenuOpen();
        if (!want) { if (shown) { ShowWindow(g_nkm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_nkm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_nkm_game, &o);
        g_nkm_W = cr.right; g_nkm_H = cr.bottom;
        const NkmGeo g = NkmGeom(cr.right, cr.bottom, g_cfg_nkm_y, g_cfg_nkm_w);
        RECT want_r = {o.x, o.y + g.top, o.x + cr.right, o.y + g.top + g.pitch * n};
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_nkm_wnd, HWND_TOPMOST, want_r.left, want_r.top, want_r.right - want_r.left, want_r.bottom - want_r.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastVer = -1;
        }
        const LONG v = g_nkm_ver; if (v != lastVer) { lastVer = v; InvalidateRect(g_nkm_wnd, 0, FALSE); }
    }
}
