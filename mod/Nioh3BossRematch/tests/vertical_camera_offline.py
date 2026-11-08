from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
src=(root/'src/bosslog.cpp').read_text()
cam=(root.parent/'Nioh3CamKiProbe/src/probe.cpp').read_text()
pub=src[src.index('std::atomic<float> g_cameraVerticalTarget'):src.index('void FkSlot0Tick(')]
apply=cam[cam.index('static bool ApplySoulVertical('):cam.index('static void OutTouch(')]
pre=r'''
#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "../../../shared/soul_camera_vertical.h"
void Log(const char*,...){}
ULONGLONG ms=100;ULONGLONG TestMs(){return ms;}
#define GetTickCount64 TestMs
std::atomic<int> g_cameraFollowVertical{1};
struct {uint64_t owner=123,seal=456;double capT=1;}g_fh4;
struct {uint64_t actorGlobal=1;}g_fhEnd;
float player[4]={10,100,20,1},seal[4]={30,100,40,1};bool valid=true;int writes=0;
int FormHideSealPresent(uint64_t,uint64_t){return valid?1:0;}
bool Copy(void* out,uint64_t a,size_t n){if(a==123+0x110)memcpy(out,player,n);else if(a==456+0x110)memcpy(out,seal,n);else return false;return true;}
bool Rd(uintptr_t a,void* p,size_t n){memcpy(p,(void*)a,n);return true;}
bool Wr(uintptr_t a,const void* p,size_t n){++writes;memcpy((void*)a,p,n);return true;}
bool Fin3(const float* p){return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]);}
'''
post=r'''
int checks=0;void check(bool v){++checks;if(!v){printf("FAIL %d\n",checks);exit(1);}}
int main(){
 SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==0);
 soulcamera::Spring s;
 for(int i=0;i<120;++i){ms+=16;SoulCameraVerticalTick(123);check(s.Step(Nioh3BossRematch_CameraVerticalTarget(),1.f/60)==0);}
 seal[1]=300;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==200);
 float prev=0;for(int i=0;i<60;++i){float y=s.Step(200,1.f/60);check(y>=prev&&y<=200);prev=y;}check(std::fabs(s.y-200)<.01f);
 // Both eye and target shift along WORLD Y; x/z, distance and view direction preserved.
 alignas(float) unsigned char out[64]={};float eye[4]={10,30,40,1},tgt[4]={12,45,80,1};
 memcpy(out+0x10,eye,16);memcpy(out+0x20,tgt,16);check(ApplySoulVertical(out,s.y));
 float e[4],t[4];memcpy(e,out+0x10,16);memcpy(t,out+0x20,16);
 check(e[0]==10&&e[2]==40&&t[0]==12&&t[2]==80&&std::fabs((t[1]-e[1])-15)<.001f);
 int before=writes;check(!ApplySoulVertical(out,0)&&writes==before);
 // Exit returns smoothly and exactly to zero; no player or seal write occurred.
 g_fh4.owner=0;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==0);
 prev=s.y;for(int i=0;i<120;++i){float y=s.Step(0,1.f/60,false);check(y<=prev&&y>=0);prev=y;}check(s.y==0&&player[1]==100&&seal[1]==300);
 // New session at a different height rebases; grounded travel down a slope adds no double movement.
 g_fh4.owner=123;g_fh4.capT=2;player[1]=seal[1]=500;SoulCameraVerticalTick(123);
 player[1]=seal[1]=450;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==0);
 seal[1]=350;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==-100);
 seal[1]=10000;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==9550);
 seal[1]=-10000;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==-10450);
 ms+=251;check(Nioh3BossRematch_CameraVerticalTarget()==0);
 g_cameraFollowVertical=0;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==0);
 g_cameraFollowVertical=1;valid=false;SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==0);
 valid=true;seal[1]=std::numeric_limits<float>::quiet_NaN();SoulCameraVerticalTick(123);check(Nioh3BossRematch_CameraVerticalTarget()==0);
 soulcamera::Spring a,b;for(int i=0;i<60;++i)a.Step(200,1.f/60);for(int i=0;i<30;++i)b.Step(200,1.f/30);
 check(std::fabs(a.y-b.y)<.001f);check(std::isfinite(a.Step(NAN,.016f)));
 // Full initial height, tall rise and steady unity gain (no 400 cap).
 g_fh4.owner=123;g_fh4.capT=3;player[1]=100;seal[1]=2100;SoulCameraVerticalTick(123);
 check(Nioh3BossRematch_CameraVerticalTarget()==2000);
 soulcamera::Spring tall;for(int i=0;i<30;++i)tall.Step(2000,1.f/60);
 check(std::fabs(tall.y-2000)<100);
 for(int i=0;i<12;++i)tall.Step(0,1.f/60,false);
 check(std::fabs(tall.y)<.01f);
 printf("vertical camera target/export/spring/output: %d checks PASS\n",checks);
}
'''
out=root/'build/vertical-camera-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'test.cpp';unit.write_text(pre+pub+apply+post)
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'test.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
