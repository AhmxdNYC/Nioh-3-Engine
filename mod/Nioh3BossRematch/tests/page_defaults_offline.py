"""Actual BR page-reset dispatch, scoped values and shipped-INI defaults; no game."""
from pathlib import Path
import subprocess, os, re
root=Path(__file__).resolve().parents[1]
repo=root.parents[1]
code=r'''#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <tuple>
#include <cassert>
#include "../../Nioh3Training/src/practice_stats.h"
#include "../../Nioh3Training/src/boss_vitals.h"
#define BR_SOULCORE 1
constexpr int uSettingFirst=5,kConfigGroupN=6,uPlayer=909,uFightOpts=908,uBossVitals=930,uBoss=940,uMoves=901,uMoveCat=915,uSoulTune=950,uRematch=902,uGate=900,uPracticeHub=906,uTools=905,uActions=2;
// Dojo staged start (#89329): Actions page resets Full climb/Final wave only for a row with staged waves.
int g_uiBoss=4,stagedAsk=-1;bool staged=false;volatile int g_stagedFinal=0;
bool RowStaged(int row,uint16_t*,uint16_t*,uint32_t*){stagedAsk=row;return staged;}
struct {int page=0;} g_uiPages;char g_msg[512];int g_bvSlot=2,g_bossSlot=3,g_moveCat=1;
namespace setting_ui {enum class Kind {Choice,Number,Manual};}
struct Def {int group;setting_ui::Kind kind;const char* initial;};
Def kConfigSettings[]={{0,setting_ui::Kind::Choice,"1"},{0,setting_ui::Kind::Manual,"ship-path"},{1,setting_ui::Kind::Number,"120"}};
constexpr int kConfigN=3;
struct Value {std::string text;bool changed=false;};struct {Value values[3];}g_configValues;
int saves=0;void SaveUiSettings(){++saves;}
std::vector<std::tuple<int,int,int,int>> calls;
bool available=true,failStat=false;int mode=2,g_stSec=7;bool waves=false;uint32_t activeChr=123;
int stats[PS_N],vitals[4];
bool PstAvailable(){return available;}int PstSet(int id,int val){calls.emplace_back(1,id,val,1);stats[id]=val;return failStat?-1:val;}
bool BvAvailable(){return available;}int BvSet(int slot,int id,int val){calls.emplace_back(2,slot,id,val);vitals[id]=val;return val;}
uint32_t SlotChr(int){return activeChr;}uint32_t MoveBoss(){return activeChr;}int GateMode(){return mode;}
bool BossFightItemsAvailable(){return available;}bool WavesGateNow(){return waves;}int SpawnedCount(){return activeChr?1:0;}
int PfzCall(int v){calls.emplace_back(13,v,0,0);return v;}
int timerSet(int ms,int idle){calls.emplace_back(14,ms,idle,0);return 0;}
void BossGone(){strcpy_s(g_msg,"gone");}
int playerSet(int v,int persist){calls.emplace_back(3,v,persist,0);return v;}
void deathSet(int a,int b,int c,int p){calls.emplace_back(4,a,b,c);assert(p==1);}
void nofinSet(int v,int p){calls.emplace_back(5,v,p,0);}
int tuneSet(int id,int v,int p){calls.emplace_back(6,id,v,p);return v;}
int lastSet(uint32_t chr,int v){calls.emplace_back(7,(int)chr,v,0);return -1;}
int stanceSet(int slot,int idle){calls.emplace_back(8,slot,idle,0);return 1;}
int aggroSet(int slot,int on){calls.emplace_back(9,slot,on,0);return on;}
int gapSet(uint32_t chr,int ms,int p){calls.emplace_back(10,(int)chr,ms,p);return ms;}
int allowSet(uint32_t chr,uint32_t id,int on,int p){calls.emplace_back(11,(int)chr,(int)id,on);assert(p==1);return 1;}
int repeatSet(uint32_t chr,uint32_t id,int on){calls.emplace_back(12,(int)chr,(int)id,on);return 1;}
void* TrainingExport(const char* s){
 if(!available)return nullptr;
 #define EXP(n,f) if(!strcmp(s,"Nioh3Training_" n))return reinterpret_cast<void*>(&f)
 EXP("SetPlayerCantDie",playerSet);EXP("SetDeathCfg",deathSet);EXP("SetNoFinisher",nofinSet);EXP("SetTuning",tuneSet);
 EXP("SetRepeatLast",lastSet);EXP("SetSlotStance",stanceSet);EXP("SetSlotAggro",aggroSet);EXP("SetRepeatGapChr",gapSet);
 EXP("PracticeFreeze",PfzCall);EXP("StanceTimer",timerSet);
 EXP("SetMoveAllowed",allowSet);EXP("SetMoveRepeat",repeatSet);
 return nullptr;
}
namespace bosslog {int forms=0,cores=0,tune=0;bool SoulCoreFormsSet(int on,void*){forms=on;return true;}void FormCoreReset(){++cores;}void SoulTuneReset(){++tune;}}
struct N3Move {uint32_t chr,id;int cat,allowed;bool repeat,sequence;};
N3Move kN3Moves[]={{123,10,1,0,true,false},{123,11,2,0,true,false},{999,12,1,0,true,false},{123,0,1,0,true,false},{123,13,1,1,false,true}};
constexpr int N3_MOVE_COUNT=5,kMcSeq=99;
bool N3MoveProtected(uint32_t id){return id==0;}int N3MoveCat(const N3Move* m){return m->cat;}
bool MoveRepeating(const N3Move& m){return m.repeat;}int MoveAllowed(const N3Move& m){return m.allowed;}
bool SeqHead(uint32_t chr,uint32_t id){for(auto&m:kN3Moves)if(m.chr==chr&&m.id==id)return m.sequence;return false;}
#include "page_defaults_menu.h"
int checks=0;void check(bool b){++checks;if(!b){printf("FAIL %d\n",checks);exit(1);}}
bool has(int a,int b,int c,int d){for(auto&v:calls)if(v==std::make_tuple(a,b,c,d))return true;return false;}
void reset(int page){g_uiPages.page=page;calls.clear();available=true;failStat=false;activeChr=123;mode=2;waves=false;for(auto&v:stats)v=777;for(auto&v:vitals)v=777;}
int main(){
 for(int page:{uPlayer,uFightOpts,uBossVitals,uBoss,uMoves,uMoveCat,uSoulTune})check(PageHasDefaults(page));
 for(int page=0;page<6;++page)check(PageHasDefaults(uSettingFirst+page));
 for(int page:{0,1,2,3,900,903,904,907})check(!PageHasDefaults(page));
 reset(0);check(!ResetPageDefaults()&&calls.empty());
 reset(uPlayer);check(ResetPageDefaults());check(has(3,1,1,0));
 for(int id:{PS_PLAYER_INF_KI,PS_PLAYER_INF_NIN,PS_PLAYER_INF_SOUL,PS_DMG_DEALT,PS_KI_DEALT,PS_PLAYER_SPEED})check(stats[id]==PstDefault(id));
 check(stats[PS_DMG_TAKEN]==777&&stats[PS_FRAME_METER]==777);check(bosslog::forms==1&&bosslog::cores==1&&bosslog::tune==0);
 reset(uSoulTune);check(ResetPageDefaults()&&bosslog::tune==1&&calls.empty());
 reset(uFightOpts);check(ResetPageDefaults());check(has(4,0,1,0)&&has(5,0,1,0));check(has(6,0,6000,1)&&has(6,1,1000,1));check(calls.size()==4);
 for(int page:{uRematch,uGate}) {reset(page);mode=1;check(PageHasDefaults(page)&&ResetPageDefaults());check(calls.size()==4&&has(4,0,1,0));}
 reset(uGate);waves=true;check(!PageHasDefaults(uGate)&&!ResetPageDefaults()&&calls.empty());
 reset(uPracticeHub);check(PageHasDefaults(uPracticeHub)&&ResetPageDefaults()&&calls.size()==1&&has(13,0,0,0));
 reset(uTools);check(PageHasDefaults(uTools)&&ResetPageDefaults()&&g_stSec==3&&calls.size()==1&&has(14,0,0,0));
 reset(uTools);activeChr=0;check(!PageHasDefaults(uTools));
 reset(uBossVitals);check(ResetPageDefaults());check(vitals[0]==0&&vitals[1]==0&&vitals[2]==100&&vitals[3]==100);
 check(stats[PS_DMG_TAKEN]==100&&stats[PS_BOSS_KI_DRAIN]==0&&stats[PS_KI_FLOOR]==100&&stats[PS_BOSS_SPEED]==100);check(stats[PS_PLAYER_INF_KI]==777);
 reset(uBoss);check(ResetPageDefaults());check(has(7,123,0,0)&&has(8,3,1,0)&&has(9,3,0,0)&&has(10,123,1000,1));check(calls.size()==4);
 reset(uBoss);mode=1;check(ResetPageDefaults()&&has(8,3,0,0));
 reset(uBoss);activeChr=0;check(!ResetPageDefaults()&&calls.empty());
 reset(uMoves);check(ResetPageDefaults()&&calls.size()==1&&has(7,123,0,0));
 reset(uMoveCat);g_moveCat=1;check(ResetPageDefaults());check(calls.size()==2&&has(11,123,10,1)&&has(12,123,10,0));
 reset(uMoveCat);g_moveCat=kMcSeq;kN3Moves[4].repeat=true;check(ResetPageDefaults()&&calls.size()==1&&has(12,123,13,0));
 reset(uActions);staged=true;g_stagedFinal=1;check(PageHasDefaults(uActions)&&stagedAsk==g_uiBoss&&ResetPageDefaults()&&g_stagedFinal==0&&calls.empty());
 reset(uActions);staged=false;g_stagedFinal=1;check(!PageHasDefaults(uActions)&&!ResetPageDefaults()&&g_stagedFinal==1);
 reset(uPlayer);failStat=true;check(!ResetPageDefaults());
 reset(uSettingFirst);for(auto&v:g_configValues.values)v={"custom",false};check(ResetPageDefaults()&&saves==1);
 check(g_configValues.values[0].text=="1"&&g_configValues.values[0].changed);check(g_configValues.values[1].text=="custom"&&!g_configValues.values[1].changed);check(g_configValues.values[2].text=="custom"&&!g_configValues.values[2].changed);
 printf("BR page defaults dispatch: %d checks PASS\n",checks);
}
'''
out=root/'build/page-defaults-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'test.cpp';unit.write_text(code)
setup=Path(os.environ.get('N3_MSVC_SETUP','C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat'));exe=out/'test.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout+r.stderr);r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
# Lock reset constants to shipped files, so updating shipping values requires updating reset behavior.
def ini_values(path):
 section='';out={}
 for line in path.read_text().splitlines():
  line=line.strip()
  if line.startswith('['):section=line[1:-1]
  elif '=' in line and not line.startswith(';'):
   key,val=line.split('=',1);out[(section,key.strip())]=val.strip()
 return out
tr=ini_values(repo/'mod/Nioh3Training/Nioh3Training.ini')
for key,value in {'PlayerCantDie':'1','BossLoot':'0','DeathBlowRequired':'1','CutsceneOnKill':'0','KiBreakNoFinisher':'0','FinisherKillTimeoutMs':'6000','KiBreakHp0WindowMs':'1000'}.items():
 assert tr['Summon',key]==value,(key,tr['Summon',key],value)
assert tr['Training','RepeatGapMs']=='1000' and tr['Training','BossAggro']=='0'
print('9 shipped defaults cross-checks PASS')

import re
br=ini_values(root/'Nioh3BossRematch.ini')
keys=['SealExitRevealMs','SealExitSmokeFwd','SealExitSmokeSide','SealExitSmokeRepeatMs','FormHideDelayMs','SealSpeed','SealTailSpeed','SmokeFreezeStart','SmokeFreezeEnd','SealExitAtLastHitMs','FormCrouchCancel']
source=(root/'src/bosslog.cpp').read_text()
fallbacks=dict(re.findall(r'GetPrivateProfileStringW\(L"Spirit", L"([^"]+)", L"([^"]+)"',source))
expected=[float(br.get(('Spirit',key),fallbacks.get(key,''))) for key in keys]
found=[]
for path,pattern in [(root/'src/bosslog.cpp',r'static const double shipped\[\] = \{([^}]+)\}'),(root/'src/soul_core_tuning_menu.h',r'kSctDefault\[kSctN\] = \{([^}]+)\}')]:
 values=[float(x) for x in re.search(pattern,path.read_text()).group(1).split(',')]
 assert values[:len(keys)]==expected,(path.name,values,expected)
 found.append(values)
assert found[0]==found[1][:len(found[0])] and len(found[1])==len(found[0])+1,(found)  # menu = shipped + Start+end combo row (#128855 throw layers, #129070)
print('20 soul-core shipped-default cross-checks PASS')
