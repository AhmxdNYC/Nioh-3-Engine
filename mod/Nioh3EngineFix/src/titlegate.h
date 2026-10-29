// Log-only candidate. No admission API, save writes, INI override or game calls.
#pragma once
#include "titlegate_core.h"
namespace titlegate {
struct GuardedRead {
    template<class T> bool operator()(uintptr_t address,T& out) const {
        __try {out=*reinterpret_cast<const volatile T*>(address);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out=T{};return false;}
    }
};
inline void Tick(uintptr_t base,void (*log)(const char*,...)) {
    static Gate gate;static int previous=-1;static uint32_t previousState=~0u;
    static uintptr_t previousTop=0;
    GuardedRead rd;const Sample s=Read(base,rd);const uint64_t now=GetTickCount64();
    const Verdict v=gate.Update(s,now);
    if(previous==(int)v && previousState==s.state && previousTop==s.top)return;
    previous=v;previousState=s.state;previousTop=s.top;
    static const char* names[]={"unreadable","not-title","loading","state","queue","press-any","settling","candidate"};
    log("TITLEGATE verdict=%s candidate=%d menuIdle=%d stableMs=%llu Enabled=0 logOnly=1 readable=%d title=%d state=%u queueEmpty=%d loading=%d top=%p +30=%p +38=%p +40=%p +58=%p",
        names[v],v==Candidate,MenuIdle(s),(unsigned long long)(gate.tracking?now-gate.since:0),
        s.readable,s.title,s.state,s.queueEmpty,s.loading,(void*)s.top,(void*)s.p30,(void*)s.p38,(void*)s.p40,(void*)s.p58);
}
}
