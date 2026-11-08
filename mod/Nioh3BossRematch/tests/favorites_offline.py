"""Execute actual list builders, row mapping and favorite cursor-update branch offline."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'src/rematch.cpp').read_text()
def section(a,b): return s[s.index(a):s.index(b,s.index(a))]
maprow=section('int FavRowOfItem(', 'const char* FavMark(')
header=section('int Items(int* out, int cap)', '    if (g_uiPages.page == uRoot)')
train=section('    if (g_uiPages.page == uTrainHere) {','    if (g_uiPages.page == uTools) {')
arena=section('    if (g_uiPages.page == uTraining) {','    if (g_uiPages.page == uSummon) {')
change=section('void UiChange(int dir)', '    if (item == kItDeathCut')+'}\n'
pre=r'''#include "rematch_core.h"
#include <atomic>
#include <cassert>
#include <cstdio>
using rcore::Row;
constexpr int uRematch=1,uBosses=2,uTrainHere=3,uTraining=4;
constexpr int kItFight=0x10000,kItTrainHere=0x50000,kItTeleport=0x40000,kMaxRows=100,kItListFilter=-68,kItBack=-5;
struct {int page=uRematch;} g_uiPages;
struct {bool showAll=true,hiddenDuels=true,directStart=true;} g_cfg;
std::vector<Row> g_rows(4);std::atomic<uint8_t> g_status[4];std::atomic<bool> g_live{true};
std::set<std::string> favs;rcore::ListCycle cycle;int stop=0,g_sel=0,g_confirm=-9;
char ListFavKind(){return g_uiPages.page==uTrainHere||g_uiPages.page==uTraining?'T':'B';}
bool FavHas(char kind,const char* id){return favs.count(rcore::FavKey(kind,id))!=0;}
void EnsureListCycle(){rcore::BuildListCycle(g_rows,cycle);}
bool RowInCurrentList(const Row& r){return rcore::RowInListStop(r,cycle,stop,FavHas(ListFavKind(),r.id));}
void CycleListFilter(int){}
'''
mid=r'''
bool FavChange(int item,int dir){int row=FavRowOfItem(item);if(row<0)return false;
 auto key=rcore::FavKey(ListFavKind(),g_rows[row].id);if(dir>0)favs.insert(key);else favs.erase(key);return true;}
'''
post=r'''
int main(){
 for(int i=0;i<4;++i){snprintf(g_rows[i].id,sizeof(g_rows[i].id),"R%d",i);g_rows[i].route=rcore::kR;g_rows[i].missionId=1;g_rows[i].fightStart=true;g_status[i]=(i==0||i==2)?2:0;}
 int items[104];int checks=0;
 for(int page:{uBosses,uRematch,uTrainHere,uTraining}) {
  g_uiPages.page=page;stop=0;favs.clear();const int base=page==uTrainHere?kItTrainHere:page==uTraining?kItTeleport:0;
  int n=Items(items,104);assert(n==6&&items[0]==kItListFilter&&items[5]==kItBack);++checks;
  assert(items[1]==base&&items[2]==base+2&&items[3]==base+1&&items[4]==base+3);++checks;
  g_sel=4;UiChange(1);n=Items(items,104);
  assert(items[1]==base+3&&g_sel==1&&items[g_sel]==base+3);++checks;
  assert(items[2]==base&&items[3]==base+2&&items[4]==base+1);++checks;
  int tiny[2];assert(Items(tiny,2)==2&&tiny[1]==base+3);++checks; // sort before truncation
  UiChange(-1);Items(items,104);assert(g_sel==4&&items[4]==base+3);++checks;
  UiChange(1);stop=cycle.Stops()-1;Items(items,104);g_sel=1;UiChange(-1);
  n=Items(items,104);assert(n==2&&g_sel>=0&&g_sel<n);++checks; // removed row is no longer visible
 }
 for(int i=0;i<4;++i){g_rows[i].route=rcore::kE;strcpy_s(g_rows[i].region,"Waves");}
 for(int page:{uBosses,uRematch,uTrainHere,uTraining}) {
  g_uiPages.page=page;EnsureListCycle();stop=cycle.Stops()-2;favs.clear();favs.insert(rcore::FavKey(ListFavKind(),"R3"));
  const int base=page==uTrainHere?kItTrainHere:page==uTraining?kItTeleport:0;
  int n=Items(items,104);assert(n==6&&items[1]==base+3&&items[2]==base&&items[3]==base+1&&items[4]==base+2);++checks;
 }
 printf("favorites production builders/cursor: %d checks PASS\n",checks);
}
'''
out=root/'build/favorites-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'test.cpp';unit.write_text(pre+maprow+mid+header+train+arena+'addBossList();add(kItBack);return n;}\n'+change+post)
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'test.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout+r.stderr);r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
