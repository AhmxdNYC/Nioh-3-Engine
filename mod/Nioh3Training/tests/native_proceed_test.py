"""Execute production Proceed hook with mocked script reader/UI; no game."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(__file__).parents[1]/'src/training.c').read_text()
def fn(sig):
 a=s.index(sig);b=s.index('{',a);depth=1;i=b+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
pre=r"""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#define N3T_API
#define Log(...) ((void)0)
typedef unsigned long DWORD;typedef long LONG;
static LONG InterlockedIncrement(volatile LONG*p){return ++*p;}
static struct {int active,want;}g_rh;
static struct {uint32_t mission;}row={0x3513};
static struct {int confirmed,warped;void*unused;typeof(row)*row;}g_en;
static struct {int active;}g_tr;
static struct {int up,id,tIn,left;float pos[4];}g_gate;
static uint8_t st[4];static int match=1;
static const uint8_t*GameState(void){return st;}
static int NativePhaseMatches(void){return match&&g_en.confirmed&&g_en.row;}
static int g_cfg_gate_cw2=1,g_cfg_gate=1,g_cfg_gate_always=1;
static int g_cfg_gate_block=1;
static volatile LONG g_cw2_n,g_endconf_n;
static DWORD tick=1000,g_gate_tUse,g_gate_nat_t;
static DWORD GetTickCount(void){return tick;}
static int page,opened,original,alive,peek=1,pops,scroll;
static int ScrollGateLive(void){return scroll;}
static void Cw2Pop(void*ctx){pops++;}
static uint32_t textId=0x3E0C230,subId=10;
static uint64_t current=1234;
static int32_t button;
static uintptr_t B;
static uintptr_t Grva(uintptr_t r){assert(r==0x44BAE44);return (uintptr_t)&button;}
static int Cw2Peek(void*ctx,uint32_t*t,uint32_t*s,uint64_t*c,uint16_t*n){*t=textId;*s=subId;*c=current;*n=7;return peek;}
static int PlayerPos(float*q){q[0]=10000;q[1]=q[2]=q[3]=0;return 1;}
static int GateBossAlive(void){return alive;}
static int GateMenuOpen(void){return page;}
static int GateUse(void){opened++;g_gate_tUse=tick;return 2;}
static uint64_t o_cw2(void*p){original++;return 99;}
static uint64_t o_endconf(void*p,int action){original++;return 99;}
static void reset(void){
 memset(&g_en,0,sizeof g_en);memset(&g_gate,0,sizeof g_gate);memset(&g_tr,0,sizeof g_tr);
 memset(&g_rh,0,sizeof g_rh);memset(st,0,sizeof st);
 g_en.confirmed=g_en.warped=match=peek=1;g_en.row=&row;g_cfg_gate_cw2=g_cfg_gate=g_cfg_gate_always=1;
 tick=1000;g_gate_tUse=0;page=opened=original=alive=pops=scroll=0;button=0;textId=0x3E0C230;subId=10;current=1234;
}
"""
main=r"""
int main(void){
 // Native route, no Training session, no synthetic gadget, far from stored pose.
 reset();assert(Nioh3Training_NativeGateMission()==0x3513);assert(h_cw2(0)==7);
 assert(opened==1&&original==0&&button==-1&&pops==1);
 // A held-win timer is deliberately not consulted. Repeated request cannot stack UI.
 assert(h_cw2(0)==7&&opened==1);tick+=2000;page=1;assert(h_cw2(0)==7&&opened==1);
 reset();textId=0x3E8D85E;assert(h_cw2(0)==7&&opened==1&&button==-1);
 reset();textId=55;assert(h_cw2(0)==99&&original==1&&!opened&&button==0&&pops==0);
 // #91781: duel/fight boxes pass through with the args left for the original (blank box before).
 reset();textId=65585054;assert(h_cw2(0)==99&&original==1&&!opened&&pops==0);
 reset();scroll=1;textId=65930289;assert(h_cw2(0)==99&&original==1&&pops==0);
 reset();current=0;assert(h_cw2(0)==99&&!opened&&pops==0);
  reset();subId=0xFFFFFFFF;assert(h_cw2(0)==99&&!opened);
 reset();peek=0;assert(h_cw2(0)==99&&!opened);
 reset();match=0;assert(h_cw2(0)==99&&!opened);
 reset();st[2]=1;assert(h_cw2(0)==99&&!opened);
 reset();g_rh.want=1;assert(h_cw2(0)==99&&!opened);
 reset();g_en.warped=0;assert(h_cw2(0)==99&&!opened);
 reset();g_cfg_gate_cw2=0;assert(h_cw2(0)==99&&!opened);
 // Ordinary mission and unrelated C++ actions remain native.
 reset();g_en.confirmed=0;assert(h_cw2(0)==99&&!opened);
 reset();assert(h_endconf(0,0x2613)==0&&opened==1&&!original);
 reset();assert(h_endconf(0,0x536)==99&&!opened);
 // Old practice synthetic gate interception preserved.
 reset();g_en.confirmed=0;g_tr.active=g_gate.up=1;g_gate.id=10;
 assert(h_cw2(0)==7&&opened==1&&button==-1);
 puts("PASS native Proceed: 18 cases (args popped once only when blocked), immediate page, cancel script result, no duplicate UI, ownership/load guards, ordinary/practice preserved");
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);c=p/'test.c';exe=p/'test.exe'
 c.write_text(pre+fn('N3T_API uint32_t Nioh3Training_NativeGateMission(')+fn('static __attribute__((noinline)) uint64_t h_cw2(')+fn('static uint64_t h_endconf(')+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
