"""Compile the real HasSkill + MotionSpeed hook bodies with fake game access. No game."""
from pathlib import Path
import subprocess
here = Path(__file__).resolve().parent
src = (here.parent / 'src/bosslog.cpp').read_text()
def function(signature):
    start = src.index(signature)
    brace = src.index('{', start)
    level = 1
    end = brace + 1
    while level:
        level += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end]
pre = r'''
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <intrin.h>
#include "seal_use_gate.h"
double clockNow=10; uint32_t action=0x7D0; bool airborne=false; float speed=300, drag=0;
double Now(){return clockNow;} uint64_t FkPlayer(){return 42;}
bool Rd(uint64_t, uint64_t& a){a=42;return true;}
uint32_t PlayerActId(uint64_t){return action;}
bool PlayerInAir(float*,float* =nullptr){return airborne;}
float PlayerGroundSpeed(){return speed;}
void AirSampleTick(uint64_t){} float SealRateMul(uint64_t){return 1;}
void RunThrowDrag(uint64_t,float m){drag=m;}
unsigned PadButtons(void*,void*){return 0;}
template<class... T> void Log(const char*,T...){}
bool UseCategoryAct(uint32_t a){return (a>>8)==0x28;}
bool UseConfirmedSince(double){return false;}
std::atomic<bool> g_scForms{true},g_orbHooked{true};
// Equipment polling is tested separately; expose the current forms toggle here.
void PickPoll(){} bool ScOn(){return g_scForms.load();}
std::atomic<float> g_ftMul{8},g_tailMul{4};
std::atomic<int> g_rtMaxMs{1200},g_rtConfirmMs{200},g_runFadeMs{100},g_runFadeSpeed{200},g_ftLayerLog{0},g_ftN{0};
std::atomic<double> g_rtT0{-100},g_rtAcceptT{-100},g_rtPendingT{-100},g_rtFirstT{-100},g_runFadeAt{0},g_rtClosedT{-100};
std::atomic<uint32_t> g_rtPendingAct{~0u},g_rtAct{~0u},g_rtClosedAct{~0u},g_ftActs[4],g_acCalls{0};
std::atomic<uint64_t> g_acCallers[16]; uint64_t g_base=0;
struct {uint64_t owner=0;} g_fh4;
double g_throwDoneT=-100;
float NativeRate(uint64_t,uint8_t,uint8_t){return 1.1f;}
void* g_ftOrig=(void*)&NativeRate;
uint64_t NativeSkill(uint64_t,uint32_t,uint32_t){return 0x100;}
auto g_hasSkillOrig=&NativeSkill;
'''
tests = r'''
void reset(){clockNow+=5; action=0x7D0;airborne=false;g_scForms=true;g_fh4.owner=0;drag=0;
 g_rtPendingT=-100;g_rtT0=-100;g_rtAcceptT=-100;g_runFadeAt=0;g_throwDoneT=-100;}
void query(){HkHasSkill(1,0xEDC9,0);}
float tick(uint32_t a,double dt=.016){action=a;clockNow+=dt;return HkMotionSpeed(1,0,0);}
int main(){
 // Ordinary talisman, whether the query precedes the action or occurs inside it.
 for(bool already: {false,true}) {reset(); if(already)action=0x2718; query();
   assert(g_rtT0==-100 && g_runFadeAt==0 && g_rtAcceptT==-100);
   for(int i=0;i<90;i++){query();assert(tick(0x2718)==1.1f);assert(drag==0 && g_runFadeAt==0);}}
 // Empty UP repeatedly queried for several seconds cannot accelerate idle/run.
 reset();for(int i=0;i<180;i++){query();assert(tick(0x7D0)==1.1f);assert(g_runFadeAt==0);}
 // Each known player seal animation confirms quickly, then cancellation is immediate.
 for(uint32_t a: {0x28FDu,0x28B6u,0x28B2u,0x28FBu}){reset();query();
   assert(tick(a)==8.8f && drag==8 && g_runFadeAt>0);
   assert(tick(0x2718)==1.1f && drag==0 && g_runFadeAt==0);}
 // A random same-category action is NOT evidence of a seal.
 reset();query();assert(tick(0x2810)==1.1f && g_runFadeAt==0);
 reset();query();assert(tick(0x28B2,.3)==1.1f); // expired, no configured FastThrow fallback in fixture
 // Forms off preserves native answer and rate; AirBlock still denies before arming.
 reset();g_scForms=false;assert(HkHasSkill(1,0xEDC9,0)==0x100);assert(tick(0x28FD)==1.1f);
 reset();airborne=true;assert(HkHasSkill(1,0xEDC9,0)==0x100);assert(g_rtPendingT==-100);
 reset();HkHasSkill(1,123,0);assert(g_rtPendingT==-100);
 puts("item29 actual hook replay PASS: talisman, empty UP, four seal actions, cancellation, timeout, forms OFF, AirBlock");
}
'''
build=here.parent/'build'
build.mkdir(exist_ok=True)
unit=build/'item29_hooks.cpp'
unit.write_text(pre+function('float HkMotionSpeed(')+function('uint64_t HkHasSkill(')+tests)
exe=build/'item29_hooks.exe'
subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/W4','/I'+str(here.parent/'src'),
                '/Fo'+str(build/'item29_hooks.obj'),'/Fe'+str(exe),str(unit)],check=True)
subprocess.run([str(exe)],check=True)
