"""human #121923: 'Despawn all' (and removing the last boss slot) ends the Training session, so the mod menu stops reopening on the
Practice page (EngineFix #13529 reads Nioh3Training_SessionMode / _SessionActive). A refused session start never leaves Practice set.

Live 23:03: 'DESPAWN ALL (menu): 1 deleted ... slots reset' but g_tr.active stayed 1 -> SessionMode 2 -> 'opened on the Practice page'.
Runs the production RqTick / TrainStopIfNoBoss / SlotDespawn / SlotAlive / AnySlotAlive / SessionActive / SessionMode / TrainStart
bodies against mocks. No boss ids involved. N3T_TRAINING_SRC=<file> runs it against another training.c (e.g. the pre-fix one).
"""
from pathlib import Path
import os, subprocess, sys, tempfile

root = Path(__file__).resolve().parents[1]
s = Path(os.environ.get("N3T_TRAINING_SRC", root / "src/training.c")).read_text(encoding="utf-8")

def body(sig):
    a = s.index(sig + ' {'); b = s.index('{', a); d = 1; i = b + 1
    while d:
        d += (s[i] == '{') - (s[i] == '}'); i += 1
    return s[a:i]

def line(start):
    a = s.index(start); return s[a:s.index('\n', a)]

helper = 'static void TrainStopIfNoBoss(const char* why)'
fn = [
    'static int AnySlotAlive(void);',
    body('static int SlotAlive(int s)'),
    body('static int SessionBossAlive(void)') if 'static int SessionBossAlive(void) {' in s else '',
    body(helper) if helper + ' {' in s else '',
    body('static void SlotDespawn(int s, int deferDead)'),
    body('static void RqTick(void)'),
    body('static int AnySlotAlive(void)'),
    line('N3T_API int Nioh3Training_SessionActive(void)'),
    line('N3T_API int Nioh3Training_SessionMode(void)'),
    body('static int TrainStart(const char* tok, float d)'),
]

pre = r'''
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define N3T_API
#define N3T_SLOTS 4
#define MAXSP 32
static char lastLog[256];
#define Log(...) snprintf(lastLog, sizeof lastLog, __VA_ARGS__)
static struct { int active, round, nodeath, beaten, hasStart, bodyGone, lost, practice, arena; uint32_t chr, bgm; char tok[32]; DWORD tCmd, tWin, tBeat, tRespawn, tReport, tFade, tFadeAt; uint64_t fadeAct; float pp[4], bp[3], face; } g_tr;
static struct { int active; } g_sp, g_ar_, g_np, g_fb;
static struct { int active, placement; } g_ar; static int g_arena_placement, g_cfg_tr_dist;
static uint64_t g_sp_actor[MAXSP]; static int g_nsp, g_sp_slot_next = -1, g_practice_next;
static uint64_t g_slot_act[N3T_SLOTS]; static uint32_t g_slot_chr[N3T_SLOTS]; static int g_slot_native[N3T_SLOTS], g_slot_ih[N3T_SLOTS];
static struct { int phase; } g_sph[N3T_SLOTS];
static volatile LONG g_rq_op, g_rq_slot; static volatile uint32_t g_rq_mission, g_rq_chr;
static uint8_t gs[6]; static const uint8_t* GameState(void) { return gs; }
static int nativeAlive, stops; static char stopWhy[64];
static int NativeSlotAlive(int s) { (void)s; return nativeAlive; }
static int NativeSlotsActive(void) { return nativeAlive; }
static const char* SummonTok(uint32_t m) { (void)m; return 0; }
static int SummonStart(const char* t, float d) { (void)t; (void)d; return 1; }
static int Nioh3Training_Summon(uint32_t m) { (void)m; return 1; }
static int Actors(uint64_t* o, int m) { int n = 0; for (int i = 0; i < g_nsp && n < m; ++i) if (g_sp_actor[i]) o[n++] = g_sp_actor[i]; return n; }  // the world = every not-deleted spawn
static int Despawn2(int i, int defer) { (void)defer; if (!g_sp_actor[i]) return 0; g_sp_actor[i] = 0; return 1; }
static int DespawnAll(const char* why) { (void)why; int n = 0; g_sp.active = 0; for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) { g_sp_actor[i] = 0; ++n; } g_nsp = 0; return n; }
static void SumNoDeathRelease(const char* w) { (void)w; }
static void TrainStop(const char* why) { ++stops; snprintf(stopWhy, sizeof stopWhy, "%s", why); g_tr.active = 0; }  // production TrainStop ends with g_tr.active = 0
static void TrainClear(const char* w) { (void)w; }
static uint32_t TokChr(const char* t, uint32_t* b) { (void)t; (void)b; return 1; }
static int TrNoDeath(void) { return 0; }
static int PlayerSetNoDeath(int on) { (void)on; return 1; }
static void IhDefaultSession(void) {} static void BossMatchDefaults(void) {} static void FightBeginSession(void) {}
'''

main = r'''
static uint64_t actA = 0xA000, actB = 0xB000;
static void Practice(int nBosses) {  // a running Practice session ('Training here') with nBosses summoned slots
    memset(&g_tr, 0, sizeof g_tr); g_tr.active = 1; g_tr.practice = 1; stops = 0; stopWhy[0] = 0; nativeAlive = 0; g_sp.active = 0;
    memset(g_slot_act, 0, sizeof g_slot_act); memset(g_sp_actor, 0, sizeof g_sp_actor); g_nsp = 0;
    if (nBosses > 0) { g_sp_actor[g_nsp++] = actA; g_slot_act[0] = actA; g_slot_chr[0] = 1; }
    if (nBosses > 1) { g_sp_actor[g_nsp++] = actB; g_slot_act[1] = actB; g_slot_chr[1] = 2; }
}
static void Op(LONG op, int slot) { g_rq_slot = slot; g_rq_op = op; RqTick(); }
int main(void) {
    /* live #121923: one Practice boss, Despawn all -> session ended, menu mode back to 0 (first page) */
    Practice(1); assert(Nioh3Training_SessionMode() == 2);
    Op(2, 0);
    assert(Nioh3Training_SessionActive() == 0);
    assert(stops == 1 && "Despawn all must end the Training session");
    assert(Nioh3Training_SessionMode() == 0 && "menu would reopen on the Practice page");
    /* two bosses: Remove A keeps the session (B still up), Remove B (the last) ends it */
    Practice(2);
    Op(3, 0); assert(stops == 0 && Nioh3Training_SessionMode() == 2);
    Op(3, 1); assert(stops == 1 && Nioh3Training_SessionMode() == 0);
    /* Remove on an already empty slot (e.g. the boss removed for the Return gate) does not end the session */
    Practice(0); Op(3, 2); assert(stops == 0 && Nioh3Training_SessionMode() == 2);
    /* regression #122030: all 4 slots taken by earlier summons, so the session boss (last spawn) has no slot.
       Removing the 4 slot bosses keeps the session while its own boss lives; Despawn all (deletes it too) still ends it. */
    Practice(0); for (int k = 0; k < 4; ++k) { g_sp_actor[g_nsp] = 0x1000 + k; g_slot_act[k] = 0x1000 + k; g_slot_chr[k] = 10 + k; ++g_nsp; }
    g_sp_actor[g_nsp++] = 0xD000;  /* session boss, no slot */
    for (int k = 0; k < 4; ++k) Op(3, k);
    assert(stops == 0 && Nioh3Training_SessionMode() == 2 && "last slot Remove ended a session whose boss is alive");
    Op(2, 0); assert(stops == 1 && Nioh3Training_SessionMode() == 0);
    /* same, but the session boss already won and its body is gone: the last Remove ends it */
    Practice(0); g_slot_act[0] = 0x1000; g_sp_actor[g_nsp++] = 0x1000; g_tr.bodyGone = 1; Op(3, 0); assert(stops == 1);
    /* a native boss still alive keeps a Boss Match session */
    Practice(1); g_tr.practice = 0; g_slot_native[2] = 1; g_slot_act[2] = 0xC000; nativeAlive = 1;
    Op(2, 0); assert(stops == 0 && Nioh3Training_SessionMode() == 1);
    g_slot_native[2] = 0; g_slot_act[2] = 0; nativeAlive = 0;
    /* no session: Despawn all of plain summons stops nothing */
    Practice(1); g_tr.active = 0; Op(2, 0); assert(stops == 0 && Nioh3Training_SessionMode() == 0);
    /* a refused session start (load running) does not leave Practice set for the next session */
    memset(&g_tr, 0, sizeof g_tr); g_practice_next = 1; gs[2] = 1;
    assert(TrainStart("X", 300.0f) == 0 && !g_tr.active);
    assert(g_practice_next == 0 && "refused start leaked the Practice flag");
    gs[2] = 0; g_practice_next = 1; assert(TrainStart("X", 300.0f) == 1 && g_tr.practice == 1 && g_practice_next == 0);
    puts("PASS despawn-ends-session: Despawn all / last Remove end the Training session (SessionMode 0); live native boss / other boss keeps it; refused start clears Practice");
    return 0;
}
'''

with tempfile.TemporaryDirectory() as d:
    p = Path(d); (p / 't.c').write_text(pre + '\n'.join(fn) + main)
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O0', '-UNDEBUG', str(p / 't.c'), '-o', str(p / 't.exe')], check=True)
    sys.exit(subprocess.run([str(p / 't.exe')]).returncode)
