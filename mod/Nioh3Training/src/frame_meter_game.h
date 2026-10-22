// Practice frame meter (SF6 style), game side. Included by training.c after frame_probe_game.h. State machine: frame_meter.h (tests/frame_meter_test.c).
// Input: the FRAMEPROBE signals only (frame_probe_game.h): the boss action id seen by the pump, open hit windows (attack collision still in its pool
// slot) and hits created since the last tick, with the GreatAttack bit (hit +0x13C bit 10 = unblockable, notes/yoritomo-hit-source-22305.md).
// No game memory is read or written beyond what the probe already does. Generic: the first live Training boss slot, no per-boss table.
// Clock: GAME time (user #22575): the game's own step per update tick from FpGameStep (frame_probe_game.h), so one cell is 1/60 s of game time at
// any frame rate; it also stops while the mod menu pauses the game (MENU PAUSE), the F9 freeze or a load.
// Drawing: a click-through topmost layered GDI window over the game (the same fallback renderer as the Rematch standalone menu and the fade window),
// so no Present hook and no other ASI is involved. Hidden while the mod menu is open, the game is not the foreground window or no data yet.
// Ini (Nioh3Training.ini [Practice]): FrameMeter = 0/1 (default 0, the Practice hub row 'Frame meter'), FrameMeterY = top of the meter in % of the
// screen height (default 10: below the boss name and health bar).
#include "frame_meter.h"
static volatile LONG g_fz_hold, g_fz_over, g_fz_frames, g_fz_steps; static int g_fz_vk_freeze, g_fz_vk_step;  // practice_freeze_game.h (Practice freeze + frame advance)
static const char* FzKeyName(int vk, char* b, int n);
static FmMeter g_fm, g_fmp; static volatile LONG g_fmp_show; static SRWLOCK g_fm_lock = SRWLOCK_INIT;
static volatile LONG g_fm_ver, g_fm_show, g_fm_thread; static int g_fm_slot = -1; static double g_fm_clock, g_fm_last; static int g_cfg_fm_y = 10;
static HWND g_fm_wnd, g_fm_game; static uint64_t g_fm_actor; static char g_fm_who[48] = "Boss A";
// Label of the followed boss: "Boss B - <name>" (name from the arena table row of its character id, when there is one).
static void FmWho(int slot, uint32_t chr, char* out, int n) {
    const char* nm = 0;
    for (int i = 0; i < g_nrows && chr; ++i) if (g_rows[i].chr == chr && g_rows[i].name[0]) { nm = g_rows[i].name; break; }
    if (nm) snprintf(out, n, "Boss %c - %s", 'A' + slot, nm); else snprintf(out, n, "Boss %c", 'A' + slot);
}
static DWORD WINAPI FmThread(LPVOID p);
static void FmLoadIni(const char* ini) {
    g_cfg_fm_y = (int)GetPrivateProfileIntA("Practice", "FrameMeterY", 10, ini); if (g_cfg_fm_y < 0 || g_cfg_fm_y > 90) g_cfg_fm_y = 10;
    Log("FRAME METER: [Practice] FrameMeter %d, FrameMeterY %d %% (drawn from the FRAMEPROBE hook signals; GDI overlay window)", g_pst.v[PS_FRAME_METER], g_cfg_fm_y);
}
// FpTick (game thread, FpLock held), once per pump tick.
static void FmFeed(double gdt) {
    const int wantB = g_pst.v[PS_FRAME_METER], wantP = g_pst.v[PS_PLAYER_METER];
    if ((!wantB && !wantP) || !g_pst_live) { g_fm_show = 0; g_fmp_show = 0; g_fm_slot = -1; g_fm_last = 0; return; }
    if (!g_fm_thread && InterlockedExchange(&g_fm_thread, 1) == 0) { HANDLE t = CreateThread(0, 0, FmThread, 0, 0, 0); if (t) CloseHandle(t); else Log("FRAME METER: draw thread failed (%lu)", GetLastError()); }
    double dt = gdt;
    if (dt < 0) dt = 0; if (dt > 150) dt = 150;
    const uint8_t* gs = GameState();
    if (((g_menu_frozen || g_dbg_frozen) && !g_fz_over) || (gs && gs[2])) dt = 0;  // a Practice frame step runs one frame through every stop request
    g_fm_clock += dt;
    // player bar (user #22796): same sampler, same clock, fed from the player's g_fp entry
    if (!wantP) g_fmp_show = 0;
    else if (g_fp[FP_PL].valid) {
        int active = g_fp[FP_PL].pulse, ub = g_fp[FP_PL].pulseUb;
        for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == FP_PL && g_fpw[w].act == g_fp[FP_PL].run.act) { active = 1; if (g_fpw[w].great) ub = 1; }
        AcquireSRWLockExclusive(&g_fm_lock);
        FmSample(&g_fmp, g_fm_clock, g_fp[FP_PL].run.act, active, ub);
        ReleaseSRWLockExclusive(&g_fm_lock);
        g_fmp_show = 1;
    }
    g_fp[FP_PL].pulse = g_fp[FP_PL].pulseUb = 0;
    if (!wantB) { g_fm_show = 0; g_fm_slot = -1; for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = 0; InterlockedIncrement(&g_fm_ver); return; }
    int valid[N3T_SLOTS], busy[N3T_SLOTS];
    for (int i = 0; i < N3T_SLOTS; ++i) {
        valid[i] = g_fp[i].valid; busy[i] = g_fp[i].valid && (g_fp[i].pulse || g_fp[i].pulseUb);
        for (int w = 0; w < FP_WIN && valid[i] && !busy[i]; ++w) if (g_fpw[w].used && g_fpw[w].slot == i && g_fpw[w].act == g_fp[i].run.act) busy[i] = 1;
    }
    const int s = FmPickSlot(g_fm_slot, N3T_SLOTS, valid, busy);  // user #22918: the boss that is attacking, not always the first slot
    if (s < 0) { for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = 0; InterlockedIncrement(&g_fm_ver); return; }  // no boss: the last picture stays
    int active = g_fp[s].pulse, ub = g_fp[s].pulseUb;
    for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = 0;
    // an open window of this boss is active only while it belongs to the CURRENT action: a window opened by the previous action (its hitbox still
    // open when the action id changes) is closed for the meter, so an idle/move action after an attack gets no fake active/recovery run
    for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == s && g_fpw[w].act == g_fp[s].run.act) { active = 1; if (g_fpw[w].great) ub = 1; }
    AcquireSRWLockExclusive(&g_fm_lock);
    if (s != g_fm_slot || g_fp[s].a != g_fm_actor) {  // another slot, or a new boss (retry / respawn / swap) in the same slot: fresh meter
        FmReset(&g_fm); g_fm_slot = s; g_fm_actor = g_fp[s].a;
        FmWho(s, g_fp[s].chr, g_fm_who, sizeof g_fm_who);
        Log("FRAME METER: following %s (chr %u)", g_fm_who, g_fp[s].chr);
    }
    FmSample(&g_fm, g_fm_clock, g_fp[s].run.act, active, ub);
    ReleaseSRWLockExclusive(&g_fm_lock);
    InterlockedIncrement(&g_fm_ver); g_fm_show = 1;
}
// ---- drawing (own thread) ----
#define FM_KEY RGB(255, 0, 255)
static HFONT g_fm_font; static int g_fm_fontPx;
static COLORREF FmColor(uint8_t st) {
    switch (st) {
    case FM_NEUTRAL: return RGB(58, 62, 70);
    case FM_PENDING: return RGB(96, 100, 110);
    case FM_STARTUP: return RGB(40, 200, 90);     // SF6: startup green
    case FM_ACTIVE: return RGB(235, 50, 60);      // active red
    case FM_ACTIVE_UB: return RGB(190, 70, 250);  // unblockable purple
    case FM_RECOVERY: return RGB(40, 120, 235);   // recovery blue
    default: return RGB(24, 26, 32);
    }
}
static void FmFill(HDC dc, int x, int y, int w, int h, COLORREF c) { RECT r = {x, y, x + w, y + h}; HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
static int FmGeom(int W, int H, int* cw, int* barH, int* lineH, int* pad) {  // returns the window height
    const float k = H > 0 ? (float)H / 1080.0f : 1.0f;
    *cw = (int)((float)W * 0.6f / FM_CELLS); if (*cw < 1) *cw = 1;
    *barH = (int)(22 * k); if (*barH < 8) *barH = 8;
    *lineH = (int)(20 * k) + 2; *pad = (int)(6 * k) + 1;
    return *pad * 2 + *lineH * 3 + *barH + 6;
}
static int FmPanels(void) { return (g_pst.v[PS_FRAME_METER] && g_fm_show ? 1 : 0) + (g_pst.v[PS_PLAYER_METER] && g_fmp_show ? 1 : 0); }
static void FmPanel(HDC dc, int w, int h, int H, int oy, const FmMeter* mp, const char* who) {
    const FmMeter m = *mp;
    int cw, barH, lineH, pad; const int ph = FmGeom(w, H, &cw, &barH, &lineH, &pad); (void)h;
    const int px = lineH - 4;
    if (!g_fm_font || g_fm_fontPx != px) { if (g_fm_font) DeleteObject(g_fm_font); g_fm_fontPx = px; g_fm_font = CreateFontW(-px, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas"); }
    const int barW = cw * FM_CELLS, x0 = (w - barW) / 2;
    FmFill(dc, x0 - pad, oy, barW + 2 * pad, ph, RGB(16, 18, 24));
    HGDIOBJ of = SelectObject(dc, g_fm_font); SetBkMode(dc, TRANSPARENT);
    char l1[200], l2[220]; FmText(&m.cur, l1, sizeof l1, l2, sizeof l2);
    char head[240], fz[96] = ""; if (g_fz_hold) { char a[16], b[16]; snprintf(fz, sizeof fz, "  FROZEN +%ld f (%s step, %s resume)", g_fz_frames, FzKeyName(g_fz_vk_step, a, sizeof a), FzKeyName(g_fz_vk_freeze, b, sizeof b)); }
    snprintf(head, sizeof head, "FRAME METER  %s  %s%s", who, l1, fz);
    SetTextColor(dc, m.cur.ub ? RGB(215, 150, 255) : RGB(235, 235, 235)); TextOutA(dc, x0, oy + pad, head, (int)strlen(head));
    SetTextColor(dc, RGB(230, 225, 150)); TextOutA(dc, x0, oy + pad + lineH, l2, (int)strlen(l2));
    const int by = oy + pad + lineH * 2 + 3;
    FmFill(dc, x0 - 1, by - 1, barW + 2, barH + 2, RGB(8, 8, 10));
    for (int i = 0; i < FM_CELLS; ++i) {
        const uint8_t c = FmViewCell(&m, i);
        FmFill(dc, x0 + i * cw, by, cw > 2 ? cw - 1 : cw, barH, FmColor(c & 0x7F));
        if (c & FM_EDGE) FmFill(dc, x0 + i * cw, by - 3, 1, barH + 6, RGB(250, 250, 250));  // action start
    }
    // legend
    static const struct { uint8_t st; const char* t; } lg[] = {{FM_STARTUP, "startup"}, {FM_ACTIVE, "active"}, {FM_ACTIVE_UB, "unblockable"}, {FM_RECOVERY, "recovery"}, {FM_NEUTRAL, "no hit"}, {FM_PENDING, "no hit yet"}};
    int lx = x0; const int ly = by + barH + 4;
    SetTextColor(dc, RGB(180, 185, 200));
    for (int i = 0; i < (int)(sizeof lg / sizeof lg[0]); ++i) {
        FmFill(dc, lx, ly + lineH / 4, lineH / 2, lineH / 2, FmColor(lg[i].st)); lx += lineH / 2 + 4;
        TextOutA(dc, lx, ly, lg[i].t, (int)strlen(lg[i].t)); SIZE sz; GetTextExtentPoint32A(dc, lg[i].t, (int)strlen(lg[i].t), &sz); lx += sz.cx + lineH;
    }
    const char* tail = "1 cell = 1/60 s of game time";
    SIZE ts; GetTextExtentPoint32A(dc, tail, (int)strlen(tail), &ts); if (lx + ts.cx < x0 + barW) TextOutA(dc, x0 + barW - ts.cx, ly, tail, (int)strlen(tail));
    SelectObject(dc, of);
}
static void FmPaint(HDC dc, int w, int h, int H) {
    static FmMeter m, mp; AcquireSRWLockShared(&g_fm_lock); m = g_fm; mp = g_fmp; ReleaseSRWLockShared(&g_fm_lock);
    FmFill(dc, 0, 0, w, h, FM_KEY);
    int cw, barH, lineH, pad; const int ph = FmGeom(w, H, &cw, &barH, &lineH, &pad); int oy = 0; char who[48];
    if (g_pst.v[PS_FRAME_METER] && g_fm_show) { AcquireSRWLockShared(&g_fm_lock); snprintf(who, sizeof who, "%s", g_fm_who); ReleaseSRWLockShared(&g_fm_lock); FmPanel(dc, w, h, H, oy, &m, who); oy += ph; }
    if (g_pst.v[PS_PLAYER_METER] && g_fmp_show) FmPanel(dc, w, h, H, oy, &mp, "Player");
}
static BOOL CALLBACK FmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_fm_game = h; return FALSE;
}
static int g_fm_H;  // game client height (font / bar scale)
static LRESULT CALLBACK FmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom); HGDIOBJ ob = SelectObject(mem, bmp);
        __try { FmPaint(mem, r.right, r.bottom, g_fm_H); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static DWORD WINAPI FmThread(LPVOID p) {
    (void)p;
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = FmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingFrameMeter";
    RegisterClassExW(&wc);
    g_fm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_fm_wnd) { Log("FRAME METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    SetLayeredWindowAttributes(g_fm_wnd, FM_KEY, 235, LWA_COLORKEY | LWA_ALPHA);
    Log("FRAME METER: overlay window ready");
    int shown = 0; LONG lastVer = -1; RECT last = {0};
    for (;;) {
        Sleep(33);
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_fm_game || !IsWindow(g_fm_game)) { g_fm_game = 0; EnumWindows(FmFind, 0); }
        const int want = (g_pst.v[PS_FRAME_METER] || g_pst.v[PS_PLAYER_METER]) && g_pst_live && FmPanels() && g_fm_game && !IsIconic(g_fm_game) && GetForegroundWindow() == g_fm_game && !GateMenuOpen();
        if (!want) { if (shown) { ShowWindow(g_fm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_fm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_fm_game, &o);
        int cw, barH, lineH, pad; g_fm_H = cr.bottom; const int hh = FmGeom(cr.right, cr.bottom, &cw, &barH, &lineH, &pad) * FmPanels();
        RECT want_r = {o.x, o.y + cr.bottom * g_cfg_fm_y / 100, o.x + cr.right, o.y + cr.bottom * g_cfg_fm_y / 100 + hh};
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_fm_wnd, HWND_TOPMOST, want_r.left, want_r.top, want_r.right - want_r.left, hh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastVer = -1;
        }
        const LONG v = g_fm_ver; if (v != lastVer) { lastVer = v; InvalidateRect(g_fm_wnd, 0, FALSE); }
    }
}
