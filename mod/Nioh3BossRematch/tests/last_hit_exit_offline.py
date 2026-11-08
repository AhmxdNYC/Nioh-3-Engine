"""Run the actual last-hit tick, settings API and tuning menu with bounded fake collision memory."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
src=(root/'src/bosslog.cpp').read_text(encoding='utf-8')
tick=src[src.index('struct SealTailHitWatch'):src.index('struct SlSnap')]
tune=src[src.index('double SoulTune('):src.index('// human #61869: Player settings')]
load=src[src.index('    const int lastHitMs = IniInt'):src.index('    g_plFadeMs = IniInt')]
counts=src[src.index('const wchar_t* SecHits()'):src.index('// pc #57269',src.index('const wchar_t* SecHits()'))].replace('SealHitsFor(', 'ReadLearnedHits(')
pre=r'''
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <string>
#include <map>
#include <set>
#include <cassert>
std::map<std::wstring,std::wstring> ini;std::wstring writtenSection;std::map<std::wstring,std::wstring> legacyIni;
BOOL FakeWrite(const wchar_t* section,const wchar_t* key,const wchar_t* value,const wchar_t*){writtenSection=section;ini[key]=value;return TRUE;}
#define WritePrivateProfileStringW FakeWrite
int IniInt(const wchar_t*,const wchar_t* key,int d){auto i=ini.find(key);return i==ini.end()?d:std::stoi(i->second);}
const wchar_t* IniPath(){return L"memory-only";}
#define GetPrivateProfileIntW FakeReadProfile
UINT FakeReadProfile(const wchar_t* section,const wchar_t* key,int d,const wchar_t*){
 auto& data=(!wcscmp(section,L"SealHits")||!wcscmp(section,L"SealHitsA")||!wcscmp(section,L"SealHitsA2")||!wcscmp(section,L"SealHits2"))?legacyIni:ini;
 auto i=data.find(key);return i==data.end()?d:std::stoi(i->second);
}
void Log(const char*,...){}
namespace bosslog {
struct {uint64_t owner=0,seal=0;} g_fh4;
struct {double t=-100;} g_pressSnap;
std::atomic<uint32_t> g_pickCore{0};

std::atomic<int> g_sehMode{0},g_ghostOn{1},g_coverOn{1},g_formSid{0},g_sieMode{2},g_exitAtLastHitMs{0},g_lastHitGraceMs{50},g_exitLeadMs{600},g_sehAtk{1},g_sealEndMinMs{700},g_formMaxSec{20},g_exitSmokeAtMs{1450},g_sxfOn{0},g_ghostHideMs{0},g_revealMs{100},g_fhDelayMs{400};
std::atomic<int> g_frz[2];std::atomic<int> g_fxrRepeatMs{0};
std::atomic<bool> g_fhFaceReq{false},g_ftHooked{true};
std::atomic<uint64_t> g_tailSeal{0},g_ghostSeal{0},g_ghostFadeSeal{0},g_ghostReq{0};
std::atomic<double> g_tailT0{0},g_ghostT{0},g_ghostHideAt{0},g_ghostReqAt{0},g_fxrT0{0};
std::atomic<float> g_tailMul{4},g_sealMul{1.4f},g_tailRampMs{100},g_fxrFwd{0},g_fxrSide{0};
uint64_t g_sieFireSeal=0,g_sdHeld=0,g_base=0,g_sehKid[8]={};double g_sehKidT[8]={};
const char* g_sieWhy="idle",*g_fh4EndWhy="test-end";
int g_ghostFadeMs=0,g_sxfN=0,g_sxfOwner=0,g_sxfBone=0,g_sxfMs=0;
float g_ghostFade0=0;double clockNow=10;int hits=2,smokes=0,despawns=0,hidden=0,releases=0;
bool censusMode=false,censusBodies=false;std::map<uint64_t,uint64_t> census;
bool persistent=false,extraAttack=false;uint64_t extraOwner=456;
bool attack=false,poolReadable=true,bodyOnly=false,manualAction=false;
uint32_t action=0x4E57,coreId=0x5129B;
struct SieSample {uint32_t act=0xFFFFFFFF;};
void SieRead(uint64_t,SieSample& s){s.act=action;}
double Now(){return clockNow;} uint64_t Grva(uint64_t r){return r;}uint64_t FkPlayer(){return 123;}
void SmokeFreezeTick(){}void SealExitReplayTick(){}void FxMuteTick(){}
bool SealExitCore(uint32_t){return true;}int SealExitReplayQueue(uint64_t,uint64_t){++smokes;return 1;}
'''+counts+r'''
int SealHitsFor(uint32_t id){return hits<0?ReadLearnedHits(id):hits;}
int tailPresent=1;uint64_t ActorTableGlobal(){return 1;}int FormHideSealPresent(uint64_t,uint64_t){return tailPresent;}
void SealExitReplayBegin(double){}void SealExitReplayBound(double){}void CoreTplBind(uint32_t,double){}
int SealQueueDespawn(uint64_t){++despawns;return 1;}void SealDeferRelease(const char*){++releases;}
int SealExitFxSpawn(uint64_t){return 0;}void GhostHideSeal(uint64_t){++hidden;}
template<class T>bool Rd(uint64_t a,T& v){
 if(censusMode) for(const auto& c:census){
  if(a==c.first+8){v=(T)c.second;return true;}
  if(a==c.first+0xB0){v=(T)(censusBodies&&c.first<=0x21000?0:0x30000);return true;}
  if(a==c.first+0x1E8){v=(T)0xFFFFFFFF;return true;}
 }
 if(a==0x45C6F38){v=(T)0x10000;return poolReadable;}
 if(a==456){v=(T)(coreId<<4);return true;}
 if(a==0x21008){v=(T)extraOwner;return true;}
 if(a==0x210B0){v=(T)0x30000;return true;}
 if(a==0x211E8){v=(T)0xFFFFFFFF;return true;}
 if(a==0x20008){v=(T)456;return true;}
 if(a==0x200B0){v=(T)(bodyOnly?0:0x30000);return true;}
 if(a==0x201E8){v=(T)0xFFFFFFFF;return true;}
 return false;
}
bool Copy(void* dst,uint64_t a,size_t n){if(a!=0x10020||!poolReadable)return false;memset(dst,0,n);if(censusMode){int i=0;for(const auto& c:census)((uint64_t*)dst)[i++]=c.first;return true;}if(attack||persistent)((uint64_t*)dst)[0]=0x20000;if(extraAttack)((uint64_t*)dst)[1]=0x21000;return true;}
'''
pre += src[src.index('struct PressHitSource'):src.index('// human #61056',src.index('struct PressHitSource'))]
ui=r'''
int FormCoreSeen(uint32_t*,int*,int){return 0;}bool FormCorePendingTransforms(int){return true;}
bool FormCorePendingSet(int,bool b){return b;}bool FormCoreTransforms(uint32_t){return true;}bool FormCoreToggle(uint32_t){return false;}
bool SoulCoreFormsSet(int,bool*){return true;}bool SoulCoreForms(bool*){return true;}
void LoadExitSetting(){
'''+load+r'''}
}
char g_msg[512];int g_sel=0,g_confirm=0;constexpr uint32_t cWhite=1,cDim=2,cGray=3,cYellow=4;
struct {void Enter(int,int){}} g_uiPages;
#include "soul_core_tuning_menu.h"
using namespace bosslog;
int checks=0;double base=0;
void check(bool b){++checks;if(!b){printf("FAIL %d\n",checks);fflush(stdout);exit(1);}}
void start(int count=2,int offset=0,bool bound=true,bool atBind=false,uint32_t id=0x5129B){
 g_tailHitWatches.clear();tailPresent=1;g_sehMode=0;g_ghostOn=1;g_coverOn=1;g_fh4.owner=0;SealEndOnHitTick();
 g_pickCore=0;g_pressSnap.t=-100;g_hitSourceSnapshot=-100;g_pressHitSources.clear();clockNow+=20;base=clockNow;g_sehKid[0]=456;g_sehKidT[0]=base;g_fh4.owner=123;g_fh4.seal=bound?456:0;hits=count;g_exitAtLastHitMs=offset;
 censusMode=false;censusBodies=false;census.clear();persistent=false;extraAttack=false;extraOwner=456;g_sehKid[1]=0;g_sehKidT[1]=0;g_sieMode=2;g_sieFireSeal=0;g_sdHeld=0;attack=atBind;action=0x4E57;coreId=id;manualAction=false;poolReadable=true;bodyOnly=false;
 g_ghostReq=0;g_ghostSeal=0;g_sealEndMinMs=700;smokes=despawns=hidden=releases=0;SealEndOnHitTick();
}
void step(double dt,bool active){clockNow=base+dt;attack=active;if(!manualAction)action=0x4E59;SealEndOnHitTick();}
void first(){step(.8,true);step(1.0,false);}
int main(){
 g_pressSnap.t=1;clockNow=1.27;SealEndOnHitNoteSpawn(789);clockNow=1.38;SealEndOnHitNoteSpawn(456);
 check(g_pressHitSources.size()==2);SealEndOnHitNoteSpawn(789);check(g_pressHitSources.size()==2&&g_pressHitSources[0].at==1.27);
 for(int i=0;i<12;++i)SealEndOnHitNoteSpawn(1000+i);check(g_pressHitSources.size()==14);
 g_pressSnap.t=2;clockNow=2.1;SealEndOnHitNoteSpawn(789);check(g_pressHitSources.size()==1&&g_pressHitSources[0].at==2.1);
 for(int i=0;i<8;++i){g_sehKid[i]=0;g_sehKidT[i]=0;}
 ini.clear();LoadExitSetting();check(g_exitAtLastHitMs==0);
 ini[L"SealExitAtLastHitMs"]=L"-9999";LoadExitSetting();check(g_exitAtLastHitMs==-9999);
 ini[L"SealExitAtLastHitMs"]=L"-900";LoadExitSetting();check(g_exitAtLastHitMs==-500);
 ini[L"SealExitAtLastHitMs"]=L"9000";LoadExitSetting();check(g_exitAtLastHitMs==1000);
 start();first();check(smokes==0&&g_ghostReq==0);step(1.5,true);
 check(smokes==1&&g_ghostReq==456&&hidden==1&&despawns==0&&releases==0);
 check(std::abs(g_ghostReqAt.load()-(base+1.6))<.00001);step(1.6,true);step(1.7,false);check(smokes==1);
 start(2,200);first();step(1.5,true);check(!smokes);step(1.6,false);g_sieFireSeal=456;step(1.69,false);check(!smokes);step(1.701,false);check(smokes==1&&!despawns);
 start(2,500);first();step(1.5,true);step(1.6,false);step(1.7,true);check(!smokes);step(2.001,true);check(smokes==1); // later burst never shifts scheduled exit
 start(2);first();g_sieFireSeal=456;step(1.2,false);check(smokes==1); // missing expected bursts retain idle safety
 start(2,-500);first();check(!smokes);step(1.5,true);check(smokes==1&&!despawns);
 start(2,-9999);first();step(1.5,true);check(!smokes);step(1.7,false);check(!g_ghostReq);step(1.74,false);check(!g_ghostReq);step(1.751,false);check(g_ghostReq==456);
 start(0);first();step(1.5,true);step(1.7,false);check(!smokes&&!g_ghostReq);g_sieFireSeal=456;step(2,false);check(smokes==1&&g_ghostReq==456);
 start(1);step(.201,true);check(!smokes);step(.71,true);check(smokes==1); // min guard still applies
 start(2,0,false);first();step(1.5,true);check(!smokes);g_fh4.seal=456;step(1.6,true);check(!smokes); // late bind cannot classify earlier bursts as attacks
 manualAction=true;action=0x4E5A;step(1.7,true);check(!smokes);step(1.8,false);step(2,true);step(2.1,false);step(2.3,true);check(smokes==1);
 start(1);bodyOnly=true;step(1,true);check(!smokes);bodyOnly=false;step(1.1,true);check(smokes==1);
 start();first();g_sdHeld=456;step(1.5,true);check(smokes==1&&!releases&&!despawns); // never release/de-spawn on OPEN
 start();g_sieMode=1;first();step(1.5,true);check(!smokes);g_sieFireSeal=456;step(1.6,true);check(smokes==1);
 start();g_ghostOn=0;first();step(1.5,true);check(!smokes&&!despawns); // natural ghost-off remains off
 start();poolReadable=false;step(1.5,true);check(!smokes);poolReadable=true;step(1.6,true);check(!smokes); // no invented final burst
 start(1);g_sehMode=1;step(1,true);check(smokes==1&&!despawns); // explicit mode uses ghost too
 // #81956: unknown core learns at completed idle exit, then arms on the next use.
 ini.clear();start(-1);first();step(1.5,true);step(1.667,false);check(!ini.count(L"5129B"));
 g_sieFireSeal=456;step(3.33,false);check(ini[L"5129B"]==L"2"&&g_ghostReq==456&&writtenSection==L"SealHitsA3");
 start(-1);first();step(1.5,true);check(g_ghostReq==456&&smokes==1);check(ini[L"5129B"]==L"2");
 // Max only: a shorter completed attack cannot overwrite a higher known count.
 ini[L"5129B"]=L"3";start(-1);first();step(1.5,true);step(1.7,false);g_sieFireSeal=456;step(3.33,false);check(ini[L"5129B"]==L"3");
 // Idle signal while colliders are open, unreadable pool, no bursts, cap, and cut exits are not completion evidence.
 ini.clear();start(-1);first();g_sieFireSeal=456;step(1.5,true);check(!ini.count(L"5129B"));
 ini.clear();start(-1);first();poolReadable=false;g_sieFireSeal=456;step(3.33,false);check(!ini.count(L"5129B"));
 ini.clear();start(-1);g_sieFireSeal=456;step(3.33,false);check(!ini.count(L"5129B"));
 ini.clear();start(-1);first();step(21,false);check(!ini.count(L"5129B"));
 ini.clear();start(-1);first();g_fh4EndWhy="capture-change";g_fh4.owner=0;SealEndOnHitTick();check(!ini.count(L"5129B"));
 // Natural despawn completion and idle-only mode still learn, using the active attack-only section.
 ini.clear();start(-1);first();g_fh4EndWhy="seal-despawn";g_fh4.owner=0;SealEndOnHitTick();check(ini[L"5129B"]==L"1");g_fh4EndWhy="test-end";
 ini.clear();start(-1);g_sieMode=1;first();step(1.5,true);step(1.7,false);g_sieFireSeal=456;step(3.33,false);check(ini[L"5129B"]==L"2");
 // #81988: 60B86 bind collider is appearance. Even an old bad count of 1 cannot fire at the .7 s minimum.
 ini.clear();start(1,0,true,true,0x60B86);manualAction=true;step(.3,true);action=0x4E59;step(.53,true);step(.7,true);
 check(!smokes&&!g_ghostReq&&!ini.count(L"60B86")); // initial open interval overlaps the action change
 step(.8,false);step(1.2,true);check(smokes==1&&g_ghostReq==456); // real attack's fresh burst
 g_fh4EndWhy="seal-despawn";g_fh4.owner=0;SealEndOnHitTick();check(!ini.count(L"60B86"));g_fh4EndWhy="test-end"; // early rule cannot teach itself
 // Unknown core learns only its two post-appearance attack bursts, then next use obeys that count.
 ini.clear();start(-1,0,true,true,0x60B86);manualAction=true;step(.02,false);step(.05,true);step(.09,false); // another burst within bind window
 action=0x4E59;step(.53,false);step(1.2,true);step(1.5,false);step(2,true);step(2.2,false);
 check(!smokes&&!ini.count(L"60B86"));g_sieFireSeal=456;step(3.33,false);check(ini[L"60B86"]==L"2");
 start(-1,0,true,true,0x60B86);manualAction=true;step(.2,false);action=0x4E59;step(.53,false);step(1.2,true);check(!smokes);
 step(1.5,false);step(2,true);check(smokes==1&&ini[L"60B86"]==L"2");
 // Appear-only natural despawn and idle fallback teach nothing, including invalid/idle action transitions.
 ini.clear();start(-1,0,true,true,0x60B86);manualAction=true;step(.02,false);action=1;step(.05,true);action=0xFFFFFFFF;step(.7,true);step(.9,false);
 g_sieFireSeal=456;step(2,false);check(!ini.count(L"60B86"));
 ini.clear();start(-1,0,true,true,0x60B86);manualAction=true;step(.2,false);g_fh4EndWhy="seal-despawn";g_fh4.owner=0;SealEndOnHitTick();check(!ini.count(L"60B86"));g_fh4EndWhy="test-end";
 // #82001 exact whole-form interval: 4E57 -> 4E59 .53 -> 4E5A 1.27 -> FFFF 3.119.
 for(int count : {-1,1}) {
  ini.clear();start(count,0,true,true,0x60B86);manualAction=true;
  action=0x4E59;step(.53,true);step(.7,true);action=0x4E5A;step(1.27,true);step(3.118,true);
  check(!smokes&&!g_ghostReq&&!ini.count(L"60B86"));
  action=0xFFFF;step(3.119,false);g_fh4EndWhy="seal-despawn";g_fh4.owner=0;SealEndOnHitTick();
  check(!ini.count(L"60B86"));g_fh4EndWhy="test-end";
 }
 // Ibaraki's two ordinary attack bursts still exit at final OPEN 1.55, not CLOSE 1.67.
 ini.clear();start(2);manualAction=true;action=0xC53;first();step(1.55,true);check(smokes==1&&g_ghostReq==456);
 check(std::abs(g_ghostReqAt.load()-(base+1.65))<.00001);step(1.67,false);check(smokes==1);
 // A complete natural despawn learns the filtered count (not the appearance burst).
 ini.clear();start(-1,0,true,true,0x60B86);manualAction=true;step(.2,false);action=0x4E59;step(.53,false);step(1.2,true);step(1.5,false);
 g_fh4EndWhy="seal-despawn";g_fh4.owner=0;SealEndOnHitTick();check(ini[L"60B86"]==L"1");g_fh4EndWhy="test-end";
 // #82033: inclusive 100-ms boundary, and no action gate even when action is unreadable.
 for(double dt : {0.0,0.05,0.1,0.2}) {
  ini.clear();start(1);manualAction=true;step(dt,true);step(.71,true);check(!smokes);
  step(.8,false);step(1.1,true);check(smokes==1);
 }
 start(1);manualAction=true;action=0xFFFFFFFF;step(.201,true);step(.71,true);check(smokes==1);
 // Live regression timings: both Ibaraki bursts in the unchanged bind action C53.
 start(2);manualAction=true;action=0xC53;step(1.351,true);step(1.45,false);check(!smokes);
 step(1.634,true);check(smokes==1&&g_ghostReq==456);
 // #82077: persistent appearance must not mask later direct or already-tracked helper hit windows.
 for(uint64_t owner : {uint64_t(456),uint64_t(789)}) {
  ini.clear();start(-1,0,true,true,0x60B86);persistent=true;extraOwner=owner;
  if(owner!=456){g_sehKid[1]=owner;g_sehKidT[1]=base+.3;}
  extraAttack=true;step(.6,false);extraAttack=false;step(.9,false);
  extraAttack=true;step(1.3,false);extraAttack=false;step(1.5,false);check(!smokes);
  g_sieFireSeal=456;step(3.15,false);check(ini[L"60B86"]==L"2");
  start(-1,0,true,true,0x60B86);persistent=true;extraOwner=owner;
  if(owner!=456){g_sehKid[1]=owner;g_sehKidT[1]=base+.3;}
  extraAttack=true;step(.6,false);extraAttack=false;step(.9,false);check(!smokes);
  extraAttack=true;step(1.3,false);check(smokes==1&&g_ghostReq==456);
 }
 // A foreign collider is never promoted to owned merely by appearing near the form.
 ini.clear();start(-1,0,true,true,0x60B86);persistent=true;extraOwner=999;extraAttack=true;step(.6,false);
 extraAttack=false;step(.9,false);g_sieFireSeal=456;step(3.15,false);check(!ini.count(L"60B86"));
 // Once an excluded object disappears, its address can become a new attack window.
 start(1,0,true,true);step(.4,false);step(.8,true);check(smokes==1);
 // Live Doji final opening stays at 1.668 seconds.
 start(2);manualAction=true;action=0xC53;step(1.351,true);step(1.45,false);step(1.668,true);
 check(smokes==1&&std::abs(g_ghostReqAt.load()-(base+1.768))<.00001);
 // #82127: legacy low counts are ignored in both modes, including old built-in seeds.
 ini.clear();legacyIni[L"60B86"]=L"1";legacyIni[L"5129B"]=L"2";
 g_sehAtk=1;check(ReadLearnedHits(0x60B86)==0&&ReadLearnedHits(0x5129B)==0);
 g_sehAtk=0;check(ReadLearnedHits(0x60B86)==0&&ReadLearnedHits(0x5129B)==0);g_sehAtk=1;
 // An early current-generation count can raise itself from additional observed tail bursts.
 ini.clear();ini[L"60B86"]=L"1";start(-1,0,true,true,0x60B86);persistent=true;
 extraAttack=true;step(.533,false);step(.7,false);check(smokes==1&&g_tailHitWatches.size()==1);
 g_fh4.owner=0;extraAttack=false;step(.9,false);check(ini[L"60B86"]==L"1");
 extraAttack=true;step(1.3,false);check(ini[L"60B86"]==L"2"&&smokes==1&&writtenSection==L"SealHitsA3");
 extraAttack=false;step(1.6,false);extraAttack=true;step(2.1,false);check(ini[L"60B86"]==L"3"&&smokes==1);
 tailPresent=0;step(3.15,false);check(g_tailHitWatches.empty());
 // No extra burst: never teach a count solely because our own last-hit rule fired.
 ini.clear();start(1);step(.533,true);step(.7,true);g_fh4.owner=0;step(.9,false);tailPresent=0;step(3.15,false);
 check(!ini.count(L"5129B")&&smokes==1);
 // Unreadable pool/liveness never manufactures a close/reopen; separate observers survive a new form.
 ini.clear();ini[L"60B86"]=L"1";start(-1,0,true,true,0x60B86);persistent=true;extraAttack=true;step(.533,false);step(.7,false);
 poolReadable=false;step(.9,false);poolReadable=true;step(1.1,false);check(ini[L"60B86"]==L"1");
 g_fh4.owner=0;extraAttack=false;step(1.2,false);g_fh4.owner=123;g_fh4.seal=777;extraAttack=true;step(1.4,false);
 check(ini[L"60B86"]==L"2"&&smokes==1);
 // Max-only and stored section survive live mode changes; unknown liveness cannot invent edges.
 ini.clear();ini[L"60B86"]=L"1";start(-1,0,true,true,0x60B86);persistent=true;extraAttack=true;step(.533,false);step(.7,false);
 ini[L"60B86"]=L"5";g_fh4.owner=0;extraAttack=false;step(.9,false);extraAttack=true;step(1.2,false);check(ini[L"60B86"]==L"5");
 tailPresent=-1;extraAttack=false;step(1.4,false);tailPresent=1;extraAttack=true;step(1.6,false);check(ini[L"60B86"]==L"5");
 extraAttack=false;step(1.8,false);ini[L"60B86"]=L"2";g_sehAtk=0;extraAttack=true;step(2.1,false);
 check(ini[L"60B86"]==L"3"&&writtenSection==L"SealHitsA3");g_sehAtk=1;
 step(31.0,false);check(g_tailHitWatches.empty());
 // Doji known corrected count still exits on the second live opening at 1.517 s.
 ini.clear();ini[L"5129B"]=L"2";start(-1);first();step(1.517,true);
 check(smokes==1&&std::abs(g_ghostReqAt.load()-(base+1.617))<.00001);
 // #82179 exact reported census: bind 1, second appearance .084/.100/.117,
 // same objects through action change .533; NEW real-hit objects at 1.276 and 1.310, one remains 1.342..3.126.
 for(double second : {.084,.100,.117}) {
  ini.clear();start(-1,0,true,true,0x60B86);censusMode=true;census={{0x20000,456},{0x21000,456}};
  step(second,false);censusBodies=true;step(.3,false);censusBodies=false;step(.533,false);step(.7,false);check(!smokes);
  census[0x22000]=456;step(1.276,false);census[0x23000]=456;step(1.310,false);check(!smokes);
  census={{0x20000,456}};step(1.342,false);step(3.125,false);census.clear();step(3.126,false);
  g_fh4EndWhy="seal-despawn";g_fh4.owner=0;SealEndOnHitTick();check(ini[L"60B86"]==L"1");g_fh4EndWhy="test-end";
  start(-1,0,true,true,0x60B86);censusMode=true;census={{0x20000,456},{0x21000,456}};step(second,false);
  step(.533,false);step(.7,false);check(!smokes);census[0x22000]=456;step(1.276,false);
  check(smokes==1&&std::abs(g_ghostReqAt.load()-(base+1.376))<.00001);
  census[0x23000]=456;step(1.310,false);check(ini[L"60B86"]==L"1"&&smokes==1); // same cluster in tail
 }
 // A new cluster can open while older hit objects persist. Appearance owner flips do not reclassify it.
 ini.clear();start(2,0,true,true);censusMode=true;census={{0x20000,456}};step(.2,false);
 census[0x20000]=789;g_sehKid[1]=789;g_sehKidT[1]=base;step(.533,false);check(!smokes);
 census[0x22000]=456;step(1.0,false);census[0x23000]=456;step(1.066,false);check(!smokes);
 census[0x24000]=456;step(1.517,false);check(smokes==1&&std::abs(g_ghostReqAt.load()-(base+1.617))<.00001);
 // Corrected tail observer also counts new-object clusters while the old attack remains open.
 ini.clear();start(1);censusMode=true;census={{0x22000,456}};step(.7,false);g_fh4.owner=0;step(.8,false);
 census[0x23000]=456;step(1.2,false);check(ini[L"5129B"]==L"2");census[0x24000]=456;step(1.25,false);check(ini[L"5129B"]==L"2");
 // #82289: first actor precedes observer start; second actor is the visible bound seal.
 ini.clear();start(-1,0,false,false,0xC17E3);
 g_fh4.owner=0;SealEndOnHitTick();clockNow=base+.4;g_pressSnap.t=base;g_pickCore=0x16127;
 g_sehKid[0]=789;g_sehKidT[0]=base+.27;g_sehKid[1]=456;g_sehKidT[1]=base+.38;
 g_hitSourceSnapshot=base;g_pressHitSources={{789,base+.27},{456,base+.38}};
 g_fh4.owner=123;g_fh4.seal=456;censusMode=true;census={{0x20000,789}};SealEndOnHitTick();
 step(.61,false);check(!smokes);census[0x22000]=789;step(1.28,false);check(!smokes);
 census.clear();step(1.5,false);g_sieFireSeal=456;step(2,false);
 check(ini[L"16127"]==L"1"&&!ini.count(L"C17E3")&&g_ghostReq==456);
 // Tail identity is bound actor C17E3, while learning stays under pressed 16127.
 g_fh4.owner=0;step(2.1,false);census[0x23000]=789;step(2.4,false);
 check(ini[L"16127"]==L"2"&&!g_tailHitWatches.empty());
 // A stale actor from before the snapshot cannot supply a hit; >8 sources remain eligible.
 start(1,0,false);g_fh4.owner=0;SealEndOnHitTick();g_pressSnap.t=base;g_pickCore=0x16127;
 g_sehKid[0]=789;g_sehKidT[0]=base-.1;g_sehKid[1]=0;g_hitSourceSnapshot=base;
 for(int i=0;i<12;++i)g_pressHitSources.push_back({1000u+(uint64_t)i,base+.1});
 clockNow=base+.4;g_fh4.owner=123;g_fh4.seal=456;SealEndOnHitTick();
 censusMode=true;census={{0x22000,789}};step(1.0,false);check(!smokes);
 census[0x23000]=1011;step(1.3,false);check(smokes==1);
 bool live=false;g_exitAtLastHitMs=0;g_ftHooked=false;check(SoulTune(9,0,&live)==0&&live);
 check(SoulTune(9,1,&live)==50&&ini[L"SealExitAtLastHitMs"]==L"50");
 check(SoulTune(9,-1,&live)==0);g_exitAtLastHitMs=-500;check(SoulTune(9,-1,&live)==-500);
 g_exitAtLastHitMs=1000;check(SoulTune(9,1,&live)==1000);g_exitAtLastHitMs=-9999;check(SoulTune(9,1,&live)==50);
 std::vector<int> rows;SctAddPage([&](int i){rows.push_back(i);});check(rows.size()==10&&rows.back()==kItSctBase+9);
 char text[512];uint32_t color=0;g_exitAtLastHitMs=0;SctRowText(kItSctBase+9,true,">",text,sizeof text,&color);
 check(strstr(text,"Exit after last hit starts:")&&strstr(text,"0 ms"));printf("PREVIEW|%s\n",text);
 SctChange(kItSctBase+9,1);check(g_exitAtLastHitMs==50&&ini[L"SealExitAtLastHitMs"]==L"50");
 SctChange(kItSctBase+9,-1);check(g_exitAtLastHitMs==0);check(strstr(SctFooter(kItSctBase+9),"Unknown counts"));
 g_exitAtLastHitMs=-9999;SctRowText(kItSctBase+9,false,"",text,sizeof text,&color);check(strstr(text,"Legacy close"));
 g_frz[0]=1;SctRowText(kItSctBase+7,false,"",text,sizeof text,&color);check(strstr(text,"ON"));
 printf("last-hit tick, ghost safety, settings and menu: %d checks PASS (offline)\n",checks);
}
'''
out=root/'build/last-hit-test';out.mkdir(parents=True,exist_ok=True);unit=out/'last-hit.cpp';unit.write_text(pre+tick+tune+ui,encoding='utf-8')
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'last-hit.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "last-hit.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
