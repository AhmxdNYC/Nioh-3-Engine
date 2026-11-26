"""Compile the actual picker + menu against in-memory INI/pad stubs. Never accesses the game."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
src = (root / 'src/bosslog.cpp').read_text(encoding='utf-8')
state = src[src.index('std::atomic<bool> g_scForms{true};'):src.index('std::atomic<int> g_fkSwap{0};')]
api = src[src.index('int FormCoreSeen('):src.index('bool SoulCoreForms(bool*')]
# TASK #137000: the real moved-core table (FormCoresMoved) and its two public calls
moved = src[src.index('ScCallTable g_movedTab;'):src.index('static bool FormSteerForCore()')]
moved += 'bool FormCoreMoved(uint32_t id) { return FormCoreMovedImpl(id); }\nbool FormCoreEquippedIds(uint32_t out[2]) { return FormCoreEquippedIdsImpl(out); }\n'
pre = r'''
#include <windows.h>
#include "press_queue.h"
#include "../../shared/soul_core_api.h"  // TASK #136197
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
std::map<std::wstring,std::wstring> ini;
DWORD FakeRead(const wchar_t*,const wchar_t* key,const wchar_t* def,wchar_t* out,DWORD n,const wchar_t*) {
 auto it=ini.find(key); const wchar_t* v=it==ini.end()?def:it->second.c_str();
 wcsncpy_s(out,n,v,_TRUNCATE);return (DWORD)wcslen(out);
}
BOOL FakeWrite(const wchar_t*,const wchar_t* key,const wchar_t* value,const wchar_t*) {
 if(value)ini[key]=value;else ini.erase(key);return TRUE;
}
#define GetPrivateProfileStringW FakeRead
#define WritePrivateProfileStringW FakeWrite
const wchar_t* IniPath(){return L"memory-only";}
int IniInt(const wchar_t*,const wchar_t* key,int def){auto it=ini.find(key);return it==ini.end()?def:std::stoi(it->second);}
double clockNow=10; WORD buttons=0;
double Now(){return clockNow;}
void Log(const char*,...){}
namespace addon { namespace bosslog {
double PressSnapT(){return -100;}  // #136367 stub: no press snapshot
void CorePreloadTick(){} bool CoreResReady(uint32_t){return false;}  // #136368 stubs: no game resources
void FastActorsClear(){}  // TASK #136502 stub: no yokai registry here
std::atomic<int> g_formSteer{0};  // TASK #136197: global FormSteer stub (FormCoreCallGet default)
WORD PickPadButtons(){return buttons;}
bool formActive=false;bool PickFormActive(){return formActive;}
bool ghostAlive=false;bool PickGhostAlive(){return ghostAlive;}
void PickEquipmentTick(){} // live reads are tested independently; model delayed worker completion here
'''
ui = r'''
bool SoulCoreForms(bool* hooked){if(hooked)*hooked=true;return g_scForms.load();}
bool SoulCoreFormsSet(int dir,bool* up){*up=false;bool v=dir?dir>0:!g_scForms.load();g_scForms=v;return v;}
double SoulTune(int,int,bool*){return 0;}void FccPress(double,int){}void FccPadTick(WORD,double){}
} namespace rematch {
char g_msg[512]; int g_sel=0,g_confirm=0;
constexpr uint32_t cWhite=1,cDim=2,cGray=3,cYellow=4;
struct {void Enter(int,int){}} g_uiPages;
void* TrainingExport(const char*){return nullptr;}  // human #129277 Equip core rows: no Nioh3Training.asi here
constexpr uint32_t cGreen=5;ULONGLONG Ms(){return 0;}void ToastF(uint32_t,ULONGLONG,const char*,...){}void Command(const char*,char* r,size_t n){if(n)r[0]=0;}  // human #135018 pair keys (stub was missing)
#include "soul_core_tuning_menu.h"
} }
using namespace addon::bosslog;
using namespace addon::rematch;
int checks=0;
void check(bool v){++checks;if(!v){printf("FAIL check %d\n",checks);abort();}}
void reset(bool clearIni=true) {
 formActive=false;ghostAlive=false;pressq::Clear(g_pressQ);buttons=0;clockNow+=20;PickPoll();
 if(clearIni)ini.clear();
 g_scForms=true;g_pickOff=false;g_pickDir=0;g_pickT=-100;g_pickCore=0;g_pickLearned=false;g_offN=0;
 for(auto& v:g_offIds)v=0;
 for(auto& v:g_slotCore)v=0;
 for(auto& v:g_slotSeen)v=false;
 g_equippedReady=false;for(auto& v:g_equippedCore)v=0;
 for(auto& v:g_pendingPick)v=-1;
 for(auto& v:g_seen){v.id=0;v.dir=0;}
 CoreOffLoad();CoreCallLoad();
}
void press(int dir){buttons=0;PickPoll();clockNow+=.5;buttons=dir==1?4:2;PickPoll();}
std::string row(int slot){char b[512];uint32_t c=0;SctRowText(kItScCore+slot,false,"",b,sizeof b,&c);return b;}
int main(){
 reset();g_slotCore[1]=0x5B402;g_slotCore[2]=0xA65A1;press(2);
 const double accepted=g_pickT.load();clockNow+=.133;buttons=4;PickPoll();
 check(g_pickCore==0xA65A1&&g_pickDir==2&&g_pickT==accepted);
 formActive=true;clockNow+=1;buttons=0;PickPoll();buttons=4;PickPoll();
 check(g_pickCore==0xA65A1&&g_pickDir==2&&g_pickT==accepted);
 formActive=false;press(1);check(g_pickCore==0x5B402&&g_pickDir==1);
 uint32_t ids[10];int dirs[10];
 reset();check(FormCoreSeen(ids,dirs,10)==2);check(ids[0]==0&&ids[1]==0&&dirs[0]==1&&dirs[1]==2);
 std::vector<int> rows;SctAddCoreRows([&](int r){rows.push_back(r);});check(rows.size()==2);
 check(row(0).find("LEFT seal (not used yet)")!=std::string::npos);
 check(row(1).find("DOWN seal (not used yet)")!=std::string::npos);
 check(FormCoreSeen(nullptr,dirs,10)==0&&FormCoreSeen(ids,dirs,0)==0);
 check(FormCoreSeen(ids,dirs,1)==1&&dirs[0]==1);
 check(!ScCoreAt(-1,&ids[0],&dirs[0]));
 // OFF is applied before the very first spawn, and persists even when no core has been used yet.
 SctChange(kItScCore,-1);check(!FormCorePendingTransforms(1));check(ini[L"FormCorePendingLeft"]==L"0");
 reset(false);check(!FormCorePendingTransforms(1));press(1);check(!ScOn());
 PickLearn(0x5129B);check(!FormCoreTransforms(0x5129B)&&!ScOn());
 check(g_pendingPick[1]==-1&&ini.count(L"FormCorePendingLeft")==0);
 check(ini[L"FormCoresOff"]==L"5129B"&&ini[L"FormCoreLeft"]==L"5129B");
 check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0x5129B&&ids[1]==0);
 check(row(0).find("Core 5129B")!=std::string::npos&&row(1).find("not used yet")!=std::string::npos);
 // Secondary/ambient spawns do not consume another pending choice or replace the learned ID.
 PickLearn(0xEE49E);check(g_slotCore[1]==0x5129B);
 clockNow+=4;PickLearn(0x4735);check(FormCoreSeen(ids,dirs,10)==2);
 press(2);check(ScOn());PickLearn(0xA2ACB);check(FormCoreTransforms(0xA2ACB));
 check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0x5129B&&ids[1]==0xA2ACB);
 // The same core on both directions is one row. Its setting is by ID; a different core is independent.
 press(2);PickLearn(0x5129B);check(!ScOn());
 check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0x5129B&&dirs[0]==3&&ids[1]==0xA2ACB&&dirs[1]==0);
 press(1);PickLearn(0x5B402);check(ScOn()&&FormCoreTransforms(0x5B402)&&!FormCoreTransforms(0x5129B));
 check(FormCoreSeen(ids,dirs,10)==3&&ids[0]==0x5B402&&ids[1]==0x5129B);
 SctChange(kItScCore+1,1);check(FormCoreTransforms(0x5129B));
 check(FormCoreCallGet(0x5129B)==SC_CALL_TRANSFORM&&row(1).find("moveable")==std::string::npos);  // TASK #136987: no Transform moveable choice
 // TASK #136502 / #136987: A after Transform = Fast summon: an OFF core (game's own summon) with call 3, saved in both keys
 SctChange(kItScCore+1,0);check(!FormCoreTransforms(0x5129B)&&FormCoreCallGet(0x5129B)==SC_CALL_FAST_SUMMON&&ini[L"FormCoresOff"].find(L"5129B")!=std::wstring::npos&&ini[L"FormCoreCalls"].find(L"5129B:3")!=std::wstring::npos);
 check(row(1).find("Fast summon")!=std::string::npos);
 press(2);check(!ScOn()&&FastPress());clockNow+=11;press(1);press(2);check(FastPress());  // a Fast summon press: vanilla gates + the fast flag
 CoreCallLoad();check(FormCoreCallGet(0x5129B)==SC_CALL_FAST_SUMMON);
 check(FormCoreToggle(0x5129B)&&FormCoreCallGet(0x5129B)!=SC_CALL_FAST_SUMMON&&ini[L"FormCoreCalls"].find(L"5129B")==std::wstring::npos);  // a flip ends Fast summon
 check(!FormCoreToggle(0x5129B)&&FormCoreCallGet(0x5129B)==SC_CALL_GAME);  // OFF again = plain Game's own, not a stale Fast summon
 check(FormCoreCallSet(0x5129B,SC_CALL_FAST_SUMMON)==SC_CALL_FAST_SUMMON);
 SctChange(kItScCore+1,0);check(!FormCoreTransforms(0x5129B)&&FormCoreCallGet(0x5129B)==SC_CALL_GAME&&ini[L"FormCoreCalls"].find(L"5129B")==std::wstring::npos);
 press(2);check(!ScOn()&&!FastPress());  // Game's own: no fast flag
 SctChange(kItScCore+1,-1);check(FormCoreCallGet(0x5129B)==SC_CALL_FAST_SUMMON);
 SctChange(kItScCore+1,-1);check(FormCoreCallGet(0x5129B)==SC_CALL_TRANSFORM&&FormCoreTransforms(0x5129B));
 check(FormCoreCallSet(0x5129B,SC_CALL_GAME)==SC_CALL_GAME&&!FormCoreTransforms(0x5129B));
 // same path for any id; reload keeps the calls; an unknown call is refused
 check(FormCoreCallSet(0xABCDE,SC_CALL_TRANSFORM_MOVE)==SC_CALL_TRANSFORM&&FormCoreCallSet(0xABCDE,9)==SC_CALL_TRANSFORM);  // TASK #136987: 2 = Transform
 check(ini[L"FormCoreCalls"].find(L"ABCDE:1")!=std::wstring::npos&&ini[L"FormCoreCalls"].find(L":2")==std::wstring::npos);
 ini[L"FormCoreCalls"]=L"ABCDE:2";CoreCallLoad();check(FormCoreCallGet(0xABCDE)==SC_CALL_TRANSFORM&&FormCoreCallGet(0x5129B)==SC_CALL_GAME);  // an old saved 2 loads as Transform
 g_formSteer=1;check(FormCoreCallGet(0x777)==SC_CALL_TRANSFORM);g_formSteer=0;check(FormCoreCallGet(0x777)==SC_CALL_TRANSFORM);
 FormCoreCallSet(0xABCDE,-1);
 // Cached last-session directions never masquerade as equipped/current-session cores.
 reset(false);check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0&&ids[1]==0);
 check(!FormCorePendingTransforms(2));
 // Explicit ON must transfer too, including when a newly learned core was previously OFF.
 SctChange(kItScCore+1,1);check(g_pendingPick[2]==1);
 press(2);check(ScOn());PickLearn(0x5129B);check(ScOn()&&FormCoreTransforms(0x5129B));
 check(g_pendingPick[2]==-1);
 // An explicit Right matching the default still stores intent for an unknown core.
 reset();check(FormCoreToggle(0x5B402)==false);SctChange(kItScCore,1);
 check(g_pendingPick[1]==1);press(1);PickLearn(0x5B402);check(FormCoreTransforms(0x5B402)&&ScOn());
 // Two different pending choices remain independent through load/learn order.
 reset();SctChange(kItScCore,-1);SctChange(kItScCore+1,1);reset(false);
 press(2);PickLearn(0xA2ACB);check(g_pendingPick[1]==0&&g_pendingPick[2]==-1);
 press(1);PickLearn(0x5129B);check(!FormCoreTransforms(0x5129B)&&FormCoreTransforms(0xA2ACB));
 // Global OFF still wins, timeout releases per-press suppression, empty spawn IDs are ignored.
 g_scForms=false;press(2);check(!ScOn());g_scForms=true;
 press(1);check(!ScOn());buttons=0;clockNow+=11;PickPoll();check(ScOn());
 reset();press(1);PickLearn(0);check(g_slotCore[1]==0&&!g_pickLearned);
 // Invalid stored pending values and API directions cannot mutate a seal.
 ini[L"FormCorePendingLeft"]=L"9";reset(false);check(g_pendingPick[1]==-1);
 check(FormCorePendingSet(0,false)&&FormCorePendingSet(3,false));
 // UI snapshot races with learning: a formerly unknown row changes the learned core, not a stale pending slot.
 press(1);PickLearn(0x5129B);check(!FormCorePendingSet(1,false));
 check(!FormCoreTransforms(0x5129B)&&g_pendingPick[1]==-1);
 // Bounded history, no duplicated ID, all current directions still represented when history is full.
 reset();for(uint32_t id=1;id<20;++id){press(id%2?1:2);PickLearn(id);}
 int n=FormCoreSeen(ids,dirs,10);check(n<=10&&ids[0]==19&&ids[1]==18);
 for(int i=0;i<n;++i)for(int j=i+1;j<n;++j)check(!ids[i]||ids[i]!=ids[j]);
 // Capacity failure never leaves a direction permanently overriding subsequently learned core IDs.
 reset();for(uint32_t id=1;id<=16;++id)FormCoreToggle(id);
 SctChange(kItScCore,-1);check(g_pendingPick[1]==-1&&std::string(g_msg).find("full")!=std::string::npos);
 ini[L"FormCorePendingLeft"]=L"0";reset(false);press(1);PickLearn(99);
 check(g_pendingPick[1]==-1&&g_offN==16&&FormCoreTransforms(99));
 // Publishing equipment requires no press. IDs dedupe with learned history and settings remain by core ID.
 reset();g_equippedCore[0]=0x5129B;g_equippedCore[1]=0xA2ACB;g_equippedReady=true;
 check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0x5129B&&dirs[0]==4&&ids[1]==0xA2ACB&&dirs[1]==8);
 check(row(0).find("Yang 1")!=std::string::npos&&row(1).find("Yang 2")!=std::string::npos);
 SctChange(kItScCore,-1);check(!FormCoreTransforms(0x5129B)&&FormCoreTransforms(0xA2ACB));
 press(1);PickLearn(0x5129B);check(FormCoreSeen(ids,dirs,10)==2);
 g_equippedCore[1]=0x5129B;check(FormCoreSeen(ids,dirs,10)==1&&dirs[0]==12);
 g_equippedCore[0]=0x5B402;check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0x5B402);
 g_equippedCore[0]=0;g_equippedCore[1]=0;check(FormCoreSeen(ids,dirs,10)==1&&ids[0]==0x5129B&&dirs[0]==0);
 reset();FormCorePendingSet(1,false);g_equippedCore[0]=0x5129B;g_equippedReady=true;
 check(FormCoreSeen(ids,dirs,10)==2&&ids[0]==0x5129B&&ids[1]==0&&dirs[1]==1);
 // First press uses current equipped ID, including OFF, without learning a seal first.
 reset();g_equippedCore[0]=0x5129B;g_equippedCore[1]=0xA2ACB;g_equippedReady=true;
 FormCoreToggle(0x5129B);press(1);check(g_pickCore==0x5129B&&!ScOn()&&g_slotCore[1]==0);
 press(2);check(g_pickCore==0xA2ACB&&ScOn());
 g_slotCore[1]=0x5129B;g_equippedCore[0]=0x5B402;press(1);check(g_pickCore==0x5B402&&ScOn());
 clockNow+=3.87;PickLearn(0x5B402);check(g_slotCore[1]==0x5B402&&g_pickLearned);
 press(2);clockNow+=3.87;PickLearn(0x999);check(!g_pickLearned); // late unrelated spawn cannot learn
 press(2);clockNow+=8.01;PickLearn(0xA2ACB);check(!g_pickLearned);
 press(2);clockNow+=5.49;PickLearn(0xA2ACB);check(g_pickLearned);
 g_equippedCore[0]=0;press(1);check(g_pickCore==0); // ready empty slot does not reuse stale learned ID
 g_equippedReady=false;press(1);check(g_pickCore==0x5B402); // unavailable equipment keeps learned fallback
 // Exact labels from production menu code, retained for the clearly marked offline visual preview.
 reset();printf("PREVIEW|Before any seal use|%s|%s\n",row(0).c_str(),row(1).c_str());
 SctChange(kItScCore,-1);printf("PREVIEW|LEFT game summon selected before first use|%s|%s\n",row(0).c_str(),row(1).c_str());
 press(1);PickLearn(0x5129B);press(2);PickLearn(0xA2ACB);
 printf("PREVIEW|After each seal is identified (simulated)|%s|%s\n",row(0).c_str(),row(1).c_str());
 reset();g_equippedCore[0]=0x5129B;g_equippedCore[1]=0xA2ACB;g_equippedReady=true;
 printf("PREVIEW|Equipped rows after save load (simulated, no seal press)|%s|%s\n",row(0).c_str(),row(1).c_str());
 reset();for(uint32_t id=1;id<=16;++id)FormCoreToggle(id);
 g_pendingPick[1]=0;g_pendingPick[2]=0;PendingPickSave(1);PendingPickSave(2);
 g_slotCore[1]=0x5129B;g_slotSeen[1]=true;g_pickOff=true;
 FormCoreReset();check(g_offN==0&&ini[L"FormCoresOff"].empty());
 for(uint32_t id=1;id<=16;++id)check(FormCoreTransforms(id));
 check(g_pendingPick[1]==-1&&g_pendingPick[2]==-1&&!ini.count(L"FormCorePendingLeft")&&!ini.count(L"FormCorePendingDown"));
 check(g_slotCore[1]==0x5129B&&g_slotSeen[1]&&g_pickOff); // learned identity and ongoing press retained
 // TASK #137000: a core the detection saw move is remembered (overlay TM), saved, read back after a restart; others stay T
 ini.erase(L"FormCoresMoved");g_movedLoaded=false;sc_table_clear(&g_movedTab);
 check(!FormCoreMoved(0x5129B)&&!FormCoreMoved(0));FormCoreMarkMoved(0x5129B);FormCoreMarkMoved(0x5129B);FormCoreMarkMoved(0);
 check(FormCoreMoved(0x5129B)&&!FormCoreMoved(0x5216E)&&ini[L"FormCoresMoved"]==L"5129B:1");
 FormCoreMarkMoved(0xCF875);check(ini[L"FormCoresMoved"]==L"5129B:1,CF875:1");
 g_movedLoaded=false;sc_table_clear(&g_movedTab);check(FormCoreMoved(0xCF875)&&FormCoreMoved(0x5129B)&&!FormCoreMoved(0xA2ACB));
 {uint32_t e[2]={9,9};g_equippedCore[0]=0x5129B;g_equippedCore[1]=0;g_equippedReady=true;check(FormCoreEquippedIds(e)&&e[0]==0x5129B&&e[1]==0);g_equippedReady=false;check(!FormCoreEquippedIds(e));}
 printf("form-core picker + menu: %d checks PASS (offline; no game)\n",checks);
}
'''
out = root / 'build' / 'form-core-test'
out.mkdir(parents=True, exist_ok=True)
unit = out / 'picker.cpp'
unit.write_text(pre + state + api + moved + ui, encoding='utf-8')
setup = Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
exe = out / 'picker.exe'
cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "picker.obj"}" /Fe"{exe}" "{unit}"'
result = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(result.stdout, end='')
print(result.stderr, end='')
result.check_returncode()
result = subprocess.run([str(exe)], check=True, timeout=30, capture_output=True, text=True)
print(result.stdout, end='')
(out / 'menu-preview.txt').write_text('\n'.join(line for line in result.stdout.splitlines() if line.startswith('PREVIEW|')), encoding='utf-8')
