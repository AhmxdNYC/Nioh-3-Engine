#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace soulcamera {
constexpr float Limit = 100000.f; // invalid/teleport sanity bound, not a jump-height cap
struct Target {
    uint64_t player=0, seal=0; double session=-1; float sealY=0, playerY=0;
    float Sample(uint64_t p,uint64_t s,double t,float sy,float py,bool valid) {
        if(!valid || !p || !s || !std::isfinite(sy) || !std::isfinite(py)) { *this={}; return 0; }
        player=p;seal=s;session=t;sealY=sy;playerY=py;
        return (std::clamp)(sy-py,-Limit,Limit);
    }
};
struct Spring {
    float y=0,v=0,filtered=0,filterVelocity=0,lastTarget=0,blendStart=0,blendVelocity=0,elapsed=0,maxStep=0;
    float pendingTarget=0; unsigned rejectedSpikes=0;
    uint64_t session=0; bool wasActive=false,blending=false,started=false,finished=false,pending=false,hasTarget=false;
    float Step(float target,float dt,bool active=true,uint64_t id=1) {
        started=finished=false;
        if(!std::isfinite(target))target=lastTarget;
        target=(std::clamp)(target,-Limit,Limit);
        if(!std::isfinite(dt)||dt<=0)return y;
        dt=(std::min)(dt,.1f);
        if(active!=wasActive || (active && session!=id)) {
            blendStart=y;blendVelocity=v;elapsed=0;blending=true;started=true;maxStep=0;
            wasActive=active;session=active?id:0;
            pending=false;hasTarget=false;
            if(active)rejectedSpikes=0;
        }
        // A >80-unit isolated sample is held for one frame. A persistent new
        // height is accepted on confirmation, then eased by the spring (no cap).
        if(active) {
            if(!hasTarget || std::fabs(target-lastTarget)<=80.f ||
               (pending && std::fabs(target-pendingTarget)<=80.f)) {
                lastTarget=target;pending=false;hasTarget=true;
            } else {pendingTarget=target;pending=true;++rejectedSpikes;}
        }
        // Exact critically damped update: a stationary step's error halves in
        // 0.25 s ((1+w*t)*exp(-w*t)=0.5). Preserve velocity across target changes.
        if(active) {
            constexpr float omega=1.67834699f/.25f;
            const float error=filtered-lastTarget, j=filterVelocity+omega*error;
            const float decay=std::exp(-omega*dt);
            filtered=lastTarget+(error+j*dt)*decay;
            filterVelocity=(filterVelocity-omega*j*dt)*decay;
        }
        const float previous=y;
        if(blending) {
            if(!started)elapsed+=dt;
            const float t=(std::min)(elapsed/.7f,1.f);
            const float w=t*t*t*(t*(6*t-15)+10);
            const float dw=30*t*t*(t-1)*(t-1)/.7f;
            const float goal=active?filtered:0, goalV=active?filterVelocity:0;
            // Smootherstep handover plus a dying tangent preserves the applied
            // velocity on interrupted enter/exit, not just the position.
            y=blendStart+(goal-blendStart)*w+blendVelocity*elapsed*(1-w);
            v=goalV*w+(goal-blendStart)*dw+blendVelocity*(1-w-elapsed*dw);
            if(t==1){blending=false;finished=true;}
        } else {y=active?filtered:0;v=active?filterVelocity:0;}
        maxStep=(std::max)(maxStep,std::fabs(y-previous));return y;
    }
};
}
