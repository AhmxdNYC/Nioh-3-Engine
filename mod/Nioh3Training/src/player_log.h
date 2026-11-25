// human #122228: record the player's in-game inputs and what the character does, on one timeline.
// [Record] PlayerLog = 1 writes Nioh3PlayerLog.txt next to the ASI (a new file per game start, the previous one kept as .old):
//   hh:mm:ss.mmm  +ms  PAD  down A          (button edges: A B X Y LB RB LT RT LS RS Up Down Left Right Start Back; up edges add 'held N ms')
//   hh:mm:ss.mmm  +ms  PAD  move N / stop   (left stick, 8 directions, dead zone 8000)
//   hh:mm:ss.mmm  +ms  ACT  0x271F -> 0x2A31 (prev ran 412 ms)   (the player's action id = dword [[ctrl+0xE8]], same read as boss moves)
//   PAD / ACT lines end with "| 61 fps 16.4 ms" = fps of the last 1 s clock window and the wall time of the newest game frame (#127933).
// [Record] PlayerClock = 1 (#127918 / #127933) adds one CLK line per second to the same file (player_clock.h):
//   hh:mm:ss.mmm  +ms  CLK  wall 1000 ms  game 998 ms  ratio 1.00  eng 1000 ms 1.00  fps 61  worst 18.2 ms  [nostep N] [stalls N]
//   game = the player's step [ctrl+0x40] (1/60 s units) summed once per engine update (counter G+0x610 moved), eng = the
//   engine clock (ticks x time scale / fps-for-dt). ratio < 0.9 with fps under the cap = the game runs in slow motion.
//   #128463: PlayerClock also adds the move's own speed (player_speed.h): ACT lines end "anim 0.61x" for the move that just
//   ended, and one SPD line per slow stretch >= 300 ms: SPD  slow 0x61D8 980 ms  anim 0.61x (min 0.55x)  step 1.00x  t 4.2 -> 14.1
// #128517 (one final logging update, human #128465):
//   SET  lines at log start: Training version, each mod ASI's size + date, every [Record] key, game window size, EngineFix
//        MaxFps / FG / FrameGenToggle (identifies the player's build and setup without asking).
//   WPN  line at start and on change: active weapon slot (the game's Player::GetActiveWeaponSlot getter, style_meter_game.h)
//        and Samurai/Ninja style byte chr +0xA95C. Weapon item ids / skill ids are not mapped yet (no proven offset).
//   CLK  lines add  dtfps N (the engine's fps-for-dt G+0x614: under the real fps = the engine clamps dt) and  load N
//        (counted frames with the loading flag GameState()[2] set).
// GH #178: with PlayerClock, each SPD line is followed by one SPDW line naming the cause of the slow stretch (player_speed.h):
//   SPDW  0x61D7  total 0.71x = mods [6 0.71x t 41 vt 38FEF30] x base 1.00x x 684 off  step 0.69x  (min total ..  mdl ..  dc0 ..)
//   total = [ctrl+0xdc4] channel 0 (the frame step factor), mods = the active speed-modifier slots counted in channel 0
//   (index, rate [+0xc], timer [+8], vtable rva), base = total / (mods x 684) = MoveSpeedWorldScale or an override, 684 =
//   [ai+0x684] when the getter applies it. Values from the sample in the middle of the stretch. SEH reads only.
// Read only: XInputGetState (its own reader, nothing masked or injected), the player's action id and step, the engine
// update counter. Game thread, every pump tick.
// 0 = off (default) for both keys: nothing is read or written.
// #135494 (human #135492): HITSTOP lines (Player log on only). The player's hit-stop = motion speed ctrl+0x3CC below 1 (0.3 on a hit,
// drained by 0x7D13EC; GH #159). One start + one end line per episode, counted on engine frames (G+0x610 moved):
//   HITSTOP  start  act 0x61D7  speed 0.30  | fps 80  tier 120  VRR armed 1  HitStopFix 1
//   HITSTOP  end    act 0x61D7  real 111 ms  frames 9  speed 0.30  | fps 80  tier 120  VRR armed 1  HitStopFix 1  k 1.50
//   real ms = last normal frame before -> last slow frame (QPC), the same measure as the pc2 A/B poller. act = ACT id at the start.
//   fps = last 1 s clock window, tier / armed / HitStopFix / k (drain scale) from EngineFix Nioh3EngineFix_HitStopInfo ("-" = no export).
//   Rate limit: at most 8 episodes logged per second; the next logged start says how many were skipped.
#ifndef N3_PLAYER_LOG_H
#define N3_PLAYER_LOG_H
#include "player_clock.h"
#include "player_speed.h"  // #128463 move speed (anim time [ctrl+0x60]) in ACT + SPD lines
static int g_cfg_plog, g_cfg_pclk;
static PlClk g_pl_clk;
static PlSpd g_pl_spd;
static FILE* g_pl_f;
static DWORD g_pl_t0, g_pl_flush, g_pl_act_t, g_pl_down_t[20];
static uint32_t g_pl_act = 0xFFFFFFFFu;
static WORD g_pl_btn; static int g_pl_dir = -1, g_pl_lt, g_pl_rt, g_pl_off;
typedef DWORD (WINAPI* PlXGet)(DWORD, void*);
static PlXGet g_pl_xget;
static char g_pl_ini[MAX_PATH];  // #128100 ini path (menu row) / #128517 SET header
static int SmWeaponSlot(void);  // style_meter_game.h (#127057), resolved once on a worker thread
static int g_pl_wslot = -2, g_pl_style = -2; static uint64_t g_pl_wme; static uint32_t g_pl_loadN; static int g_pl_dtfps = -1;
static void PlLine(DWORD now, const char* kind, const char* fmt, ...) {
    if (!g_pl_f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_pl_f, "%02u:%02u:%02u.%03u  +%-7lu %s  ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, now - g_pl_t0, kind);
    va_list ap; va_start(ap, fmt); vfprintf(g_pl_f, fmt, ap); va_end(ap);
    if (kind[0] == 'P' || kind[0] == 'A') fprintf(g_pl_f, "  | %d fps %.1f ms", g_pl_clk.fps, g_pl_clk.frameMs);  // #127933 PAD / ACT
    fputc('\n', g_pl_f);
}
static BOOL CALLBACK PlWndCb(HWND h, LPARAM lp) {
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h)) return TRUE;
    RECT r; if (!GetClientRect(h, &r)) return TRUE;
    long* best = (long*)lp; const long a = (r.right - r.left) * (r.bottom - r.top);
    if (a > best[0] * best[1]) { best[0] = r.right - r.left; best[1] = r.bottom - r.top; }
    return TRUE;
}
// #128517 setup header (log start only; all file reads, no game memory)
static void PlSetup(void) {
    const DWORD now = GetTickCount();
    static const char* const kAsi[] = {"Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi", "Nioh3CamKiProbe.asi"};
    char line[512]; int n = snprintf(line, sizeof line, "Training v%d", Nioh3Training_Version());
    for (int k = 0; k < 4; ++k) {
        char f[MAX_PATH]; WIN32_FILE_ATTRIBUTE_DATA d; snprintf(f, sizeof f, "%s\\%s", g_dir, kAsi[k]);
        if (!GetFileAttributesExA(f, GetFileExInfoStandard, &d)) { n += snprintf(line + n, sizeof line - n, "  %s none", kAsi[k]); continue; }
        FILETIME lt; SYSTEMTIME st; FileTimeToLocalFileTime(&d.ftLastWriteTime, &lt); FileTimeToSystemTime(&lt, &st);
        n += snprintf(line + n, sizeof line - n, "  %s %lu B %04u-%02u-%02u %02u:%02u", kAsi[k], d.nFileSizeLow, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        if (n >= (int)sizeof line) break;
    }
    PlLine(now, "SET", "%s", line);
    char sec[1024]; DWORD m = g_pl_ini[0] ? GetPrivateProfileSectionA("Record", sec, sizeof sec, g_pl_ini) : 0;
    n = snprintf(line, sizeof line, "[Record]");
    for (const char* q = sec; m && *q && n < (int)sizeof line - 2; q += strlen(q) + 1) n += snprintf(line + n, sizeof line - n, " %s", q);
    PlLine(now, "SET", "%s", m ? line : "[Record] (none)");
    long wh[2] = {0, 0}; EnumWindows(PlWndCb, (LPARAM)wh);
    char ef[MAX_PATH]; snprintf(ef, sizeof ef, "%s\\Nioh3EngineFix.ini", g_dir);
    PlLine(now, "SET", "window %ldx%ld  screen %dx%d  EngineFix MaxFps %d  FG %d  FrameGenToggle %d", wh[0], wh[1], GetSystemMetrics(SM_CXSCREEN),
           GetSystemMetrics(SM_CYSCREEN), (int)GetPrivateProfileIntA("VRR", "MaxFps", -1, ef), (int)GetPrivateProfileIntA("Patches", "FG", -1, ef),
           (int)GetPrivateProfileIntA("Menu", "FrameGenToggle", -1, ef));
}
static int PlOpen(void) {
    if (g_pl_f) return 1;
    if (g_pl_off) return 0;
    char p[MAX_PATH], o[MAX_PATH + 8];
    snprintf(p, sizeof p, "%s\\Nioh3PlayerLog.txt", g_dir); snprintf(o, sizeof o, "%s.old", p);
    DeleteFileA(o); MoveFileA(p, o);
    g_pl_f = fopen(p, "w");
    if (!g_pl_f) { g_pl_off = 1; Log("PLAYERLOG: cannot open %s - off", p); return 0; }
    HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll");
    g_pl_xget = x ? (PlXGet)(void*)GetProcAddress(x, "XInputGetState") : 0;
    g_pl_t0 = GetTickCount();
    PlLine(g_pl_t0, "LOG", "Nioh3 player log (human #122228): PAD = controller edges, ACT = player action id changes, | fps + newest frame ms%s%s", g_pl_xget ? "" : " (no XInput: actions only)",
           g_cfg_pclk ? ", CLK = game clock vs wall clock once a second" : "");
    PlLine(g_pl_t0, "LOG", "HITSTOP = the player's hit-stop start/end: real ms, engine frames, speed ctrl+0x3CC, fps, VRR tier/armed, HitStopFix (#135494)");
    PlSetup();
    Log("PLAYERLOG: writing %s (pad %s)", p, g_pl_xget ? "XInputGetState" : "missing");
    return 1;
}
static const char* const kPlBtn[16] = {"Up", "Down", "Left", "Right", "Start", "Back", "LS", "RS", "LB", "RB", "?400", "?800", "A", "B", "X", "Y"};
static const char* const kPlDir[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
static void PlPad(DWORD now) {
    struct { DWORD pkt; WORD b; BYTE lt, rt; SHORT lx, ly, rx, ry; } s;
    if (!g_pl_xget) return;
    int slot = -1;
    for (DWORD u = 0; u < 4 && slot < 0; ++u) if (g_pl_xget(u, &s) == ERROR_SUCCESS) slot = (int)u;
    if (slot < 0) return;
    const WORD ch = (WORD)(s.b ^ g_pl_btn);
    for (int k = 0; k < 16; ++k) if (ch & (1u << k)) {
        if (s.b & (1u << k)) { g_pl_down_t[k] = now; PlLine(now, "PAD", "down %s", kPlBtn[k]); }
        else PlLine(now, "PAD", "up   %s  held %lu ms", kPlBtn[k], now - g_pl_down_t[k]);
    }
    g_pl_btn = s.b;
    const int lt = s.lt > 30, rt = s.rt > 30;
    if (lt != g_pl_lt) { if (lt) { g_pl_down_t[16] = now; PlLine(now, "PAD", "down LT"); } else PlLine(now, "PAD", "up   LT  held %lu ms", now - g_pl_down_t[16]); g_pl_lt = lt; }
    if (rt != g_pl_rt) { if (rt) { g_pl_down_t[17] = now; PlLine(now, "PAD", "down RT"); } else PlLine(now, "PAD", "up   RT  held %lu ms", now - g_pl_down_t[17]); g_pl_rt = rt; }
    int dir = -1;
    const double x = s.lx, y = s.ly;
    if (x * x + y * y > 8000.0 * 8000.0) { double a = atan2(y, x); if (a < 0) a += 6.283185307179586; dir = (int)((a + 0.39269908169872414) / 0.7853981633974483) % 8; }
    if (dir != g_pl_dir) { if (dir < 0) PlLine(now, "PAD", "stop"); else PlLine(now, "PAD", "move %s", kPlDir[dir]); g_pl_dir = dir; }
}
static void PlAct(DWORD now) {
    uint64_t me = PlayerChar(), ai = 0, ctrl = 0;
    if (!me || !Rd64(me + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) return;
    const uint32_t a = ActIdOf(ctrl);
    if (a == 0xFFFFFFFFu || a == g_pl_act) return;
    if (g_pl_act == 0xFFFFFFFFu) PlLine(now, "ACT", "0x%X (first read)", a);
    else if (g_cfg_pclk && PlSpdRateOf(&g_pl_spd, g_pl_act) >= 0)  // #128463 move speed of the move that ended
        PlLine(now, "ACT", "0x%X -> 0x%X (prev ran %lu ms, anim %.2fx)", g_pl_act, a, now - g_pl_act_t, PlSpdRateOf(&g_pl_spd, g_pl_act));
    else PlLine(now, "ACT", "0x%X -> 0x%X (prev ran %lu ms)", g_pl_act, a, now - g_pl_act_t);
    g_pl_act = a; g_pl_act_t = now;
}
// #128517 weapon slot + Samurai/Ninja style, logged at start and on change (-1 = not read yet / unknown)
static void PlWpn(DWORD now) {
    const uint64_t me = PlayerChar();
    const int ws = me ? SmWeaponSlot() : -1;
    int sty = -1;
    if (me) { uint8_t v = 0xFF; __try { v = *(volatile uint8_t*)(me + 0xA95C); } __except (EXCEPTION_EXECUTE_HANDLER) { v = 0xFF; } sty = v <= 1 ? v : -1; }
    if (ws == g_pl_wslot && sty == g_pl_style && me == g_pl_wme) return;
    if (me == g_pl_wme && ws < 0 && g_pl_wslot >= 0 && sty == g_pl_style) return;  // a missed read is not a change
    g_pl_wslot = ws; g_pl_style = sty; g_pl_wme = me;
    if (!me) { PlLine(now, "WPN", "no player"); return; }
    PlLine(now, "WPN", "slot %d  style %s  (weapon/skill ids not mapped yet)", ws, sty == 0 ? "Samurai" : sty == 1 ? "Ninja" : "?");
}
// #135494 hit-stop episodes (see the header)
typedef struct { int on, logged, skip, nWin; uint32_t cnt, frames, act; double tPrev, t0, tLast; float minSpd; DWORD win; int seeded; } PlHs;
static PlHs g_pl_hs;
static int (*p_efHsInfo)(int*, int); static int g_pl_hs_try;
static void PlHsSuffix(char* b, size_t n, int withK) {
    if (!p_efHsInfo && !g_pl_hs_try) { g_pl_hs_try = 1; HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi"); p_efHsInfo = ef ? (int (*)(int*, int))GetProcAddress(ef, "Nioh3EngineFix_HitStopInfo") : 0; }
    int v[5] = {0, 0, 0, 0, 0};
    if (!p_efHsInfo || p_efHsInfo(v, 5) < 5) { snprintf(b, n, "| fps %d  tier -  VRR armed -  HitStopFix -", g_pl_clk.fps); return; }
    const int l = snprintf(b, n, "| fps %d  tier %d  VRR armed %d  HitStopFix %s", g_pl_clk.fps, v[3], v[2] ? 1 : 0, !v[1] ? "n/a" : v[0] ? "1" : "0");
    if (withK && l > 0 && (size_t)l < n) snprintf(b + l, n - (size_t)l, "  k %.2f", v[4] / 1000.0);
}
static void PlHitStop(DWORD now, uint64_t ctrl, uint32_t cnt, double ms) {
    PlHs* h = &g_pl_hs;
    if (!h->seeded) { h->seeded = 1; h->cnt = cnt; h->tPrev = ms; return; }
    if (cnt == h->cnt) return;  // same engine frame
    const uint32_t k = cnt - h->cnt; h->cnt = cnt;
    float sp = 1.0f; __try { sp = *(volatile float*)(ctrl + 0x3CC); } __except (EXCEPTION_EXECUTE_HANDLER) { sp = 1.0f; }
    const int slow = sp > 0.0f && sp < 0.999f && k <= PLCLK_MAX_STEP;
    char sx[128];
    if (slow) {
        if (!h->on) {
            h->on = 1; h->t0 = h->tPrev; h->frames = 0; h->minSpd = sp; h->act = ActIdOf(ctrl);
            if (now - h->win >= 1000) { h->win = now; h->nWin = 0; }
            h->logged = h->nWin < 8;
            if (h->logged) {
                ++h->nWin; PlHsSuffix(sx, sizeof sx, 0);
                char sk[40] = ""; if (h->skip) snprintf(sk, sizeof sk, "  (%d skipped)", h->skip);
                PlLine(now, "HITSTOP", "start  act 0x%X  speed %.2f%s  %s", h->act, sp, sk, sx); h->skip = 0;
            } else ++h->skip;
        }
        h->frames += k; h->tLast = ms; if (sp < h->minSpd) h->minSpd = sp;
    } else if (h->on) {
        h->on = 0;
        if (h->logged) {
            PlHsSuffix(sx, sizeof sx, 1);
            PlLine(now, "HITSTOP", "end    act 0x%X  real %.0f ms  frames %u  speed %.2f%s  %s", h->act, h->tLast - h->t0, h->frames, h->minSpd,
                   k > PLCLK_MAX_STEP ? "  (cut: load/stall)" : "", sx);
        }
    }
    h->tPrev = ms;
}
// GH #178 SPDW reader: SEH reads of ctrl's speed slots (no game calls). Each slot's vfunc code is matched once per vtable
// (PlSpdCodeActive / PlSpdCodeRate); an unrecognised vtable is logged as '?' with its rva.
static PlSpdSamp g_pl_wsamp;
static struct { uint64_t vt; uint8_t act, rate; float one; } g_pl_wvt[16];
static int g_pl_wvtN;
static __attribute__((noinline)) void PlwKinds(uint64_t vt, uint8_t* act, uint8_t* rate, float* one) {
    for (int i = 0; i < g_pl_wvtN; ++i) if (g_pl_wvt[i].vt == vt) { *act = g_pl_wvt[i].act; *rate = g_pl_wvt[i].rate; *one = g_pl_wvt[i].one; return; }
    uint8_t a = 0, r = 0; float o = 1.0f;
    __try {
        const uint64_t fa = *(volatile uint64_t*)(vt + 0x18), fr = *(volatile uint64_t*)(vt + 0x20);
        uint8_t b[25]; int32_t d = 0;
        for (int i = 0; i < 16; ++i) b[i] = *(volatile uint8_t*)(fa + i);
        if (PlSpdCodeActive(b, &d) && *(volatile float*)(fa + 12 + (int64_t)d) == 0.0f) a = 1;
        for (int i = 0; i < 25; ++i) b[i] = *(volatile uint8_t*)(fr + i);
        r = (uint8_t)PlSpdCodeRate(b, &d);
        if (r == 2) o = *(volatile float*)(fr + 20 + (int64_t)d);
    } __except (EXCEPTION_EXECUTE_HANDLER) { a = 0; r = 0; }
    if (g_pl_wvtN < 16) { g_pl_wvt[g_pl_wvtN].vt = vt; g_pl_wvt[g_pl_wvtN].act = a; g_pl_wvt[g_pl_wvtN].rate = r; g_pl_wvt[g_pl_wvtN].one = o; ++g_pl_wvtN; }
    *act = a; *rate = r; *one = o;
}
static __attribute__((noinline)) int PlwRead(uint64_t ctrl, PlSpdMods* m) {
    __try {
        m->total = *(volatile float*)(ctrl + 0xDC4); m->dc0 = *(volatile float*)(ctrl + 0xDC0);
        for (int i = 0; i < PLSPD_SLOTS; ++i) {
            const uint64_t o = *(volatile uint64_t*)(ctrl + 0x6B8 + (uint64_t)i * 8);
            m->st[i] = PLSPD_OFF; m->vt[i] = 0; m->timer[i] = 0.0f; m->rate[i] = 1.0f;
            if (!o) continue;
            const uint64_t vt = *(volatile uint64_t*)o;
            m->vt[i] = (uint32_t)(vt - (uint64_t)B);
            m->timer[i] = *(volatile float*)(o + 8);
            uint8_t ak = 0, rk = 0; float one = 1.0f;
            PlwKinds(vt, &ak, &rk, &one);
            if (ak && !(m->timer[i] > 0.0f)) continue;  // default check: inactive
            if (!ak || !rk) { m->st[i] = PLSPD_UNK; continue; }
            m->rate[i] = (rk == 2 && *(volatile uint8_t*)(o + 0x10)) ? one : *(volatile float*)(o + 0xC);
            m->st[i] = PLSPD_ON;
        }
        m->on684 = 0; m->f684 = 1.0f; m->mdl = -1.0f;
        const uint64_t chr = *(volatile uint64_t*)(ctrl + 0xE0);
        if (chr) {
            const uint64_t ai = *(volatile uint64_t*)(chr + 0x3B0), mdl = *(volatile uint64_t*)(chr + 0x3A0);
            if (mdl) m->mdl = *(volatile float*)(mdl + 0x3018);
            if (ai) {
                m->f684 = *(volatile float*)(ai + 0x684);
                const uint64_t a = *(volatile uint64_t*)(ctrl + 0xE8);
                const uint64_t fl = a ? *(volatile uint64_t*)(a + 0x20) : 0;
                m->on684 = *(volatile uint8_t*)(chr + 0xACC8) && fl && (*(volatile uint64_t*)fl & 0x400000000000000ull);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
// #127933 game clock sample (once per pump tick; counts a frame only when the engine update counter moved)
static void PlClock(DWORD now) {
    static double s_qpf;
    if (!s_qpf) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); s_qpf = 1000.0 / (double)f.QuadPart; }
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    uint64_t g = 0;
    if (!Rd64(B + Grva(0x4759910), &g) || !g) return;
    uint32_t cnt = 0; double eng = -1.0; float step = -1.0f;
    __try {
        cnt = *(volatile uint32_t*)(g + 0x610);
        const int fps = *(volatile int32_t*)(g + 0x614); const float ts = *(volatile float*)(g + 0x624);
        if (fps > 0 && fps <= 1000 && ts >= 0.0f && ts < 16.0f) eng = 1000.0 * (double)ts / (double)fps;
        g_pl_dtfps = (fps > 0 && fps <= 1000) ? fps : -1;  // #128517
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    uint64_t me = PlayerChar(), ai = 0, ctrl = 0;
    float anim = -1.0f; int animOk = 0;
    if (me && Rd64(me + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl) {
        __try { step = *(volatile float*)(ctrl + 0x40); anim = *(volatile float*)(ctrl + 0x60); animOk = 1; }
        __except (EXCEPTION_EXECUTE_HANDLER) { step = -1.0f; animOk = 0; }
    }
    PlClkLine l;
    const uint32_t cnt0 = g_pl_clk.cnt, st0 = g_pl_clk.stalls; const int ok0 = g_pl_clk.ok;
    const double wall0 = g_pl_clk.lastWall;
    { const uint8_t* gs = GameState(); if (gs && gs[2] && cnt != g_pl_clk.cnt) ++g_pl_loadN; }  // #128517 loading frames this window
    const int win = PlClkSample(&g_pl_clk, (double)q.QuadPart * s_qpf, cnt, step, eng, &l);
    if (g_cfg_plog && animOk) PlHitStop(now, ctrl, cnt, (double)q.QuadPart * s_qpf);  // #135494
    // #128463 move speed: once per counted engine frame (counter moved, not a re-seed), the player's anim time [ctrl+0x60]
    if (g_cfg_pclk && ok0 && g_pl_clk.cnt != cnt0) {
        const uint32_t k = g_pl_clk.cnt - cnt0;
        const uint32_t a = animOk ? ActIdOf(ctrl) : 0xFFFFFFFFu;
        PlSpdEvt e;
        const int wasSlow = g_pl_spd.slow;
        if (!animOk || a == 0xFFFFFFFFu || k > PLCLK_MAX_STEP || g_pl_clk.stalls != st0) { const PlSpd z = {0}; g_pl_spd = z; PlSpdSampReset(&g_pl_wsamp); }
        else if (PlSpdFrame(&g_pl_spd, a, anim, step, k, g_pl_clk.lastWall - wall0, &e)) {
            PlLine(now, "SPD", "slow 0x%X %.0f ms  anim %.2fx (min %.2fx)  step %.2fx  t %.1f -> %.1f", e.act, e.ms, e.rate, e.minRate,
                   e.stepRate, e.t0, e.t1);
            const PlSpdMods* w = PlSpdSampMid(&g_pl_wsamp);  // GH #178 cause of the stretch, middle sample
            char wl[640];
            if (w) { PlSpdWFormat(wl, sizeof wl, e.act, w, e.stepRate, g_pl_wsamp.minTotal); PlLine(now, "SPDW", "%s", wl); }
            else PlLine(now, "SPDW", "0x%X  no read", e.act);
            PlSpdSampReset(&g_pl_wsamp);
        }
        if (g_pl_spd.slow) {  // GH #178: sample ctrl's speed factors every counted frame of the stretch
            if (!wasSlow) PlSpdSampReset(&g_pl_wsamp);
            PlSpdMods m; if (PlwRead(ctrl, &m)) PlSpdSampAdd(&g_pl_wsamp, &m);
        } else if (wasSlow) PlSpdSampReset(&g_pl_wsamp);
    }
    if (!win || !g_cfg_pclk) return;
    char x[96] = "";
    if (l.nostep && l.stalls) snprintf(x, sizeof x, "  nostep %u  stalls %u", l.nostep, l.stalls);
    else if (l.nostep) snprintf(x, sizeof x, "  nostep %u", l.nostep);
    else if (l.stalls) snprintf(x, sizeof x, "  stalls %u", l.stalls);
    const size_t xl = strlen(x);
    if (g_pl_loadN) snprintf(x + xl, sizeof x - xl, "  load %u", g_pl_loadN);
    g_pl_loadN = 0;
    PlLine(now, "CLK", "wall %.0f ms  game %.0f ms  ratio %.2f  eng %.0f ms %.2f  fps %d  dtfps %d  worst %.1f ms%s",
           l.wall, l.game, l.ratio, l.eng, l.engRatio, l.fps, g_pl_dtfps, l.worst, x);
}
// human #128100: the mod menu 'Player log' row (EngineFix Settings > Game) turns both [Record] keys on/off together and saves them.
// Get: 1 = both on, 0 = both off, 2 = mixed (one key set by hand in the ini), -1 = ini path not read yet.
N3T_API int Nioh3Training_PlayerLogGet(void) {
    if (!g_pl_ini[0]) return -1;
    return (g_cfg_plog && g_cfg_pclk) ? 1 : (!g_cfg_plog && !g_cfg_pclk) ? 0 : 2;
}
// on: 1 = both on, 0 = both off, -1 = flip (mixed counts as off -> both on). Applies at once (PlTick reads the flags every tick).
N3T_API int Nioh3Training_PlayerLogSet(int on) {
    if (!g_pl_ini[0]) return -1;
    if (on < 0) on = Nioh3Training_PlayerLogGet() != 1;
    on = on ? 1 : 0;
    WritePrivateProfileStringA("Record", "PlayerLog", on ? "1" : "0", g_pl_ini);
    WritePrivateProfileStringA("Record", "PlayerClock", on ? "1" : "0", g_pl_ini);
    g_cfg_plog = on; g_cfg_pclk = on;
    if (!on && g_pl_f) fflush(g_pl_f);
    Log("PLAYERLOG (#128100): menu Player log %s - [Record] PlayerLog %d PlayerClock %d saved", on ? "ON" : "OFF", on, on);
    return on;
}
static __attribute__((noinline)) void PlTick(void) {
    if ((!g_cfg_plog && !g_cfg_pclk) || !PlOpen()) return;
    const DWORD now = GetTickCount();
    PlClock(now);
    if (g_cfg_plog) { PlPad(now); PlAct(now); PlWpn(now); }
    if (now - g_pl_flush >= 1000) { g_pl_flush = now; fflush(g_pl_f); }
}
#endif
