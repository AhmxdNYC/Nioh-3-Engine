"""#121854 (human v1.8.24): after a native Boss Match win the attempt is retired at the gate (#80065) so a new Fight now is accepted.
The gate page's Fight again must still resolve the same row afterwards (production NativeGateRow / pump revalidation), and that
ownership must end on a newer attempt, a mission change or a load. Fails on the pre-fix source (row no longer owned)."""
from pathlib import Path
import subprocess, sys, tempfile
src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'src/rematch.cpp'
s = src.read_text()
def fn(sig):
    if sig not in s: return ''
    a = s.index(sig); e = s.index('{', a) + 1; d = 1
    while d:
        d += (s[e] == '{') - (s[e] == '}'); e += 1
    return s[a:e]
def block(start, end):
    if start not in s: return ''
    a = s.index(start); return s[a:s.index(end, a)]
retire = fn('void RetireWonAttempt(')
# pre-fix: the Tick retire site is inline (Log + ToIdle) and the pump checks the open attempt only
legacy_retire = '' if retire else r'''void RetireWonAttempt(){Log("retired");ToIdle("won native attempt settled at the gate",true);}'''
owned = fn('bool NativeAgainOwned(') or r'''bool NativeAgainOwned(int row,const Snap& s){return !(row < 0 || (size_t)row >= g_rows.size() ||
 g_attemptPhase.load() != aActive || g_attempt.row != row || g_attempt.mission != s.ms28 || (!WavesGateNow() && g_rows[(size_t)row].missionId != NativeGateMission()));}'''
code = r'''
#include <atomic>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <vector>
using ULONGLONG=uint64_t;
struct Snap{bool msOk=true;uint32_t ms28=0;};
constexpr int aNone=0,aAwaitLoad=1,aActive=2,aReturning=3,aQuarantined=4,kIdle=0,kFight=1;
struct Row{uint32_t missionId=0;}; std::vector<Row> g_rows(8);
struct NativeAttempt{uint64_t sequence=0;uint32_t mission=0;int row=-1;} g_attempt;
uint64_t g_attemptSequence=0;
std::atomic<int> g_attemptPhase{aNone},g_state{kIdle},g_activeRow{-1};
uint32_t gateMission=0; Snap live; bool restoreFails=false;
void Log(const char*,...){}
uint32_t NativeGateMission(){return gateMission;}
bool WavesGateNow(){return false;}
Snap ReadSnap(){return live;}
void ToIdle(const char*,bool normal){g_attemptPhase=normal&&!restoreFails?aNone:aQuarantined;g_attempt=NativeAttempt{};g_state=kIdle;g_activeRow=-1;}
@RETIRED@
@RETIRE@
@GATEROW@
@OWNED@
void Active(int row,uint32_t m){g_attempt=NativeAttempt{};g_attempt.sequence=++g_attemptSequence;g_attempt.row=row;g_attempt.mission=m;
 g_attemptPhase=aActive;g_state=kFight;g_activeRow=row;g_rows[row].missionId=m;gateMission=m;live.ms28=m;live.msOk=true;}
int main(){
 Active(5,0x7B35);assert(NativeGateRow()==5&&NativeAgainOwned(5,live)); // open attempt at the gate (worked before)
 RetireWonAttempt();assert(g_attemptPhase==aNone&&g_activeRow==-1);
 assert(NativeGateRow()==5);              // #121854: was -1 -> 'native boss row is no longer owned'
 assert(NativeAgainOwned(5,live));        // the pump accepts the queued Fight again
 assert(!NativeAgainOwned(4,live));       // another row is not owned
 live.ms28=0x1234;assert(NativeGateRow()==-1&&!NativeAgainOwned(5,live));live.ms28=0x7B35; // mission changed
 gateMission=0;assert(NativeGateRow()==-1);gateMission=0x7B35; // Training no longer at its native gate
 g_state=kFight;assert(NativeGateRow()==-1);g_state=kIdle;  // a new Fight now is running
 assert(NativeGateRow()==5);
 ++g_attemptSequence;assert(NativeGateRow()==-1&&!NativeAgainOwned(5,live)); // newer attempt (AttemptBegin / AttemptReturning)
 Active(3,0xE18C);restoreFails=true;RetireWonAttempt();restoreFails=false;assert(NativeGateRow()==-1); // quarantined: no Fight again
 g_attemptPhase=aNone;Active(2,0xE18C);RetireWonAttempt();assert(NativeGateRow()==2);
 Active(6,0x4D8E);assert(NativeGateRow()==6);g_activeRow=-1;g_attemptPhase=aNone;g_state=kIdle;assert(NativeGateRow()==-1); // new attempt superseded the old win
 puts("PASS retired gate Fight again: a won native attempt retired at the gate still owns the same row's Fight again; newer attempt, mission change, quarantine and a new fight end it (#121854)");
}
'''
code = code.replace('@RETIRED@', block('std::atomic<int> g_retiredRow', 'int RetiredGateRow(') + fn('int RetiredGateRow(')) \
           .replace('@RETIRE@', retire or legacy_retire).replace('@GATEROW@', fn('int NativeGateRow(')).replace('@OWNED@', owned)
if retire:
    # the Tick site uses the helper and a load ends the retired gate; the pump uses the shared check
    assert 'g_attemptWonDone.exchange(false) && g_attemptPhase.load() == aActive) RetireWonAttempt();' in s
    assert 'if (s.st[2] && g_retiredRow.load() >= 0) g_retiredRow.store(-1);' in s
    assert 'kind == kReqNativeAgain && !NativeAgainOwned(row, s)' in s
with tempfile.TemporaryDirectory() as d:
    p = Path(d); c = p / 'test.cpp'; exe = p / 'test.exe'; c.write_text(code)
    subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-O0', '-UNDEBUG', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
