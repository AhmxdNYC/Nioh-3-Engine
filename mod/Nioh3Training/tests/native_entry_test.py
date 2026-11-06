"""Exercise the actual native-entry state machine with mocked game observations."""
from pathlib import Path
import subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
a=s.index('static int g_native_fallback,');b=s.index('// bridge 6047: stage cell',a)
production=s[a:s.index('// Exact placement and spawn-key identity;',a)]+s[s.index('static void EnterTick(void) {',a):b]
production=production.replace('__attribute__((noinline)) ', '')
preamble=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
typedef unsigned long DWORD;
#define N3T_API
static char lastLog[512];
#define Log(...) snprintf(lastLog,sizeof lastLog,__VA_ARGS__)
typedef struct {uint32_t mission,placement;int world;char id[16];float p[4];float b[3];} ArenaRow;
typedef struct {uint32_t box;float target[4];} NativeEntryBox;
static NativeEntryBox testBox={111,{20,30,40,50}};static int boxEntry;
static const NativeEntryBox* NativeEntryBoxFor(const ArenaRow*r){return boxEntry?&testBox:0;}
static DWORD tick;static uint8_t state[3];static int ready=5,world=9,player=1,readable=1,warpOk=1;
static uint32_t running=123;static int actor,warps,fallbacks,g_cfg_enable=1,g_cfg_fn_summon=1,g_cfg_bossdist=900;
static ArenaRow* g_pick;
static int g_nsp; static uint64_t g_sp_actor[1];
static int ActorHp(uint64_t a,uint64_t* hp,uint64_t* maxhp){*hp=*maxhp=0;return 0;}
static int placed;static float pos[3]={7,100,7};
static uint64_t ActorByHandle(uint32_t h){return placed;}
static int PlayerPos(float o[4]){o[0]=pos[0];o[1]=pos[1];o[2]=pos[2];o[3]=0;return 1;}
static void NativeRosterTick(DWORD now) {}
static void NativePhaseTick(DWORD now) {}
static struct {int started;} g_np;
static DWORD GetTickCount(void){return tick;}
static const uint8_t* GameState(void){return state;}
static int PlayerChar(void){return player;}
static int StageState(void){return ready;}
static int CurWorld(void){return world;}
static uint32_t wavePlacement;
static uint32_t NativeEntryWaveLive(const ArenaRow*r){return wavePlacement;}
static uint32_t NativeEntryPlacement(const ArenaRow* r){return (actor || placed) ? 789 : 0;}
static int NativeBossReady(const ArenaRow* r,uint32_t p){assert(p==789);return actor;}
static float lastWarp[3];
static int Warp(float x,float y,float z,float yaw,int full){++warps;lastWarp[0]=x;lastWarp[1]=y;lastWarp[2]=z;if(warpOk){pos[0]=x;pos[1]=y;pos[2]=z;}return warpOk;}
"""
main=r"""
static int MsRead(uint32_t* m,uint8_t* j,uint32_t* mode){*m=running;return readable;}
static int FightNowImpl(uint32_t m){assert(g_native_fallback && !g_cfg_fn_summon);assert(g_pick->mission==m);++fallbacks;return 1;}
static ArenaRow row={123,456,9,"R123",{1,2,3,4},{5000,2,3}};
static void Reset(void){memset(&g_en,0,sizeof g_en);g_en.active=1;g_en.row=&row;g_en.t0=100;
 tick=100;memset(state,0,sizeof state);ready=5;world=9;player=readable=warpOk=1;running=123;
 boxEntry=0;actor=warps=fallbacks=placed=0;pos[0]=7;pos[1]=100;pos[2]=7;g_native_fallback=g_native_roster=0;g_cfg_enable=g_cfg_fn_summon=1;}
static void Land(void){Nioh3Training_NativeEntryResult(123,1);state[2]=1;EnterTick();state[2]=0;tick=1000;EnterTick();tick=4000;EnterTick();}
int main(void){
 Reset();assert(NativeFirst(&row));g_native_fallback=1;assert(!NativeFirst(&row));g_native_fallback=0;
 g_native_roster=1;assert(!NativeFirst(&row));g_native_roster=0;row.id[0]='S';assert(!NativeFirst(&row));row.id[0]='D';assert(NativeFirst(&row));row.id[0]='R';
 Reset();state[2]=1;EnterTick();assert(!g_en.sawLoad); // unadmitted cannot adopt a load
 Nioh3Training_NativeEntryResult(999,1);assert(!g_en.confirmed);Nioh3Training_NativeEntryResult(123,0);assert(!g_en.active);
 Reset();Nioh3Training_NativeEntryResult(123,1);tick=50000;EnterTick();assert(!warps&&!fallbacks); // no observed load
 Reset();Land();assert(warps==1&&g_en.warped);tick=13999;EnterTick();assert(warps==1);tick=14000;EnterTick();assert(warps==2&&g_en.moved&&!fallbacks);tick=28999;EnterTick();assert(!fallbacks);tick=29000;EnterTick();assert(fallbacks==1&&!g_en.active&&g_en.fellBack);EnterTick();assert(fallbacks==1&&g_cfg_fn_summon==1&&!g_native_fallback);
 Reset();Land();actor=1;tick=34000;EnterTick();assert(!fallbacks&&!g_en.active); // boss beats timeout
 Reset();Land();state[2]=1;tick=40000;EnterTick();assert(g_en.active&&!fallbacks);state[2]=0;tick=40001;EnterTick();assert(!fallbacks);tick=50000;EnterTick();assert(warps==2&&!fallbacks);tick=65000;EnterTick();assert(fallbacks==1); // intro doesn't cancel watch or fall back while loading
 Reset();Land();ready=4;tick=40000;EnterTick();assert(!fallbacks);ready=5;tick=40001;EnterTick();assert(!fallbacks);
 Reset();Land();player=0;tick=40000;EnterTick();assert(!fallbacks);
 Reset();Land();world=8;tick=40000;EnterTick();assert(!fallbacks);
 Reset();Land();readable=0;tick=40000;EnterTick();assert(!fallbacks);
 Reset();Land();running=999;tick=40000;EnterTick();assert(!fallbacks&&!g_en.active);
 Reset();warpOk=0;Land();assert(warps==1&&!g_en.active&&!fallbacks);
 Reset();Nioh3Training_NativeEntryResult(123,1);tick=300101;EnterTick();assert(!g_en.active&&!fallbacks);
 Reset();Land();tick=14000;EnterTick();actor=1;tick=20000;EnterTick();assert(!fallbacks&&!g_en.active&&g_en.route==2); // boss appears after the near move: native, no fallback
 Reset();Land();warpOk=0;tick=14000;EnterTick();assert(!g_en.moved&&!fallbacks);tick=33999;EnterTick();assert(!fallbacks);tick=34000;EnterTick();assert(fallbacks==1); // failed move keeps the 30 s wait
 row.b[0]=100;Reset();Land();tick=14000;EnterTick();assert(warps==1&&!g_en.moved);tick=33999;EnterTick();assert(!fallbacks);tick=34000;EnterTick();assert(fallbacks==1);row.b[0]=5000; // already close: no move
 Reset();Land();placed=1;tick=4300;EnterTick();assert(!g_en.active&&g_en.route==2&&g_en.dormant&&warps==1);tick=40000;EnterTick();assert(warps==1&&!fallbacks); // dormant teacher: no move, no fallback, no delete
 Reset();Land();assert(warps==1);tick=4300;EnterTick();pos[1]=-400;tick=4600;EnterTick();assert(warps==1&&!g_en.fell);tick=4900;EnterTick();assert(warps==1&&!g_en.fell); // script teleport lower: one drop is not a fall
 Reset();Land();tick=4300;EnterTick();pos[1]=-400;tick=4600;EnterTick();pos[1]=-800;tick=4900;EnterTick();assert(g_en.fell&&warps==2&&lastWarp[1]==100);assert(g_en.active&&!fallbacks); // fall from the row's spot returns to the mission spawn
 Reset();Land();tick=4300;EnterTick();tick=4600;EnterTick();tick=5600;EnterTick();assert(g_en.safe[1]==2); // 1 s standing on the warp target makes it the solid spot
 pos[1]=-500;tick=5900;EnterTick();pos[1]=-900;tick=6200;EnterTick();assert(g_en.fell&&lastWarp[1]==2);
 Reset();g_np.started=1;Land();placed=1;tick=4300;EnterTick();assert(strstr(lastLog,"native boss placement")&&strstr(lastLog,"request-to-boss")&&!strstr(lastLog,"script-dormant"));
 Reset();g_np.started=0;Land();placed=1;tick=4300;EnterTick();assert(strstr(lastLog,"script-dormant"));
 Reset();boxEntry=1;Land();assert(warps==1&&lastWarp[0]==20&&lastWarp[1]==30&&g_en.nudged);tick=14000;EnterTick();assert(warps==1&&!fallbacks);tick=34000;EnterTick();assert(fallbacks==1); // leave GatherWarp and temp guard to script; no near-boss move
 Reset();Land();wavePlacement=321;tick=4300;EnterTick();assert(!g_en.active&&g_en.route==2&&!fallbacks&&strstr(lastLog,"native wave member"));
 wavePlacement=0;tick=200000;EnterTick();assert(!fallbacks); // scripted gap after wave1 never causes replacement
 puts("PASS native-entry: near-boss move, routing, admission, ready timer, native boss, intro, wrong mission, load/player/world/read failures, warp failure, timeout, one-shot synchronous fallback");
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
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(p/'entry.c'),'-o',str(p/'entry.exe')],check=True)
 subprocess.run([str(p/'entry.exe')],check=True)
