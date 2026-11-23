"""Execute actual list builders, row mapping and favorite cursor-update branch offline."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'src/rematch.cpp').read_text()
def section(a,b): return s[s.index(a):s.index(b,s.index(a))]
maprow=section('int FavRowOfItem(', 'const char* FavMark(')
header=section('int ItemsBase(int* out, int cap)', '    if (g_uiPages.page == uRoot)').replace('int ItemsBase(', 'int Items(')
train=section('    if (g_uiPages.page == uTrainHere) {','    if (g_uiPages.page == uTools) {')
arena=section('    if (g_uiPages.page == uTraining) {','    if (g_uiPages.page == uSummon) {')
change=section('void UiChange(int dir)', '    if (item == kItDeathCut')+'}\n'
pre=r'''#include "rematch_core.h"
#include <atomic>
#include <cassert>
#include <cstdio>
using rcore::Row;
constexpr int uRematch=1,uBosses=2,uTrainHere=3,uTraining=4;
constexpr int kItFight=0x10000,kItTrainHere=0x50000,kItTeleport=0x40000,kMaxRows=100,kItListFilter=-68,kItBack=-5,kItPageDefaults=-69;
struct {int page=uRematch;} g_uiPages;
struct {bool showAll=true,hiddenDuels=true,directStart=true;} g_cfg;
std::vector<Row> g_rows(4);std::atomic<uint8_t> g_status[4];std::atomic<bool> g_live{true};
std::set<std::string> favs;rcore::ListCycle g_listCycle;rcore::ListCycle& cycle=g_listCycle;int stop=0,g_sel=0,g_confirm=-9;
char ListFavKind(){return g_uiPages.page==uTrainHere||g_uiPages.page==uTraining?'T':'B';}
bool FavHas(char kind,const char* id){return favs.count(rcore::FavKey(kind,id))!=0;}
int& ListStopRef(){return stop;}
void EnsureListCycle(){rcore::BuildListCycle(g_rows,g_listCycle);}
bool RowInCurrentList(const Row& r){return rcore::RowInListStop(r,cycle,stop,FavHas(ListFavKind(),r.id));}
constexpr int kItStagedStart=-70;bool g_stagedFinal=false;char g_msg[4]={};void Log(const char*,...){}
void CycleListFilter(int){}
uint64_t g_now=0,g_step=1000;uint64_t NavNowMs(){return g_now+=g_step;}
'''
mid=r'''
bool FavChange(int item,int dir){int row=FavRowOfItem(item);if(row<0)return false;
 auto key=rcore::FavKey(ListFavKind(),g_rows[row].id);if(dir>0)favs.insert(key);else favs.erase(key);return true;}
'''
post=r'''
int main(){
 for(int i=0;i<4;++i){snprintf(g_rows[i].id,sizeof(g_rows[i].id),"R%d",i);g_rows[i].route=rcore::kR;g_rows[i].missionId=1;g_rows[i].fightStart=true;g_status[i]=(i==0||i==2)?2:0;}
 // #95418/#96693: one case-insensitive A-Z run by name (beaten rows no longer first). A-Z = R1 Alpha, R3 bravo, R2 Charlie, R0 delta.
 {const char* nm[4]={"delta","Alpha","Charlie","bravo"};for(int i=0;i<4;++i)strcpy_s(g_rows[i].name,nm[i]);}
 int items[104];int checks=0;
 for(int page:{uBosses,uRematch,uTrainHere,uTraining}) {
  g_uiPages.page=page;stop=0;favs.clear();const int base=page==uTrainHere?kItTrainHere:page==uTraining?kItTeleport:0;
  int n=Items(items,104);assert(n==6&&items[0]==kItListFilter&&items[5]==kItBack);++checks;
  assert(items[1]==base+1&&items[2]==base+3&&items[3]==base+2&&items[4]==base);++checks; // A-Z, status ignored
  g_sel=4;UiChange(1);n=Items(items,104);
  assert(items[1]==base&&g_sel==1&&items[g_sel]==base);++checks; // favorite first
  assert(items[2]==base+1&&items[3]==base+3&&items[4]==base+2);++checks; // rest A-Z
  int tiny[2];assert(Items(tiny,2)==2&&tiny[1]==base);++checks; // sort before truncation
  UiChange(-1);Items(items,104);assert(g_sel==4&&items[4]==base);++checks;
  UiChange(1);stop=cycle.Stops()-1;Items(items,104);g_sel=1;UiChange(-1);
  n=Items(items,104);assert(n==2&&g_sel>=0&&g_sel<n);++checks; // removed row is no longer visible
 }
 { // #89941: held Left in the Favorites filter (press, 400 ms, then 90 ms repeats) removes one favorite, not all of them
  g_uiPages.page=uTrainHere;EnsureListCycle();favs.clear();for(const char* id:{"R0","R1","R2"})favs.insert(rcore::FavKey('T',id));stop=cycle.Stops()-1;
  int n=Items(items,104);assert(n==5);++checks;g_sel=1;
  g_step=1000;UiChange(-1);g_step=400;UiChange(-1);g_step=90;for(int k=0;k<20;++k)UiChange(-1);
  assert(favs.size()==2);++checks;
  n=Items(items,104);assert(n==4&&g_sel>=1&&g_sel<=2);++checks;
  g_step=1000;UiChange(-1);assert(favs.size()==1);++checks; // a fresh press after release still works
  g_step=1000;g_sel=1;UiChange(1);g_step=400;UiChange(1);assert(favs.size()==1);++checks; // held Right on a kept row is harmless
 }
 for(int i=0;i<4;++i){g_rows[i].route=rcore::kE;strcpy_s(g_rows[i].region,"Waves");}
 for(int page:{uBosses,uRematch,uTrainHere,uTraining}) {
  g_uiPages.page=page;EnsureListCycle();stop=cycle.Stops()-2;favs.clear();favs.insert(rcore::FavKey(ListFavKind(),"R3"));
  const int base=page==uTrainHere?kItTrainHere:page==uTraining?kItTeleport:0;
  int n=Items(items,104);assert(n==6&&items[1]==base+3&&items[2]==base+1&&items[3]==base+2&&items[4]==base);++checks; // favorite R3, then A-Z
 }
 printf("favorites production builders/cursor: %d checks PASS\n",checks);
}
'''
out=root/'build/favorites-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'test.cpp';unit.write_text(pre+maprow+mid+header+train+arena+'addBossList();add(kItBack);return n;}\n'+change+post)
import os
setup=next((Path(c) for c in [os.environ.get('MSVC_SETUP',''),'C:/nioh3-engine/tools/msvc/setup_x64.bat','C:/Users/User/pmsvc/msvc/setup_x64.bat'] if c and Path(c).exists()),None);assert setup,'MSVC setup_x64.bat not found (set MSVC_SETUP)'
exe=out/'test.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout+r.stderr);r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
