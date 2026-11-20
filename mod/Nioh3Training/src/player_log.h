// human #122228: record the player's in-game inputs and what the character does, on one timeline.
// [Record] PlayerLog = 1 writes Nioh3PlayerLog.txt next to the ASI (a new file per game start, the previous one kept as .old):
//   hh:mm:ss.mmm  +ms  PAD  down A          (button edges: A B X Y LB RB LT RT LS RS Up Down Left Right Start Back; up edges add 'held N ms')
//   hh:mm:ss.mmm  +ms  PAD  move N / stop   (left stick, 8 directions, dead zone 8000)
//   hh:mm:ss.mmm  +ms  ACT  0x271F -> 0x2A31 (prev ran 412 ms)   (the player's action id = dword [[ctrl+0xE8]], same read as boss moves)
// Read only: XInputGetState (its own reader, nothing masked or injected) and the player's action id. Game thread, every pump tick.
// 0 = off (default): nothing is read or written.
#ifndef N3_PLAYER_LOG_H
#define N3_PLAYER_LOG_H
static int g_cfg_plog;
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
    PlLine(g_pl_t0, "LOG", "Nioh3 player log (human #122228): PAD = controller edges, ACT = player action id changes%s", g_pl_xget ? "" : " (no XInput: actions only)");
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
static __attribute__((noinline)) void PlTick(void) {
    if (!g_cfg_plog || !PlOpen()) return;
    const DWORD now = GetTickCount();
    PlPad(now); PlAct(now);
    if (now - g_pl_flush >= 1000) { g_pl_flush = now; fflush(g_pl_f); }
}
#endif
