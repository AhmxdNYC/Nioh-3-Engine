"""Exercise actual cold-smoke dispatch and cover selection with native-call stubs; no game access."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
source = (root / 'src/bosslog.cpp').read_text(encoding='utf-8')
body = source[source.index('bool CoverNativeSmoke('):source.index('void RelocateExitReplay(')]
mute_start = source.index('        const double ct = g_coverT.load(std::memory_order_relaxed);', source.index('void HkFxRec('))
mute_end = source.index('\n    }\n    const double t0', mute_start)
body += '\nvoid GameSmoke(uint64_t d, uint64_t c) {\n' + source[mute_start:mute_end] + '\n}\n'
pre = r'''
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cstring>
bool guardGet=true,guardPlay=true,component=true,throwGet=false,throwPlay=false,warm=false,copyOk=true,invalidHandle=false,zeroHandle=false;
int gets=0,plays=0,checks=0;
bool t_fxReplaying=false,t_coldCover=false;
uint64_t g_base=0;
void Log(const char*,...) {}
double Now(){return 10.;}
bool FormHideCodeMatches(uint64_t rva,const uint8_t*,size_t){return rva==0x2C7E90?guardGet:guardPlay;}
uint64_t FakeGet(uint64_t player){assert(player==123);assert(t_fxReplaying&&t_coldCover);++gets;
 if(throwGet)RaiseException(0xE0000001,0,0,nullptr);return component?456:0;}
void FakePlay(uint64_t comp,int* h,uint32_t key,int bone,uint8_t flag){
 assert(comp==456&&key==0x62557&&bone==0&&flag==0);assert(t_fxReplaying&&t_coldCover);++plays;
 if(throwPlay)RaiseException(0xE0000002,0,0,nullptr);*h=invalidHandle?-1:zeroHandle?0:42;}
uint64_t Grva(uint64_t rva){return rva==0x2C7E90?(uint64_t)&FakeGet:(uint64_t)&FakePlay;}
struct FxRec {uint32_t id=0x62557;uint8_t blk[0x60]={};};
struct FxPlay {double at;FxRec r;};
constexpr int kFxRecMax=8;
FxPlay g_fxPlay[kFxRecMax];int g_fxPlayN=0;
FxRec g_frzRec[2];bool g_frzOk[2]={};
std::atomic<int> g_coverMade{0},g_coverOn{1},g_sealShowMs{400},g_plOutMs{250},g_plOutAtMs{150},g_pressSlot{1},g_fhDelayMs{400},g_showFadeMs{250};
std::atomic<double> g_coverT{0},g_sealShowAt{0},g_plOutT0{0};
std::atomic<uint64_t> g_plOutChr{0};
std::atomic<uint32_t> g_slotCore[3]{};
std::atomic<uint32_t> g_pickCore{0};
bool ScOn(){return true;}
bool Copy(void* dst,uint64_t,size_t n){if(copyOk)memset(dst,0,n);return copyOk;}
bool CoverAt(FxRec&,const float*,uint32_t){return warm;}
void check(bool condition){++checks;assert(condition);}
void reset(){guardGet=guardPlay=component=copyOk=true;throwGet=throwPlay=warm=invalidHandle=false;
 gets=plays=0;t_fxReplaying=t_coldCover=false;zeroHandle=false;g_coverMade=0;g_fxPlayN=0;g_coverOn=1;g_frzOk[0]=false;}
'''
post = r'''
int main(){
 reset();CoverStartSmoke(123,10);check(gets==1&&plays==1);check(g_coverMade==1&&g_fxPlayN==0);
 check(!t_fxReplaying&&!t_coldCover);check(g_sealShowAt>10&&g_plOutT0>10);
 reset();warm=true;CoverStartSmoke(123,10);check(gets==0&&plays==0);check(g_coverMade==1&&g_fxPlayN==1&&g_frzOk[0]);
 reset();guardGet=false;CoverStartSmoke(123,10);check(gets==0&&plays==0&&g_coverMade==0);
 reset();guardPlay=false;CoverStartSmoke(123,10);check(gets==0&&plays==0&&g_coverMade==0);
 reset();component=false;CoverStartSmoke(123,10);check(gets==1&&plays==0&&g_coverMade==0);
 reset();invalidHandle=true;CoverStartSmoke(123,10);check(plays==1&&g_coverMade==0);
 reset();zeroHandle=true;CoverStartSmoke(123,10);check(plays==1&&g_coverMade==0&&g_fxPlayN==0);
 check(!t_fxReplaying&&!t_coldCover);
 float matrix[16]={};matrix[13]=123;GameSmoke((uint64_t)matrix,0x62557);check(matrix[13]==123);
 reset();CoverStartSmoke(123,10);GameSmoke((uint64_t)matrix,0x62557);check(matrix[13]==123-100000.f);
 // A failed new press must clear a previous successful press, for both cold slots and repeats.
 reset();CoverStartSmoke(123,10);check(g_coverMade==1);zeroHandle=true;
 CoverStartSmoke(123,11);check(g_coverMade==0);g_pressSlot=2;
 CoverStartSmoke(123,12);check(g_coverMade==0&&plays==3);
 reset();throwGet=true;CoverStartSmoke(123,10);check(g_coverMade==0&&!t_fxReplaying&&!t_coldCover);
 reset();throwPlay=true;CoverStartSmoke(123,10);check(g_coverMade==0&&!t_fxReplaying&&!t_coldCover);
 reset();CoverStartSmoke(0,10);check(gets==0&&plays==0&&g_coverMade==0);
 reset();g_coverOn=0;CoverStartSmoke(123,10);check(gets==0&&plays==0&&g_coverMade==0);
 reset();t_fxReplaying=true;t_coldCover=true;check(CoverNativeSmoke(123));check(t_fxReplaying&&t_coldCover);
 printf("cold-smoke native dispatch + cover: %d checks PASS (offline; no game)\n",checks);
}
'''
out = root / 'build/cold-smoke-test'
out.mkdir(parents=True, exist_ok=True)
unit = out / 'cold-smoke.cpp'
unit.write_text(pre + body + post, encoding='utf-8')
setup = Path('C:/nioh3-engine/tools/msvc/setup_x64.bat')
exe = out / 'cold-smoke.exe'
cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "cold-smoke.obj"}" /Fe"{exe}" "{unit}"'
result = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(result.stdout, end='')
print(result.stderr, end='')
result.check_returncode()
subprocess.run([str(exe)], check=True, timeout=30)
