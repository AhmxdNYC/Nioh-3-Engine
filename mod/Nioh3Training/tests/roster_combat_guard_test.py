"""Execute the production clear-wait query and BR veto with mocked actor state."""
from pathlib import Path
import subprocess, sys, tempfile
root = Path(__file__).resolve().parents[3]
tr = (root / 'mod/Nioh3Training/src/training.c').read_text()
br = (root / 'mod/Nioh3BossRematch/src/rematch.cpp').read_text()
def fn(s, sig):
    a = s.index(sig); b = s.index('{', a); n = 1; i = b + 1
    while n:
        n += (s[i] == '{') - (s[i] == '}'); i += 1
    return s[a:i] + '\n'
code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#define N3T_API
#define Log(...) ((void)0)
struct Row {unsigned count;} row={2};
static struct {bool ended;} g_np;
static struct {Row*row;bool native,ended,complete;unsigned killed,owned,index,handle;} g_fb;
static bool matches,exportPresent;
static unsigned actors[4],deadActors[4];
static int NativePhaseMatches(){return matches;}
static int FallbackMulti(){return g_fb.row && g_fb.row->count>1;}
static uint64_t RosterNativeActor(unsigned i){return actors[i];}
static uint64_t ActorByHandle(unsigned h){return h;}
static void ActorDeadNow(uint64_t a,int*d,uint64_t*h,uint64_t*m){*d=deadActors[a];*h=*d?0:100;*m=100;}
'''
code += fn(tr, 'N3T_API int Nioh3Training_NativeRosterPending(')
code += r'''
static auto TrainingExport(const char*) -> int(*)(){return exportPresent?Nioh3Training_NativeRosterPending:nullptr;}
'''
code += fn(br, 'bool VetoRosterClearWait(')
code += r'''
static void reset(){g_np={};g_fb={&row,true,false,false,0,0,0,0};matches=exportPresent=true;
 actors[0]=1;actors[1]=0;deadActors[1]=1;deadActors[2]=0;}
int main(){
 reset();assert(VetoRosterClearWait(0)); // native first death, second identity missing
 g_fb.killed=1;assert(VetoRosterClearWait(0)); // same after frame poll
 g_fb.owned=2;g_fb.index=1;g_fb.handle=2;assert(VetoRosterClearWait(0)); // live owned next member
 deadActors[2]=1;assert(!VetoRosterClearWait(0)); // final death BEFORE poll: native clear permitted
 reset();actors[1]=2;assert(VetoRosterClearWait(0)); // real native second still alive
 deadActors[2]=1;assert(!VetoRosterClearWait(0)); // both native actors dead before poll
 reset();g_fb.killed=3;assert(!VetoRosterClearWait(0));
 reset();g_fb.complete=true;assert(!VetoRosterClearWait(0));
 reset();g_fb.ended=true;assert(!VetoRosterClearWait(0));
 reset();g_np.ended=true;assert(!VetoRosterClearWait(0));
 reset();matches=false;assert(!VetoRosterClearWait(0)); // another mission/world, unconfirmed load
 reset();g_fb.native=false;assert(!VetoRosterClearWait(0)); // Practice/fallback-only unaffected
 reset();g_fb.row=nullptr;assert(!VetoRosterClearWait(0));
 reset();row.count=1;assert(!VetoRosterClearWait(0));row.count=2;
 reset();exportPresent=false;assert(!VetoRosterClearWait(0)); // old Training preserves native behavior
 puts("PASS 14 production roster clear-wait cases: pending/dead native and dynamic identities, final-only clear, exit, mismatch, Practice, single row, missing export");
}
'''
# Optional real-dump ABI check: execute BR's parser over this binding's bytes.
dump = Path('C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe')
if dump.exists():
    hooks = (root / 'mod/Nioh3BossRematch/src/hooks.cpp').read_text()
    data = dump.read_bytes()[0x23a3260:0x23a3760]
    abi = '#include <cstdint>\n#include <cstring>\n#include <cassert>\n#include <cstdio>\n'
    abi += 'struct {uint64_t base,end;} g_img;\n'
    abi += 'bool Copy(void*d,uint64_t s,size_t n){memcpy(d,(void*)s,n);return true;}\n'
    abi += ''.join(fn(hooks, sig) for sig in ['int LeaRcx(', 'int MovzxEbxW(', 'uint64_t CallTarget(', 'bool ParseNative('])
    abi += 'int main(){unsigned char b[]={' + ','.join(map(str,data)) + '};g_img.base=(uint64_t)b-0x23a3260;g_img.end=g_img.base+0x5000000;uint64_t c=0,f=0;assert(ParseNative((uint64_t)b,&c,&f));'
    abi += 'assert(c-(uint64_t)b+0x23a3260==0x139938);assert(f-(uint64_t)b+0x23a3260==0x1181730);puts("PASS actual ClearGadget binding ctor/fin ABI");}'
    with tempfile.TemporaryDirectory() as folder:
        p=Path(folder); src=p/'abi.cpp'; exe=p/'abi.exe';src.write_text(abi)
        subprocess.run([sys.executable,'-m','ziglang','c++','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder); src=p/'guard.cpp'; exe=p/'guard.exe'; src.write_text(code)
    subprocess.run([sys.executable,'-m','ziglang','c++','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
