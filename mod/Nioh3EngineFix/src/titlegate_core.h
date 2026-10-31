#pragma once
#include <cstdint>
#include "gamerva.h"
namespace titlegate {
struct Sample {
    bool readable=false, title=false, queueEmpty=false, loading=true;
    uintptr_t manager=0, top=0, p30=0, p38=0, p40=0, p58=0;
    uint32_t state=~0u;
};
inline bool PressAny(const Sample& s) {
    return s.p30==s.top+0x30 && s.p38==s.p30 && s.p40==s.p30 && s.p58==0;
}
inline bool MenuIdle(const Sample& s) {
    return s.readable && s.title && s.state==3 && s.queueEmpty && !PressAny(s);
}
enum Verdict { Unreadable, NotTitle, Loading, State, Queue, Press, Settling, Candidate };
inline Verdict Reason(const Sample& s) {
    if(!s.readable)return Unreadable;
    if(!s.title)return NotTitle;
    if(s.loading)return Loading;
    if(s.state!=3)return State;
    if(!s.queueEmpty)return Queue;
    if(PressAny(s))return Press;
    return Settling;
}
struct Gate {
    bool tracking=false;uint64_t since=0,last=0;uintptr_t manager=0,top=0;
    // Live-reset PRESS ANY BUTTON lives inside the title object (title=1, state 3, queue empty, not loading):
    // its +30/+38/+40/+58 alternate between the self-pointer signature and all zero, so it never
    // stays Settling for a second. When set, Press counts as idle title for stability. Boot
    // press-any is a non-title object (title=0) and stays NotTitle; loading/state/queue still deny.
    bool admitPressAny=false;
    Verdict Update(const Sample& s,uint64_t now) {
        const Verdict reason=Reason(s);
        if(reason!=Settling && !(admitPressAny && reason==Press)){tracking=false;last=now;return reason;}
        if(!tracking || manager!=s.manager || top!=s.top || now<last || now-last>500){
            tracking=true;since=now;manager=s.manager;top=s.top;
        }
        last=now;
        return now-since>=1000?Candidate:reason;
    }
};
// Reader is injectable for offline fault/torn-read tests; production loads are SEH guarded.
template<class Reader> Sample Read(uintptr_t base,Reader& rd) {
    Sample s;uintptr_t m=0,b=0,e=0,t=0,id=0,q0=0,q1=0,gm=0;
    uint8_t loading=1;
    if(!rd(base+Grva(0x4709B50),m)||!m||!rd(m,b)||!rd(m+8,e)||!b||e<=b||e-b>512||((e-b)&7)||
       !rd(e-8,t)||!t||!rd(t+8,id)||!rd(t+0x10,s.state)||
       !rd(m+0x18,q0)||!rd(m+0x20,q1)||!rd(base+Grva(0x4759910),gm)||!gm||!rd(gm+0x58A,loading))return s;
    s.title=id==base+Grva(0x44C91D0);
    if(s.title && (!rd(t+0x30,s.p30)||!rd(t+0x38,s.p38)||!rd(t+0x40,s.p40)||!rd(t+0x58,s.p58)))return s;
    uintptr_t a=0;uint32_t st=0;uint8_t lb=1;
    // Recheck every predicate-bearing value, not just the object address.
    if(!rd(base+Grva(0x4709B50),a)||a!=m||!rd(m,a)||a!=b||!rd(m+8,a)||a!=e||
       !rd(e-8,a)||a!=t||!rd(t+8,a)||a!=id||!rd(t+0x10,st)||st!=s.state||
       !rd(m+0x18,a)||a!=q0||!rd(m+0x20,a)||a!=q1||
       !rd(base+Grva(0x4759910),a)||a!=gm||!rd(gm+0x58A,lb)||lb!=loading)return s;
    if(s.title && (!rd(t+0x30,a)||a!=s.p30||!rd(t+0x38,a)||a!=s.p38||
                  !rd(t+0x40,a)||a!=s.p40||!rd(t+0x58,a)||a!=s.p58))return s;
    s.readable=true;s.manager=m;s.top=t;s.queueEmpty=q0==q1;s.loading=loading!=0;
    return s;
}
}
