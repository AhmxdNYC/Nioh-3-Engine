"""Exercise actual cover adapter, clock policy and export calls without the game."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/"src/training.c").read_text()
def fn(sig):
 a=s.index(sig+" {"); b=s.index("{",a); e=b+1; d=1
 while d: d+=(s[e]=="{")-(s[e]=="}");e+=1
 return s[a:e]
code=r"""
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "entry_cover.h"
typedef uint32_t DWORD;
typedef struct {uint32_t mission;} ArenaRow;
static DWORD now;
static DWORD GetTickCount(void){return now;}
static void Log(const char* f,...){(void)f;}
static struct {int active;} g_rh;
static int g_rh_ls_on,g_cfg_entry_ls=1,installed=1;
static struct {int on;} g_el;
static struct {int confirmed,route,warped,fellBack,dormant,waveOwned;} g_en;
static int g_entryPlayerHit;
static uint64_t playerChar=1;
static uint64_t PlayerChar(void){return playerChar;}
static int mask,screen,shows,hides;
static int Mask(int m,int ms){mask=ms?m:0;return 1;}
static int Load(int on,const char*t,const char*p){return 1;}
static int (*p_osdMask)(int,int)=Mask;
static int (*p_osdLoad)(int,const char*,const char*)=Load;
static int (*p_entryTip)(int);
static int tipOn=-1;
static int Tip(int on){tipOn=on;return 1;}
static void EntryLs(int on,const char* why){g_el.on=screen=on;if(on)++shows;else ++hides;}
static void EntryLsBegin(ArenaRow*r){EntryLs(1,"begin");}
static int PhDamageEntryInstall(void){return installed;}
static uint32_t scenes[9]; static uint8_t gs[6];
static int EvSnap(uint32_t*out){memcpy(out,scenes,sizeof scenes);int n=0;for(int i=0;i<9;i++)n+=out[i]!=0;return n;}
static const uint8_t* GameState(void){return gs;}
static EntryCover g_entryCover;
static DWORD g_entryProtectUntil;
static uint32_t g_entryScenes[9];
"""
for sig in ("static int EntryCoverProtect(void)","static void EntryCoverEnd(const char* why)","static void EntryCoverFight(void)","static void EntryCoverBegin(ArenaRow* r)","static void EntryCoverTick(void)"):
 code+=fn(sig)+"\n"
code+=r"""
int main(void){
 ArenaRow row={123};now=100;g_en.confirmed=1;g_en.route=1;
 EntryCoverBegin(&row);assert(screen && mask && EntryCoverProtect());
 now=12000;EntryCoverTick();assert(screen); // native load ends, still covered
 now=15000;g_en.warped=1;EntryCoverTick();assert(screen); // arena warp is not a fight
 now=18000;g_en.route=2;EntryCoverTick();assert(screen); // alive actor alone is not FIGHT start
 scenes[0]=99;EntryCoverTick();assert(screen&&mask&&g_entryCover.active); // mission event is NOT visual intro proof
 now=17360;EntryCoverTick();assert(screen&&mask); // door opens, still covered
 now=18123;gs[2]=1;EntryCoverTick();assert(screen&&mask); // native transition after door
 scenes[0]=0;gs[2]=0;EntryCoverTick();assert(screen&&mask);
 now=19300;EntryCoverFight();assert(!screen&&!mask&&!g_entryCover.active&&EntryCoverProtect());
 now+=501;assert(!EntryCoverProtect());
 g_en.fellBack=1;g_en.route=3;EntryCoverBegin(&row);EntryCoverFight();assert(screen); // history-open before actor
 g_en.route=4;EntryCoverTick();assert(!screen&&!g_entryCover.active);
 g_en.fellBack=0;g_en.route=1;now=30000;EntryCoverBegin(&row);now+=89999;EntryCoverTick();assert(screen);
 now++;EntryCoverTick();assert(!screen&&!mask&&!g_entryCover.active); // hard wall-clock limit
 now+=501;assert(!EntryCoverProtect());
 EntryCoverBegin(&row);g_en.confirmed=0;EntryCoverTick();assert(!screen);
 g_en.confirmed=1;g_en.route=5;EntryCoverBegin(&row);EntryCoverTick();assert(!screen);
 g_en.route=1;g_rh.active=1;EntryCoverBegin(&row);EntryCoverTick();assert(!screen);g_rh.active=0;
 installed=0;EntryCoverBegin(&row);assert(!g_entryCover.active&&!screen);installed=1;
 g_cfg_entry_ls=0;EntryCoverBegin(&row);assert(!g_entryCover.active);g_cfg_entry_ls=1;
 scenes[0]=42;EntryCoverBegin(&row);EntryCoverTick();assert(screen); // old scene not an intro
 p_entryTip=Tip;tipOn=-1;EntryCoverBegin(&row);assert(tipOn==1&&screen);
 EntryCoverEnd("tip");assert(tipOn==0&&!g_entryCover.active);p_entryTip=0;
 EntryCoverEnd("test");now=0xfffffff0u;EcBegin(&g_entryCover,now);assert(EcStep(&g_entryCover,now+90000u,0,0,0)==EC_TIMEOUT);
 g_en.confirmed=1;g_en.route=2;g_en.warped=1;g_en.dormant=1;g_en.fellBack=0;playerChar=1;gs[2]=0;g_entryPlayerHit=0;
 now=50000;EntryCoverBegin(&row);EntryCoverTick();assert(!screen&&!mask&&!g_entryCover.active); // lesson: in the mission, teacher dormant
 gs[2]=1;g_en.dormant=1;EntryCoverBegin(&row);EntryCoverTick();assert(screen&&g_entryCover.active); // still loading, keep the cover
 gs[2]=0;EntryCoverTick();assert(!screen&&!g_entryCover.active); // load clear, then drop
 g_en.dormant=0;g_en.route=1;g_entryPlayerHit=1;EntryCoverBegin(&row);assert(screen); // begin clears a stale hit
 g_entryPlayerHit=1;EntryCoverTick();assert(!screen&&!g_entryCover.active); // a real hit drops it before FIGHT
 g_entryPlayerHit=0;g_en.dormant=0;g_en.waveOwned=0;g_en.route=2;g_en.warped=1;EntryCoverBegin(&row);EntryCoverTick();assert(screen&&g_entryCover.active); // normal boss stays covered
 g_en.waveOwned=1;EntryCoverTick();assert(!screen&&!mask&&!g_entryCover.active); // live wave body: do not keep the 90 s screen
 g_en.waveOwned=1;gs[2]=1;EntryCoverBegin(&row);EntryCoverTick();assert(screen&&g_entryCover.active); // still loading, keep it
 gs[2]=0;EntryCoverTick();assert(!screen&&!g_entryCover.active);g_en.waveOwned=0;
 now=60000;EntryCoverFight();assert(!screen&&!g_entryCover.active);
 puts("PASS actual entry cover: native timeline, mission-event false intro regression, fallback actor gate, timeout/wrap, refusal/failure/return, unavailable guard, protection expiry, lesson dormant and player hit");
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/"test.c").write_text(code)
 subprocess.run([sys.executable,"-m","ziglang","cc","-O2","-UNDEBUG","-I"+str(root/"src"),str(p/"test.c"),"-o",str(p/"test.exe")],check=True)
 subprocess.run([str(p/"test.exe")],check=True)
