// human #126496 / #126502: style meter, game side. Included by training.c after frame_meter_game.h. Scoring: style_meter.h (tests/style_meter_test.c).
// Runs from the FRAMEPROBE pump (FpTick, game thread, FpLock held): SmCapture before FmFeed, SmFeed after it. Not tied to Training / Practice:
// [StyleMeter] StyleMeter 1 installs the pump on its own (FpTick gate) and the player entry (FpPlayerTick gate) in any mission.
// Inputs (all read only; no game memory is written):
//   hits landed / taken  NfHpDamage (HP damage 787268 in the hit apply 329890) -> SmHpEvent: attacker of the hit (325F40) == the player = a hit landed
//                        on any actor (normal enemies too); victim = the player's HP = damage taken (also counted when Infinite health cuts it to 0).
//                        The hook is the existing one (HpDmgHookInstall, shared with Infinite health / damage multipliers / KiBreakNoFinisher); with
//                        StyleMeter 1 it is installed if nothing else did. Fallback when it cannot be installed: HP drops of the boss slots and of the player.
//   blocked hits         (BlockPct) the same hit apply: the player's hit on a live enemy (not the player, not same team) that did Ki damage (KiDamage
//                        844604 at 329E9D, PstKiDamage -> SmKiEvent; the existing PstKiHookInstall, installed for this if nothing else did) or
//                        came to the HP damage call with 0, and did NOT take HP = the enemy guarded it (SmBlkQ, style_meter.h). Needs the HP hook.
//   enemy hit window     g_fp[slot].pulse / open g_fpw windows of the boss slots (BvActor: Training bosses AND native bosses with a HUD HP bar,
//                        NativeSlotsTick) = the frame meter's 'active' signal.
//   player i-frames      FmPlayerIfrBits (frame meter I-FR signal); Ki pulse landed FmPlayerKiLanded (frame meter Ki check); action id g_fp[FP_PL].run.act.
// Drawing: own click-through topmost layered window (same approach as the frame meter: GDI into a 32-bit DIB, UpdateLayeredWindow), right side.
// Sprites: style/rank_atlas.png (approved set #126376) embedded with .incbin, decoded once with WIC (ole32 loaded at run time; no new link deps).
#include "style_meter.h"
#include "style_meter_fx.h"  // #127085 pop-ups + motion
#include "style_result.h"    // boss fight result card (average rank first boss hit -> boss death)
#include "style_atlas_gen.h"
#define COBJMACROS
#include <wincodec.h>
__asm__(".section .rdata,\"dr\"\n\t.p2align 4\n\t.globl g_sm_png\ng_sm_png:\n\t.incbin \"style/rank_atlas.png\"\n\t.globl g_sm_png_end\ng_sm_png_end:\n\t.byte 0\n\t.text\n");
extern const unsigned char g_sm_png[], g_sm_png_end[];
static SmCfg g_smc; static SmState g_sm; static SRWLOCK g_sm_lock = SRWLOCK_INIT;
#define SM_DEF_X 200  // default meter place (StyleMeterX / Y = -1), 1/1000 of the game window: top left
#define SM_DEF_Y 270
static int g_cfg_sm_x = -1, g_cfg_sm_y = -1, g_cfg_sm_scale = 100, g_cfg_sm_alpha = 100;  // human #128855 StyleMeterOpacity %
static int g_cfg_sm_actlog, g_cfg_sm_movelog;
static int g_cfg_sm_ingame = 1;  // DrawInGame: 1 = meter + result card drawn into the game's frames by Nioh3EngineFix (OBS Game Capture records them)
static int g_cfg_sm_always;  // human #129172 AlwaysOn 1 = meter stays on screen (rank D, empty bar) between hits
#define SM_BAR_HK_DEF 1.7f  // = style_meter_draw.h default (#127273)
static float g_sm_bar_hk = SM_BAR_HK_DEF;  // human #129225 [StyleMeter] BarHeightPct (100 = 1.7x sprite gauge height), live
#define SM_BAR_HK g_sm_bar_hk
static int g_sm_letter_pct = 60;  // human #129306 [StyleMeter] LetterPct rank letter size (60 = old fixed SM_LETTER_PCT), live
#define SM_LETTER_PCT g_sm_letter_pct
#define SM_BAR_STYLE g_sm_bar_style
static int g_sm_letter_style = 0;  // human #129310 [StyleMeter] LetterStyle 1 = plain game-HUD serif text letter, 0 = sprite letter, live
#define SM_LETTER_STYLE g_sm_letter_style
static unsigned g_sm_bar_rgb = 0xA89A6Cu;  // human #129320 [StyleMeter] BarColor RRGGBB hex for the BarStyle 1 fill (default game gold), live
#define SM_GAME_BAR_RGB g_sm_bar_rgb
static int g_sm_bar_style = 0;  // human #129306 [StyleMeter] BarStyle 0 = shaded rank-colour bar, 1 = flat game-HUD bar (gold, dark outline, no sheen), live
static uint64_t g_sm_maRun;  // #127295 the player move (run key) that scored the martial art bonus (move log tag)
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
static void SmFxPopLoss(const char* label, float pts) {  // a loss (loop penalty): pts > 0 = the amount lost, queued negative, drawn "-N"
    if (!g_cfg_sm_pops || pts <= 0.f) return;
    AcquireSRWLockExclusive(&g_smfx_lock); SmPopPush2(&g_smfx_q, label, -pts, 0.f, SmWallMs(), g_cfg_sm_popms); ReleaseSRWLockExclusive(&g_smfx_lock);
}
static HWND g_sm_game;
// Boss fight result ([StyleMeter] ResultCard, default on with the meter). g_sr: game thread only (SmFeed). The card: g_smfx_lock.
static uint32_t g_sm_defl[SM_DEFLECT_SKILLS] = {SM_DEFLECT_SKILL}; static int g_sm_ndefl = 1;  // DeflectSkill list (low u16 of the skill word)
static int g_cfg_sr_scale = SR_SCALE_PCT, g_cfg_sr_hitpen = SR_HIT_PENALTY; static int g_cfg_sr = 1, g_cfg_sr_hold = SR_HOLD_PCT, g_cfg_sr_delay = SR_END_DELAY_MS, g_cfg_sr_show = SR_SHOW_MS;
static SrFight g_sr;
static volatile LONG g_sm_combo, g_sm_comboMile, g_sm_comboMilePts, g_sm_comboMileGen;  // + the last combo milestone (hits, bonus) and its generation  // human: current combo (landed hits), published for the draw thread
static volatile LONG g_sm_allyHits, g_sr_allyMask;  // SmHpEvent: hits on an enemy by a same-team actor (summoned guardian spirit), boss slots hit so
static volatile LONG g_sr_hitMask;  // SmHpEvent: bit s = the player's hit took HP off boss slot s since the last tick
static SrResult g_sr_card; static double g_sr_cardT = -1e18; static LONG g_sr_cardGen;
static HWND g_sr_wnd;
static void SmReadIni(const char* ini, int reset) {
    g_cfg_sm = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeter", 0, ini) ? 1 : 0;
    g_cfg_sm_x = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterX", -1, ini); if (g_cfg_sm_x < -1 || g_cfg_sm_x > 1100) g_cfg_sm_x = -1;  // human #129276: >1000 lets the visible bar reach the mini map edge (box padding hangs off-screen)
    g_cfg_sm_y = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterY", -1, ini); if (g_cfg_sm_y < -1 || g_cfg_sm_y > 1000) g_cfg_sm_y = -1;
    g_cfg_sm_actlog = (int)GetPrivateProfileIntA("StyleMeter", "ActLog", 0, ini);
    g_cfg_sm_always = (int)GetPrivateProfileIntA("StyleMeter", "AlwaysOn", 0, ini) ? 1 : 0;  // human #129172
    { int bp = (int)GetPrivateProfileIntA("StyleMeter", "BarHeightPct", 100, ini); if (bp < 50 || bp > 400) bp = 100; g_sm_bar_hk = SM_BAR_HK_DEF * (float)bp / 100.f; }  // human #129225 thicker bar
    { int lp = (int)GetPrivateProfileIntA("StyleMeter", "LetterPct", 60, ini); if (lp < 15 || lp > 150) lp = 60; g_sm_letter_pct = lp; }  // human #129306 smaller rank letter
    g_sm_bar_style = (int)GetPrivateProfileIntA("StyleMeter", "BarStyle", 0, ini) ? 1 : 0;
    g_sm_letter_style = (int)GetPrivateProfileIntA("StyleMeter", "LetterStyle", 0, ini) ? 1 : 0;  // human #129310
    { char cb[16] = {0}; GetPrivateProfileStringA("StyleMeter", "BarColor", "", cb, sizeof cb, ini); char* e = 0; const char* q = cb[0] == '#' ? cb + 1 : cb;
      const unsigned long v = strtoul(q, &e, 16); g_sm_bar_rgb = (q[0] && e && *e == 0 && v <= 0xFFFFFFul) ? (unsigned)v : 0xA89A6Cu; }  // human #129320 purple fill  // human #129306 game-HUD look
    g_cfg_sm_movelog = (int)GetPrivateProfileIntA("StyleMeter", "MoveLog", 1, ini);
    g_cfg_sm_pops = (int)GetPrivateProfileIntA("StyleMeter", "PopUps", 1, ini) ? 1 : 0;  // #127085
    g_cfg_sm_motion = (int)GetPrivateProfileIntA("StyleMeter", "Motion", 1, ini) ? 1 : 0;
    g_cfg_sm_popms = (int)GetPrivateProfileIntA("StyleMeter", "PopUpMs", SM_POP_LIFE_MS, ini); if (g_cfg_sm_popms < 300 || g_cfg_sm_popms > 5000) g_cfg_sm_popms = SM_POP_LIFE_MS;
    g_cfg_sm_shakepts = (int)GetPrivateProfileIntA("StyleMeter", "ShakePts", SM_SHAKE_PTS, ini); if (g_cfg_sm_shakepts < 1) g_cfg_sm_shakepts = SM_SHAKE_PTS;  // #127003: on with the style meter (SmFeed only runs when StyleMeter=1)
    g_cfg_sm_scale = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterScale", 100, ini); if (g_cfg_sm_scale < 25 || g_cfg_sm_scale > 400) g_cfg_sm_scale = 100;
    g_cfg_sm_ingame = (int)GetPrivateProfileIntA("StyleMeter", "DrawInGame", 1, ini) != 0;
    g_cfg_sm_alpha = (int)GetPrivateProfileIntA("StyleMeter", "StyleMeterOpacity", 100, ini); if (g_cfg_sm_alpha < 20 || g_cfg_sm_alpha > 100) g_cfg_sm_alpha = 100;
    g_cfg_sr = (int)GetPrivateProfileIntA("StyleMeter", "ResultCard", 1, ini) ? 1 : 0;
    g_cfg_sr_hitpen = (int)GetPrivateProfileIntA("StyleMeter", "ResultHitPenalty", SR_HIT_PENALTY, ini); if (g_cfg_sr_hitpen < 0 || g_cfg_sr_hitpen > 700) g_cfg_sr_hitpen = SR_HIT_PENALTY;
    g_cfg_sr_scale = (int)GetPrivateProfileIntA("StyleMeter", "ResultScalePct", SR_SCALE_PCT, ini); if (g_cfg_sr_scale < 50 || g_cfg_sr_scale > 300) g_cfg_sr_scale = SR_SCALE_PCT;
    g_cfg_sr_hold = (int)GetPrivateProfileIntA("StyleMeter", "ResultHoldPct", SR_HOLD_PCT, ini); if (g_cfg_sr_hold < 0 || g_cfg_sr_hold > 100) g_cfg_sr_hold = SR_HOLD_PCT;
    g_cfg_sr_delay = (int)GetPrivateProfileIntA("StyleMeter", "ResultEndDelayMs", SR_END_DELAY_MS, ini); if (g_cfg_sr_delay < 0 || g_cfg_sr_delay > 20000) g_cfg_sr_delay = SR_END_DELAY_MS;
    g_cfg_sr_show = (int)GetPrivateProfileIntA("StyleMeter", "ResultShowMs", SR_SHOW_MS, ini); if (g_cfg_sr_show != 0 && (g_cfg_sr_show < 2000 || g_cfg_sr_show > 600000)) g_cfg_sr_show = SR_SHOW_MS;  // 0 = until confirm
    SmCfg c; SmCfgDefaults(&c);
    c.hit = (float)(int)GetPrivateProfileIntA("StyleMeter", "HitPts", SM_HIT_PTS, ini);
    c.dodge = (float)(int)GetPrivateProfileIntA("StyleMeter", "DodgePts", SM_DODGE_PTS, ini);
    c.deflect = (float)(int)GetPrivateProfileIntA("StyleMeter", "DeflectPts", SM_DEFLECT_PTS, ini);
    {   // DeflectSkill = "0x67D2" or a comma list of skill words (hex) that are the universal deflect; empty / bad = the default
        char buf[128] = {0}; GetPrivateProfileStringA("StyleMeter", "DeflectSkill", "", buf, sizeof buf, ini);
        uint32_t v[SM_DEFLECT_SKILLS]; int n = 0; const char* p = buf;
        while (*p && n < SM_DEFLECT_SKILLS) { while (*p == ' ' || *p == ',') ++p; if (!*p) break; char* e; const unsigned long x = strtoul(p, &e, 16); if (e == p || x == 0 || x > 0xFFFF) { n = -1; break; } v[n++] = (uint32_t)x; p = e; }
        if (n > 0) { for (int i = 0; i < n; ++i) g_sm_defl[i] = v[i]; g_sm_ndefl = n; }
        else { g_sm_defl[0] = SM_DEFLECT_SKILL; g_sm_ndefl = 1; if (buf[0]) Log("STYLE METER: DeflectSkill '%s' ignored (hex skill words, e.g. 0x67D2)", buf); }
    }
    c.ki = (float)(int)GetPrivateProfileIntA("StyleMeter", "KiPts", SM_KI_PTS, ini);
    c.kiPerfect = (float)(int)GetPrivateProfileIntA("StyleMeter", "KiPerfectPts", SM_KI_PERFECT_PTS, ini);  // #127075
    c.repeatPct = (int)GetPrivateProfileIntA("StyleMeter", "RepeatPct", SM_REPEAT_PCT, ini);
    c.repeatFloorPct = (int)GetPrivateProfileIntA("StyleMeter", "RepeatFloorPct", SM_REPEAT_FLOOR_PCT, ini);
    c.multiPct = (int)GetPrivateProfileIntA("StyleMeter", "MultiHitPct", SM_MULTI_PCT, ini);
    c.multiHitCap = (int)GetPrivateProfileIntA("StyleMeter", "MultiHitCap", SM_MULTI_CAP, ini);
    c.loopReps = (int)GetPrivateProfileIntA("StyleMeter", "LoopReps", SM_LOOP_REPS, ini);
    c.loopPts = (float)(int)GetPrivateProfileIntA("StyleMeter", "LoopPenaltyPts", SM_LOOP_PTS, ini);
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
    c.switchMs = (int)GetPrivateProfileIntA("StyleMeter", "WeaponSwitchMs", SM_SWITCH_MS, ini);
    c.gs = (float)(int)GetPrivateProfileIntA("StyleMeter", "GuardianSpiritPts", SM_GS_PTS, ini);         // #127213
    c.gsMs = (int)GetPrivateProfileIntA("StyleMeter", "GuardianSpiritMs", SM_GS_MS, ini);
    c.gsSummonMs = (int)GetPrivateProfileIntA("StyleMeter", "GuardianSummonMs", SM_GS_SUMMON_MS, ini);
    c.comboMs = (int)GetPrivateProfileIntA("StyleMeter", "ComboMs", SM_COMBO_MS, ini); c.comboStep = (int)GetPrivateProfileIntA("StyleMeter", "ComboStep", SM_COMBO_STEP, ini);
    c.comboBase = (int)GetPrivateProfileIntA("StyleMeter", "ComboBonus", SM_COMBO_BASE, ini); c.comboInc = (int)GetPrivateProfileIntA("StyleMeter", "ComboBonusStep", SM_COMBO_INC, ini);
    c.comboMax = (int)GetPrivateProfileIntA("StyleMeter", "ComboBonusMax", SM_COMBO_MAX, ini);
    c.ma = SmMaPtsResolve((int)GetPrivateProfileIntA("StyleMeter", "MartialArtPts", -1, ini), c.nin);       // #127295 unset = NinjutsuPts
    c.blockPct = (int)GetPrivateProfileIntA("StyleMeter", "BlockPct", SM_BLOCK_PCT, ini);                // human: a guarded hit = this % of a hit
    c.soul = (float)(int)GetPrivateProfileIntA("StyleMeter", "SoulCorePts", SM_SOUL_PTS, ini);            // #126850 / #126913 (PstPlayerSoul use count)
    c.burst = (float)(int)GetPrivateProfileIntA("StyleMeter", "BurstBreakPts", SM_BURST_PTS, ini);  // human: Burst Break
    c.burstMs = (int)GetPrivateProfileIntA("StyleMeter", "BurstBreakMs", SM_BURST_MS, ini);
    c.burstSkill = (int)GetPrivateProfileIntA("StyleMeter", "BurstBreakSkillOnly", 0, ini);
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
    Log("STYLE METER: bonuses startup x1..x%.2f over %d..%d f, counter %.0f, style switch %.0f, ninjutsu %.0f, soul core %.0f, weapon swap attack %.0f within %d ms, guardian spirit %.0f (gauge drop within %d ms), martial art %.0f (action row skill word), burst break %.0f within %d ms%s, blocked hit %d %% of a hit (#126821 #126843 #126854 #126913 #127057 #127213 #127295)",
        1.f + g_smc.startupPct / 100.f, g_smc.startupMinF, g_smc.startupMaxF, g_smc.counter, g_smc.style, g_smc.nin, g_smc.soul, g_smc.swap, g_smc.swapMs, g_smc.gs, g_smc.gsMs, g_smc.ma,
        g_smc.burst, g_smc.burstMs, g_smc.burstSkill ? " (skill / guardian spirit hits only)" : "", g_smc.blockPct);
    Log("STYLE METER: [StyleMeter] StyleMeter %d X %d Y %d Scale %d%% | hit %.0f dodge %.0f ki %.0f repeat %d%% (floor %d%%) multi-hit %d%% decay %.0f/s +%d%%/rank +%d%%/idle s after %d ms, combat %d ms, damage grace %d ms, ki needs a hit within %d ms, ranks %.0f %.0f %.0f %.0f %.0f %.0f %.0f",
        g_cfg_sm, g_cfg_sm_x, g_cfg_sm_y, g_cfg_sm_scale, g_smc.hit, g_smc.dodge, g_smc.ki, g_smc.repeatPct, g_smc.repeatFloorPct, g_smc.multiPct,
        g_smc.decayPerS, g_smc.decayRankPct, g_smc.decayRampPct, g_smc.decayDelayMs, g_smc.combatMs, g_smc.damageGraceMs, g_smc.kiHitMs, g_smc.rankPts[0], g_smc.rankPts[1], g_smc.rankPts[2], g_smc.rankPts[3], g_smc.rankPts[4], g_smc.rankPts[5], g_smc.rankPts[6]);
    Log("STYLE METER: multi-hit cap %d hits per use%s, loop penalty -%.0f per use after %d cycles of a 1- or 2-move loop%s",
        g_smc.multiHitCap, g_smc.multiHitCap ? "" : " (off)", g_smc.loopPts, g_smc.loopReps, g_smc.loopReps ? "" : " (off)");
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
typedef struct { const char* key; int def, lo, hi, negDef; } SmMenuKey;  // negDef: an ini value < 0 means 'default place' -> report def
static const SmMenuKey sm_menu_keys[] = {
    {"StyleMeter", 0, 0, 1}, {"RankPtsPct", 100, 25, 400}, {"DrainPct", 100, 0, 500}, {"DecayDelayMs", SM_DECAY_DELAY_MS, 0, 15000},
    {"KiPerfectPts", SM_KI_PERFECT_PTS, 0, 200},
    {"PopUps", 1, 0, 1}, {"Motion", 1, 0, 1}, {"StyleMeterScale", 100, 25, 400}, {"PopUpMs", SM_POP_LIFE_MS, 300, 5000},
    {"HitPts", SM_HIT_PTS, 0, 200}, {"DodgePts", SM_DODGE_PTS, 0, 200}, {"KiPts", SM_KI_PTS, 0, 200}, {"CounterPts", SM_COUNTER_PTS, 0, 200},
    {"NinjutsuPts", SM_NIN_PTS, 0, 200}, {"SoulCorePts", SM_SOUL_PTS, 0, 200}, {"StyleSwitchPts", SM_STYLE_PTS, 0, 200},
    {"WeaponSwapPts", SM_SWAP_PTS, 0, 200}, {"StartupBonusPct", SM_STARTUP_PCT, 0, 500}, {"RepeatPct", SM_REPEAT_PCT, 0, 100},
    {"DecayRampPct", SM_DECAY_RAMP_PCT, 0, 200},
    {"StyleMeterX", SM_DEF_X, 0, 1100, 1}, {"StyleMeterY", SM_DEF_Y, 0, 1000, 1}, {"StyleMeterOpacity", 100, 20, 100},  // human #128855 (SM_DEF_X / Y = the -1 default place)
    {"AlwaysOn", 0, 0, 1},  // human #129172 index 23
    {"ResultCard", 1, 0, 1},  // index 24: boss fight result card (style_result.h)
};
#define SM_MENU_KEYS ((int)(sizeof sm_menu_keys / sizeof sm_menu_keys[0]))
static const char* SmKey(int what) { return what >= 0 && what < SM_MENU_KEYS ? sm_menu_keys[what].key : 0; }
static int SmKeyDefault(int what) { return sm_menu_keys[what].def; }
static int SmKeyClamp(int what, int v) { const SmMenuKey* k = &sm_menu_keys[what]; return v < k->lo ? k->lo : v > k->hi ? k->hi : v; }
N3T_API int Nioh3Training_StyleKeys(void) { return SM_MENU_KEYS; }  // #127379 how many 'what' rows this build knows
N3T_API int Nioh3Training_StyleGet(int what) {
    const char* k = SmKey(what);
    if (!k || !g_sm_ini[0]) return -1;
    const int raw = (int)GetPrivateProfileIntA("StyleMeter", k, SmKeyDefault(what), g_sm_ini);
    if (raw < 0 && sm_menu_keys[what].negDef) return SmKeyDefault(what);
    return SmKeyClamp(what, raw);
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
// Result card, written by SmHpEvent (hit apply) for SmFeed: the actor of each boss slot hit since the last tick (the slot can be cleared
// before SmFeed runs when the hit killed it) and the slots whose hit was lethal (raw damage >= HP: a boss torn down before HP 0 is read).
static volatile LONG64 g_sr_hitAct[32]; static volatile LONG g_sr_lethalMask;
// Blocked hits (BlockPct): the player's hits on an enemy, noted per (hit object, victim status block) from both damage calls of the hit apply.
static SmBlkQ g_sm_blk; static SRWLOCK g_sm_blk_lock = SRWLOCK_INIT;
static void SmBlkAdd(uint64_t hit, uint64_t vst, int fl, float ki) { AcquireSRWLockExclusive(&g_sm_blk_lock); SmBlkNote(&g_sm_blk, hit, vst, fl, ki); ReleaseSRWLockExclusive(&g_sm_blk_lock); }
// The victim of the player's hit is a live enemy: [hit+0x1C8] (the hit apply's victim) is not the player, owns status block vst, is not on the
// player's team (ally / summon), status+0x48 != 1 (dead) and HP (status+0x58) > 0. Read only.
static __attribute__((noinline)) int SmBlkVictim(uint64_t hit, uint64_t me, uint64_t vst) {
    uint64_t v = 0, st = 0;
    if (!Rd64(hit + 0x1C8, &v) || !v || v == me || !Rd64(v + 0x3A0, &st) || !st || st != vst) return 0;
    const int tm = ActorTeam(me); if (tm >= 0 && ActorTeam(v) == tm) return 0;
    __try { if (*(volatile int32_t*)(vst + 0x48) == 1 || *(volatile int64_t*)(vst + 0x58) <= 0) return 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
// PstKiDamage (Ki damage 844604 at 329E9D, same hit apply, game thread): K = victim status + 0x98, amt = the hit's Ki damage before any mod scaling.
static void SmKiEvent(uint64_t K, float amt, uint64_t hit) {
    if (!g_cfg_sm || g_smc.blockPct <= 0 || g_hpdmg_hooked != 1 || !(amt > 0.f) || !K || !hit || !g_pst_atk_ok) return;  // no HP hook = no way to tell a guarded hit
    __try {
        const uint64_t me = PlayerChar(); uint64_t st = 0;
        if (!me || (Rd64(me + 0x3A0, &st) && st && st + 0x98 == K)) return;  // Ki damage on the player
        if (((uint64_t (*)(uint64_t))(B + Grva(0x325F40)))(hit) != me) return;
        if (SmBlkVictim(hit, me, K - 0x98)) SmBlkAdd(hit, K - 0x98, SM_BLK_KI, amt);
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
}
// NfHpDamage (hit apply, game thread): raw = the hit's HP damage before any mod scaling / Infinite-health cut.
static void SmHpEvent(uint64_t hp, int raw, uint64_t hit) {
    if (!g_cfg_sm || !hp || !hit || !g_pst_atk_ok || (raw <= 0 && g_smc.blockPct <= 0)) return;
    __try {
        const uint64_t me = PlayerChar(); uint64_t st = 0;
        if (!me) return;
        if (Rd64(me + 0x3A0, &st) && st && st + 0x38 == hp) {
            if (raw <= 0) return;
            const uint64_t atk = ((uint64_t (*)(uint64_t))(B + Grva(0x325F40)))(hit);
            if (atk != me) InterlockedIncrement(&g_sm_taken);
            return;
        }
        const uint64_t atk = ((uint64_t (*)(uint64_t))(B + Grva(0x325F40)))(hit);
        if (atk == me && g_smc.blockPct > 0) {  // blocked hits: a hit that took HP is never one; a 0 HP call on a live enemy may be
            if (raw > 0) SmBlkAdd(hit, hp - 0x38, SM_BLK_HP, 0.f);
            else if (SmBlkVictim(hit, me, hp - 0x38)) SmBlkAdd(hit, hp - 0x38, SM_BLK_HP0, 0.f);
        }
        if (raw <= 0) return;
        int ally = 0;
        if (atk && atk != me) { const int tm = ActorTeam(me); ally = tm >= 0 && ActorTeam(atk) == tm; }  // human: summoned guardian spirit (filtered by SmFeed's summon window)
        if (atk == me || ally) {
            InterlockedIncrement(atk == me ? &g_sm_hits : &g_sm_allyHits);
            int boss = 0;
            for (int s = 0; s < N3T_SLOTS && s < 32; ++s) {  // the victim is a boss slot: the fight starts / adds that boss; Burst Break (always on)
                const uint64_t a = g_slot_act[s]; uint64_t bs = 0;
                if (a && Rd64(a + 0x3A0, &bs) && bs && bs + 0x38 == hp) {
                    InterlockedExchange64(&g_sr_hitAct[s], (LONG64)a);
                    if (*(volatile uint64_t*)(hp + 0x20) <= (uint64_t)raw) InterlockedOr(&g_sr_lethalMask, (LONG)(1u << s));  // st+0x58 = HP now
                    InterlockedOr(atk == me ? &g_sr_hitMask : &g_sr_allyMask, (LONG)(1u << s)); boss = 1; break;
                }
            }
            const uint64_t vmx = *(volatile uint64_t*)(hp + 0x18), pmx = st ? *(volatile uint64_t*)(st + 0x50) : 0;
            if (g_cfg_sr && !boss && atk == me && pmx && vmx >= 3 * pmx) {  // diagnosis: a big enemy (max HP >= 3x the player's) that is not a boss slot
                static uint64_t seen[48]; static int nSeen;                  // (a Picture Scroll boss with no HUD bar would show up here)
                int k = 0; while (k < nSeen && seen[k] != hp) ++k;
                if (k == nSeen && nSeen < 48) { seen[nSeen++] = hp;
                    Log("STYLE RESULT: hit a big enemy that is not a boss slot (HP %llu / %llu, player max %llu, hp block %p): no card for it (not admitted, see NATIVE SLOT lines)",
                        *(volatile uint64_t*)(hp + 0x20), vmx, pmx, (void*)hp); }
            }
        }
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
// #127213 Spirit Force gauge (guardian spirit skills spend it; notes/style-gs-ma-127077.md): st = [actor+0x3A0], block G = st+0x1D8, entry
// picked like the game's 0x377D30: [[G+8]+0x188] & 2 and idx = (int)[G+0x218] != 4 -> G+0x38 + idx*0x70, else G+0x1D0 (= st+0x3A8).
// Entry = {float cur, float max}. Returns cur (-1 = unreadable / out of range / the status block changed this tick), *mx = max. Read only.
static __attribute__((noinline)) float SmSpiritForce(uint64_t actor, float* mx) {
    static uint64_t pst; uint64_t st = 0; float c = -1.f, m = 0.f; *mx = 0.f;
    if (!actor || !Rd64(actor + 0x3A0, &st) || !st) { pst = 0; return -1.f; }
    __try {
        const uint64_t g = st + 0x1D8, h = *(volatile uint64_t*)(g + 8);
        uint64_t e = g + 0x1D0;
        if (h && (*(volatile uint8_t*)(h + 0x188) & 2)) { const int32_t i = *(volatile int32_t*)(g + 0x218); if (i != 4) { if (i < 0 || i > 3) return -1.f; e = g + 0x38 + (uint64_t)i * 0x70; } }
        c = *(volatile float*)e; m = *(volatile float*)(e + 4);
    } __except (EXCEPTION_EXECUTE_HANDLER) { pst = 0; return -1.f; }
    const int same = st == pst; pst = st;
    if (!same || !(m > 0.f && m < 1e6f && c >= 0.f && c <= m * 1.01f)) return -1.f;
    *mx = m; return c;
}
// #127295 martial art marker: the player's current action object P = [ctrl+0xE8] (id = dword [P], the act the frame probe logs), its file row
// R = [P+0x20], skill word = dword [R+0xDC] (what the game's AttackSkill test cond 0x2F reads; SmSkillRow). Only when [P] == act, so a row
// of the next action is never charged to this move. Returns 1 skill row, 0 not, -1 unreadable; *w = the word. Read only.
static __attribute__((noinline)) int SmSkillWord(uint64_t ctrl, uint32_t act, uint32_t* w) {
    *w = 0xFFFFFFFFu; uint64_t p = 0, r = 0;
    if (!ctrl || act == SM_ACT_NONE || !Rd64(ctrl + 0xE8, &p) || p < 0x10000) return -1;
    __try {
        if (*(volatile uint32_t*)p != act) return -1;
        r = *(volatile uint64_t*)(p + 0x20); if (r < 0x10000) return -1;
        *w = *(volatile uint32_t*)(r + 0xDC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { *w = 0xFFFFFFFFu; return -1; }
    return SmSkillRow(*w);
}
// human: Burst Break. A boss's current action row R = [[ctrl+0xE8]+0x20] (same object as SmSkillWord; only when [P] == act): u16 R+0x04
// bit 0x2000 = GreatAttack (the red unblockable attack, auto_moves.h), u16 R+0xE0 = its next action id. Returns 1 burst row, 0 not,
// -1 unreadable; *fl = the flag word, *next = the next id (SM_ACT_NONE unread). Read only.
static __attribute__((noinline)) int SmBurstRow(uint64_t ctrl, uint32_t act, uint16_t* fl, uint32_t* next) {
    *fl = 0; *next = SM_ACT_NONE; uint64_t p = 0, r = 0;
    if (!ctrl || act == SM_ACT_NONE || !Rd64(ctrl + 0xE8, &p) || p < 0x10000) return -1;
    __try {
        if (*(volatile uint32_t*)p != act) return -1;
        r = *(volatile uint64_t*)(p + 0x20); if (r < 0x10000) return -1;
        *fl = *(volatile uint16_t*)(r + 0x04); *next = *(volatile uint16_t*)(r + 0xE0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { *fl = 0; *next = SM_ACT_NONE; return -1; }
    return (*fl & 0x2000) != 0;
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
    // blocked hits (BlockPct): need the HP hook (a hit that took HP must be seen) and the Ki hook (the guarded hit's Ki damage)
    if (g_smc.blockPct > 0 && g_hpdmg_hooked == 1 && g_pst_atk_ok && !g_pst_ki_hooked) PstKiHookInstall();
    SmBlkEv blkEv[4]; int blocked;
    AcquireSRWLockExclusive(&g_sm_blk_lock); blocked = SmBlkTake(&g_sm_blk, blkEv, 4); ReleaseSRWLockExclusive(&g_sm_blk_lock);
    if (g_smc.blockPct <= 0 || g_hpdmg_hooked != 1) blocked = 0;
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
    // #127213 guardian spirit skill: Spirit Force spent at the start of this move (SmGsStep), paid once when the move scores (SmGsPay)
    static SmGsMove gsm; static int gsInit; static uint64_t gsPtsRun; static float gsPts;
    if (!gsInit) { gsInit = 1; SmGsReset(&gsm); }
    float sfMax = 0.f; const float sfPrev = gsm.prev, sfCur = pl ? SmSpiritForce(g_fp[FP_PL].a, &sfMax) : -1.f;
    const int gsMark = pl ? SmGsStep(&gsm, g_sm_clock, run, sfCur, g_smc.gsMs) : 0;
    if (!pl) SmGsReset(&gsm);
    if (run != gsPtsRun) { gsPtsRun = run; gsPts = 0.f; }
    static SmSummon gsum; static int gsumInit;
    if (!gsumInit) { gsumInit = 1; SmSummonReset(&gsum); }
    if (gsMark) SmSummonOpen(&gsum, g_sm_clock, run, act);  // any guardian spirit spend: its spirit's own hits may follow
    const LONG allyHits = InterlockedExchange(&g_sm_allyHits, 0), allyMask = InterlockedExchange(&g_sr_allyMask, 0);
    const int summonLive = SmSummonLive(&gsum, &g_smc, g_sm_clock);
    // #127295 martial art: the move's action row carries the game's skill word (SmSkillWord) and it is not a guardian spirit / ninjutsu move
    static SmMaMove mam; static uint64_t ninRun; static int ninMove;
    if (run != ninRun) { ninRun = run; ninMove = 0; }
    if (nin) ninMove = 1;
    uint32_t skw = 0xFFFFFFFFu; const int skill = pl ? SmSkillWord(g_fp[FP_PL].ctrl, act, &skw) : -1;
    const int maMark = pl ? SmMaStep(&mam, run, skill, gsm.gs || ninMove) : 0;
    // multi-segment martial arts (Infinite Retribution 25351 -> 25352 -> 25353): back-to-back runs of one skill word are one use for the
    // multi-hit cap / loop penalty (style_meter.h SmChainStep); variety, startup and the counter bonus stay per run
    static SmChain useCh; static int useChInit;
    if (!useChInit) { useChInit = 1; SmChainReset(&useCh); }
    const uint64_t useKey = SmChainStep(&useCh, pl ? run : 0, act, skill, skw);
    if (!pl) SmMaReset(&mam);
    const LONG soul = InterlockedExchange(&g_soul_uses, 0);  // #126913 soul core uses (PstPlayerSoul: inventory drop in the seal throw / form start)
    if (g_cfg_sm_movelog && pl) {  // #127003 one STYLE MOVE line per player move (hit or miss) when it ends; ids are logged, not interpreted
        static uint64_t mrun; static SmMoveRec mr; static double mt0, soulT = -1e18; static int mon, mlines;
        static unsigned (*fu)(void); static int futried;
        if (!fu && !futried) { futried = 1; HMODULE rm = GetModuleHandleA("Nioh3BossRematch.asi"); fu = rm ? (unsigned (*)(void))GetProcAddress(rm, "Nioh3BossRematch_FormUseTick") : 0; }
        if (soul > 0) soulT = g_sm_clock;
        if (mon) { mr.landed += (int)hits; if (nin) mr.nin = 1; }
        if (run != mrun) {
            if (mon && mlines < 20000) { ++mlines; if (g_sm_swapRun == mrun) mr.swapped = 1; if (g_sm_maRun == mrun) mr.ma = 1; mr.ms = g_sm_clock - mt0; char b[400]; SmMoveFmt(b, sizeof b, &mr); Log("STYLE MOVE: t %.0f %s", mt0, b); }
            mrun = run; mt0 = g_sm_clock; mon = act != SM_ACT_NONE && act != 0;
            memset(&mr, 0, sizeof mr); mr.act = act; mr.startupF = -1.f; mr.enemy = g_sm_enemy;
            uint8_t v = 0xFF; __try { v = *(volatile uint8_t*)(g_fp[FP_PL].a + 0xA95C); } __except (EXCEPTION_EXECUTE_HANDLER) { v = 0xFF; }
            mr.style = v <= 1 ? v : -1; mr.slot = wslot;
            mr.sf0 = sfPrev; mr.sfMin = sfCur; mr.sfMax = sfMax;  // #127213 gauge just before the move / at its start
            SmPadNow(&mr.btn, &mr.lt, &mr.rt);
            const unsigned ft = fu ? fu() : 0;
            mr.soulForm = g_sm_clock - soulT <= SM_MOVE_SOUL_MS || (ft && GetTickCount() - ft <= SM_MOVE_SOUL_MS);
        }
        if (mon) { if (g_fp[FP_PL].run.hits > mr.windows) mr.windows = g_fp[FP_PL].run.hits; if (g_fp[FP_PL].run.great) mr.great = 1;
                   if (startupF >= 0.f && mr.startupF < 0.f) mr.startupF = startupF;
                   if (sfCur >= 0.f) { if (mr.sfMin < 0.f || sfCur < mr.sfMin) mr.sfMin = sfCur; if (sfMax > mr.sfMax) mr.sfMax = sfMax; }
                   if (gsMark) mr.gs = 1;
                   if (skill >= 0 && !mr.skillRd) { mr.skillRd = 1; mr.skill = skw; } }
    }
    const int sty = pl ? SmStyleSwitched(g_fp[FP_PL].a, g_sm_clock) : 0;
    const int fight = g_pst_live != 0;
    // Dodge / Deflect / Counter. Deflect: the move's action-row skill word (SmSkillWord) is in DeflectSkill (the universal Samurai deflect;
    // the deflect-guard bytes ctrl+0xEA7 / +0xEA8 of the outside model #37168 never came up live, so they are only logged). Other skill
    // move (counter): any other skill word, or this move spent Spirit Force (guardian spirit) / a ninjutsu gauge. Every clean avoid also logs the guard bytes seen in its window and how far the player moved
    // (actor+0x110 position), so a short live session can confirm the signal or replace it.
    uint8_t avNow = 0;  // bit 0 ea5 auto-guard, 1 ea7, 2 ea8 (deflect guard), 3 eaa
    if (pl && g_fp[FP_PL].ctrl) { const uint64_t c = g_fp[FP_PL].ctrl;
        __try { avNow = (uint8_t)((*(volatile uint8_t*)(c + 0xEA5) ? 1 : 0) | (*(volatile uint8_t*)(c + 0xEA7) ? 2 : 0) | (*(volatile uint8_t*)(c + 0xEA8) ? 4 : 0) | (*(volatile uint8_t*)(c + 0xEAA) ? 8 : 0)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { avNow = 0; } }
    int deflectNow = 0;
    if (pl && skill > 0) for (int i = 0; i < g_sm_ndefl; ++i) if ((skw & 0xFFFFu) == g_sm_defl[i]) deflectNow = 1;
    const int skillNow = pl && !deflectNow && (skill > 0 || gsm.gs || ninMove);
    static uint64_t ctrPaidRun;  // the move that already counted as a counter (hit counter or avoid counter: one per move)
    static int avIn; static uint8_t avFl; static float avP0[3], avMove; static int avHave;
    {   float p3[3]; int hp = 0;
        if (pl) __try { for (int k = 0; k < 3; ++k) p3[k] = *(volatile float*)(g_fp[FP_PL].a + 0x110 + 4 * k); hp = 1; } __except (EXCEPTION_EXECUTE_HANDLER) { hp = 0; }
        if (g_sm_enemy && !avIn) { avIn = 1; avFl = 0; avMove = 0.f; avHave = hp; if (hp) memcpy(avP0, p3, sizeof avP0); }
        if (avIn || g_sm.dodge.inWin) { avFl |= avNow;
            if (hp && avHave) { const float dx = p3[0] - avP0[0], dy = p3[1] - avP0[1], dz = p3[2] - avP0[2]; const float d = sqrtf(dx * dx + dy * dy + dz * dz); if (d > avMove) avMove = d; } }
    }
    // Result card. srSlotA / srSlotT: the actor of each boss slot and the wall ms it was first seen there (kept after the slot clears), so
    // a boss hit in the end wait can be told apart: next form (seen as the fight went down) or the next boss of a gauntlet (SrBossHitEx).
    uint64_t srHit[SR_BOSSES], srLethal[SR_BOSSES]; double srSeen[SR_BOSSES]; int nSrHit = 0, nSrLethal = 0;
    static uint64_t srSlotA[N3T_SLOTS]; static double srSlotT[N3T_SLOTS];
    const double srWall = SmWallMs();
    for (int s = 0; s < N3T_SLOTS; ++s) if (g_slot_act[s] && g_slot_act[s] != srSlotA[s]) { srSlotA[s] = g_slot_act[s]; srSlotT[s] = srWall; }
    const LONG bbPl = InterlockedExchange(&g_sr_hitMask, 0), bbAl = summonLive ? allyMask : 0;  // human: Burst Break, per boss slot
    {   const LONG m = bbPl | bbAl, lm = InterlockedExchange(&g_sr_lethalMask, 0);
        if (g_cfg_sr && m) for (int s = 0; s < N3T_SLOTS && s < 32 && nSrHit < SR_BOSSES; ++s) if (m & (LONG)(1u << s)) {
            uint64_t a = BvActor(s);
            if (!a) a = (uint64_t)g_sr_hitAct[s];  // the slot cleared since the hit (the hit killed it): still that boss's hit
            if (!a) continue;
            srHit[nSrHit] = a; srSeen[nSrHit] = srSlotA[s] == a ? srSlotT[s] : srWall; nSrHit++;
            if ((lm & (LONG)(1u << s)) && nSrLethal < SR_BOSSES) srLethal[nSrLethal++] = a;
        }
        if (!g_cfg_sr && g_sr.on) SrReset(&g_sr);
    }
    // human: Burst Break inputs per boss slot (row reads outside the lock); stepped under it once this tick's hit points are known
    static SmBurst bbs[N3T_SLOTS]; static int bbInit;
    if (!bbInit) { bbInit = 1; for (int s = 0; s < N3T_SLOTS; ++s) SmBurstReset(&bbs[s]); }
    int bbRow[N3T_SLOTS]; uint16_t bbFl[N3T_SLOTS]; uint32_t bbNext[N3T_SLOTS];
    const int bbSkill = pl && (skill > 0 || gsm.gs || ninMove);  // the player's move is a skill / guardian spirit / ninjutsu move
    for (int s = 0; s < N3T_SLOTS; ++s) { bbFl[s] = 0; bbNext[s] = SM_ACT_NONE; bbRow[s] = g_fp[s].valid ? SmBurstRow(g_fp[s].ctrl, g_fp[s].run.act, &bbFl[s], &bbNext[s]) : -1; }
    const int srWas = g_sr.on;
    SrResult srRes[2]; int srDone = 0;  // [0] a gauntlet split this tick, then the fight's own end (a newer card replaces the older one)
    AcquireSRWLockExclusive(&g_sm_lock);
    for (int i = 0; i < nSrHit; ++i) {
        if (SrBossHitEx(&g_sr, &g_sm, srHit[i], srSeen[i], SR_FORM_MS) == 1) {  // the next boss: this fight's card now, a fresh fight from this hit
            Log("STYLE RESULT: next boss %p hit %.1f s after the fight went down (first seen %+.1f s from it, a next form is seen within %.1f s): card now, new fight",
                (void*)srHit[i], (srWall - g_sr.endWall) / 1000.0, (srSeen[i] - g_sr.endWall) / 1000.0, SR_FORM_MS / 1000.0);
            if (srDone < 2) srRes[srDone++] = SrFinishEx(&g_sr, &g_sm, g_cfg_sr_hold, g_cfg_sr_hitpen);
            SrReset(&g_sr); SrBossHitEx(&g_sr, &g_sm, srHit[i], srSeen[i], SR_FORM_MS);
            Log("STYLE RESULT: fight started (next boss of a gauntlet, meter %s%s)", sm_rank_name[g_sm.rank], g_sm.active ? "" : " idle");
        }
    }
    const int r0 = g_sm.rank, a0 = g_sm.active;
    const int avoid = SmDodgeStepEx(&g_sm.dodge, g_sm_enemy, ifr, taken > 0, deflectNow, skillNow);
    const int dodge = avoid == SM_AV_DODGE, deflect = avoid == SM_AV_DEFLECT, avCtr = avoid == SM_AV_COUNTER;
    float ph = 0.f, pk = -1.f, pd = -1.f, pn = -1.f, ps = -1.f;  // human #126744: per-event play log (points each event scored)
    const uint32_t c0 = g_sm.counters, hc0 = g_sm.countedHits, cap0 = g_sm.cappedHits, lp0 = g_sm.loopPens; const float lpl0 = g_sm.loopLost;
    if (swapEv) SmOnSwap(&g_sm, g_sm_clock);
    for (LONG i = 0; i < hits && i < 64; ++i) ph += SmOnHitEx(&g_sm, &g_smc, g_sm_clock, act, useKey, startupF, ctr && run != ctrPaidRun);
    float pb = 0.f;  // blocked hits: BlockPct % of a hit of this move (no counter bonus: that is paid by a hit that lands)
    for (int i = 0; i < blocked && i < 64; ++i) pb += SmOnBlockedHit(&g_sm, &g_smc, g_sm_clock, act, useKey, startupF, 0);
    (void)rsT;  // the old swap attack window (SmSwapHit, WeaponSwapMs) is replaced by the any-switch rule
    const float psw = hits > 0 && !(g_sm.lpOn && g_sm.lpPen) ? SmWeaponHit(&g_sm, &g_smc, g_sm_clock, wslot) : 0.f;
    if (psw > 0.f) g_sm_swapRun = run;
    const int ctrHit = g_sm.counters != c0;
    if (gsMark) g_sm.gsUses++;
    gsPts += ph + pb;
    const float pgs0 = SmGsPay(&g_sm, &g_smc, &gsm, g_sm_clock, gsPts);  // #127213 value = the move's own points so far
    if (pgs0 > 0.f && gsm.run == gsum.run) gsum.paid = 1;  // the player's own hits paid this move's bonus
    float sumB = 0.f; const float psum = SmSummonHits(&g_sm, &g_smc, &gsum, g_sm_clock, (int)allyHits, &sumB);  // human: summoned spirit hits
    const float pgs = pgs0 + sumB;
    float pbb = 0.f, bbHitPts = 0.f; int bbN = 0; SmBurstEnd bbE[N3T_SLOTS]; int bbR[N3T_SLOTS], bbNew[N3T_SLOTS], bbCand[N3T_SLOTS];
    for (int s = 0; s < N3T_SLOTS; ++s) {  // human: Burst Break (style_meter.h SmBurstStep)
        const int valid = g_fp[s].valid && g_fp[s].a, pbit = s < 32 && (bbPl >> s) & 1, ab = s < 32 && (bbAl >> s) & 1;
        const int hk = pbit ? (bbSkill ? 2 : 1) : ab ? 2 : 0;
        const int was = bbs[s].on && bbs[s].burst, hitsWas = bbs[s].hits; const uint64_t keyWas = bbs[s].key;
        bbR[s] = SmBurstStep(&bbs[s], &g_smc, g_sm_clock, valid, (uint64_t)g_fp[s].run.f0 << 32 | g_fp[s].run.act, g_fp[s].run.act, bbRow[s], bbNext[s], g_fp[s].run.great, hk, pbit ? ph : psum, &bbE[s]);
        bbNew[s] = valid && bbs[s].burst && !(was && bbs[s].key == keyWas);          // this run was just marked a burst run (log)
        bbCand[s] = valid && bbs[s].key == keyWas && bbs[s].hits > hitsWas ? hk : 0;  // a counted hit on the live burst run (log)
        if (bbR[s] == SM_BB_AWARD) { pbb += SmOnBurst(&g_sm, &g_smc, g_sm_clock); bbN++; bbHitPts += bbE[s].hitPts; }
    }
    // human: a guardian spirit spend in combat holds the drain while the spirit winds up (its first hit lands ~1.5 s later)
    if (gsMark && SmInCombat(&g_sm, &g_smc, g_sm_clock) && g_sm.lastScore < g_sm_clock + SM_GS_HOLD_MS) g_sm.lastScore = g_sm_clock + SM_GS_HOLD_MS;
    // human: combo of landed hits (the player's and, inside a summon window, the spirit's); milestone bonus every ComboStep hits
    // multi-hit cap / loop penalty: only the hits that scored (SmOnHitEx / SmSummonHits counted them) feed the combo
    const uint32_t loopN = g_sm.loopPens - lp0, capN = g_sm.cappedHits - cap0; const float loopLost = g_sm.loopLost - lpl0;
    uint32_t comboMile = 0; const float pcombo = SmComboHits(&g_sm, &g_smc, g_sm_clock, (int)(g_sm.countedHits - hc0), &comboMile);
    if (maMark) g_sm.maUses++;
    const float pma = SmMaPay(&g_sm, &g_smc, &mam, g_sm_clock, gsPts);  // #127295 same value, once per move
    if (pma > 0.f) g_sm_maRun = run;
    for (int i = 0; i < nin; ++i) pn = (pn < 0 ? 0 : pn) + SmOnSkill(&g_sm, &g_smc, g_sm_clock, 0);
    float pc = -1.f;
    for (LONG i = 0; i < soul && i < 8; ++i) pc = (pc < 0 ? 0 : pc) + SmOnSkill(&g_sm, &g_smc, g_sm_clock, 1);
    if (sty) ps = SmOnStyle(&g_sm, &g_smc, g_sm_clock);
    if (ki) pk = SmOnKiEx(&g_sm, &g_smc, g_sm_clock, kiPerf);
    float pp = -1.f;
    if (dodge) pd = SmOnDodge(&g_sm, &g_smc, g_sm_clock);
    if (ctrHit) ctrPaidRun = run;
    if (deflect) pp = SmOnDeflect(&g_sm, &g_smc, g_sm_clock);
    float pac = -1.f;
    if (avCtr) { pac = SmOnAvoidCounter(&g_sm, &g_smc, g_sm_clock, run != ctrPaidRun); ctrPaidRun = run; }
    if (taken > 0) SmOnDamaged(&g_sm, &g_smc, g_sm_clock);
    SmComboTick(&g_sm, &g_smc, g_sm_clock);
    InterlockedExchange(&g_sm_combo, (LONG)g_sm.combo);
    SmTick(&g_sm, &g_smc, g_sm_clock, dt);
    if (g_sr.on) {  // fight result: moves that landed, martial arts that scored, then this tick's time at the meter's value
        if (hits > 0 || blocked > 0) SrMove(&g_sr, act);
        if (pma > 0.f) SrMartialArt(&g_sr, act);
        SrTick(&g_sr, &g_sm, &g_smc, dt);
    }
    const int vis = pl && (g_cfg_sm_always || SmVisible(&g_sm, &g_smc, g_sm_clock, fight));  // human #129172
    const int r1 = g_sm.rank, a1 = g_sm.active; const float gg = SmGauge(&g_sm, &g_smc);
    const uint32_t nh = g_sm.hits, nd = g_sm.dodges, nk = g_sm.kis, nt = g_sm.damaged;
    const float gp = g_sm.pts;
    if (g_sr.on) {  // boss deaths / player death / a boss leaving alive / a load (actor table reads, still under the lock for SrFinish's counters)
        SrBossNow now[SR_BOSSES]; memset(now, 0, sizeof now);
        char bl[320]; int bn = 0; bl[0] = 0;  // per boss state, for the log lines below
        for (int i = 0; i < g_sr.nBoss; ++i) {
            const uint64_t a = g_sr.boss[i]; int lethal = 0;
            for (int k = 0; k < nSrLethal; ++k) if (srLethal[k] == a) lethal = 1;
            uint64_t c = 1, mx = 0; int d = 0;
            if (!ActorInTable(a)) { now[i].gone = 1; now[i].hp0 = lethal; }  // torn down: killed if HP 0 was seen (SrStep) or this tick's hit was lethal
            else { ActorDeadNow(a, &d, &c, &mx); now[i].dead = d; now[i].hp0 = mx && c == 0; }
            if (bn < (int)sizeof bl - 64) bn += snprintf(bl + bn, sizeof bl - bn, " %p %s", (void*)a, now[i].gone ? (g_sr.hp0[i] || lethal ? "gone after HP 0" : g_sr.dead[i] ? "gone dead" : "gone with HP left")
                : d ? "dead" : now[i].hp0 ? "HP 0" : "up");
        }
        const uint64_t me = PlayerChar(); const uint8_t* lgs = GameState();
        const int loading = !me || (lgs && lgs[2]);  // a load, or the mission ended / was left (no player character)
        int pdead = 0;
        if (!loading) { int d = 0; uint64_t c = 1, mx = 0; ActorDeadNow(me, &d, &c, &mx); pdead = d; }
        const int wasEnding = g_sr.ending;
        const int st = SrStep(&g_sr, now, pdead, loading, srWall, g_cfg_sr_delay);
        if (st == 1) Log("STYLE RESULT: every boss of the fight is down (bosses:%s), card in %d ms unless its next form is hit", bl, g_cfg_sr_delay);
        else if (st == 2) {
            if (!wasEnding) Log("STYLE RESULT: %s with every boss of the fight down (bosses:%s): card now", !me ? "the player character went away" : "a load started", bl);
            if (srDone < 2) srRes[srDone++] = SrFinishEx(&g_sr, &g_sm, g_cfg_sr_hold, g_cfg_sr_hitpen);
            SrReset(&g_sr);
        }
        else if (st < 0) { Log("STYLE RESULT: fight dropped, no card (%s; %.1f s in; bosses:%s)", sr_drop_name[g_sr.drop], g_sr.ms / 1000.0, bl); SrReset(&g_sr); }
    }
    ReleaseSRWLockExclusive(&g_sm_lock);
    if (!srWas && g_sr.on) Log("STYLE RESULT: fight started (first hit on a boss, meter %s%s)", sm_rank_name[g_sm.rank], g_sm.active ? "" : " idle");
    for (int i = 0; i < srDone; ++i) {
        char b[700]; SrFmt(b, sizeof b, &srRes[i]); Log("STYLE RESULT: %s", b);
        AcquireSRWLockExclusive(&g_smfx_lock); g_sr_card = srRes[i]; g_sr_cardT = SmWallMs(); g_sr_cardGen++; ReleaseSRWLockExclusive(&g_smfx_lock);
    }
    {   static int bbl;  // human: Burst Break log (the live test reads these: burst run seen / hit in its wind-up / its end)
        for (int s = 0; s < N3T_SLOTS && bbl < 3000; ++s) {
            if (bbNew[s]) { ++bbl; Log("STYLE BURST: boss %c act 0x%X (%u) is a burst run (row flags 0x%04X, next 0x%X) at t %.0f", 'A' + s, g_fp[s].run.act, g_fp[s].run.act, bbFl[s], bbNext[s], g_sm_clock); }
            if (bbCand[s]) { ++bbl; Log("STYLE BURST: candidate boss %c act 0x%X hit by %s%s in its wind-up (%.0f ms into the run, player act %u skill word 0x%X gs %d ninjutsu %d i-frame counter %d)", 'A' + s, g_fp[s].run.act,
                (bbPl >> s) & 1 ? "the player" : "the summoned spirit", bbCand[s] == 2 ? " (skill move)" : "", g_fp_gms - g_fp[s].run.t0, act, skw, gsm.gs, ninMove, ctr); }
            if (bbR[s]) { ++bbl; const SmBurstEnd* e = &bbE[s];
                Log("STYLE BURST: %s boss %c act 0x%X -> 0x%X, %d hit%s, last %.0f ms before the end (window %d ms)%s%s%s%s", bbR[s] == SM_BB_AWARD ? "BURST BREAK" : "no break,", 'A' + s, e->act, e->newAct, e->hits, e->hits == 1 ? "" : "s", e->ago, g_smc.burstMs,
                    e->great ? " | unblockable collider already out" : "", e->cont ? " | the next action continues the burst (row next / burst row)" : "", e->late ? " | too late" : "", e->noSkill ? " | not a skill hit (BurstBreakSkillOnly)" : ""); }
        }
    }
    {   // #127085 one pop-up per scored bonus (plain hits are not bonuses); a big bonus tick shakes the meter
        float big = 0.f;
        if (bbN) { if (bbHitPts > 0.f) SmFxPop2("Burst Break!", bbHitPts, pbb); else SmFxPop("Burst Break!", pbb); big += pbb; }  // human: the breaking hit's value, then the bonus (BurstBreakPts 0: the hit value only)
        if (ctrHit && g_smc.counter > 0.f) { SmFxPop2("Counter!", ph - g_smc.counter > 0.f ? ph - g_smc.counter : 0.f, g_smc.counter); big += g_smc.counter; }  // #127171 hit value, then the counter bonus
        if (pk > 0.f) { const int pf = kiPerf && g_smc.kiPerfect > 0.f && pk > g_smc.kiPerfect; SmFxPop2(pf ? "Perfect Ki" : "Ki Pulse", pf ? pk - g_smc.kiPerfect : pk, pf ? g_smc.kiPerfect : 0.f); big += pk; }
        if (pd > 0.f) { SmFxPop("Dodge!", pd); big += pd; }
        if (pp > 0.f) { SmFxPop("Deflect!", pp); big += pp; }
        if (pac > 0.f) { SmFxPop("Counter!", pac); big += pac; }
        if (pn > 0.f) { SmFxPop("Ninjutsu", pn); big += pn; }
        if (pc > 0.f) { SmFxPop("Soul Core", pc); big += pc; }
        if (ps > 0.f) { SmFxPop("Style Switch", ps); big += ps; }
        if (psw > 0.f) { SmFxPop2("Weapon Switch", ph, psw); big += psw; }  // the swap attack's hit value, then the swap bonus
        if (pgs0 > 0.f) { SmFxPop2("Guardian Spirit", gsPts, pgs0); big += pgs0; }  // #127213 the skill's own points, then the bonus
        if (sumB > 0.f) { SmFxPop2("Guardian Spirit", psum, sumB); big += sumB; }  // human: summoned spirit's first hit (its points, then the bonus)
        if (pcombo > 0.f) {  // human: announced under the combo counter (not a pop-up line, which got lost among the others)
            InterlockedExchange(&g_sm_comboMile, (LONG)comboMile); InterlockedExchange(&g_sm_comboMilePts, (LONG)(pcombo + 0.5f)); InterlockedIncrement(&g_sm_comboMileGen);
            big += pcombo; }
        if (pma > 0.f) { SmFxPop2("Martial Art", gsPts, pma); big += pma; }  // #127295 the martial art's own points, then the bonus
        if (loopN && loopLost > 0.f) SmFxPopLoss("Repetitive!", loopLost);  // loop penalty: shown "-N"
        if (big >= (float)g_cfg_sm_shakepts) { AcquireSRWLockExclusive(&g_smfx_lock); g_smfx_shakeT = SmWallMs(); ReleaseSRWLockExclusive(&g_smfx_lock); }
    }
    if (blocked > 0) {  // live check of the guarded-hit signal: one line per blocked hit (first 40, then at most one a second)
        static int bl; static DWORD blT; static uint32_t blN; const DWORD now = GetTickCount();
        blN += (uint32_t)blocked;
        if (bl < 40 || now - blT >= 1000) {
            bl++; blT = now;
            for (int i = 0; i < blocked && i < 4; ++i) Log("STYLE BLOCK: guarded hit %p on enemy status %p (signals: Ki damage %d (%.1f), HP damage call with 0 %d; no HP taken) -> +%.1f (%d %% of a hit; %u blocked so far, Ki hook %d)",
                (void*)blkEv[i].hit, (void*)blkEv[i].vic, !!(blkEv[i].fl & SM_BLK_KI), blkEv[i].ki, !!(blkEv[i].fl & SM_BLK_HP0), pb / (float)blocked, g_smc.blockPct, blN, (int)g_pst_ki_hooked);
        }
    }
    if (hits > 0 || blocked > 0 || allyHits > 0 || pcombo > 0.f || ki || avoid || taken > 0 || nin || sty || soul > 0 || swapEv || gsMark || pgs > 0.f || maMark || pma > 0.f || bbN) {
        static int evl;
        if (evl++ < 4000) {
            char e[400]; int n = 0; e[0] = 0;
            if (hits > 0) n += snprintf(e + n, sizeof e - n, " hit x%ld +%.1f (startup %.0f f%s)", (long)hits, ph, startupF, ctrHit ? ", counter" : "");
            if (blocked > 0) n += snprintf(e + n, sizeof e - n, " blocked x%d +%.1f", blocked, pb);
            if (pcombo > 0.f) n += snprintf(e + n, sizeof e - n, " combo %u +%.0f", comboMile, pcombo);
            if (loopN) n += snprintf(e + n, sizeof e - n, " repetitive x%u -%.0f (loop %u/%u uses)", loopN, loopLost, (unsigned)g_sm.lpN1, (unsigned)g_sm.lpN2);
            if (capN) n += snprintf(e + n, sizeof e - n, " over multi-hit cap x%u", capN);
            if (allyHits > 0) n += snprintf(e + n, sizeof e - n, summonLive ? " summoned spirit hit x%ld +%.1f" : " ally hit x%ld (no summon window, not scored)", (long)allyHits, psum);
            if (nin) n += snprintf(e + n, sizeof e - n, " ninjutsu +%.0f", pn);
            if (swapEv) n += snprintf(e + n, sizeof e - n, " weapon slot -> %d", wslot);
            if (psw > 0.f) n += snprintf(e + n, sizeof e - n, " weapon switch +%.0f", psw);
            if (gsMark) n += snprintf(e + n, sizeof e - n, " spirit force %.1f->%.1f/%.1f", gsm.base, sfCur, sfMax);
            if (pgs > 0.f) n += snprintf(e + n, sizeof e - n, " guardian spirit +%.0f", pgs);
            if (maMark) n += snprintf(e + n, sizeof e - n, " skill row 0x%X%s", skw, mam.other ? " (guardian spirit / ninjutsu move)" : "");
            if (pma > 0.f) n += snprintf(e + n, sizeof e - n, " martial art +%.0f", pma);
            if (bbN) n += snprintf(e + n, sizeof e - n, " burst break x%d +%.0f", bbN, pbb);
            if (soul > 0) n += snprintf(e + n, sizeof e - n, " soul core x%ld +%.0f", (long)soul, pc);
            if (sty) n += snprintf(e + n, sizeof e - n, " style switch +%.0f", ps);
            if (ki && pk > 0) n += snprintf(e + n, sizeof e - n, " ki%s +%.0f", kiPerf ? " perfect" : "", pk);
            else if (ki) n += snprintf(e + n, sizeof e - n, " ki +0 (no hit in %d ms)", g_smc.kiHitMs);
            if (avoid) n += snprintf(e + n, sizeof e - n, " %s +%.0f (guard bytes ea5 %d ea7 %d ea8 %d eaa %d, moved %.2f m, skill move %d word 0x%X)", deflect ? "deflect" : avCtr ? "counter (avoid)" : "dodge", deflect ? pp : avCtr ? pac : pd,
                avFl & 1, (avFl >> 1) & 1, (avFl >> 2) & 1, (avFl >> 3) & 1, avMove / 100.f, g_sm.dodge.sawSkill, skw);
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
    if (!g_sm_enemy && !g_sm.dodge.inWin) avIn = 0;
    // FmFeed's early return (frame meter off) leaves the hook's pulses set; they are consumed here so a stale pulse is not a window next tick
    for (int s = 0; s <= N3T_SLOTS; ++s) g_fp[s].pulse = g_fp[s].pulseUb = g_fp[s].pulseGrab = 0;
}
// ---- drawing (own thread) ----
#include "style_meter_draw.h"
#include "style_result_draw.h"
static BOOL CALLBACK SmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_sm_wnd || h == g_sr_wnd || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_sm_game = h; return FALSE;
}
// In-game drawing (DrawInGame = 1): the finished bitmap goes to Nioh3EngineFix, which draws it into the game's own frame, so OBS Game
// Capture, the Steam recorder and screenshots include it (a topmost window over the game is not part of the game's image). Without
// Nioh3EngineFix, or while its overlay is not drawing, the meter and the card use their own topmost windows as before.
typedef int (*EfLayerSetFn)(int id, int x, int y, int w, int h, int refW, int refH, const void* px, int stride, float alpha);
static EfLayerSetFn p_efLayerSet; static void (*p_efLayerHide)(int); static int (*p_efLayerLive)(void);
enum { SM_LAYER_METER = 0, SM_LAYER_CARD = 1 };
static int SmLayerOk(void) {
    static ULONGLONG nextTry; static int said = -1;
    if (!g_cfg_sm_ingame) return 0;
    if (!p_efLayerSet) {
        const ULONGLONG t = GetTickCount64(); if (t < nextTry) return 0; nextTry = t + 3000;
        HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi"); if (!ef) ef = GetModuleHandleA("Nioh3EngineFix.dll");
        if (!ef) return 0;
        p_efLayerHide = (void (*)(int))GetProcAddress(ef, "Nioh3EngineFix_LayerHide"); p_efLayerLive = (int (*)(void))GetProcAddress(ef, "Nioh3EngineFix_LayerLive");
        EfLayerSetFn set = (EfLayerSetFn)GetProcAddress(ef, "Nioh3EngineFix_LayerSet");
        if (!set || !p_efLayerHide || !p_efLayerLive) { if (said != 2) { said = 2; Log("STYLE METER: Nioh3EngineFix has no layer API (older version) - own window, not recorded by OBS Game Capture"); } nextTry = t + 600000; return 0; }
        p_efLayerSet = set;
    }
    const int live = p_efLayerLive() != 0;
    if (live != said) { said = live; Log(live ? "STYLE METER: drawing into the game's frames through Nioh3EngineFix (OBS Game Capture records it)" : "STYLE METER: Nioh3EngineFix overlay not drawing - own window"); }
    return live;
}
static void SmLayerHide(int id) { if (p_efLayerHide) p_efLayerHide(id); }
static void SmRender(HWND wnd, const SmCanvas* c, const SmDrawSt* st, int lay, int cx, int cy, int refW, int refH) {
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
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)(255 * g_cfg_sm_alpha / 100), AC_SRC_ALPHA};  // human #128855 opacity
        if (lay) p_efLayerSet(SM_LAYER_METER, cx, cy, w, h, refW, refH, px, w * 4, (float)g_cfg_sm_alpha / 100.f);
        else UpdateLayeredWindow(wnd, scr, 0, &sz, mem, &src, 0, &bf, ULW_ALPHA);
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
// Confirm for the result card: XInput A on any pad (xinput1_4 directly, like SmPadNow; Steam Input presents PlayStation pads as XInput, so
// Cross works through it), or Enter on the keyboard. Polled from the draw thread only while the game window is in front.
static int SrConfirmDown(void) {
    typedef DWORD (WINAPI *XgsFn)(DWORD, void*); static XgsFn xgs; static int tried;
    if (!xgs && !tried) { tried = 1; HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll"); xgs = x ? (XgsFn)GetProcAddress(x, "XInputGetState") : 0; }
    if (GetAsyncKeyState(VK_RETURN) & 0x8000) return 1;
    if (xgs) for (DWORD i = 0; i < 4; ++i) { uint8_t st[16] = {0}; if (xgs(i, st) == 0 && (*(uint16_t*)(st + 4) & 0x1000)) return 1; }  // XINPUT_GAMEPAD_A
    return 0;
}
// Result card window (draw thread): shows the newest g_sr_card until the player presses confirm (A / Enter; ResultShowMs > 0 also times it
// out), centred near the top of the game window. While the game is unfocused, loading or in a menu the card hides and comes back after.
// The panel is composed once per card / size; only the grade letter's motion is redrawn, then just the fade.
static void SrCardTick(int ok, int lay) {
    static LONG gen; static SrResult r; static double t0 = -1e18, closeT = -1e18; static int shown, w, h, settled, closed = 1, wasDown = 1, sawUp, wasLay = -1, dirty;
    static HDC mem; static HBITMAP bmp; static uint32_t* px; static uint32_t* base; static RECT last;
    AcquireSRWLockShared(&g_smfx_lock);
    if (g_sr_cardGen != gen) { gen = g_sr_cardGen; r = g_sr_card; t0 = g_sr_cardT; w = 0; closed = 0; wasDown = 1; sawUp = 0; }
    ReleaseSRWLockShared(&g_smfx_lock);
    const double now = SmWallMs(), age = now - t0;
    if (!closed && ok && shown && age >= SR_ARM_MS) {
        if (SrConfirmEdge(SrConfirmDown(), &wasDown, &sawUp)) { closed = 1; closeT = now; Log("STYLE RESULT: card closed by the player (confirm)"); }
    } else if (!closed) wasDown = 1;  // a press must start while the card is up and armed
    const float a = SrCardAlpha(age, g_cfg_sr_show, closed ? now - closeT : -1.0);
    if (lay != wasLay) { if (shown) { if (wasLay == 1) SmLayerHide(SM_LAYER_CARD); else if (g_sr_wnd) ShowWindow(g_sr_wnd, SW_HIDE); } shown = 0; dirty = 1; wasLay = lay; }
    if (!ok || a <= 0.f || !g_cfg_sr || !g_sm_game) { if (shown) { if (lay) SmLayerHide(SM_LAYER_CARD); else ShowWindow(g_sr_wnd, SW_HIDE); shown = 0; } return; }
    if (!g_sr_wnd) {
        WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = SmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingStyleResult";
        RegisterClassExW(&wc);
        g_sr_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
        if (!g_sr_wnd) { Log("STYLE RESULT: card window failed (%lu)", GetLastError()); g_cfg_sr = 0; return; }
    }
    RECT cr; GetClientRect(g_sm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_sm_game, &o);
    if (cr.right <= 0 || cr.bottom <= 0) return;
    int pct = g_cfg_sr_scale;  // ResultScalePct (its own size, not the meter's), but always fits the window
    { const float kf = (float)cr.bottom / 1080.f, mx1 = 0.9f * cr.bottom / SR_CARD_H / kf * 100.f, mx2 = 0.9f * cr.right / SR_CARD_W / kf * 100.f;
      if (pct > mx1) pct = (int)mx1; if (pct > mx2) pct = (int)mx2; }
    const SrGeo g = SrGeom(cr.bottom, pct);
    if (g.w != w || g.h != h || !mem) {
        if (bmp) DeleteObject(bmp); if (mem) DeleteDC(mem); free(base); bmp = 0; mem = 0; base = 0; px = 0;
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = g.w; bi.bmiHeader.biHeight = -g.h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        HDC scr = GetDC(0); mem = CreateCompatibleDC(scr); ReleaseDC(0, scr);
        bmp = mem ? CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, 0, 0) : 0;
        base = (uint32_t*)malloc((size_t)g.w * g.h * 4);
        if (!bmp || !px || !base) { if (bmp) DeleteObject(bmp); if (mem) DeleteDC(mem); free(base); bmp = 0; mem = 0; base = 0; px = 0; w = 0; return; }
        SelectObject(mem, bmp);
        __try { SrComposeBase(mem, px, &g, &r); GdiFlush(); } __except (EXCEPTION_EXECUTE_HANDLER) { memset(px, 0, (size_t)g.w * g.h * 4); }
        memcpy(base, px, (size_t)g.w * g.h * 4);
        w = g.w; h = g.h; settled = 0; dirty = 1;
    }
    if (!settled) {
        __try { SrComposeFrame(mem, px, base, &g, &r, age); } __except (EXCEPTION_EXECUTE_HANDLER) { memcpy(px, base, (size_t)g.w * g.h * 4); }
        if (age > SR_FADE_IN_MS + SM_UP_MS + 200) settled = 1;  // the letter's motion is over: the frame stays as it is
        dirty = 1;
    }
    const RECT want_r = {o.x + (cr.right - g.w) / 2, o.y + (cr.bottom - g.h) / 2, 0, 0};  // centred
    const float alpha = a * (float)g_cfg_sm_alpha / 100.f;
    if (lay) {  // in-game: the image only when it changed; otherwise just the fade / position / keep-alive
        GdiFlush();
        if (p_efLayerSet(SM_LAYER_CARD, want_r.left - o.x, want_r.top - o.y, g.w, g.h, cr.right, cr.bottom, dirty ? (const void*)px : 0, g.w * 4, alpha) || !dirty) dirty = 0;
        shown = 1; last = want_r;
        return;
    }
    if (!shown || want_r.left != last.left || want_r.top != last.top) {
        SetWindowPos(g_sr_wnd, HWND_TOPMOST, want_r.left, want_r.top, g.w, g.h, SWP_NOACTIVATE | SWP_SHOWWINDOW); last = want_r; shown = 1; }
    HDC scr = GetDC(0); POINT src = {0, 0}; SIZE sz = {g.w, g.h};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)(255.f * alpha), AC_SRC_ALPHA};
    UpdateLayeredWindow(g_sr_wnd, scr, 0, &sz, mem, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(0, scr);
}
static DWORD WINAPI SmThread(LPVOID p) {
    (void)p;
    __try { SmAtlasLoad(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_sm_atlas_dc = 0; Log("STYLE METER: atlas decode faulted, text fallback"); }
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = SmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingStyleMeter";
    RegisterClassExW(&wc);
    g_sm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_sm_wnd) { Log("STYLE METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    Log("STYLE METER: overlay window ready");
    int shown = 0, lastRank = -1, lastGauge = -1, seenRank = -1, wasLay = 0; RECT last = {0};
    float eased = 0.f; SmShownBar bar = {-1, 0.f}; int upFrom = -1; double lastT = SmWallMs(), rankUpT = -1e18;
    for (;;) {
        Sleep(33);
        const double now = SmWallMs(), dtw = now - lastT; lastT = now;
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_sm_game || !IsWindow(g_sm_game)) { g_sm_game = 0; EnumWindows(SmFind, 0); }
        const int gameOk = g_sm_game && !IsIconic(g_sm_game);
        const int fg = gameOk && (GetForegroundWindow() == g_sm_game || g_cfg_fm_unfocused);
        const uint8_t* lgs = GameState(); const int loading = lgs && lgs[2];
        const int menu = GateMenuOpen();
        const int lay = SmLayerOk();
        SrCardTick(g_cfg_sm && gameOk && fg && !loading && !menu, lay);
        if (lay != wasLay) { if (shown) { if (wasLay) SmLayerHide(SM_LAYER_METER); else ShowWindow(g_sm_wnd, SW_HIDE); } shown = 0; wasLay = lay; }
        const int want = g_cfg_sm && g_sm_vis && gameOk && fg && !loading && !menu;
        if (!want) { if (shown) { if (lay) SmLayerHide(SM_LAYER_METER); else ShowWindow(g_sm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_sm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_sm_game, &o);
        if (cr.right <= 0 || cr.bottom <= 0) continue;
        const SmCanvas cv = SmCanvasGeom(cr.bottom, g_cfg_sm_pops); const SmGeo g = cv.g;
        // human: default place top left, below the game's health / Ki bars and the FPS counter, away from the fight: box right edge at 200/1000
        // of the width (room for the widest pop-ups), letter centre at 270/1000 of the height
        const int rx = g_cfg_sm_x >= 0 ? cr.right * g_cfg_sm_x / 1000 : cr.right * SM_DEF_X / 1000;
        const int cy = g_cfg_sm_y >= 0 ? cr.bottom * g_cfg_sm_y / 1000 : cr.bottom * SM_DEF_Y / 1000;
        const int left = o.x + rx - g.w, top = o.y + cy - (int)(SM_LETTER_H_1080P * SM_LK * SM_SIZE_K * (float)cr.bottom / 1080.f * (float)g_cfg_sm_scale / 100.f) / 2 - g.pad;
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
        {   // human: combo counter; after the combo ends the last count fades out over 1 s
            static int cmbShow; static double cmbEnd = -1.0;
            const int cmb = (int)g_sm_combo;
            if (cmb >= 2) { cmbShow = cmb; cmbEnd = -1.0; } else if (cmbShow && cmbEnd < 0) cmbEnd = now;
            float ca = cmbShow ? (cmbEnd < 0 ? 1.f : 1.f - (float)((now - cmbEnd) / 1000.0)) : 0.f;
            if (ca <= 0.f) { ca = 0.f; cmbShow = 0; cmbEnd = -1.0; }
            st.combo = cmbShow; st.comboA = ca;
            static LONG mileGen; static double mileT = -1e18;  // milestone banner: 1.6 s, then fades over 0.6 s
            if (g_sm_comboMileGen != mileGen) { mileGen = g_sm_comboMileGen; mileT = now; }
            const double ma = now - mileT;
            st.mileMs = ma; st.mile = (int)g_sm_comboMile; st.milePts = (int)g_sm_comboMilePts;
            st.mileA = ma < 0 ? 0.f : ma < 1600 ? 1.f : ma < 2200 ? 1.f - (float)((ma - 1600) / 600.0) : 0.f;
        }
        const int anim = st.npops > 0 || st.scale != 1.f || st.dx != 0.f || st.dy != 0.f || st.glow > 0.f || eased != (float)gauge || showRank != rank || (st.upMs >= 0.0 && st.upMs < SM_UP_MS) || (st.comboA > 0.f && st.comboA < 1.f) || (st.mileMs >= 0 && st.mileMs < 2300);
        const int gpx = (int)(sm_atlas_gauge_fill[2] * g.k) * gauge / 1000;  // redraw only when the gauge moves a pixel (or something animates)
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            if (!lay) SetWindowPos(g_sm_wnd, HWND_TOPMOST, want_r.left, want_r.top, cv.w, cv.h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastRank = -1;
        }
        static int wasAnim, lastAlpha = -1;
        static int lastCombo = -1;
        if (showRank != lastRank || gpx != lastGauge || anim || wasAnim || g_cfg_sm_alpha != lastAlpha || st.combo != lastCombo) {
            lastRank = showRank; lastGauge = gpx; wasAnim = anim; lastAlpha = g_cfg_sm_alpha; lastCombo = st.combo;
            __try { SmRender(g_sm_wnd, &cv, &st, lay, want_r.left - o.x, want_r.top - o.y, cr.right, cr.bottom); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        } else if (lay) p_efLayerSet(SM_LAYER_METER, want_r.left - o.x, want_r.top - o.y, cv.w, cv.h, cr.right, cr.bottom, 0, 0, (float)g_cfg_sm_alpha / 100.f);  // keep it alive
    }
}
