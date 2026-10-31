"""Compile actual stance-timer/default function bodies with synthetic session state."""
from pathlib import Path
import subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
def body(name):
    start=s.index('static void '+name+'(void) {')
    end=start+s[start:].index('{')+1
    depth=1
    while depth:
        depth += (s[end]=='{')-(s[end]=='}')
        end+=1
    return s[start:end]
fixture=r'''
#include <windows.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
static struct { int active,practice; } g_tr;
static volatile LONG g_st_due,g_st_idle,g_st_armed,g_st_bmgen,g_bm_gen;
static int MvBossMatch(void) { return g_tr.active && !g_tr.practice; }
static int g_bmd_on,g_bmd_loot,g_bmd_nofin,g_cfg_loot,g_cfg_nofin,changes;
#define N3T_SLOTS 2
#define Log(...) ((void)0)
static void BossMatchRestore(const char* why) { (void)why; }
static int SlotAlive(int s) { (void)s;return 1; }
static int Nioh3Training_SetSlotStance(int s,int idle) { (void)s;(void)idle;++changes;return 1; }
'''+body('BossMatchDefaults')+'\n'+body('StanceTimerTick')+r'''
int main(void) {
 g_tr.active=1;g_st_armed=1;g_st_due=0;g_st_idle=1;
 BossMatchDefaults();assert(!g_st_armed);
 g_st_armed=1;StanceTimerTick();assert(!g_st_armed && !changes);
 g_tr.practice=1;StanceTimerTick();assert(!changes);
 g_st_armed=1;StanceTimerTick();assert(changes==2 && !g_st_armed);
 g_tr.practice=0;BossMatchDefaults();g_st_bmgen=g_bm_gen;g_st_armed=1;StanceTimerTick();assert(changes==4 && !g_st_armed); /* #59523 armed in the match fires */
 BossMatchDefaults();g_st_armed=1;StanceTimerTick();assert(changes==4 && !g_st_armed); /* next match: old one cancelled */
 puts("PASS: match entry cancels old timer; match tick cancels new timer; Training timer still fires");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'timer.c').write_text(fixture)
    subprocess.run([sys.executable,'-m','ziglang','cc','-O2',str(p/'timer.c'),'-o',str(p/'timer.exe')],check=True)
    subprocess.run([str(p/'timer.exe')],check=True)
