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
static struct {int confirmed,route,warped,fellBack;} g_en;
static int mask,screen,shows,hides;
static int Mask(int m,int ms){mask=ms?m:0;return 1;}
static int Load(int on,const char*t,const char*p){return 1;}
static int (*p_osdMask)(int,int)=Mask;
static int (*p_osdLoad)(int,const char*,const char*)=Load;
static void EntryLs(int on,const char* why){g_el.on=screen=on;if(on)++shows;else ++hides;}
static void EntryLsBegin(ArenaRow*r){EntryLs(1,"begin");}
static int PhDamageEntryInstall(void){return installed;}
static uint32_t scenes[9]; static uint8_t gs[6]; static uint32_t seq;
static int EvSnap(uint32_t*out){memcpy(out,scenes,sizeof scenes);int n=0;for(int i=0;i<9;i++)n+=out[i]!=0;return n;}
static const uint8_t* GameState(void){return gs;}
static uint32_t SeqState(void){return seq;}
static EntryCover g_entryCover;
static DWORD g_entryProtectUntil;
static uint32_t g_entryScenes[9];
"""
for sig in ("static int EntryCoverProtect(void)","static void EntryCoverEnd(const char* why)","static void EntryCoverFight(void)","static void EntryCoverBegin(ArenaRow* r)","static int EntryCoverHold(void)","static void EntryCoverTick(void)"):
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
 g_en.fellBack=0;g_en.route=2;seq=0;gs[2]=0;now=30000;EntryCoverBegin(&row);now+=89999;EntryCoverTick();assert(screen);
 now++;EntryCoverTick();assert(!screen&&!mask&&!g_entryCover.active); // landed, not loading: 90 s still ends it
 now+=501;assert(!EntryCoverProtect());
 g_en.route=1;gs[2]=0;seq=0;now=40000;EntryCoverBegin(&row);now+=90000;EntryCoverTick();assert(screen&&g_entryCover.active); // pending entry holds past 90 s
 now=40000+179999;EntryCoverTick();assert(screen&&g_entryCover.active);
 now++;EntryCoverTick();assert(!screen&&!g_entryCover.active); // 180 s safety
 g_en.route=2;gs[2]=1;seq=0;now=50000;EntryCoverBegin(&row);now+=120000;EntryCoverTick();assert(screen&&g_entryCover.active); // 58A load flag holds past 90 s
 gs[2]=0;EntryCoverTick();assert(!screen&&!g_entryCover.active); // load cleared, entry not pending: end on the next tick
 g_en.route=2;gs[2]=0;seq=8;now=60000;EntryCoverBegin(&row);now+=100000;EntryCoverTick();assert(screen); // mission-load sequencer still running
 seq=0;EntryCoverTick();assert(!screen);
 g_en.route=1;gs[2]=1;seq=5;now=70000;EntryCoverBegin(&row);now+=100000;g_en.route=5;EntryCoverTick();assert(!screen&&!g_entryCover.active); // cancel/return still ends at once
 gs[2]=0;seq=0;
 EntryCoverBegin(&row);g_en.confirmed=0;EntryCoverTick();assert(!screen);
 g_en.confirmed=1;g_en.route=5;EntryCoverBegin(&row);EntryCoverTick();assert(!screen);
 g_en.route=1;g_rh.active=1;EntryCoverBegin(&row);EntryCoverTick();assert(!screen);g_rh.active=0;
 installed=0;EntryCoverBegin(&row);assert(!g_entryCover.active&&!screen);installed=1;
 g_cfg_entry_ls=0;EntryCoverBegin(&row);assert(!g_entryCover.active);g_cfg_entry_ls=1;
 scenes[0]=42;EntryCoverBegin(&row);EntryCoverTick();assert(screen); // old scene not an intro
 EntryCoverEnd("test");now=0xfffffff0u;EcBegin(&g_entryCover,now);assert(EcStep(&g_entryCover,now+90000u,0,0,0,0)==EC_TIMEOUT);
 EcBegin(&g_entryCover,now);g_entryCover.covered=1;assert(EcStep(&g_entryCover,now+90000u,0,0,0,1)==EC_WAIT&&g_entryCover.active);
 assert(EcStep(&g_entryCover,now+180000u,0,0,0,1)==EC_TIMEOUT);
 EcBegin(&g_entryCover,now);assert(EcStep(&g_entryCover,now+100000u,1,0,0,1)==EC_END);
 puts("PASS actual entry cover: native timeline, mission-event false intro regression, fallback actor gate, timeout/wrap, load and pending hold, 180 s safety, refusal/failure/return, unavailable guard, protection expiry");
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/"test.c").write_text(code)
 subprocess.run([sys.executable,"-m","ziglang","cc","-O2","-UNDEBUG","-I"+str(root/"src"),str(p/"test.c"),"-o",str(p/"test.exe")],check=True)
 subprocess.run([str(p/"test.exe")],check=True)
