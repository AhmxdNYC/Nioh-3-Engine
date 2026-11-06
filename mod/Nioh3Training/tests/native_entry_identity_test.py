"""Execute exact placement/record validation and same-row selection, without a game."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
a=s.index('static __attribute__((noinline)) uint64_t NativeEntryActor(')
b=s.index('static __attribute__((noinline)) int NativeBossReady(',a)
code=s[a:b].replace('__attribute__((noinline)) ','').replace('__try','if (1)').replace('__except (EXCEPTION_EXECUTE_HANDLER)','else')
pre=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "fallback_rosters_gen.h"
typedef struct {char id[16];uint32_t chr,placement;} ArenaRow;
static uint64_t wrapper[4],actor,registry,player,owned;
static uint32_t record[2];
static uint64_t FindWrapper(uint32_t p){return p==record[0]?(uint64_t)wrapper:0;}
static int Rd64(uint64_t p,uint64_t*v){*v=*(uint64_t*)p;return 1;}
static uint64_t PlayerChar(void){return player;}
static int InSp(uint64_t a){return a==owned;}
static uint64_t ActorByHandle(uint32_t p){return p==record[0]?registry:0;}
'''
main=r'''
int main(void){
 ArenaRow r={"R1DEA",889237,4092708};
 record[0]=4097963;record[1]=268880;wrapper[0]=actor=registry=1234;wrapper[3]=(uint64_t)record;
 assert(NativeEntryPlacement(&r)==4097963); // native branch creates third roster member
 record[1]++;assert(!NativeEntryPlacement(&r));record[1]--;
 registry++;assert(!NativeEntryPlacement(&r));registry--;
 player=actor;assert(!NativeEntryPlacement(&r));player=0;
 owned=actor;assert(!NativeEntryPlacement(&r));owned=0;
 strcpy(r.id,"R0034");assert(!NativeEntryPlacement(&r)); // another mission's named actor is not accepted
 strcpy(r.id,"R1DEA");wrapper[0]=0;assert(!NativeEntryPlacement(&r));wrapper[0]=actor;
 record[0]=r.placement;record[1]=r.chr;assert(NativeEntryPlacement(&r)==r.placement);
 puts("PASS native-entry identity: alternate, wrong key, stale registry, player, dynamic, wrong row, absent, primary");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(pre+code+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG','-I',str(root/'src'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
