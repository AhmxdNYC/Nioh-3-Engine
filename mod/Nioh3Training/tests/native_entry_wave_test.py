"""Execute production native-wave observer against generated same-mission identities."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'src/training.c').read_text()
a=s.index('static uint32_t NativeEntryWaveLive(');b=s.index('static int g_native_fallback,',a);code=s[a:b]
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "native_entry_boxes_gen.h"
typedef struct {char id[16];} ArenaRow;
static uint32_t placement,chr;static uint64_t hp=100,maxhp=100;static int activeAI=1;
static uint64_t NativeEntryActor(uint32_t p,uint32_t c){return p==placement&&c==chr?123:0;}
static int NativeBossReady(const ArenaRow*r,uint32_t p){return hp&&maxhp&&activeAI;}
'''
main=r'''
int main(void){
 unsigned tested=0;
 for(unsigned i=0;i<sizeof g_native_entry_wave_members/sizeof*g_native_entry_wave_members;i++){
  const NativeEntryWaveMember*m=&g_native_entry_wave_members[i];ArenaRow r;snprintf(r.id,sizeof r.id,"%s",m->id);
  placement=m->placement;chr=m->chr;hp=maxhp=100;assert(NativeEntryWaveLive(&r)==placement);
  hp=0;assert(!NativeEntryWaveLive(&r));hp=100;maxhp=0;assert(!NativeEntryWaveLive(&r));maxhp=100;activeAI=0;assert(!NativeEntryWaveLive(&r));activeAI=1;
  chr++;assert(!NativeEntryWaveLive(&r));chr--;strcpy(r.id,"NO_SUCH_ROW");assert(!NativeEntryWaveLive(&r));tested++;
 }
 assert(tested>0);printf("PASS native wave observer: %u generated identities, live/dead/unready/wrong-key/wrong-row\n",tested);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'wave.c';exe=p/'wave.exe';src.write_text(pre+code+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG','-I',str(root/'src'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
