"""GH #108 / #44: a user despawn of a dead (HP 0) boss fades it and deletes it FadeDeleteMs later; a live boss is deleted at once.
Runs the production Despawn2 / DespawnDeadDefer / DeadDelTick / DespawnAll bodies against fake actor memory."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
def body(sig):
 a=s.index(sig+' {'); b=s.index('{',a); d=1;i=b+1
 while d:
  d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
code=r'''
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define Log(...) ((void)0)
static uint64_t B; static uint64_t Grva(uint64_t r){return r;}
static uint64_t g_sp_actor[4]; static uint32_t g_sp_handle[4]; static int g_nsp; static struct{int active;}g_sp; static int g_watch; static struct{uint64_t a;}g_mon;
static uint32_t actA[8]={0x11,0x22},actB[8]={0x33,0x44}; static uint64_t hpA=0,hpB=500,maxhp=1000; static int inA=1,inB=1,fades,forgets,deletes,loading;
static uint64_t lastDel; static DWORD tick=1000; static uint8_t gs[3];
static DWORD MyTick(void){return tick;}
#define GetTickCount MyTick
static void FakeGaugeHide(const char*w){}
static int Actors(uint64_t*o,int m){int n=0;if(inA)o[n++]=(uint64_t)actA;if(inB)o[n++]=(uint64_t)actB;return n;}
static int ActorInTable(uint64_t x){uint64_t c[4];int n=Actors(c,4);for(int i=0;i<n;i++)if(c[i]==x)return 1;return 0;}
static int ActorHp(uint64_t a,uint64_t*c,uint64_t*m){if(a==(uint64_t)actA){*c=hpA;*m=maxhp;return 1;}if(a==(uint64_t)actB){*c=hpB;*m=maxhp;return 1;}return 0;}
static int FadeOutActor(int i,float f){fades++;return 1;}
static DWORD FadeDeleteMs(void){return 6000;}
static const uint8_t* GameState(void){gs[2]=(uint8_t)loading;return gs;}
static void SlotForget(uint64_t a){forgets++;}
static void NativeDelete(uint64_t a){deletes++;lastDel=a;}
'''
fn=['static int DespawnDeadDefer(int idx);',
    body('static int Despawn2(int idx, int deferDead)'),
    'static int Despawn(int idx) { return Despawn2(idx, 0); }']
dd=s.index('static struct { uint64_t a; uint32_t h0, h1; DWORD due; } g_dd[16];')
fn.append(s[dd:s.index('\n',dd)])
fn.append(body('static int DespawnDeadDefer(int idx)'))
fn.append(body('static void DeadDelTick(void)'))
fn.append(body('static int DespawnAll(const char* why)'))
code+='\n'.join(f.replace('((void (*)(uint64_t))(B + Grva(0x2BF7E8)))(a)','NativeDelete(a)') for f in fn)
assert code.count('NativeDelete(a)')>=2, 'native delete call not found in Despawn2 / DeadDelTick'
code+=r'''
int main(void){
 /* menu despawn all: dead A deferred + faded, live B deleted at once */
 g_nsp=2;g_sp_actor[0]=(uint64_t)actA;g_sp_actor[1]=(uint64_t)actB;
 assert(DespawnAll("menu")==2); assert(deletes==1&&lastDel==(uint64_t)actB&&fades==1&&g_nsp==0&&!g_sp_actor[0]);
 inB=0; tick+=5999; DeadDelTick(); assert(deletes==1);            /* not yet due */
 loading=1; tick+=1; DeadDelTick(); assert(deletes==1);           /* due but loading: waits */
 loading=0; tick+=1000; DeadDelTick(); assert(deletes==2&&lastDel==(uint64_t)actA);
 DeadDelTick(); assert(deletes==2);                               /* entry consumed */
 /* actor gone / handle reused before due: no delete */
 g_nsp=1;g_sp_actor[0]=(uint64_t)actA; assert(DespawnAll("command")==1); actA[1]=0x99; tick+=7000; DeadDelTick(); assert(deletes==2);
 actA[1]=0x22; g_nsp=1;g_sp_actor[0]=(uint64_t)actA; DespawnAll("menu"); inA=0; tick+=7000; DeadDelTick(); assert(deletes==2); inA=1;
 /* internal paths (session clear, body fade) keep the immediate delete */
 g_nsp=1;g_sp_actor[0]=(uint64_t)actA; assert(DespawnAll("train clear")==1&&deletes==3);
 g_nsp=1;g_sp_actor[0]=(uint64_t)actA; assert(Despawn(0)==1&&deletes==4);
 /* live boss through the deferring path is deleted at once */
 hpA=10; g_nsp=1;g_sp_actor[0]=(uint64_t)actA; assert(Despawn2(0,1)==1&&deletes==5);
 puts("PASS GH #108 dead-boss despawn: menu/command defer + fade, live/internal immediate, load wait, gone/reused handle skipped");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(code);exe=Path(td)/'test.exe'
 subprocess.run([sys.executable,'-m','ziglang','cc','-target','x86_64-windows-gnu','-fms-extensions','-Xclang','-fasync-exceptions',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
