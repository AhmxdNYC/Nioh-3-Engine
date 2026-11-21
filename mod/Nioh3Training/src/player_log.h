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
// Read only: XInputGetState (its own reader, nothing masked or injected), the player's action id and step, the engine
// update counter. Game thread, every pump tick.
// 0 = off (default) for both keys: nothing is read or written.
#ifndef N3_PLAYER_LOG_H
#define N3_PLAYER_LOG_H
#include "player_clock.h"
static int g_cfg_plog, g_cfg_pclk;
static PlClk g_pl_clk;
static FILE* g_pl_f;
static DWORD g_pl_t0, g_pl_flush, g_pl_act_t, g_pl_down_t[20];
static uint32_t g_pl_act = 0xFFFFFFFFu;
static WORD g_pl_btn; static int g_pl_dir = -1, g_pl_lt, g_pl_rt, g_pl_off;
typedef DWORD (WINAPI* PlXGet)(DWORD, void*);
static PlXGet g_pl_xget;
static void PlLine(DWORD now, const char* kind, const char* fmt, ...) {
    if (!g_pl_f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_pl_f, "%02u:%02u:%02u.%03u  +%-7lu %s  ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, now - g_pl_t0, kind);
    va_list ap; va_start(ap, fmt); vfprintf(g_pl_f, fmt, ap); va_end(ap);
    if (kind[0] == 'P' || kind[0] == 'A') fprintf(g_pl_f, "  | %d fps %.1f ms", g_pl_clk.fps, g_pl_clk.frameMs);  // #127933 PAD / ACT
    fputc('\n', g_pl_f);
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
    else PlLine(now, "ACT", "0x%X -> 0x%X (prev ran %lu ms)", g_pl_act, a, now - g_pl_act_t);
    g_pl_act = a; g_pl_act_t = now;
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
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    uint64_t me = PlayerChar(), ai = 0, ctrl = 0;
    if (me && Rd64(me + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl) {
        __try { step = *(volatile float*)(ctrl + 0x40); } __except (EXCEPTION_EXECUTE_HANDLER) { step = -1.0f; }
    }
    PlClkLine l;
    if (!PlClkSample(&g_pl_clk, (double)q.QuadPart * s_qpf, cnt, step, eng, &l) || !g_cfg_pclk) return;
    char x[48] = "";
    if (l.nostep && l.stalls) snprintf(x, sizeof x, "  nostep %u  stalls %u", l.nostep, l.stalls);
    else if (l.nostep) snprintf(x, sizeof x, "  nostep %u", l.nostep);
    else if (l.stalls) snprintf(x, sizeof x, "  stalls %u", l.stalls);
    PlLine(now, "CLK", "wall %.0f ms  game %.0f ms  ratio %.2f  eng %.0f ms %.2f  fps %d  worst %.1f ms%s",
           l.wall, l.game, l.ratio, l.eng, l.engRatio, l.fps, l.worst, x);
}
// human #128100: the mod menu 'Player log' row (EngineFix Settings > Game) turns both [Record] keys on/off together and saves them.
// Get: 1 = both on, 0 = both off, 2 = mixed (one key set by hand in the ini), -1 = ini path not read yet.
static char g_pl_ini[MAX_PATH];
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
    if (g_cfg_plog) { PlPad(now); PlAct(now); }
    if (now - g_pl_flush >= 1000) { g_pl_flush = now; fflush(g_pl_f); }
}
#endif
