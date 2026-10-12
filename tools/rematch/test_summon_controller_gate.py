"""Actual frozen paused-menu Summon regression; no ASI edits/game/native calls."""
from pathlib import Path
import subprocess, tempfile
ROOT=Path(r'D:/nioh3-wt/training-yamagata-hold')
assert subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()=='dbd6752b0b54c51e037a2999976ce94a348ab676'
assert subprocess.run(['git','-C',str(ROOT),'diff','--quiet','HEAD','--','mod']).returncode==0
source=(ROOT/'mod/Nioh3BossRematch/src/rematch.cpp').read_text()
start=source.index('        if(kind==kReqPracticeSummon || kind==kReqPracticeCancel) {')
end=source.index('} else if (kind == kReqExit',start)
branch=source[start:end]+'}'
harness=(ROOT/'mod/Nioh3BossRematch/tests/training_entry_hold_harness.cpp').as_posix()
body=r'''
void ConsumeActualSummonBranch(){const uint32_t r=g_req.exchange(0),kind=r&0xFF;g_pend=r;const Snap s=ReadSnap();
BRANCH
}
void SelectSummon(){g_uiPages.page=uPractice;g_sel=1;g_req=0;g_pend=0;}
int main(){int checks=0;
 Seed();SelectSummon();CHECK(!Query());
 const auto witnessed=g_hold.boxAt;
 gm[0x588+3]=1;g_hold.boxAt=witnessed-2000; // genuine observation aged while script stopped polling
 const auto aged=g_hold.boxAt;g_lastTick=Ms();HoldTick(ReadSnap(),Ms());
 CHECK(g_hold.phase==training_entry_hold::WaitingAtBox && g_hold.boxAt==aged);
 CHECK(!UiActHosted());CHECK((g_req.load()&0xFF)==kReqPracticeSummon);
 ConsumeActualSummonBranch();CHECK(g_req==0 && g_pend==0);
 CHECK(g_hold.phase==training_entry_hold::WaitingAtBox && g_hold.boxAt==aged);
 CHECK(!HoldRelease()); // ownership preservation never manufactures fresh evidence
 gm[0x588+3]=0;g_lastTick=Ms();CHECK(!Query());
 CHECK(g_hold.boxAt>aged && g_hold.phase==training_entry_hold::WaitingAtBox && !g_req);
 // Frozen code lost the intent: only a second selection after a real poll works.
 CHECK(!UiActHosted());ConsumeActualSummonBranch();
 CHECK(g_hold.phase==training_entry_hold::Released);CHECK(Query()==1);CHECK(!HoldRelease());
 // Fresh paused polling is distinct, if the native script actually produces it.
 Seed();SelectSummon();gm[0x588+3]=1;CHECK(!Query());
 CHECK(!UiActHosted());ConsumeActualSummonBranch();CHECK(g_hold.phase==training_entry_hold::Released);
 // Required source-of-truth negatives for queued-intent integration.
 Seed();CHECK(!Query());g_hold.boxAt=Ms()-2000;stock=0;CHECK(!Query());CHECK(!HoldRelease());
 Seed();CHECK(!Query());g_attemptPublishedSequence=2;CHECK(Query()==1);CHECK(!HoldRelease());
 Seed();CHECK(!Query());addon::failRead=(uint64_t)gm+0x588;CHECK(Query()==1);addon::failRead=0;CHECK(!HoldRelease());
 Seed();CHECK(!Query());g_hold.deadline=Ms();CHECK(Query()==1);CHECK(!HoldRelease());
 Seed();CHECK(!Query());HoldStop("explicit cancel");CHECK(Query()==1);CHECK(!HoldRelease());
 printf("PASS %d actual-source checks: stale paused selection consumed/refused; only real fresh stock-true observation permits release; no playable claim\n",checks);
}
'''.replace('BRANCH',branch)
with tempfile.TemporaryDirectory(prefix='n3-summon-controller-') as temp:
 out=Path(temp);cpp=out/'summon.cpp';exe=out/'summon.exe'
 cpp.write_text('#define main frozen_baseline_main\n#include "'+harness+'"\n#undef main\n'+body)
 setup=Path(r'C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
 cmd=f'call "{setup}" >nul && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX /DUNICODE /D_UNICODE "{cpp}" /Fo"{out / "summon.obj"}" /Fe"{exe}" /link kernel32.lib user32.lib'
 result=subprocess.run('cmd /d /s /c "'+cmd+'"',capture_output=True,text=True,timeout=60)
 assert result.returncode==0,result.stdout+result.stderr
 result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
 print(result.stdout.strip());assert result.returncode==0,result.stdout+result.stderr
