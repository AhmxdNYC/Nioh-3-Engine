#pragma once
#include <cstdint>
// Presentation state only. Never owns native requests, recovery records or saves.
namespace transition_cover {
enum Phase : uint32_t { Off, AwaitPresent, AwaitNative, Loading, Ready, Failed, Cancelled };
struct Model {
    uint64_t token=0, deadline=0, requestedAt=0, attempt=0, settledAt=0, lastSample=0;
    uint32_t row=0, mission=0, start=0, placement=0;
    Phase phase=Off;
    bool presented=false, issued=false, bossEntry=false, placementEntry=false;
    const char* reason="";
    bool Active() const { return phase>=AwaitPresent && phase<=Loading; }
    bool Begin(uint64_t next,uint32_t r,uint32_t m,uint32_t s,uint32_t p,uint64_t now) {
        if(Active()||!next||!m||!s||!p)return false;
        *this={};token=next;row=r;mission=m;start=s;placement=p;
        requestedAt=now;deadline=now+90000;phase=AwaitPresent;reason="Preparing covered transition";return true;
    }
    void End(Phase state,const char* why){phase=state;reason=why;settledAt=0;}
    void Expire(uint64_t now){
        if(!Active())return;
        if(now<requestedAt || now>=deadline)End(Failed,"Transition timed out; native request/recovery remains owned");
        else if(phase==AwaitPresent && now-requestedAt>=5000)End(Failed,"Cover was not presented; no native request sent");
    }
    bool Presented(uint64_t id,uint64_t now){
        Expire(now);if(id!=token||phase!=AwaitPresent)return false;
        presented=true;phase=AwaitNative;reason="Starting requested boss";return true;
    }
    bool Issue(uint64_t sequence){
        if(phase!=AwaitNative||!presented||issued||!sequence)return false;
        issued=true;attempt=sequence;return true;
    }
    void Cancel(uint64_t id){if(id==token&&Active())End(Cancelled,"Transition cover cancelled; native request is not cancelled");}
    void Entry(uint64_t sequence,uint32_t m,uint32_t p,bool begin){
        if(!Active()||!issued||sequence!=attempt||m!=mission)return;
        if(begin)bossEntry=true;else if(p==placement)placementEntry=true;
    }
    void Observe(uint64_t now,uint64_t sequence,bool matched,bool uncertain,bool valid,bool exactMission,
                 bool loading,bool player,bool paused,bool fightActive,bool controlReady,bool bossReady){
        Expire(now);if(!Active()||!issued)return;
        if(uncertain||sequence>attempt){End(Failed,"Native request refused or uncertain; recovery remains owned");return;}
        if(sequence==attempt&&matched)phase=Loading;
        if(phase!=Loading){if(now-requestedAt>=10000)End(Failed,"Native request was not admitted; no automatic retry");return;}
        const bool continuous=lastSample&&now>=lastSample&&now-lastSample<=500;
        lastSample=now;
        if(sequence!=attempt||!continuous||!valid||!exactMission||loading||!player||paused||!fightActive||!controlReady||!bossReady||!bossEntry||!placementEntry){settledAt=0;return;}
        if(!settledAt)settledAt=now;
        if(now-settledAt>=500)End(Ready,"Requested boss and player control ready");
    }
};
}
