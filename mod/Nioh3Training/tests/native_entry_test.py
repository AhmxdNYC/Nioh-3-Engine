"""Exercise the actual native-entry state machine with mocked game observations."""
from pathlib import Path
import subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
a=s.index('static int g_native_fallback,');b=s.index('// bridge 6047: stage cell',a)
production=s[a:b].replace('__attribute__((noinline)) ', '')
preamble=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint32_t DWORD;
#define N3T_API
#define Log(...) ((void)0)
typedef struct {uint32_t mission,placement;int world;char id[16];float p[4];} ArenaRow;
static DWORD tick;static uint8_t state[3];static int ready=5,world=9,player=1,readable=1,warpOk=1;
static uint32_t running=123;static int actor,warps,fallbacks,g_cfg_enable=1,g_cfg_fn_summon=1;
static ArenaRow* g_pick;
static DWORD GetTickCount(void){return tick;}
static const uint8_t* GameState(void){return state;}
static int PlayerChar(void){return player;}
static int StageState(void){return ready;}
static int CurWorld(void){return world;}
static int NativeBossReady(const ArenaRow* r){return actor;}
static int Warp(float x,float y,float z,float yaw,int full){++warps;return warpOk;}
"""
main=r"""
static int MsRead(uint32_t* m,uint8_t* j,uint32_t* mode){*m=running;return readable;}
static int FightNowImpl(uint32_t m){assert(g_native_fallback && !g_cfg_fn_summon);assert(g_pick->mission==m);++fallbacks;return 1;}
static ArenaRow row={123,456,9,"R123",{1,2,3,4}};
static void Reset(void){memset(&g_en,0,sizeof g_en);g_en.active=1;g_en.row=&row;g_en.t0=100;
 tick=100;memset(state,0,sizeof state);ready=5;world=9;player=readable=warpOk=1;running=123;
 actor=warps=fallbacks=0;g_native_fallback=g_native_roster=0;g_cfg_enable=g_cfg_fn_summon=1;}
static void Land(void){Nioh3Training_NativeEntryResult(123,1);state[2]=1;EnterTick();state[2]=0;tick=1000;EnterTick();tick=4000;EnterTick();}
int main(void){
 Reset();assert(NativeFirst(&row));g_native_fallback=1;assert(!NativeFirst(&row));g_native_fallback=0;
 g_native_roster=1;assert(!NativeFirst(&row));g_native_roster=0;row.id[0]='S';assert(!NativeFirst(&row));row.id[0]='D';assert(NativeFirst(&row));row.id[0]='R';
 Reset();state[2]=1;EnterTick();assert(!g_en.sawLoad); // unadmitted cannot adopt a load
 Nioh3Training_NativeEntryResult(999,1);assert(!g_en.confirmed);Nioh3Training_NativeEntryResult(123,0);assert(!g_en.active);
 Reset();Nioh3Training_NativeEntryResult(123,1);tick=50000;EnterTick();assert(!warps&&!fallbacks); // no observed load
 Reset();Land();assert(warps==1&&g_en.warped);tick=33999;EnterTick();assert(!fallbacks);tick=34000;EnterTick();assert(fallbacks==1&&!g_en.active&&g_en.fellBack);EnterTick();assert(fallbacks==1&&g_cfg_fn_summon==1&&!g_native_fallback);
 Reset();Land();actor=1;tick=34000;EnterTick();assert(!fallbacks&&!g_en.active); // boss beats timeout
 Reset();Land();state[2]=1;tick=40000;EnterTick();assert(g_en.active&&!fallbacks);state[2]=0;tick=40001;EnterTick();tick=70000;EnterTick();assert(fallbacks==1); // intro doesn't cancel watch or fall back while loading
 Reset();Land();ready=4;tick=40000;EnterTick();assert(!fallbacks);ready=5;tick=40001;EnterTick();assert(!fallbacks);
 Reset();Land();player=0;tick=40000;EnterTick();assert(!fallbacks);
 Reset();Land();world=8;tick=40000;EnterTick();assert(!fallbacks);
 Reset();Land();readable=0;tick=40000;EnterTick();assert(!fallbacks);
 Reset();Land();running=999;tick=40000;EnterTick();assert(!fallbacks&&!g_en.active);
 Reset();warpOk=0;Land();assert(warps==1&&!g_en.active&&!fallbacks);
 Reset();Nioh3Training_NativeEntryResult(123,1);tick=300101;EnterTick();assert(!g_en.active&&!fallbacks);
 puts("PASS native-entry: routing, admission, ready timer, native boss, intro, wrong mission, load/player/world/read failures, warp failure, timeout, one-shot synchronous fallback");
}
"""
# Integration guards: real caller must choose native before deferred fade/cross-world routes.
entry=s[s.index('static int FightNowEntry(uint32_t mission) {'):s.index('static int XwPrewarm(')]
assert entry.index('if (NativeFirst(r))') < entry.index('if (!g_cfg_fade')
assert 'g_en.active = 0;' in entry
assert 'NativeListed' not in s and 'g_native_list' not in s
assert 'NativeMissionRow(xr) || g_cfg_xprewarm < 2' in s
br=(root.parent/'Nioh3BossRematch/src/rematch.cpp').read_text()
execbody=br[br.index('void ExecDirect('):br.index('// E (row >=')]
assert execbody.index('UnbeatenArm(')<execbody.index('CallRequestById(')
assert 'TrainingEntryAdmission entryAdmission{r.missionId}' in execbody
assert execbody.index('entryAdmission.accepted = true')>execbody.index('if (!AttemptMatch(')
assert 's.msOk && s.queueOk && !s.st[2] && g_attemptPhase.load() == aNone' in br
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'entry.c').write_text(preamble+production+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2',str(p/'entry.c'),'-o',str(p/'entry.exe')],check=True)
 subprocess.run([str(p/'entry.exe')],check=True)
