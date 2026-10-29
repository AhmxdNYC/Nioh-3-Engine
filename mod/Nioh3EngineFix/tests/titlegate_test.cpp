#include "../src/titlegate_core.h"
#include <cstdio>
#include <cstdlib>
#include <map>
using namespace titlegate;
int checks=0;
void check(bool b){++checks;if(!b){printf("FAIL %d\n",checks);exit(1);}}
struct Memory {
    std::map<uintptr_t,uint64_t> mem;int calls=0,fail=0,change=0;
    template<class T> bool operator()(uintptr_t p,T& out){
        ++calls;if(calls==fail||!mem.count(p))return false;
        out=(T)(mem[p]+(calls==change?1:0));return true;
    }
};
Memory fixture(){
    Memory r;r.mem={{0x4709B50,0x1000},{0x1000,0x2000},{0x1008,0x2008},
        {0x2000,0x3000},{0x3008,0x44C91D0},{0x3010,3},{0x1018,0x4000},
        {0x1020,0x4000},{0x4759910,0x5000},{0x558A,0},
        {0x3030,0x6000},{0x3038,0x6010},{0x3040,0x6020},{0x3058,1}};return r;
}
int main(){
    auto r=fixture();auto s=Read(0,r);const int reads=r.calls;
    check(s.readable&&MenuIdle(s));
    for(int i=1;i<=reads;++i){auto f=fixture();f.fail=i;check(!Read(0,f).readable);}
    // Mutate each recheck, proving changes to all admission fields fail closed.
    for(int i=15;i<=reads;++i){auto f=fixture();f.change=i;check(!Read(0,f).readable);}
    Gate g;check(g.Update(s,0)==Settling);check(g.Update(s,100)==Settling);
    for(int i=200;i<1000;i+=100)check(g.Update(s,i)==Settling);
    check(g.Update(s,1000)==Candidate);
    auto press=s;press.p30=press.p38=press.p40=s.top+0x30;press.p58=0;
    check(PressAny(press)&&!MenuIdle(press));check(g.Update(press,1100)==Press);
    check(g.Update(s,1200)==Settling);
    auto quit=s;quit.state=10;check(g.Update(quit,1300)==State);
    auto system=s;system.state=9;check(g.Update(system,1400)==State);
    for(uint32_t state=0;state<20;++state)if(state!=3){auto t=s;t.state=state;check(!MenuIdle(t)&&g.Update(t,1500)==State);}
    auto loading=s;loading.loading=true;check(g.Update(loading,1600)==Loading);
    auto gameplay=s;gameplay.title=false;check(!MenuIdle(gameplay)&&g.Update(gameplay,1700)==NotTitle);
    auto queued=s;queued.queueEmpty=false;check(!MenuIdle(queued)&&g.Update(queued,1800)==Queue);
    auto bad=s;bad.readable=false;check(g.Update(bad,1900)==Unreadable);
    check(g.Update(s,2000)==Settling);check(g.Update(s,3000)==Settling); // polling gap resets
    check(g.Update(s,2900)==Settling); // clock rollback resets
    auto moved=s;moved.top+=0x100;check(g.Update(moved,3000)==Settling);
    moved.manager+=0x100;check(g.Update(moved,3100)==Settling);
    for(int i=3200;i<4100;i+=100)check(g.Update(moved,i)==Settling);
    check(g.Update(moved,4100)==Candidate);
    // Exact requested discriminator: any changed field breaks the press-any signature.
    for(int i=0;i<4;++i){auto t=press;uintptr_t* fields[]={&t.p30,&t.p38,&t.p40,&t.p58};++*fields[i];check(MenuIdle(t));}
    for(auto address:{uintptr_t(0x1000),uintptr_t(0x1008),uintptr_t(0x2000),uintptr_t(0x4709B50),uintptr_t(0x4759910)}){
        auto f=fixture();f.mem[address]=0;check(!Read(0,f).readable);
    }
    // Save-action gate: live-reset PRESS ANY BUTTON (log 2026-10-28) flickers between the self-pointer
    // signature and all-zero fields on the same title object; it must become a candidate after 1 s ...
    {
        Gate a;a.admitPressAny=true;auto zero=s;zero.p30=zero.p38=zero.p40=zero.p58=0;
        auto pab=press;check(PressAny(pab)&&!PressAny(zero));
        for(int i=0;i<1000;i+=100)check(a.Update(i%200?zero:pab,10000+i)==(i%200?Settling:Press));
        check(a.Update(pab,11000)==Candidate);check(a.Update(zero,11100)==Candidate);
        check(a.Update(s,11200)==Candidate); // pressing through to the main menu keeps the same object
        // ... while every non-title/busy reason still denies and resets stability.
        auto deny=[&](Sample t,Verdict want,uint64_t at){
            check(a.Update(t,at)==want);for(uint64_t n=at+100;n<at+1100;n+=100)check(a.Update(pab,n)==Press);
            check(a.Update(pab,at+1100)==Candidate);
        };
        auto pl=pab;pl.loading=true;deny(pl,Loading,12000);
        auto pn=pab;pn.title=false;deny(pn,NotTitle,14000);
        auto pq=pab;pq.queueEmpty=false;deny(pq,Queue,16000);
        auto pu=pab;pu.readable=false;deny(pu,Unreadable,18000);
        for(uint32_t state=0;state<20;++state)if(state!=3){auto t=pab;t.state=state;deny(t,State,20000+state*2000);}
        check(a.Update(pab,59200)==Candidate);
        auto pm=pab;pm.top+=0x100;pm.p30=pm.p38=pm.p40=pm.top+0x30; // object replaced: restart the second
        for(uint64_t n=59300;n<60300;n+=100)check(a.Update(pm,n)==Press);
        check(a.Update(pm,60300)==Candidate);
        check(a.Update(pm,60900)==Press); // >500 ms polling gap restarts
        // Default (log-only TITLEGATE) gate keeps press-any strictly denied.
        Gate strict;for(int i=0;i<=3000;i+=100)check(strict.Update(pab,i)==Press);
    }
    printf("PASS %d checks; %d guarded reads/rechecks per title sample\n",checks,reads);
}
