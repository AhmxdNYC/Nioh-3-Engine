"""Production native phase grace regression: no resolved roster required."""
from pathlib import Path
import runpy, subprocess,sys,tempfile
b=runpy.run_path(str(Path(__file__).with_name('fallback_sequence_test.py')))
fn=b['fn'];s=b['s'];pre=b['preamble']
pre=pre.replace('static struct {int armed,active,ended,releasing;DWORD pendingAt;uint32_t killedAtArm;} g_np;','').replace('static void NativePhaseEvent(int phase) {}','').replace('static int NativeNamedLive(void){return 0;}','')
pre=pre.replace('int bodyGone;} g_tr;', 'int bodyGone,active;} g_tr;')
pre+=r'''
#define N3T_API
#define AcquireSRWLockShared(x) ((void)0)
#define ReleaseSRWLockShared(x) ((void)0)
typedef struct {char id[16];} ArenaRow;
static int live,playerDead,starts;
static int PlayerDeadNow(void){return playerDead;}
static int NativeNamedLive(void){return live;}
static void FightBegin(int mode,const char*n,const char*id,uint32_t chr){starts++;g_fl.open=0;}
'''
a=s.index('static struct { const ArenaRow* row;');np=s[a:s.index('\n',a)]
main=r'''
static ArenaRow row={"R0C12"};
static void reset(const char*id){
 memset(&g_fb,0,sizeof g_fb);memset(&g_np,0,sizeof g_np);memset(&g_tr,0,sizeof g_tr);
 g_fb.row=FallbackRosterFind(id);g_np.armed=1;g_np.row=&row;g_fl.open=-1;
 finishes=starts=live=playerDead=0;tick=100;
}
int main(void){
 const char*ids[]={"R0C12","R945F","single-row"};
 for(int k=0;k<3;k++){
  reset(ids[k]);assert(!FallbackMulti());
  NativePhaseEvent(1);Nioh3Training_FightMarkEx(1,1,"boss",ids[k]);assert(starts==1&&g_np.logOpen);
  tick=1000;NativePhaseEvent(2);Nioh3Training_FightMark(1,2,"");assert(!finishes&&g_np.pendingAt==9000);
  // pc2 evidence: next phase begins 1.3 seconds after the first kill.
  tick=2300;NativePhaseEvent(1);Nioh3Training_FightMarkEx(1,1,"boss",ids[k]);
  assert(starts==1&&g_np.active&&!g_np.pendingAt&&Nioh3Training_RosterPending());
  NativePhaseTick(10000);assert(!finishes);
  tick=11000;NativePhaseEvent(2);NativePhaseTick(18999);assert(!finishes);
  live=1;NativePhaseTick(19000);assert(!finishes); // named boss still alive at timeout
  g_tr.active=1;g_tr.tWin=11000;live=0;NativePhaseTick(19001);assert(finishes==1&&result==FL_WIN&&g_np.ended&&g_tr.tWin==11000); // grace never shifts an existing retry/menu timer
  NativePhaseTick(30000);assert(finishes==1);
 }
 reset("R0C12");NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");tick=1000;NativePhaseEvent(2);
 assert(FightFinish(FL_QUIT)&&finishes==1&&result==FL_WIN);NativePhaseTick(20000);assert(finishes==1); // real kill + Return before timeout
 reset("R0C12");NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");tick=1000;NativePhaseEvent(2);
 live=1;assert(FightFinish(FL_QUIT)&&result==FL_QUIT); // another named boss lives
 reset("R0C12");NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");tick=1000;NativePhaseEvent(2);tick=2300;NativePhaseEvent(1);
 assert(FightFinish(FL_QUIT)&&result==FL_QUIT); // quit during resumed phase
 reset("R0034");NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");tick=1000;NativePhaseEvent(2);
 assert(FightFinish(FL_QUIT)&&result==FL_QUIT); // roster has another named member
 reset("single-row");NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");
 assert(FightFinish(FL_QUIT)&&result==FL_QUIT); // no confirmed phase end
 reset("R945F");NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");tick=1000;NativePhaseEvent(2);
 playerDead=1;NativePhaseTick(10000);assert(finishes==1&&result==FL_LOSS);
 reset("R0034");g_fb.native=1;NativePhaseEvent(1);Nioh3Training_FightMark(1,1,"boss");
 tick=1000;NativePhaseEvent(2);NativePhaseTick(10000);assert(!finishes); // resolved roster incomplete
 g_fb.complete=1;NativePhaseTick(10001);assert(finishes==1);
 reset("single-row");g_np.armed=0;g_fl.open=0;assert(FightFinish(FL_WIN)&&finishes==1); // ordinary nonnative path unchanged
 puts("PASS native phase: unresolved 1.3s restart keeps one record, full 8s grace, live boss veto, final once, early-exit win, live/incomplete/resumed quits, loss, immediate retry timer, nonnative pass-through");
}
'''
# Production dispatch and reset assertions supplement the executable transitions.
assert 'NativePhaseMatches()' in fn('N3T_API void Nioh3Training_NativePhase(')
assert 'mission == g_np.row->mission' in fn('static int NativePhaseMatches(void) {')
assert 'token == Nioh3Training_FightToken()' in fn('N3T_API void Nioh3Training_FightConfirmWin(')
assert 'memset(&g_np, 0, sizeof g_np)' in fn('static void TrainRetry(')
assert 'g_np.ended = 1' in fn('static int FightFinish(')
assert fn('static void TrainRetry(').index('FightFinish(PlayerDeadNow()') < fn('static void TrainRetry(').index('memset(&g_np')
assert 'if (g_np.armed && !g_np.ended) { FightFinish(FL_WIN); return; }' not in fn('static void TrainTick(')
assert 'if (g_tr.active) g_tr.tWin = now' in fn('static void NativeRosterTick(')
assert 'NativePhaseTick(now)' in fn('static void EnterTick(')
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);src=p/'phase.c';exe=p/'phase.exe'
 src.write_text(pre+b['header']+b['state']+'\n'+np+'\n'+''.join(fn(x) for x in ['static int FallbackMulti(', 'static void NativePhaseEvent(', 'static int FightFinish(', 'N3T_API int Nioh3Training_RosterPending(', 'static int RosterLogOpen(', 'N3T_API void Nioh3Training_FightMark(', 'N3T_API void Nioh3Training_FightMarkEx(', 'static void NativePhaseTick('])+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
