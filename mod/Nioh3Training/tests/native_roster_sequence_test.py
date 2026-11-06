"""Execute actual native roster poll/transitions with mocked game actors, offline."""
from pathlib import Path
import runpy, subprocess, sys, tempfile
base=runpy.run_path(str(Path(__file__).with_name('fallback_sequence_test.py')))
fn=base['fn']; state=base['state']; header=base['header']
pre=base['preamble'].replace('int active;} g_ar;', 'int active,world;} g_ar;')
pre=pre.replace('int bodyGone;} g_tr;', 'int bodyGone,active,arena,hasStart,round;float pp[4],bp[3],face;} g_tr;')
pre+=r'''
typedef struct {char id[16];float p[4],b[3];int world;} ArenaRow;
static ArenaRow row={"R0034",{1,2,3,4},{5,6,7},8};
static struct {int warped;ArenaRow*row;} g_en={1,&row};
static unsigned char actors[16][512];
static int present[16],deadFlags[16],playerDead;
static uint64_t health[16];
static int PlayerDeadNow(void){return playerDead;}
static int PlayerPos(float*p){for(int i=0;i<4;i++)p[i]=10+i;return 1;}
static uint64_t RosterNativeActor(unsigned i){return present[i]?(uint64_t)actors[i]:0;}
static int ActorDeadNow(uint64_t a,int*d,uint64_t*h,uint64_t*m){unsigned i=(unsigned)((unsigned char*)a-&actors[0][0])/512;*d=deadFlags[i];*h=health[i];*m=100;return *d||!*h;}
static int barOn[16], wakeCalls;
static int RosterBarOn(uint64_t a){unsigned i=(unsigned)((unsigned char*)a-&actors[0][0])/512;return i<16?barOn[i]:0;}
static void RosterWakeMember(unsigned i, uint64_t a){(void)i;(void)a;wakeCalls++;}
'''
main=r'''
static void reset(const char*id){
 memset(&g_fb,0,sizeof g_fb);memset(&g_tr,0,sizeof g_tr);memset(&g_sp,0,sizeof g_sp);memset(&g_np,0,sizeof g_np);
 memset(present,0,sizeof present);memset(deadFlags,0,sizeof deadFlags);memset(barOn,0,sizeof barOn);
 for(int i=0;i<16;i++){health[i]=100;float p[3]={101,202,303};memcpy(actors[i]+0x110,p,sizeof p);}
 g_fb.row=FallbackRosterFind(id);g_fb.native=1;g_fl.open=0;finishes=spawns=duplicates=dedupeCalls=playerDead=wakeCalls=0;g_dead_id=0;g_en.warped=1;
}
int main(void){
 // Native starts with the SECOND catalogue member: identity, not position. A shown bar means it is in the fight.
 reset("R0034");present[1]=1;barOn[1]=1;NativeRosterTick(100);assert(g_fb.seen==2&&g_fb.pose&&!wakeCalls);
 deadFlags[1]=1;health[1]=0;NativeRosterTick(200);assert(g_fb.killed==2&&!finishes&&!g_fb.complete);
 NativeRosterTick(8199);assert(!g_tr.active);NativeRosterTick(8200);
 assert(g_fb.index==0&&g_tr.chr==g_fb.row->chr[0]&&g_tr.bp[0]==101&&g_tr.bp[1]==202);
 FallbackSpawn();assert(spawns==1&&spawned==g_fb.row->chr[0]);
 FallbackTracked(99,g_tr.chr);g_dead_id=99;assert(!FallbackNext(9000)&&g_fb.complete);
 assert(FightFinish(FL_WIN)&&finishes==1);assert(!FightFinish(FL_WIN)&&finishes==1);
 // Native next phase arrives inside grace: no dynamic summon and one final win.
 reset("R0034");present[1]=1;barOn[1]=1;NativeRosterTick(100);deadFlags[1]=1;NativeRosterTick(200);
 present[0]=1;barOn[0]=1;NativeRosterTick(5000);NativeRosterTick(10000);assert(!g_tr.active&&!spawns&&!wakeCalls);
 deadFlags[0]=1;NativeRosterTick(11000);assert(g_fb.complete&&finishes==1);
 NativeRosterTick(12000);assert(finishes==1);
 // Every resolved generated roster, reverse-native order, final-only finish.
 for(unsigned k=0;k<sizeof(g_fallback_rosters)/sizeof(*g_fallback_rosters);k++){
  const FallbackRoster*r=&g_fallback_rosters[k];if(r->count<2)continue;reset(r->id);
  for(int i=(int)r->count-1;i>=0;i--){present[i]=1;barOn[i]=1;NativeRosterTick(1000+(r->count-i)*100);
   deadFlags[i]=1;NativeRosterTick(1050+(r->count-i)*100);assert(finishes==(i==0));}
  assert(g_fb.complete&&!spawns);
 }
 // HP zero during a finisher isn't an instant kill; despawn isn't a kill.
 reset("R0034");present[0]=1;barOn[0]=1;NativeRosterTick(100);health[0]=0;NativeRosterTick(200);
 NativeRosterTick(4699);assert(!g_fb.killed);health[0]=100;NativeRosterTick(4700);
 present[0]=0;NativeRosterTick(5000);assert(!g_fb.killed&&!g_tr.active);
 // Stable zero uses the existing 4.5 second death rule.
 present[0]=1;health[0]=0;NativeRosterTick(6000);NativeRosterTick(10500);assert(g_fb.killed==1);
 // Loss/quit during grace cancel all future work. Unresolved phase rows pass through.
 assert(FightFinish(FL_LOSS));NativeRosterTick(20000);assert(!g_tr.active&&g_fb.ended);
 reset("R0034");g_sp.active=1;g_tr.tRespawn=50;assert(FightFinish(FL_QUIT));assert(!g_sp.active&&!g_tr.tRespawn);
 reset("R0C12");NativeRosterTick(10000);assert(!FallbackMulti()&&!g_tr.active);
 reset("R945F");NativeRosterTick(10000);assert(!FallbackMulti()&&!g_tr.active);
 // Dedupe only owns the dynamically replaced identity; preserve future native members.
 reset("R0034");g_fb.owned=1;FallbackDedupe();assert(dedupeCalls==1);
 // Before the first kill, an unmarked body is the intro. Do nothing, even long after entry.
 reset("R0034");present[0]=1;NativeRosterTick(100);NativeRosterTick(100000);
 assert(wakeCalls==0&&!g_fb.seen&&!g_fb.woke&&!g_tr.active&&!spawns&&!finishes);
 // Inside the 8 s after a kill, do not wake. Once it passes, one wake, no second copy, no second wake.
 reset("R0034");present[0]=1;barOn[0]=1;present[1]=1;NativeRosterTick(100);
 deadFlags[0]=1;health[0]=0;NativeRosterTick(200);assert(g_fb.killed==1&&g_fb.nextAt==8200);
 NativeRosterTick(8199);assert(wakeCalls==0&&!(g_fb.seen&2)&&!g_tr.active&&!spawns);
 NativeRosterTick(8200);
 assert(wakeCalls==1&&(g_fb.seen&2)&&(g_fb.woke&2)&&!(g_fb.owned&2)&&g_fb.pose&&!g_tr.active&&!spawns&&!finishes);
 NativeRosterTick(8300);assert(wakeCalls==1&&!g_tr.active&&!finishes);
 // A script phase clears nextAt. No wake while it runs; one wake once it ends.
 reset("R0034");present[0]=1;barOn[0]=1;NativeRosterTick(100);
 deadFlags[0]=1;health[0]=0;NativeRosterTick(200);present[1]=1;
 g_np.active=1;g_fb.nextAt=0;NativeRosterTick(300);assert(wakeCalls==0&&!g_tr.active);
 g_np.active=0;NativeRosterTick(400);
 assert(wakeCalls==1&&(g_fb.woke&2)&&!(g_fb.owned&2)&&!g_tr.active&&!spawns&&!finishes);
 NativeRosterTick(500);assert(wakeCalls==1);
 // A shown bar holds the fight. A later unmarked body is left for the script.
 reset("R0034");present[0]=1;barOn[0]=1;present[1]=1;NativeRosterTick(100);NativeRosterTick(20100);
 assert(wakeCalls==0&&g_fb.seen==1&&!g_tr.active&&!spawns);
 // HP change is a real fight: no wake. The first sample was taken while another boss held the slot.
 reset("R0034");present[0]=1;barOn[0]=1;present[1]=1;health[1]=100;NativeRosterTick(100);
 deadFlags[0]=1;health[0]=0;health[1]=90;NativeRosterTick(200);
 assert(wakeCalls==0&&(g_fb.seen&2)&&!g_tr.active&&!spawns);
 // A hit on the player is a real fight even with no bar.
 reset("R0034");present[0]=1;g_fb.struck=1;NativeRosterTick(100);
 assert(wakeCalls==0&&g_fb.seen==1&&!g_tr.active&&!spawns);
 // An unresolved finisher still holds. The next unmarked body waits.
 reset("R0034");present[0]=1;barOn[0]=1;present[1]=1;NativeRosterTick(100);
 health[0]=0;NativeRosterTick(200);NativeRosterTick(4699);
 assert(!g_fb.killed&&wakeCalls==0&&!g_tr.active);
 NativeRosterTick(4700);assert(g_fb.killed==1&&wakeCalls==0&&!g_tr.active);
 NativeRosterTick(12699);assert(wakeCalls==0);NativeRosterTick(12700);assert(wakeCalls==1&&!g_tr.active&&!spawns);
 // A script phase owns the next body. A missing earlier member can still be requested; a later unmarked body cannot block that.
 reset("R0034");present[0]=1;g_np.active=1;NativeRosterTick(100);assert(wakeCalls==0&&!g_fb.seen&&!g_tr.active);
 reset("R0034");present[1]=1;g_fb.nextAt=100;NativeRosterTick(100);
 assert(wakeCalls==0&&g_tr.active&&(g_fb.owned&1)&&g_tr.chr==g_fb.row->chr[0]&&spawns==0);
 puts("PASS native roster: reverse identity order on 23 rosters, native grace, no duplicate next phase, final-only win, loss/quit, saved pose, stable death, phase pass-through, dormant wake");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);src=p/'native.c';exe=p/'native.exe'
 src.write_text(pre+header+state+'\n'+''.join(fn(x) for x in ['static int FallbackMulti(', 'static void FallbackTracked(uint32_t handle, uint32_t chr) {','static int RosterDefeat(', 'static int RosterNextIndex(', 'static int FallbackDedupe(void) {','static int FightFinish(', 'static int FallbackNext(', 'static void FallbackSpawn(', 'static void RosterCapturePose(', 'static void NativeRosterTick('])+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
