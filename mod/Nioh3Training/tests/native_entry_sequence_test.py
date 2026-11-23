"""Execute production post-gate prerequisite observer with generated identities."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'src/training.c').read_text()
a=s.index('static uint32_t NativeEntrySequenceLive(');b=s.index('static int g_native_fallback,',a);code=s[a:b]
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "native_entry_sequences_gen.h"
typedef struct {uint32_t mission,placement;} ArenaRow;
static uint32_t placement,chr;static int active=1;
static uint64_t NativeEntryActor(uint32_t p,uint32_t c){return p==placement&&c==chr?123:0;}
static int NativeBossReady(const ArenaRow*r,uint32_t p){return active;}
'''
main=r'''
int main(void){
 unsigned tested=0;
 for(unsigned i=0;i<sizeof g_native_entry_sequence_members/sizeof*g_native_entry_sequence_members;i++){
  const NativeEntrySequenceMember*m=&g_native_entry_sequence_members[i];ArenaRow r={m->mission,m->boss};
  placement=m->placement;chr=m->chr;active=1;assert(NativeEntrySequenceLive(&r)==placement);
  active=0;assert(!NativeEntrySequenceLive(&r));active=1;
  chr++;assert(!NativeEntrySequenceLive(&r));chr--;
  r.mission=0xffffffff;assert(!NativeEntrySequenceLive(&r));r.mission=m->mission;
  r.placement=0xffffffff;assert(!NativeEntrySequenceLive(&r));tested++;
 }
 assert(tested>0);printf("PASS native prerequisite observer: %u generated exact identities, inactive/wrong-key/mission/selected-boss excluded\n",tested);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'sequence.c';exe=p/'sequence.exe';src.write_text(pre+code+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG','-I',str(root/'src'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
