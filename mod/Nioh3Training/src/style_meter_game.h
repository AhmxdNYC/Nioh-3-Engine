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
#include "style_meter_fx.h"  // #127085 pop-ups + motion
#include "style_atlas_gen.h"
#define COBJMACROS
#include <wincodec.h>
__asm__(".section .rdata,\"dr\"\n\t.p2align 4\n\t.globl g_sm_png\ng_sm_png:\n\t.incbin \"style/rank_atlas.png\"\n\t.globl g_sm_png_end\ng_sm_png_end:\n\t.byte 0\n\t.text\n");
extern const unsigned char g_sm_png[], g_sm_png_end[];
static SmCfg g_smc; static SmState g_sm; static SRWLOCK g_sm_lock = SRWLOCK_INIT;
static int g_cfg_sm_x = -1, g_cfg_sm_y = -1, g_cfg_sm_scale = 100;
static int g_cfg_sm_actlog, g_cfg_sm_movelog;
static uint64_t g_sm_swapRun;  // #127057 the player move (run key) that scored the weapon swap attack bonus (move log tag)  // human #126980: [StyleMeter] ActLog=1 logs every player action id change (finds stance/weapon swap, guardian, martial art ids)
static volatile LONG g_sm_hits, g_sm_taken, g_sm_thread, g_sm_vis, g_sm_rank, g_sm_gauge;  // g_sm_gauge in 1/1000
static int g_sm_enemy; static double g_sm_clock;
// #127085 pop-ups + motion, game thread -> draw thread. [StyleMeter] PopUps 1 / Motion 1 (default on), PopUpMs, ShakePts.
static int g_cfg_sm_pops = 1, g_cfg_sm_motion = 1, g_cfg_sm_popms = SM_POP_LIFE_MS, g_cfg_sm_shakepts = SM_SHAKE_PTS;
static SRWLOCK g_smfx_lock = SRWLOCK_INIT; static SmPopQ g_smfx_q; static double g_smfx_shakeT = -1e18;
static double SmWallMs(void) { static LARGE_INTEGER f; LARGE_INTEGER c; if (!f.QuadPart) QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
// one '<label> +pts' pop-up (any thread). Later bonuses (Perfect Ki, Martial Art, Stance Swap, Guardian Spirit) call this with their own label.
static void SmFxPop2(const char* label, float pts, float bonus) {
    if (!g_cfg_sm_pops || pts <= 0.f) return;
    AcquireSRWLockExclusive(&g_smfx_lock); SmPopPush2(&g_smfx_q, label, pts, bonus, SmWallMs(), g_cfg_sm_popms); ReleaseSRWLockExclusive(&g_smfx_lock);
}
static void SmFxPop(const char* label, float pts) { SmFxPop2(label, pts, 0.f); }
static HWND g_sm_game;
static void SmReadIni(const char* ini, int reset) {
    g_cfg_sm = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeter", 0, ini) ? 1 : 0;
    g_cfg_sm_x = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterX", -1, ini); if (g_cfg_sm_x < -1 || g_cfg_sm_x > 1000) g_cfg_sm_x = -1;
    g_cfg_sm_y = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterY", -1, ini); if (g_cfg_sm_y < -1 || g_cfg_sm_y > 1000) g_cfg_sm_y = -1;
    g_cfg_sm_actlog = (int)GetPrivateProfileIntA("StyleMeter", "ActLog", 0, ini);
    g_cfg_sm_movelog = (int)GetPrivateProfileIntA("StyleMeter", "MoveLog", 1, ini);
    g_cfg_sm_pops = (int)GetPrivateProfileIntA("StyleMeter", "PopUps", 1, ini) ? 1 : 0;  // #127085
    g_cfg_sm_motion = (int)GetPrivateProfileIntA("StyleMeter", "Motion", 1, ini) ? 1 : 0;
    g_cfg_sm_popms = (int)GetPrivateProfileIntA("StyleMeter", "PopUpMs", SM_POP_LIFE_MS, ini); if (g_cfg_sm_popms < 300 || g_cfg_sm_popms > 5000) g_cfg_sm_popms = SM_POP_LIFE_MS;
    g_cfg_sm_shakepts = (int)GetPrivateProfileIntA("StyleMeter", "ShakePts", SM_SHAKE_PTS, ini); if (g_cfg_sm_shakepts < 1) g_cfg_sm_shakepts = SM_SHAKE_PTS;  // #127003: on with the style meter (SmFeed only runs when StyleMeter=1)
    g_cfg_sm_scale = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterScale", 100, ini); if (g_cfg_sm_scale < 25 || g_cfg_sm_scale > 400) g_cfg_sm_scale = 100;
    SmCfg c; SmCfgDefaults(&c);
    c.hit = (float)(int)GetPrivateProfileIntA("StyleMeter", "HitPts", SM_HIT_PTS, ini);
    c.dodge = (float)(int)GetPrivateProfileIntA("StyleMeter", "DodgePts", SM_DODGE_PTS, ini);
    c.ki = (float)(int)GetPrivateProfileIntA("StyleMeter", "KiPts", SM_KI_PTS, ini);
    c.kiPerfect = (float)(int)GetPrivateProfileIntA("StyleMeter", "KiPerfectPts", SM_KI_PERFECT_PTS, ini);  // #127075
    c.repeatPct = (int)GetPrivateProfileIntA("StyleMeter", "RepeatPct", SM_REPEAT_PCT, ini);
    c.repeatFloorPct = (int)GetPrivateProfileIntA("StyleMeter", "RepeatFloorPct", SM_REPEAT_FLOOR_PCT, ini);
    c.multiPct = (int)GetPrivateProfileIntA("StyleMeter", "MultiHitPct", SM_MULTI_PCT, ini);
    c.kiHitMs = (int)GetPrivateProfileIntA("StyleMeter", "KiHitMs", SM_KI_HIT_MS, ini);
    c.decayDelayMs = (int)GetPrivateProfileIntA("StyleMeter", "DecayDelayMs", SM_DECAY_DELAY_MS, ini);
    c.decayPerS = (float)(int)GetPrivateProfileIntA("StyleMeter", "DecayPerSec", SM_DECAY_PER_S, ini);
    {   // human #126804 menu 'Drain speed': % of DecayPerSec (100 = as set)
        const int dp = (int)GetPrivateProfileIntA("StyleMeter", "DrainPct", 100, ini);
        if (dp >= 0 && dp != 100) c.decayPerS = c.decayPerS * (float)dp / 100.f;
    }
    c.decayRankPct = (int)GetPrivateProfileIntA("StyleMeter", "DecayRankPct", SM_DECAY_RANK_PCT, ini);
    c.decayRampPct = (int)GetPrivateProfileIntA("StyleMeter", "DecayRampPct", SM_DECAY_RAMP_PCT, ini);  // #126972
    c.combatMs = (int)GetPrivateProfileIntA("StyleMeter", "CombatMs", SM_COMBAT_MS, ini);
    c.damageGraceMs = (int)GetPrivateProfileIntA("StyleMeter", "DamageGraceMs", SM_DAMAGE_GRACE_MS, ini);
    c.startupMinF = (int)GetPrivateProfileIntA("StyleMeter", "StartupMinFrames", SM_STARTUP_MIN_F, ini);   // #126821
    c.startupMaxF = (int)GetPrivateProfileIntA("StyleMeter", "StartupMaxFrames", SM_STARTUP_MAX_F, ini);
    c.startupPct = (int)GetPrivateProfileIntA("StyleMeter", "StartupBonusPct", SM_STARTUP_PCT, ini);
    c.counter = (float)(int)GetPrivateProfileIntA("StyleMeter", "CounterPts", SM_COUNTER_PTS, ini);       // #126843
    c.style = (float)(int)GetPrivateProfileIntA("StyleMeter", "StyleSwitchPts", SM_STYLE_PTS, ini);
    c.nin = (float)(int)GetPrivateProfileIntA("StyleMeter", "NinjutsuPts", SM_NIN_PTS, ini);              // #126854
    c.swap = (float)(int)GetPrivateProfileIntA("StyleMeter", "WeaponSwapPts", SM_SWAP_PTS, ini);          // #127057
    c.swapMs = (int)GetPrivateProfileIntA("StyleMeter", "WeaponSwapMs", SM_SWAP_MS, ini);
    c.soul = (float)(int)GetPrivateProfileIntA("StyleMeter", "SoulCorePts", SM_SOUL_PTS, ini);            // #126850 / #126913 (PstPlayerSoul use count)
    {   // human #126968: RankPts = "D,C,B,A,S,SS,SSS" points per rank (low ranks easier, top ranks harder); empty/bad = defaults
        char buf[128] = {0}; GetPrivateProfileStringA("StyleMeter", "RankPts", "", buf, sizeof buf, ini);
        float v[SM_RANKS]; int n = 0; const char* p = buf;
        while (*p && n < SM_RANKS) { char* e; const double d = strtod(p, &e); if (e == p || d < 1.0 || d > 100000.0) break; v[n++] = (float)d; p = e; while (*p == ',' || *p == ' ') ++p; }
        if (n == SM_RANKS && !*p) for (int i = 0; i < SM_RANKS; ++i) c.rankPts[i] = v[i];
        else if (buf[0]) Log("STYLE METER: RankPts '%s' ignored (needs %d numbers 1..100000)", buf, SM_RANKS);
    }
    {   // RankPtsPct: scales every rank's size (100 = defaults; 50 = the pre-#126755 easy meter)
        const int rp = (int)GetPrivateProfileIntA("StyleMeter", "RankPtsPct", 100, ini);
        if (rp > 0 && rp != 100) for (int i = 0; i < SM_RANKS; ++i) c.rankPts[i] = c.rankPts[i] * (float)rp / 100.f;
    }
    SmCfgClamp(&c);
    AcquireSRWLockExclusive(&g_sm_lock);
    g_smc = c;
    if (reset) SmReset(&g_sm);
    else if (g_sm.pts > g_smc.rankPts[g_sm.rank]) g_sm.pts = g_smc.rankPts[g_sm.rank];  // live re-read (#126804): keep the rank, cap the gauge
    ReleaseSRWLockExclusive(&g_sm_lock);
    Log("STYLE METER: bonuses startup x1..x%.2f over %d..%d f, counter %.0f, style switch %.0f, ninjutsu %.0f, soul core %.0f, weapon swap attack %.0f within %d ms (#126821 #126843 #126854 #126913 #127057)",
        1.f + g_smc.startupPct / 100.f, g_smc.startupMinF, g_smc.startupMaxF, g_smc.counter, g_smc.style, g_smc.nin, g_smc.soul, g_smc.swap, g_smc.swapMs);
    Log("STYLE METER: [StyleMeter] StyleMeter %d X %d Y %d Scale %d%% | hit %.0f dodge %.0f ki %.0f repeat %d%% (floor %d%%) multi-hit %d%% decay %.0f/s +%d%%/rank +%d%%/idle s after %d ms, combat %d ms, damage grace %d ms, ki needs a hit within %d ms, ranks %.0f %.0f %.0f %.0f %.0f %.0f %.0f",
        g_cfg_sm, g_cfg_sm_x, g_cfg_sm_y, g_cfg_sm_scale, g_smc.hit, g_smc.dodge, g_smc.ki, g_smc.repeatPct, g_smc.repeatFloorPct, g_smc.multiPct,
        g_smc.decayPerS, g_smc.decayRankPct, g_smc.decayRampPct, g_smc.decayDelayMs, g_smc.combatMs, g_smc.damageGraceMs, g_smc.kiHitMs, g_smc.rankPts[0], g_smc.rankPts[1], g_smc.rankPts[2], g_smc.rankPts[3], g_smc.rankPts[4], g_smc.rankPts[5], g_smc.rankPts[6]);
}
// human #126804: settings adjustable live. The ini is re-read when its write time changes (checked every ~0.5 s from the pump), so an edit of
// [StyleMeter] or a menu change applies without a restart.
static char g_sm_ini[MAX_PATH]; static FILETIME g_sm_ini_t;
static int SmIniTime(FILETIME* t) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!g_sm_ini[0] || !GetFileAttributesExA(g_sm_ini, GetFileExInfoStandard, &d)) return 0;
    *t = d.ftLastWriteTime; return 1;
}
static void SmLoadIni(const char* ini) {
    snprintf(g_sm_ini, sizeof g_sm_ini, "%s", ini);
    SmIniTime(&g_sm_ini_t);
    SmReadIni(ini, 1);
}
static void SmIniPoll(void) {
    static DWORD last; const DWORD now = GetTickCount();
    if (now - last < 500) return;
    last = now;
    FILETIME t;
    if (!SmIniTime(&t) || CompareFileTime(&t, &g_sm_ini_t) == 0) return;
    g_sm_ini_t = t;
    Log("STYLE METER: Nioh3Training.ini changed, re-reading [StyleMeter] (#126804)");
    SmReadIni(g_sm_ini, 0);
}
// Menu (Boss Rematch Player page, #126804; human #127379 full control for player testing): 'what' indexes sm_menu_keys.
// Get: -1 = unknown what. Set: writes [StyleMeter] and applies at once (value < 0 on an on/off key = toggle). Returns the stored value.
// Keep the order: Boss Rematch's menu rows use these indices (0..4 are the original #126804 / #127075 rows).
typedef struct { const char* key; int def, lo, hi; } SmMenuKey;
static const SmMenuKey sm_menu_keys[] = {
    {"StyleMeter", 0, 0, 1}, {"RankPtsPct", 100, 25, 400}, {"DrainPct", 100, 0, 500}, {"DecayDelayMs", SM_DECAY_DELAY_MS, 0, 15000},
    {"KiPerfectPts", SM_KI_PERFECT_PTS, 0, 200},
    {"PopUps", 1, 0, 1}, {"Motion", 1, 0, 1}, {"StyleMeterScale", 100, 25, 400}, {"PopUpMs", SM_POP_LIFE_MS, 300, 5000},
    {"HitPts", SM_HIT_PTS, 0, 200}, {"DodgePts", SM_DODGE_PTS, 0, 200}, {"KiPts", SM_KI_PTS, 0, 200}, {"CounterPts", SM_COUNTER_PTS, 0, 200},
    {"NinjutsuPts", SM_NIN_PTS, 0, 200}, {"SoulCorePts", SM_SOUL_PTS, 0, 200}, {"StyleSwitchPts", SM_STYLE_PTS, 0, 200},
    {"WeaponSwapPts", SM_SWAP_PTS, 0, 200}, {"StartupBonusPct", SM_STARTUP_PCT, 0, 500}, {"RepeatPct", SM_REPEAT_PCT, 0, 100},
    {"DecayRampPct", SM_DECAY_RAMP_PCT, 0, 200},
};
#define SM_MENU_KEYS ((int)(sizeof sm_menu_keys / sizeof sm_menu_keys[0]))
static const char* SmKey(int what) { return what >= 0 && what < SM_MENU_KEYS ? sm_menu_keys[what].key : 0; }
static int SmKeyDefault(int what) { return sm_menu_keys[what].def; }
static int SmKeyClamp(int what, int v) { const SmMenuKey* k = &sm_menu_keys[what]; return v < k->lo ? k->lo : v > k->hi ? k->hi : v; }
N3T_API int Nioh3Training_StyleKeys(void) { return SM_MENU_KEYS; }  // #127379 how many 'what' rows this build knows
N3T_API int Nioh3Training_StyleGet(int what) {
    const char* k = SmKey(what);
    if (!k || !g_sm_ini[0]) return -1;
    return SmKeyClamp(what, (int)GetPrivateProfileIntA("StyleMeter", k, SmKeyDefault(what), g_sm_ini));
}
N3T_API int Nioh3Training_StyleSet(int what, int value) {
    const char* k = SmKey(what);
    if (!k || !g_sm_ini[0]) return -1;
    if (value < 0 && sm_menu_keys[what].hi == 1) value = !GetPrivateProfileIntA("StyleMeter", k, SmKeyDefault(what), g_sm_ini);  // on/off toggle
    value = SmKeyClamp(what, value);
    char b[16]; snprintf(b, sizeof b, "%d", value);
    WritePrivateProfileStringA("StyleMeter", k, b, g_sm_ini);
    SmIniTime(&g_sm_ini_t);
    SmReadIni(g_sm_ini, 0);
    Log("STYLE METER: menu %s = %d (#126804)", k, value);
    return value;
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
// #126854 ninjutsu used: one of the player's 3 ninjutsu gauges (status +0x36AC + k*0x30 cur, +0x36B0 max; the Practice infinite-ninjutsu
// fields) dropped by >= 1 on the same status block. Returns the number of gauges that dropped this tick. Read only.
static __attribute__((noinline)) int SmNinjutsuUsed(uint64_t actor) {
    static uint64_t pst; static float prev[3]; static int have;
    uint64_t st = 0; int n = 0;
    if (!actor || !Rd64(actor + 0x3A0, &st) || !st) { have = 0; return 0; }
    float cur[3];
    __try { for (int k = 0; k < 3; ++k) { const float c = *(volatile float*)(st + 0x36AC + k * 0x30), m = *(volatile float*)(st + 0x36B0 + k * 0x30); cur[k] = (m > 0.f && m < 1e6f && c >= 0.f && c <= m * 1.01f) ? c : -1.f; } }
    __except (EXCEPTION_EXECUTE_HANDLER) { have = 0; return 0; }
    if (have && st == pst) for (int k = 0; k < 3; ++k) if (cur[k] >= 0.f && prev[k] >= 0.f && prev[k] - cur[k] >= 1.f) n++;
    for (int k = 0; k < 3; ++k) prev[k] = cur[k];
    pst = st; have = 1;
    return n;
}
// #127003 move logger pad snapshot: raw XInput (xinput1_4 directly, not the game's IAT slot), the first connected pad with input, else pad 0.
static void SmPadNow(uint16_t* b, uint8_t* lt, uint8_t* rt) {
    typedef DWORD (WINAPI *XgsFn)(DWORD, void*); static XgsFn xgs; static int tried;
    *b = 0; *lt = *rt = 0;
    if (!xgs && !tried) { tried = 1; HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll"); xgs = x ? (XgsFn)GetProcAddress(x, "XInputGetState") : 0; }
    if (!xgs) return;
    for (DWORD i = 0; i < 4; ++i) { uint8_t st[16] = {0}; if (xgs(i, st) != 0) continue;  // XINPUT_STATE: packet, wButtons +4, LT +6, RT +7
        const uint16_t bb = *(uint16_t*)(st + 4); if (bb || st[6] > 64 || st[7] > 64 || !*b) { *b = bb; *lt = st[6]; *rt = st[7]; } if (bb || st[6] > 64 || st[7] > 64) return; }
}
// #127057 active weapon slot = the game's own getter behind the script native "Player::GetActiveWeaponSlot" (found by name in the native
// table, like BR's sites kNative). That native pushes its result on the script VM stack, so it is not callable from here; its body is
// "fn([G2], [[G1]] + idoff)" and fn is the plain 2-load getter. The wrapper and fn bytes are checked, all addresses decoded from them
// (no fixed RVA). Resolved once on a worker thread (whole-image string scan off the game thread); any mismatch = off, logged.
static volatile LONG g_ws_state;  // 0 not started, 1 resolving, 2 ready, 3 failed
static uint32_t (*g_ws_fn)(uint64_t, uint32_t); static uint64_t g_ws_g1, g_ws_g2; static uint32_t g_ws_idoff;
static int SmHexNib(char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : 0; }
static int SmBytesAt(const uint8_t* p, const char* pat) {  // "48 8B ?? .." ; 1 = match
    for (const char* q = pat; *q;) {
        if (*q == ' ') { ++q; continue; }
        if (q[0] != '?' && *p != (uint8_t)(SmHexNib(q[0]) << 4 | SmHexNib(q[1]))) return 0;
        ++p; q += 2;
    }
    return 1;
}
static __attribute__((noinline)) const char* SmWsResolve(void) {
    static const char key[] = "Player::GetActiveWeaponSlot";
    __try {
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)B; const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(B + dos->e_lfanew);
        const IMAGE_SECTION_HEADER* sh0 = IMAGE_FIRST_SECTION(nt); const int ns = nt->FileHeader.NumberOfSections;
        const uint64_t lo = B, hi = B + nt->OptionalHeader.SizeOfImage;
        uint64_t str = 0, ent = 0; int refs = 0;
        for (int k = 0; k < ns && !str; ++k) {
            const IMAGE_SECTION_HEADER* sh = sh0 + k;
            if ((sh->Characteristics & IMAGE_SCN_MEM_EXECUTE) || !(sh->Characteristics & IMAGE_SCN_MEM_READ) || sh->Misc.VirtualSize < 64) continue;
            const uint8_t* p = (const uint8_t*)(B + sh->VirtualAddress) + 1; const uint8_t* e = (const uint8_t*)(B + sh->VirtualAddress) + sh->Misc.VirtualSize - sizeof key;
            for (; p < e; ++p) if (*p == 'P' && p[-1] == 0 && !memcmp(p, key, sizeof key)) { str = (uint64_t)p; break; }
        }
        if (!str) return "name string not found";
        for (int k = 0; k < ns && !ent; ++k) {
            const IMAGE_SECTION_HEADER* sh = sh0 + k;
            if ((sh->Characteristics & IMAGE_SCN_MEM_EXECUTE) || !(sh->Characteristics & IMAGE_SCN_MEM_READ) || sh->Misc.VirtualSize < 64) continue;
            const uint64_t* p = (const uint64_t*)(B + ((sh->VirtualAddress + 7) & ~7u)); const uint64_t* e = (const uint64_t*)(B + sh->VirtualAddress + sh->Misc.VirtualSize - 16);
            // the name is referenced from more than one table (live 1.0x: a 2nd table points at another function); the native entry is
            // the one whose function is this binding's wrapper shape
            for (; p < e; ++p) if (*p == str && p[1] > lo && p[1] < hi) {
                const uint8_t* w = (const uint8_t*)p[1]; ++refs;
                if (SmBytesAt(w, "40 53 48 81 EC 70 01 00 00") && SmBytesAt(w + 0x28, "48 8B 05 ?? ?? ?? ?? 48 8B 08 8B 91 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 54 24 20 89 44 24 20")) { ent = (uint64_t)p; break; }
            }
        }
        if (!ent) return refs ? "no reference has the wrapper bytes (game patch)" : "native table entry not found";
        const uint8_t* w = (const uint8_t*)((const uint64_t*)ent)[1];
        const uint64_t g1 = (uint64_t)(w + 0x2F) + *(const int32_t*)(w + 0x2B), g2 = (uint64_t)(w + 0x3F) + *(const int32_t*)(w + 0x3B);
        const uint8_t* fn = w + 0x44 + *(const int32_t*)(w + 0x40);
        if (g1 <= lo || g1 >= hi || g2 <= lo || g2 >= hi || (uint64_t)fn <= lo || (uint64_t)fn >= hi) return "decoded address outside the image";
        if (!SmBytesAt(fn, "48 8B 01 85 D2 75 07 8B 80 ?? ?? ?? ?? C3 8B 80 ?? ?? ?? ?? C3")) return "getter bytes differ (game patch)";
        g_ws_g1 = g1; g_ws_g2 = g2; g_ws_idoff = *(const uint32_t*)(w + 0x34); g_ws_fn = (uint32_t (*)(uint64_t, uint32_t))fn;
        Log("STYLE METER: weapon slot getter %s -> +%llX (wrapper +%llX, G1 +%llX id +0x%X, G2 +%llX) (#127057)", key, (unsigned long long)((uint64_t)fn - B),
            (unsigned long long)((uint64_t)w - B), (unsigned long long)(g1 - B), g_ws_idoff, (unsigned long long)(g2 - B));
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "fault while resolving"; }
}
static DWORD WINAPI SmWsThread(LPVOID p) {
    (void)p; const char* why = SmWsResolve();
    if (why) Log("STYLE METER: weapon slot getter off: %s (#127057)", why);
    InterlockedExchange(&g_ws_state, why ? 3 : 2); return 0;
}
static __attribute__((noinline)) int SmWeaponSlot(void) {  // -1 = unknown
    if (g_ws_state == 0 && InterlockedCompareExchange(&g_ws_state, 1, 0) == 0) { HANDLE t = CreateThread(0, 0, SmWsThread, 0, 0, 0); if (t) CloseHandle(t); else InterlockedExchange(&g_ws_state, 3); }
    if (g_ws_state != 2) return -1;
    __try {
        const uint64_t a = *(volatile uint64_t*)g_ws_g1; if (!a) return -1;
        const uint64_t b = *(volatile uint64_t*)a; if (!b) return -1;
        const uint32_t id = *(volatile uint32_t*)(b + g_ws_idoff);
        const uint64_t r = *(volatile uint64_t*)g_ws_g2; if (!r || !*(volatile uint64_t*)r) return -1;
        const uint32_t s = g_ws_fn(r, id);
        return s <= 8 ? (int)s : -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
// #126843 Samurai (0) / Ninja (1) style: chr +0xA95C (the byte Boss Rematch's FbStyleNow reads). A switch counts once the new value held
// 150 ms of game time (the byte is not the real style for a moment inside soul-core forms). Returns 1 on the tick a switch is confirmed.
static __attribute__((noinline)) int SmStyleSwitched(uint64_t actor, double t) {
    static int cur = -1, cand = -1; static double candT; static uint64_t pa;
    uint8_t v = 0xFF;
    if (actor) __try { v = *(volatile uint8_t*)(actor + 0xA95C); } __except (EXCEPTION_EXECUTE_HANDLER) { v = 0xFF; }
    if (!actor || v > 1 || actor != pa) { pa = actor; cur = v <= 1 ? v : -1; cand = -1; return 0; }
    if (cur < 0) { cur = v; return 0; }
    if (v == cur) { cand = -1; return 0; }
    if (cand != v) { cand = v; candT = t; return 0; }
    if (t - candT < 150.0) return 0;
    cur = v; cand = -1; return 1;
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
    int ifr = 0, ki = 0, kiPerf = 0;
    if (pl) {
        if (g_fm_pl_stamp == g_fp_frame) { ifr = g_fm_pl_ifr; ki = g_fm_pl_ki; kiPerf = g_fm_pl_kiperf; }  // the frame meter's player row already read them this tick
        else {
            static FmiState fs; float a = 0, b = 0, c = 0;
            ifr = (FmPlayerIfrBits(&fs) & FMF_NODMG) != 0;
            g_fm_ki_quiet = 1; ki = FmPlayerKiLanded(g_fp[FP_PL].a, &a, &b, &c); g_fm_ki_quiet = 0;
            // #127075 same perfect check as the player row (frame_meter_game.h): RB press edge inside the perfect window, this or last sample
            static int wasRB, prevPerfWin;
            const int open = FmPlayerKiOpen(g_fp[FP_PL].a), rb = FmPadRB(), r1Edge = rb && !wasRB;
            wasRB = rb;
            kiPerf = FmKiPressPerfect(open, r1Edge, c) || (r1Edge && prevPerfWin);
            prevPerfWin = open && FmKiPerfect(c);
            if (kiPerf) ki = 1;
        }
    }
    const uint32_t act = pl ? g_fp[FP_PL].run.act : SM_ACT_NONE;
    const uint64_t run = pl ? ((uint64_t)g_fp[FP_PL].run.f0 << 32 | act) : 0;
    if (g_cfg_sm_actlog && pl) {  // #126980: one line per action change, capped so a long session cannot flood the log
        static uint32_t lastAct = SM_ACT_NONE; static int n;
        if (act != lastAct && n < 5000) { ++n; Log("STYLE ACT: t %.0f act %u (was %u) enemy %d", g_sm_clock, act, lastAct, g_sm_enemy); }
        lastAct = act;
    }
    // #126821 startup of the current run = its first hit window (frame probe, player row); #126843 counter = i-frames inside an enemy
    // hit window during this same run. Both are per run, read from the frame meter's own signals (no move ids).
    const float startupF = pl && g_fp[FP_PL].run.hits > 0 && g_fp[FP_PL].run.hit[0] >= 0 ? (float)FpFrames60(g_fp[FP_PL].run.hit[0]) : -1.f;
    static uint64_t ctrRun; static int ctr;
    if (run != ctrRun) { ctrRun = run; ctr = 0; }
    if (pl && ifr && g_sm_enemy) ctr = 1;
    // #127065 one per throw (the gauge can drop twice ~10 ms apart) / #127075 only in Ninja mode (chr +0xA95C, same byte as the switch)
    static double ninT = -1e18;
    int sstyle = -1;
    if (pl) { uint8_t v = 0xFF; __try { v = *(volatile uint8_t*)(g_fp[FP_PL].a + 0xA95C); } __except (EXCEPTION_EXECUTE_HANDLER) { v = 0xFF; } sstyle = v <= 1 ? v : -1; }
    const int nin = pl ? SmNinGate(&ninT, g_sm_clock, SmNinjutsuUsed(g_fp[FP_PL].a), sstyle) : 0;
    const int wslot = pl ? SmWeaponSlot() : -1;  // #127057
    int swapEv = 0;
    {   static int prevSlot = -1; static uint64_t pa;
        const uint64_t a = pl ? g_fp[FP_PL].a : 0;
        if (wslot < 0 || a != pa) prevSlot = -1; else if (prevSlot >= 0 && wslot != prevSlot) swapEv = 1;
        if (wslot >= 0) prevSlot = wslot;
        pa = a;
    }
    static uint64_t rsRun; static double rsT;  // game ms the current player move started (swap attack window)
    if (run != rsRun) { rsRun = run; rsT = g_sm_clock; }
    const LONG soul = InterlockedExchange(&g_soul_uses, 0);  // #126913 soul core uses (PstPlayerSoul: inventory drop in the seal throw / form start)
    if (g_cfg_sm_movelog && pl) {  // #127003 one STYLE MOVE line per player move (hit or miss) when it ends; ids are logged, not interpreted
        static uint64_t mrun; static SmMoveRec mr; static double mt0, soulT = -1e18; static int mon, mlines;
        static unsigned (*fu)(void); static int futried;
        if (!fu && !futried) { futried = 1; HMODULE rm = GetModuleHandleA("Nioh3BossRematch.asi"); fu = rm ? (unsigned (*)(void))GetProcAddress(rm, "Nioh3BossRematch_FormUseTick") : 0; }
        if (soul > 0) soulT = g_sm_clock;
        if (mon) { mr.landed += (int)hits; if (nin) mr.nin = 1; }
        if (run != mrun) {
            if (mon && mlines < 20000) { ++mlines; if (g_sm_swapRun == mrun) mr.swapped = 1; mr.ms = g_sm_clock - mt0; char b[400]; SmMoveFmt(b, sizeof b, &mr); Log("STYLE MOVE: t %.0f %s", mt0, b); }
            mrun = run; mt0 = g_sm_clock; mon = act != SM_ACT_NONE && act != 0;
            memset(&mr, 0, sizeof mr); mr.act = act; mr.startupF = -1.f; mr.enemy = g_sm_enemy;
            uint8_t v = 0xFF; __try { v = *(volatile uint8_t*)(g_fp[FP_PL].a + 0xA95C); } __except (EXCEPTION_EXECUTE_HANDLER) { v = 0xFF; }
            mr.style = v <= 1 ? v : -1; mr.slot = wslot;
            SmPadNow(&mr.btn, &mr.lt, &mr.rt);
            const unsigned ft = fu ? fu() : 0;
            mr.soulForm = g_sm_clock - soulT <= SM_MOVE_SOUL_MS || (ft && GetTickCount() - ft <= SM_MOVE_SOUL_MS);
        }
        if (mon) { if (g_fp[FP_PL].run.hits > mr.windows) mr.windows = g_fp[FP_PL].run.hits; if (g_fp[FP_PL].run.great) mr.great = 1;
                   if (startupF >= 0.f && mr.startupF < 0.f) mr.startupF = startupF; }
    }
    const int sty = pl ? SmStyleSwitched(g_fp[FP_PL].a, g_sm_clock) : 0;
    const int fight = g_pst_live != 0;
    AcquireSRWLockExclusive(&g_sm_lock);
    const int r0 = g_sm.rank, a0 = g_sm.active;
    const int dodge = SmDodgeStep(&g_sm.dodge, g_sm_enemy, ifr, taken > 0);
    float ph = 0.f, pk = -1.f, pd = -1.f, pn = -1.f, ps = -1.f;  // human #126744: per-event play log (points each event scored)
    const uint32_t c0 = g_sm.counters;
    if (swapEv) SmOnSwap(&g_sm, g_sm_clock);
    for (LONG i = 0; i < hits && i < 64; ++i) ph += SmOnHitEx(&g_sm, &g_smc, g_sm_clock, act, run, startupF, ctr);
    const float psw = hits > 0 ? SmSwapHit(&g_sm, &g_smc, g_sm_clock, rsT) : 0.f;
    if (psw > 0.f) g_sm_swapRun = run;
    const int ctrHit = g_sm.counters != c0;
    for (int i = 0; i < nin; ++i) pn = (pn < 0 ? 0 : pn) + SmOnSkill(&g_sm, &g_smc, g_sm_clock, 0);
    float pc = -1.f;
    for (LONG i = 0; i < soul && i < 8; ++i) pc = (pc < 0 ? 0 : pc) + SmOnSkill(&g_sm, &g_smc, g_sm_clock, 1);
    if (sty) ps = SmOnStyle(&g_sm, &g_smc, g_sm_clock);
    if (ki) pk = SmOnKiEx(&g_sm, &g_smc, g_sm_clock, kiPerf);
    if (dodge) pd = SmOnDodge(&g_sm, &g_smc, g_sm_clock);
    if (taken > 0) SmOnDamaged(&g_sm, &g_smc, g_sm_clock);
    SmTick(&g_sm, &g_smc, g_sm_clock, dt);
    const int vis = pl && SmVisible(&g_sm, &g_smc, g_sm_clock, fight);
    const int r1 = g_sm.rank, a1 = g_sm.active; const float gg = SmGauge(&g_sm, &g_smc);
    const uint32_t nh = g_sm.hits, nd = g_sm.dodges, nk = g_sm.kis, nt = g_sm.damaged;
    const float gp = g_sm.pts;
    ReleaseSRWLockExclusive(&g_sm_lock);
    {   // #127085 one pop-up per scored bonus (plain hits are not bonuses); a big bonus tick shakes the meter
        float big = 0.f;
        if (ctrHit && g_smc.counter > 0.f) { SmFxPop2("Counter", ph - g_smc.counter > 0.f ? ph - g_smc.counter : 0.f, g_smc.counter); big += g_smc.counter; }  // #127171 hit value, then the counter bonus
        if (pk > 0.f) { const int pf = kiPerf && g_smc.kiPerfect > 0.f && pk > g_smc.kiPerfect; SmFxPop2(pf ? "Perfect Ki" : "Ki Pulse", pf ? pk - g_smc.kiPerfect : pk, pf ? g_smc.kiPerfect : 0.f); big += pk; }
        if (pd > 0.f) { SmFxPop("Clean Dodge", pd); big += pd; }
        if (pn > 0.f) { SmFxPop("Ninjutsu", pn); big += pn; }
        if (pc > 0.f) { SmFxPop("Soul Core", pc); big += pc; }
        if (ps > 0.f) { SmFxPop("Style Switch", ps); big += ps; }
        if (psw > 0.f) { SmFxPop2("Weapon Swap", ph, psw); big += psw; }  // the swap attack's hit value, then the swap bonus
        if (big >= (float)g_cfg_sm_shakepts) { AcquireSRWLockExclusive(&g_smfx_lock); g_smfx_shakeT = SmWallMs(); ReleaseSRWLockExclusive(&g_smfx_lock); }
    }
    if (hits > 0 || ki || dodge || taken > 0 || nin || sty || soul > 0 || swapEv) {
        static int evl;
        if (evl++ < 4000) {
            char e[224]; int n = 0; e[0] = 0;
            if (hits > 0) n += snprintf(e + n, sizeof e - n, " hit x%ld +%.1f (startup %.0f f%s)", (long)hits, ph, startupF, ctrHit ? ", counter" : "");
            if (nin) n += snprintf(e + n, sizeof e - n, " ninjutsu +%.0f", pn);
            if (swapEv) n += snprintf(e + n, sizeof e - n, " weapon slot -> %d", wslot);
            if (psw > 0.f) n += snprintf(e + n, sizeof e - n, " swap attack +%.0f", psw);
            if (soul > 0) n += snprintf(e + n, sizeof e - n, " soul core x%ld +%.0f", (long)soul, pc);
            if (sty) n += snprintf(e + n, sizeof e - n, " style switch +%.0f", ps);
            if (ki && pk > 0) n += snprintf(e + n, sizeof e - n, " ki%s +%.0f", kiPerf ? " perfect" : "", pk);
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
#include "style_meter_draw.h"
static BOOL CALLBACK SmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_sm_wnd || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_sm_game = h; return FALSE;
}
static void SmRender(HWND wnd, const SmCanvas* c, const SmDrawSt* st) {
    const int w = c->w, h = c->h;
    HDC scr = GetDC(0), mem = CreateCompatibleDC(scr);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = 0;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, 0, 0);
    if (bmp && px) {
        HGDIOBJ ob = SelectObject(mem, bmp);
        SmCompose(mem, px, c, st);
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
    int shown = 0, lastRank = -1, lastGauge = -1, seenRank = -1; RECT last = {0};
    float eased = 0.f; SmShownBar bar = {-1, 0.f}; int upFrom = -1; double lastT = SmWallMs(), rankUpT = -1e18;
    for (;;) {
        Sleep(33);
        const double now = SmWallMs(), dtw = now - lastT; lastT = now;
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_sm_game || !IsWindow(g_sm_game)) { g_sm_game = 0; EnumWindows(SmFind, 0); }
        const int gameOk = g_sm_game && !IsIconic(g_sm_game);
        const int fg = gameOk && (GetForegroundWindow() == g_sm_game || g_cfg_fm_unfocused);
        const uint8_t* lgs = GameState(); const int loading = lgs && lgs[2];
        const int want = g_cfg_sm && g_sm_vis && gameOk && fg && !loading && !GateMenuOpen();
        if (!want) { if (shown) { ShowWindow(g_sm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_sm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_sm_game, &o);
        if (cr.right <= 0 || cr.bottom <= 0) continue;
        const SmCanvas cv = SmCanvasGeom(cr.bottom, g_cfg_sm_pops); const SmGeo g = cv.g;
        // default anchor from rank_atlas.json: right edge at 1690/1920 of the width, letter centre at 380/1080 of the height
        const int rx = g_cfg_sm_x >= 0 ? cr.right * g_cfg_sm_x / 1000 : cr.right * 1690 / 1920;
        const int cy = g_cfg_sm_y >= 0 ? cr.bottom * g_cfg_sm_y / 1000 : cr.bottom * 380 / 1080;
        const int left = o.x + rx - g.w, top = o.y + cy - (int)(SM_LETTER_H_1080P * SM_LK * (float)cr.bottom / 1080.f * (float)g_cfg_sm_scale / 100.f) / 2 - g.pad;
        RECT want_r = {left - cv.mx, top - cv.my, left - cv.mx + cv.w, top - cv.my + cv.h};  // same meter position, margin around it
        const int rank = (int)g_sm_rank, gauge = (int)g_sm_gauge;
        // #127085 motion: rank-up bounce (seen here), shake (big bonus, game thread), glow at S+, eased fill
        // #127253 the bar fills to the end before the shown rank steps up (motion on); without motion the real rank shows at once
        int showRank = rank;
        if (g_cfg_sm_motion) { const int was = bar.rank; if (SmBarStep(&bar, rank, (float)gauge, dtw)) { rankUpT = now; upFrom = was; } showRank = bar.rank; eased = bar.g; }
        else { if (seenRank >= 0 && rank > seenRank) rankUpT = now; bar.rank = rank; bar.g = (float)gauge; }
        seenRank = rank;
        SmDrawSt st; memset(&st, 0, sizeof st); st.upMs = -1.0; st.prevRank = -1;  // #127299 no transition unless motion sets it
        st.rank = showRank; st.now = now; st.lifeMs = g_cfg_sm_popms; st.scale = 1.f;
        double shakeT;
        AcquireSRWLockShared(&g_smfx_lock); shakeT = g_smfx_shakeT; ReleaseSRWLockShared(&g_smfx_lock);
        if (g_cfg_sm_pops) {
            AcquireSRWLockExclusive(&g_smfx_lock); SmPopPrune(&g_smfx_q, now, g_cfg_sm_popms);
            st.npops = g_smfx_q.n; memcpy(st.pops, g_smfx_q.p, sizeof st.pops); ReleaseSRWLockExclusive(&g_smfx_lock);
        }
        if (g_cfg_sm_motion) {
            st.scale = 1.f; st.upMs = now - rankUpT; st.prevRank = upFrom; SmShake(now - shakeT, &st.dx, &st.dy); st.glow = 0.f;  // #127149 glow removed
        } else eased = (float)gauge;
        st.gauge = eased;
        const int anim = st.npops > 0 || st.scale != 1.f || st.dx != 0.f || st.dy != 0.f || st.glow > 0.f || eased != (float)gauge || showRank != rank || (st.upMs >= 0.0 && st.upMs < SM_UP_MS);
        const int gpx = (int)(sm_atlas_gauge_fill[2] * g.k) * gauge / 1000;  // redraw only when the gauge moves a pixel (or something animates)
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_sm_wnd, HWND_TOPMOST, want_r.left, want_r.top, cv.w, cv.h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastRank = -1;
        }
        static int wasAnim;
        if (showRank != lastRank || gpx != lastGauge || anim || wasAnim) {
            lastRank = showRank; lastGauge = gpx; wasAnim = anim;
            __try { SmRender(g_sm_wnd, &cv, &st); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
}
