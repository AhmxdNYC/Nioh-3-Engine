"""Run the actual last-hit tick, settings API and tuning menu with bounded fake collision memory."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
src=(root/'src/bosslog.cpp').read_text(encoding='utf-8')
tick=src[src.index('void SealEndOnHitTick()'):src.index('struct SlSnap')]
tune=src[src.index('double SoulTune('):src.index('// human #61869: Player settings')]
load=src[src.index('    const int lastHitMs = IniInt'):src.index('    g_plFadeMs = IniInt')]
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
#include <cassert>
std::map<std::wstring,std::wstring> ini;
BOOL FakeWrite(const wchar_t*,const wchar_t* key,const wchar_t* value,const wchar_t*){ini[key]=value;return TRUE;}
#define WritePrivateProfileStringW FakeWrite
int IniInt(const wchar_t*,const wchar_t* key,int d){auto i=ini.find(key);return i==ini.end()?d:std::stoi(i->second);}
const wchar_t* IniPath(){return L"memory-only";}
void Log(const char*,...){}
namespace bosslog {
struct {uint64_t owner=0,seal=0;} g_fh4;
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
bool attack=false,poolReadable=true,bodyOnly=false;
double Now(){return clockNow;} uint64_t Grva(uint64_t r){return r;}uint64_t FkPlayer(){return 123;}
void SmokeFreezeTick(){}void SealExitReplayTick(){}void FxMuteTick(){}
bool SealExitCore(uint32_t){return true;}int SealExitReplayQueue(uint64_t,uint64_t){++smokes;return 1;}
int SealHitsFor(uint32_t){return hits;}const wchar_t* SecHits(){return L"SealHitsA";}
void SealExitReplayBegin(double){}void SealExitReplayBound(double){}void CoreTplBind(uint32_t,double){}
int SealQueueDespawn(uint64_t){++despawns;return 1;}void SealDeferRelease(const char*){++releases;}
int SealExitFxSpawn(uint64_t){return 0;}void GhostHideSeal(uint64_t){++hidden;}
template<class T>bool Rd(uint64_t a,T& v){
 if(a==0x45C6F38){v=(T)0x10000;return poolReadable;}
 if(a==456){v=(T)(0x5129B<<4);return true;}
 if(a==0x20008){v=(T)456;return true;}
 if(a==0x200B0){v=(T)(bodyOnly?0:0x30000);return true;}
 if(a==0x201E8){v=(T)0xFFFFFFFF;return true;}
 return false;
}
bool Copy(void* dst,uint64_t a,size_t n){if(a!=0x10020||!poolReadable)return false;memset(dst,0,n);if(attack)((uint64_t*)dst)[0]=0x20000;return true;}
'''
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
void start(int count=2,int offset=0,bool bound=true){
 g_sehMode=0;g_ghostOn=1;g_coverOn=1;g_fh4.owner=0;SealEndOnHitTick();
 clockNow+=20;base=clockNow;g_sehKid[0]=456;g_sehKidT[0]=base;g_fh4.owner=123;g_fh4.seal=bound?456:0;hits=count;g_exitAtLastHitMs=offset;
 g_sieMode=2;g_sieFireSeal=0;g_sdHeld=0;attack=false;poolReadable=true;bodyOnly=false;
 g_ghostReq=0;g_ghostSeal=0;g_sealEndMinMs=700;smokes=despawns=hidden=releases=0;SealEndOnHitTick();
}
void step(double dt,bool active){clockNow=base+dt;attack=active;SealEndOnHitTick();}
void first(){step(.8,true);step(1.0,false);}
int main(){
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
 start(1);step(.1,true);check(!smokes);step(.71,true);check(smokes==1); // min guard still applies
 start(2,0,false);first();step(1.5,true);check(!smokes);g_fh4.seal=456;step(1.6,true);check(smokes==1); // late bind
 start(1);bodyOnly=true;step(1,true);check(!smokes);bodyOnly=false;step(1.1,true);check(smokes==1);
 start();first();g_sdHeld=456;step(1.5,true);check(smokes==1&&!releases&&!despawns); // never release/de-spawn on OPEN
 start();g_sieMode=1;first();step(1.5,true);check(!smokes);g_sieFireSeal=456;step(1.6,true);check(smokes==1);
 start();g_ghostOn=0;first();step(1.5,true);check(!smokes&&!despawns); // natural ghost-off remains off
 start();poolReadable=false;step(1.5,true);check(!smokes);poolReadable=true;step(1.6,true);check(!smokes); // no invented final burst
 start(1);g_sehMode=1;step(1,true);check(smokes==1&&!despawns); // explicit mode uses ghost too
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
