"""Compile the production boss-presence/export functions with controlled actor inputs."""
import pathlib, subprocess, sys, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
code=s[s.index('static int MenuPauseActorPresent('):s.index('static DWORD WINAPI MenuPauseThread')]
stubs=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef long LONG; typedef unsigned long DWORD;
#define N3T_API
#define N3T_SLOTS 2
static uint64_t g_slot_act[2];
static int table=1, deadFlag=0; static uint64_t hp=100, maxHp=100, nativeActor=0;
static DWORD now=1000;
static DWORD GetTickCount(void){return now;}
static int ActorInTable(uint64_t a){return a && table;}
static int ActorDeadNow(uint64_t a,int* dead,uint64_t* cur,uint64_t* max){(void)a;*dead=deadFlag;*cur=hp;*max=maxHp;return deadFlag||(maxHp && !hp);}
static int SlotAlive(int s){return g_slot_act[s]!=0;}
static int KbCollect(uint64_t* out,int cap){(void)cap;if(nativeActor){out[0]=nativeActor;return 1;}return 0;}
'''
checks=r'''
int main(void){
 assert(!MenuPauseBossPresent());g_slot_act[0]=1;assert(MenuPauseBossPresent());
 table=0;assert(!MenuPauseBossPresent());table=1;
 deadFlag=1;assert(!MenuPauseBossPresent());deadFlag=0;
 hp=0;assert(!MenuPauseBossPresent());hp=100;maxHp=0;assert(!MenuPauseBossPresent());maxHp=100;
 g_slot_act[0]=0;nativeActor=2;assert(MenuPauseBossPresent());nativeActor=0;assert(!MenuPauseBossPresent());
 assert(!Nioh3Training_MenuPauseAllowed());g_menu_pause_allowed=1;g_menu_pause_checked=now;assert(Nioh3Training_MenuPauseAllowed());
 now+=250;assert(!Nioh3Training_MenuPauseAllowed());g_menu_pause_checked=now;g_menu_pause_allowed=0;assert(!Nioh3Training_MenuPauseAllowed());
 puts("PASS: 12 production boss-presence and stale-signal checks");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);src=p/'test.c';exe=p/'test.exe';src.write_text(stubs+code+checks)
 subprocess.run([sys.argv[1],'cc',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
