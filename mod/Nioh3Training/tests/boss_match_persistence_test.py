"""Exercise production death-config save/set/default/restore bodies without game or INI access."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(__file__).resolve().parents[1]/'src/training.c').read_text()
def extract(signature):
    start=s.index(signature+' {');i=s.index('{',start)+1;depth=1
    while depth:
        depth+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[start:i]
preamble=r'''
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#define N3T_API
#define Log(...) ((void)0)
static int g_cfg_loot,g_cfg_deathblow,g_cfg_cutscene,g_cfg_nofin;
static int g_bmd_on,g_bmd_loot,g_bmd_nofin;
static struct { int practice; } g_tr;
static volatile LONG g_st_armed;
static char g_dir[MAX_PATH]="test";
static LPTHREAD_START_ROUTINE queued;static void* payload;static int written=-1;
static HANDLE Queue(LPTHREAD_START_ROUTINE f,void* p){assert(!queued);queued=f;payload=p;return (HANDLE)1;}
#define CreateThread(a,b,f,p,c,d) Queue(f,p)
#define CloseHandle(h) ((void)(h))
static int Write(const char* group,const char* key,const char* value,const char* path){
 (void)group;(void)path;if(key && !strcmp(key,"BossLoot"))written=atoi(value);return 1;}
#define WritePrivateProfileStringA Write
static void Flush(void){assert(queued);queued(payload);queued=0;payload=0;}
static void BossMatchRestore(const char* why);
typedef struct { int loot,blow,cut; } DeathCfgSave;
'''
parts=['static DWORD WINAPI DeathCfgSaveThread(LPVOID p)','static DWORD WINAPI NoFinSaveThread(LPVOID p)',
'N3T_API void Nioh3Training_SetNoFinisher(int on, int persist)',
'N3T_API void Nioh3Training_SetDeathCfg(int loot, int blow, int cut, int persist)',
'static void BossMatchDefaults(void)','static void BossMatchRestore(const char* why)']
main=r'''
int main(void){
 g_cfg_loot=1;g_cfg_nofin=1;BossMatchDefaults();assert(!g_cfg_loot && !g_cfg_nofin);
 Nioh3Training_SetDeathCfg(-1,1,-1,1);Flush();assert(written==1 && !g_cfg_loot);
 Nioh3Training_SetNoFinisher(1,0);assert(g_bmd_on && g_bmd_loot==1);
 Nioh3Training_SetDeathCfg(-1,-1,1,1);BossMatchRestore("end");
 g_cfg_loot=0;Flush();assert(written==1); // deferred worker uses captured preference
 g_cfg_loot=1;g_cfg_nofin=1;BossMatchDefaults();
 Nioh3Training_SetDeathCfg(0,-1,-1,1);Flush();assert(written==0);
 BossMatchRestore("end");assert(g_cfg_loot==0 && g_cfg_nofin==1);
 Nioh3Training_SetDeathCfg(1,-1,-1,1);BossMatchDefaults();Flush();assert(written==1);
 puts("PASS: unrelated saves preserve stashed loot; delayed worker snapshots; explicit overrides preserve other preference");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'save.c').write_text(preamble+'\n'.join(map(extract,parts))+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2',str(p/'save.c'),'-o',str(p/'save.exe')],check=True)
 subprocess.run([str(p/'save.exe')],check=True)
