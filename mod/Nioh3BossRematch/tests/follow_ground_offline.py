from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
src=(root/'src/bosslog.cpp').read_text(encoding='utf-8')
helper=src[src.index('struct FormFollowGroundState'):src.index('bool FormFollow(uint64_t')]
follow=src[src.index('bool FormFollow(uint64_t'):src.index('    float m[12] = {}, sm[12] = {};',src.index('bool FormFollow(uint64_t'))]+'return true;}\n'
restore=src[src.index('void Fh4Restore(uint64_t'):src.index('bool Fh4ClampRoot(')]
pre=r'''
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
float playerPos[4]={10,13674,20,1},sealPos[4]={30,13951,40,1};
bool readable=true,warpOk=true,writeOk=true,blocked=true;int warps=0,writes=0,present=1;
uint64_t currentPlayer=123,g_base=0;std::atomic<int> g_fhWarp{1};
struct {uint64_t actorGlobal=1;bool active=true;} g_fhEnd;
struct {uint64_t owner=0,root=0;float multiplier=1,fade=1,blend=0;uint8_t hide=0;} g_fh4;
std::atomic<bool> g_fh4Fault{false};
uint64_t FkPlayer(){return currentPlayer;}uint64_t Grva(uint64_t a){return a;}
void Log(const char*,...){}void FormBlockSet(bool b){blocked=b;}
bool FormHideCodeMatches(uint64_t,const uint8_t*,size_t){return false;}
int FormHideSealPresent(uint64_t,uint64_t){return present;}
bool Copy(void* out,uint64_t a,size_t n){if(!readable)return false;if(a==123+0x110){memcpy(out,playerPos,n);return true;}if(a==456+0x110){memcpy(out,sealPos,n);return true;}return false;}
bool FormPinWarpCall(uint64_t p,const float* pos){++warps;if(!warpOk||p!=123)return false;memcpy(playerPos,pos,16);return true;}
bool WrBytes(uint64_t a,const void* p,size_t n){++writes;if(!writeOk||a!=123+0x110)return false;memcpy(playerPos,p,n);return true;}
template<class T>bool Rd(uint64_t a,T& v){if(a==123+0x20){uint64_t rootValue=1000;v=(T)rootValue;return true;}v=(T)0;return true;}
template<class T>bool Fh4Set(uint64_t,T){return true;}
'''
post=r'''
int checks=0;void check(bool b){++checks;if(!b){printf("FAIL %d\n",checks);exit(1);}}
void reset(){playerPos[0]=10;playerPos[1]=13674;playerPos[2]=20;playerPos[3]=1;sealPos[0]=30;sealPos[1]=13951;sealPos[2]=40;sealPos[3]=1;readable=warpOk=writeOk=true;warps=writes=0;currentPlayer=123;present=1;g_fhWarp=1;g_fh4={};g_fh4.owner=123;g_fh4.root=1000;g_fhEnd.active=true;g_fh4Fault=false;FormFollowGroundBegin(123);}
int main(){
 reset();check(FormFollow(123,456));check(playerPos[1]==13674&&playerPos[0]==30&&playerPos[2]==40);check(sealPos[1]==13951);
 for(float y:{13674.f,13750.f,13951.f,14252.f}){sealPos[1]=y;check(FormFollow(123,456)&&playerPos[1]==13674);}
 Fh4Restore(123);check(!g_fh4.owner&&!g_followGround.owner&&!blocked&&playerPos[1]==13674);
 reset();sealPos[1]=13600;check(FormFollow(123,456)&&playerPos[1]==13600);Fh4Restore(123);check(playerPos[1]==13600); // downward remains allowed
 reset();g_fhWarp=0;check(FormFollow(123,456)&&warps==0&&writes==1&&playerPos[1]==13674);
 reset();warpOk=false;check(FormFollow(123,456)&&warps==1&&writes==1&&playerPos[1]==13674);
 reset();warpOk=writeOk=false;check(!FormFollow(123,456)&&!g_followGround.used&&playerPos[1]==13674);
 // Any exit uses actual Fh4Restore; correct elevated residual without changing X/Z.
 reset();g_followGround.used=true;playerPos[1]=14252;Fh4Restore(123);check(playerPos[1]==13674&&playerPos[0]==10&&playerPos[2]==20&&!g_fh4.owner);
 reset();g_followGround.used=true;playerPos[1]=13951;warpOk=writeOk=false;Fh4Restore(123);check(g_fh4Fault&&g_fh4.owner==123&&g_followGround.owner==123&&!blocked);
 warpOk=true;Fh4Restore(123);check(playerPos[1]==13674&&!g_fh4.owner);
 reset();playerPos[1]=13951;Fh4Restore(123);check(playerPos[1]==13951&&warps==0&&writes==0); // non-follow Doji untouched
 reset();g_followGround.used=true;playerPos[1]=13951;currentPlayer=999;Fh4Restore(999);check(warps==0&&writes==0); // replacement player never warped
 reset();readable=false;FormFollowGroundBegin(123);readable=true;check(!FormFollow(123,456)&&warps==0&&writes==0);
 reset();sealPos[1]=std::numeric_limits<float>::quiet_NaN();check(!FormFollow(123,456)&&warps==0&&writes==0);
 reset();present=0;check(!FormFollow(123,456)&&warps==0&&writes==0);
 reset();float hold[4]={55,14252,77,1};check(FormFollowGroundTarget(123,hold)&&hold[1]==13674&&hold[0]==55&&hold[2]==77);
 check(!FormFollowGroundTarget(999,hold));
 printf("FormFollow ground ceiling and actual restore: %d checks PASS (offline)\n",checks);
}
'''
out=root/'build/follow-ground-test';out.mkdir(parents=True,exist_ok=True);unit=out/'ground.cpp';unit.write_text(pre+helper+follow+restore+post,encoding='utf-8')
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'ground.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "ground.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)

