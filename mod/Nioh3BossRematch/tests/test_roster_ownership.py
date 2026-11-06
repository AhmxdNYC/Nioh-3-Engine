"""Compile production BR report ownership callbacks with mocked Training exports."""
from pathlib import Path
import subprocess, sys, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'mod/Nioh3BossRematch/src/bosslog.cpp').read_text()
def fn(sig):
 a=s.index(sig); b=s.index('{',a); e=b+1; depth=1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=r'''
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
using HMODULE=void*; using FARPROC=void(*)();
static bool pending, module=true, available=true;
static int starts,ends,phase,confirms,session;
static uint64_t token=123;
int Pending(){return pending;}
int Active(){return session;}
void Mark(int,int p,const char*){if(p==1)++starts;else{++ends;phase=p;}}
void MarkEx(int m,int p,const char*n,const char*){Mark(m,p,n);}
uint64_t Token(){return token;}
void Confirm(uint64_t t){assert(t==token);++confirms;}
HMODULE GetModuleHandleW(const wchar_t*){return module?(void*)1:nullptr;}
FARPROC GetProcAddress(HMODULE,const char*n){
 if(!strcmp(n,"Nioh3Training_RosterPending"))return available?(FARPROC)Pending:nullptr;
 if(!strcmp(n,"Nioh3Training_SessionActive"))return (FARPROC)Active;
 if(!strcmp(n,"Nioh3Training_FightMark"))return (FARPROC)Mark;
 if(!strcmp(n,"Nioh3Training_FightMarkEx"))return (FARPROC)MarkEx;
 if(!strcmp(n,"Nioh3Training_FightToken"))return (FARPROC)Token;
 if(!strcmp(n,"Nioh3Training_FightConfirmWin"))return (FARPROC)Confirm;
 assert(false);return nullptr;
}
namespace rematch {const char* FightLogName(){return "multi";}int FightLogMode(){return 1;}const char* FightLogId(){return "RTEST";}}
namespace bosslog {
std::atomic<int> g_logOwn{0},g_clearWin{0};std::atomic<uint64_t>g_logToken{0};std::atomic<uint32_t>g_logMission{0};
std::atomic<bool>g_active{true};std::atomic<double>g_endAt{0};
uint32_t RunningLogMission(){return 77;}
@PENDING@
@START@
@END@
}
namespace fight {double Now(){return 1;}
@CONFIRM@
}
void reset(){pending=false;module=available=true;starts=ends=confirms=session=0;bosslog::g_logOwn=0;bosslog::g_logToken=0;bosslog::g_logMission=0;}
int main(){
 reset();pending=true;bosslog::MaybeStart("boss");assert(starts==1&&bosslog::g_logToken==123);
 bosslog::MaybeStart("second phase");assert(starts==1&&bosslog::g_logToken==123&&bosslog::g_logMission==77);
 bosslog::g_clearWin=1;bosslog::MaybeEnd();assert(!ends&&bosslog::g_logOwn==1&&bosslog::g_clearWin==1);
 fight::ConfirmNativeWin(77);assert(!confirms&&bosslog::g_logToken==123);
 pending=false;bosslog::MaybeEnd();assert(ends==1&&phase==2&&bosslog::g_logOwn==0);
 bosslog::MaybeEnd();assert(ends==1);fight::ConfirmNativeWin(77);assert(confirms==1);fight::ConfirmNativeWin(77);assert(confirms==1);
 reset();bosslog::MaybeStart("single");bosslog::g_clearWin=1;bosslog::MaybeEnd();assert(starts==1&&ends==1&&phase==2);
 reset();bosslog::MaybeStart("single");bosslog::MaybeEnd();assert(ends==1&&phase==4);
 reset();pending=true;bosslog::MaybeStart("multi");pending=false;bosslog::MaybeEnd();assert(ends==1&&phase==4); // owner cancelled/loss is managed by Training
 reset();pending=true;session=1;bosslog::MaybeStart("dynamic");assert(!starts&&bosslog::g_logOwn==0); // Training owns row
 reset();available=false;pending=true;assert(!bosslog::RosterPending());bosslog::MaybeStart("old Training");bosslog::g_clearWin=1;bosslog::MaybeEnd();assert(starts==1&&ends==1&&phase==2);
 reset();module=false;assert(!bosslog::RosterPending());bosslog::MaybeStart("no Training");assert(!starts);
 puts("PASS BR roster ownership: first/repeated phase, suppressed end/clear, one final result, single, quit, Training owner, missing export/module");
}
'''
for key,sig in [('PENDING','bool RosterPending('),('START','void MaybeStart('),('END','void MaybeEnd('),('CONFIRM','void ConfirmNativeWin(')]:code=code.replace('@'+key+'@',fn(sig))
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'test.cpp';exe=p/'test.exe';src.write_text(code)
 subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
