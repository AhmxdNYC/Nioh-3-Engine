"""Exercise production primary selection, capture promotion, ghost lifetime and rate hooks."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
s=(root/'src/bosslog.cpp').read_text()
def between(a,b): return s[s.index(a):s.index(b,s.index(a))]
pre=r"""
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
std::map<uint64_t,std::vector<unsigned char>> mem;
template<class T>void put(uint64_t a,T x){auto& b=mem[a];b.resize(sizeof(x));memcpy(b.data(),&x,sizeof(x));}
template<class T>bool Rd(uint64_t a,T& x){auto i=mem.find(a);if(i==mem.end()||i->second.size()!=sizeof(x))return false;memcpy(&x,i->second.data(),sizeof(x));return true;}
bool WrBytes(uint64_t a,const void* x,size_t n){auto& b=mem[a];b.resize(n);memcpy(b.data(),x,n);return true;}
double clockNow=10;double Now(){return clockNow;}uint64_t FkPlayer(){return 123;}
void Log(const char*,...){}
struct {double t=10;} g_pressSnap;std::atomic<double> g_pickT{10};std::atomic<uint32_t> g_pickCore{0x16127};
std::set<uint64_t> oldActors,dead;bool AliveAtPress(uint64_t a){return oldActors.count(a)!=0;}
uint64_t ActorTableGlobal(){return 1;}int FormHideSealPresent(uint64_t,uint64_t a){return dead.count(a)?0:1;}
SRWLOCK g_fh4Lock=SRWLOCK_INIT;
struct {uint64_t owner=123,seal=0;double capT=0;bool keepSeal=false;} g_fh4;
std::atomic<uint64_t> g_ghostReq{0},g_ghostSeal{0},g_ghostFadeSeal{0},g_tailSeal{0};
std::atomic<double> g_ghostHideAt{0},g_ghostT{0},g_tailT0{0};
std::atomic<int> g_ghostHideMs{0};int g_ghostFadeMs=250;float g_ghostFade0=1;
std::atomic<float> g_tailMul{4},g_sealMul{1.4f},g_tailRampMs{250};
bool enabled=true;bool ScOn(){return enabled;}int binds=0;void FormHideEndBegin(uint64_t,uint64_t){++binds;}
"""
code=pre+between('struct PressHitSource','void SealEndOnHitNoteSpawn')+between('struct PressGhost','void Fh4KeepSeal')+between('float SealRateMul(','std::atomic<uint32_t> g_ftActs')+between('void PressGhostBegin(const std::vector<uint64_t>& actors, bool smoke) {','struct SealTailHitWatch')
code+='void adopt(uint64_t& cap,double& capT){uint64_t player=123;'+between('    double primaryAt = 0;\n    const uint64_t primary = PressPrimarySeal(&primaryAt);','    if (g_fh4.owner && (g_fh4.fromSpawn')+'}\n'
code+=r"""
struct FxRec {bool world=true;uint32_t id=1;double t=0;float blk[16]={},pl[4]={};uint64_t owner=0,a7=0;};
struct FxPlay {double at;FxRec r;};
constexpr int kFxRecMax=4;FxRec g_fxRec[kFxRecMax],g_frzRec[2];int g_fxRecN=1,g_fxPlayN=0;
FxPlay g_fxPlay[kFxRecMax];bool g_frzOk[2]={};SRWLOCK g_fxRecLock=SRWLOCK_INIT;
std::atomic<int> g_fxrOn{1},g_fxrRepeatMs{0},g_formSid{0};std::atomic<double> g_fxrT0{0};
int g_fxrCtrN=0;bool g_fxrBone=false;
bool Copy(void* out,uint64_t a,size_t n){if(n!=16)return false;float pos[4]={(float)a,0,0,0};memcpy(out,pos,16);return true;}
bool FxCenterId(uint32_t){return true;}bool CoverAt(FxRec&,float*,uint32_t){return false;}void FxFwd(float*,float,float){}
"""
code+=between('int SealExitReplayQueue(', '// human #58146: freeze switches')
code+=r"""
int checks=0;void check(bool v){++checks;if(!v){printf("FAIL %d\n",checks);exit(1);}}
void actor(uint64_t a,uint32_t id,uint64_t root,float fade=1){put(a,id<<4);put(a+0x20,root);put(root+0x248,fade);}
float fade(uint64_t root){float f=-1;Rd(root+0x248,f);return f;}
int main(){
 actor(456,0x16127,10000);actor(789,0xC17E3,20000,.5f);
 g_hitSourceSnapshot=10;g_pressHitSources={{456,10.30},{789,10.42}};double at=0;
 check(PressPrimarySeal(&at)==456&&at==10.30);
 g_pressHitSources={{789,10.30},{456,10.42}};check(PressPrimarySeal(&at)==456&&at==10.42);
 g_pickCore=0x999;check(PressPrimarySeal(&at)==789&&at==10.30);g_pickCore=0x16127;
 oldActors.insert(456);check(PressPrimarySeal(&at)==789);oldActors.clear();
 g_hitSourceSnapshot=9;check(!PressPrimarySeal(&at));g_hitSourceSnapshot=10;
 g_pressHitSources={{456,10.30},{789,10.42}};g_fh4.seal=789;g_fh4.capT=10.42;
 uint64_t cap=789;double capT=10.42;adopt(cap,capT);
 check(cap==456&&g_fh4.seal==456&&g_fh4.keepSeal&&binds==1);
 clockNow=11;g_ghostHideAt=11.05;PressGhostBegin({456,789,456,123},true);
 check(g_pressGhosts.size()==2);check(fade(10000)==1&&fade(20000)==.5f);
 clockNow=11.125;PressGhostTick();check(std::abs(fade(10000)-.5f)<.001f);check(std::abs(fade(20000)-.25f)<.001f);
 check(std::abs(SealRateMul(456)-1.7f)<.001f&&std::abs(SealRateMul(789)-1.7f)<.001f);
 clockNow=11.3;enabled=false;g_fh4.owner=0;PressGhostTick();check(fade(10000)==0&&fade(20000)==0);
 check(SealRateMul(456)==2&&SealRateMul(789)==2&&SealRateMul(999)==1);
 dead.insert(456);PressGhostTick();check(g_pressGhosts.size()==1&&SealRateMul(789)==2);
 put(789,uint32_t(0x5129B<<4));PressGhostTick();check(g_pressGhosts.empty()&&SealRateMul(789)==1);
 actor(456,0x16127,10000);dead.clear();clockNow=12;PressGhostBegin({456},false);check(fade(10000)==0);
 PressGhostForget(456);check(g_pressGhosts.empty());
 actor(456,0x16127,10000);PressGhostBegin({456},true);clockNow=33;PressGhostTick();check(g_pressGhosts.empty());
 clockNow=40;check(SealExitReplayQueue(456,123)==1);const double firstAt=g_fxPlay[0].at;const float firstX=g_fxPlay[0].r.blk[12];
 clockNow=40.1;check(SealExitReplayQueue(789,789,true)==1&&g_fxPlayN==2);
 check(g_fxPlay[0].at==firstAt&&g_fxPlay[0].r.blk[12]==firstX&&g_fxPlay[1].at==40.1);
 check(g_fxPlay[1].r.blk[12]!=firstX);
 check(SealExitReplayQueue(789,789,true)==1&&SealExitReplayQueue(789,789,true)==1);
 check(SealExitReplayQueue(789,789,true)==0&&g_fxPlayN==kFxRecMax);
 printf("primary selection, capture promotion, multi-actor ghost/rate: %d checks PASS\n",checks);
}
"""
out=root/'build/primary-ghost-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'test.cpp';unit.write_text(code);exe=out/'test.exe'
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,r.stderr);r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
