"""Execute actual native roster poll/transitions with mocked game actors, offline."""
from pathlib import Path
import runpy, subprocess, sys, tempfile
base=runpy.run_path(str(Path(__file__).with_name('fallback_sequence_test.py')))
fn=base['fn']; state=base['state']; header=base['header']
pre=base['preamble'].replace('int active;} g_ar;', 'int active,world;} g_ar;')
pre=pre.replace('int bodyGone;} g_tr;', 'int bodyGone,active,arena,hasStart,round;float pp[4],bp[3],face;} g_tr;')
pre=pre.replace('static void NativePhaseEvent(int phase) {}','')
pre=pre.replace('int armed,active,ended,releasing;DWORD pendingAt;uint32_t killedAtArm;} g_np;',
 'int armed,started,active,ended,releasing;DWORD pendingAt;uint32_t killedAtArm;} g_np;')
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
static int barOn[16], wakeCalls, barCalls;
static int barRows;
static long long RosterBarRows(void){return barRows;}
static void RosterRequestBar(unsigned i){(void)i;barCalls++;}
static int RosterBarOn(uint64_t a){unsigned i=(unsigned)((unsigned char*)a-&actors[0][0])/512;return i<16?barOn[i]:0;}
static void RosterWakeMember(unsigned i, uint64_t a){(void)i;(void)a;wakeCalls++;}
'''
main=r'''
static void reset(const char*id){
 memset(&g_fb,0,sizeof g_fb);memset(&g_tr,0,sizeof g_tr);memset(&g_sp,0,sizeof g_sp);memset(&g_np,0,sizeof g_np);
 memset(present,0,sizeof present);memset(deadFlags,0,sizeof deadFlags);memset(barOn,0,sizeof barOn);
 for(int i=0;i<16;i++){health[i]=100;float p[3]={101,202,303};memcpy(actors[i]+0x110,p,sizeof p);}
 g_fb.row=FallbackRosterFind(id);g_fb.native=1;g_fl.open=0;finishes=spawns=duplicates=dedupeCalls=playerDead=wakeCalls=barCalls=0;g_dead_id=0;g_en.warped=1;
}
static void RunR0F7F(int killFirst){
 reset("R0F7F");
 assert(g_fb.row&&g_fb.row->count==4&&g_fb.row->chr[0]==274906u);
 g_np.armed=1;
 present[0]=1;barOn[0]=1;
 tick=1000;NativePhaseEvent(1);NativeRosterTick(1000);
 assert(g_np.started&&g_np.active&&(g_fb.seen&1)&&!g_tr.active&&!finishes&&spawns==0);
 if(!killFirst){tick=2000;NativePhaseEvent(2);assert(!g_np.active&&g_np.pendingAt==10000);}
 deadFlags[0]=1;health[0]=0;NativeRosterTick(2000);
 assert(g_fb.killed==1&&!g_fb.complete&&!finishes&&!g_tr.active&&spawns==0);
 if(killFirst){assert(!g_np.active&&g_np.pendingAt==10000);tick=2000;NativePhaseEvent(2);assert(g_np.pendingAt==10000);}
 else assert(!g_np.active&&g_np.pendingAt==10000);
 NativePhaseTick(2000);assert(!finishes);
 NativeRosterTick(3999);assert(!g_tr.active&&spawns==0&&!finishes);
 NativeRosterTick(4000);
 assert(g_tr.active&&g_tr.chr==g_fb.row->chr[1]&&g_tr.tCmd==4000&&g_tr.tRespawn==4001&&spawns==0&&(g_fb.owned&2));
 NativeRosterTick(4100);assert(g_tr.tCmd==4000&&spawns==0);
 tick=4001;FallbackSpawn();g_tr.tRespawn=0;assert(spawns==1);
 const DWORD killAt[3]={18000,34000,50000};
 const DWORD reqAt[2]={20000,36000};
 for(int n=0;n<3;n++){
  FallbackTracked(100u+(unsigned)n,g_tr.chr);g_dead_id=100u+(unsigned)n;tick=killAt[n];
  int more=FallbackNext(killAt[n]);
  NativeRosterTick(killAt[n]);NativePhaseTick(killAt[n]);
  if(n<2){
   assert(more&&!g_fb.complete&&!finishes&&spawns==n+1);
   NativeRosterTick(reqAt[n]-1);assert(g_tr.tCmd==(n==0?4000u:reqAt[0])&&spawns==n+1&&!finishes);
   NativeRosterTick(reqAt[n]);
   assert(g_tr.chr==g_fb.row->chr[n+2]&&g_tr.tCmd==reqAt[n]&&g_tr.tRespawn==reqAt[n]+1&&spawns==n+1);
   NativeRosterTick(reqAt[n]+50);assert(g_tr.tCmd==reqAt[n]&&spawns==n+1);
   tick=reqAt[n]+1;FallbackSpawn();g_tr.tRespawn=0;assert(spawns==n+2);
  }else{
   assert(!more&&g_fb.complete&&g_fb.killed==0xF&&finishes==1&&result==FL_WIN&&g_np.ended&&spawns==3&&g_np.started==1);
   NativePhaseTick(killAt[n]+25000);NativeRosterTick(killAt[n]+25000);assert(finishes==1&&spawns==3);
  }
 }
}
int main(void){
 // Native starts with the SECOND catalogue member: identity, not position. A shown bar means it is in the fight.
 reset("R0034");present[1]=1;barOn[1]=1;NativeRosterTick(100);assert(g_fb.seen==2&&g_fb.pose&&!wakeCalls);
 deadFlags[1]=1;health[1]=0;NativeRosterTick(200);assert(g_fb.killed==2&&!finishes&&!g_fb.complete);
 NativeRosterTick(2199);assert(!g_tr.active);NativeRosterTick(2200);
 assert(g_fb.index==0&&g_tr.chr==g_fb.row->chr[0]&&g_tr.bp[0]==101&&g_tr.bp[1]==202);
 FallbackSpawn();assert(spawns==1&&spawned==g_fb.row->chr[0]);
 FallbackTracked(99,g_tr.chr);g_dead_id=99;assert(!FallbackNext(9000)&&g_fb.complete);
 assert(FightFinish(FL_WIN)&&finishes==1);assert(!FightFinish(FL_WIN)&&finishes==1);
 // Native body is already up when the poll runs, including after the 2 s wait: no dynamic summon, one final win.
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
 // Inside the 2 s after a kill, do not wake. Once it passes, one wake, no second copy, no second wake.
 reset("R0034");present[0]=1;barOn[0]=1;present[1]=1;NativeRosterTick(100);
 deadFlags[0]=1;health[0]=0;NativeRosterTick(200);assert(g_fb.killed==1&&g_fb.nextAt==2200);
 NativeRosterTick(2199);assert(wakeCalls==0&&!(g_fb.seen&2)&&!g_tr.active&&!spawns);
 NativeRosterTick(2200);
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
 assert(wakeCalls==0&&g_fb.seen==1&&!g_tr.active&&!spawns&&!barCalls);
 // An unresolved finisher still holds. The next unmarked body waits.
 reset("R0034");present[0]=1;barOn[0]=1;present[1]=1;NativeRosterTick(100);
 health[0]=0;NativeRosterTick(200);NativeRosterTick(4699);
 assert(!g_fb.killed&&wakeCalls==0&&!g_tr.active);
 NativeRosterTick(4700);assert(g_fb.killed==1&&wakeCalls==0&&!g_tr.active);
 NativeRosterTick(6699);assert(wakeCalls==0);NativeRosterTick(6700);assert(wakeCalls==1&&!g_tr.active&&!spawns);
 // A script phase owns the next body. A missing earlier member can still be requested; a later unmarked body cannot block that.
 reset("R0034");present[0]=1;g_np.active=1;NativeRosterTick(100);assert(wakeCalls==0&&!g_fb.seen&&!g_tr.active);
 reset("R0034");present[1]=1;g_fb.nextAt=100;NativeRosterTick(100);
 assert(wakeCalls==0&&g_tr.active&&(g_fb.owned&1)&&g_tr.chr==g_fb.row->chr[0]&&spawns==0);
 // Script phase stays up after a real kill when the next body is not in the world. Request at 2 s, win grace stays 8 s, no win.
 reset("R0B0B");g_np.armed=1;g_np.active=1;g_np.killedAtArm=0x7;g_fb.killed=0x7;
 present[3]=1;barOn[3]=1;NativeRosterTick(1000);assert(g_np.active&&!g_np.pendingAt&&!g_tr.active);
 deadFlags[3]=1;health[3]=0;NativeRosterTick(2000);
 assert(!g_np.active&&g_np.pendingAt==10000&&g_fb.nextAt==4000&&(g_fb.killed&8)&&!g_tr.active&&!finishes);
 NativeRosterTick(3999);assert(!g_tr.active&&!finishes);
 NativeRosterTick(4000);assert(g_tr.active&&g_tr.chr==598115u&&!finishes);
 NativePhaseTick(4000);assert(!finishes);
 NativePhaseTick(10000);assert(!finishes);
 assert(!FightFinish(FL_WIN)&&!finishes);
 // A phase 1.3 s after the confirm owns the next member. No request at 2 s and no early win.
 reset("R0034");g_np.armed=1;present[1]=1;barOn[1]=1;NativeRosterTick(100);
 deadFlags[1]=1;health[1]=0;NativeRosterTick(200);assert(g_fb.nextAt==2200&&!g_tr.active&&!finishes);
 tick=1500;NativePhaseEvent(1);assert(g_np.active&&g_fb.nextAt==0);
 NativeRosterTick(2200);NativePhaseTick(2200);assert(!g_tr.active&&!spawns&&!wakeCalls&&!finishes);
 g_np.active=0;present[0]=1;barOn[0]=1;NativeRosterTick(2300);
 assert(!g_tr.active&&!spawns&&(g_fb.seen&1)&&!wakeCalls&&!finishes);
 // The request already went out. A native body of that identity on a later tick is not a second request.
 reset("R0034");present[1]=1;barOn[1]=1;NativeRosterTick(100);
 deadFlags[1]=1;health[1]=0;NativeRosterTick(200);NativeRosterTick(2200);
 assert(g_tr.active&&g_tr.chr==g_fb.row->chr[0]&&spawns==0);
 present[0]=1;barOn[0]=1;NativeRosterTick(3000);assert(g_tr.tCmd==2200&&spawns==0&&!finishes);
 duplicates=1;tick=3000;FallbackSpawn();assert(spawns==0&&dedupeCalls>=1);
 // Previous kills alone do not drop a phase that has not seen a new death. A body already here keeps the phase.
 reset("R0B0B");g_np.armed=1;g_np.active=1;g_np.killedAtArm=0x7;g_fb.killed=0x7;NativeRosterTick(5000);
 assert(g_np.active&&!g_np.pendingAt&&!g_tr.active);
 present[4]=1;g_fb.killed=0xF;NativeRosterTick(5100);assert(g_np.active&&wakeCalls==0&&!g_tr.active);
 // A script-started body can fight with no bar. One gauge request, no AI wake, no second spawn.
 // Before the phase and before any kill, the same hit does not get a bar (asserted above).
 reset("R0B0B");g_np.active=1;present[1]=1;NativeRosterTick(100);assert(!barCalls&&wakeCalls==0&&!(g_fb.seen&2));
 g_fb.struck|=2;NativeRosterTick(200);assert(barCalls==1&&(g_fb.barAsked&2)&&(g_fb.seen&2)&&wakeCalls==0&&!g_tr.active&&!finishes);
 NativeRosterTick(300);assert(barCalls==1&&wakeCalls==0);
 // Refused request: ask again after 1.5 s while no bar shows; with no other bar up at most 5 counted tries; a shown bar stops it.
 barRows=0;NativeRosterTick(1600);assert(barCalls==1);NativeRosterTick(1700);assert(barCalls==2&&g_fb.barTries[1]==2);
 barOn[1]=1;NativeRosterTick(3500);assert(barCalls==2);barOn[1]=0;
 for(DWORD t=5000;t<20000;t+=1600)NativeRosterTick(t);assert(barCalls==5&&g_fb.barTries[1]==5);
 // Failing timeline (pc2 #73428): another boss's bar row is up for 99 s, then it dies (rows 0); the body gets its counted tries after that.
 reset("R0B0B");g_np.active=1;present[1]=1;NativeRosterTick(100);g_fb.struck|=2;barRows=1;NativeRosterTick(200);assert(barCalls==1&&g_fb.barTries[1]==1);
 for(DWORD t=1800;t<99200;t+=1600)NativeRosterTick(t);assert(barCalls==4&&g_fb.barTries[1]==1&&g_fb.barBusy[1]==3);
 barRows=0;NativeRosterTick(100000);assert(barCalls==5&&g_fb.barTries[1]==2);
 barRows=1;NativeRosterTick(101700);assert(barCalls==5);barRows=0;
 for(DWORD t=103400;t<120000;t+=1600)NativeRosterTick(t);assert(barCalls==8&&g_fb.barTries[1]==5);
 RunR0F7F(1);RunR0F7F(0);
 puts("PASS native roster: reverse identity order on 23 rosters, native grace, no duplicate next phase, final-only win, loss/quit, saved pose, stable death, phase pass-through, dormant wake, kill releases an empty boss mode, fighting body gets one bar, R0F7F one mode");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);src=p/'native.c';exe=p/'native.exe'
 src.write_text(pre+header+state+'\n'+''.join(fn(x) for x in ['static void NativePhaseEvent(', 'static int FallbackMulti(', 'static void FallbackTracked(uint32_t handle, uint32_t chr) {','static int RosterDefeat(', 'static int RosterNextIndex(', 'static int FallbackDedupe(void) {','static int FightFinish(', 'static int FallbackNext(', 'static void FallbackSpawn(', 'static void RosterCapturePose(', 'static void NativeRosterTick(', 'static void NativePhaseTick('])+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
