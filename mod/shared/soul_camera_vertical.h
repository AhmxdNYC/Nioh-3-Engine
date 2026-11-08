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
    float y=0,v=0,filtered=0,lastTarget=0,blendStart=0,elapsed=0,maxStep=0;
    uint64_t session=0; bool wasActive=false,blending=false,started=false,finished=false;
    float Step(float target,float dt,bool active=true,uint64_t id=1) {
        started=finished=false;
        if(!std::isfinite(target))target=0;
        target=(std::clamp)(target,-Limit,Limit);
        if(!std::isfinite(dt)||dt<=0)return y;
        dt=(std::min)(dt,.1f);
        if(active!=wasActive || (active && session!=id)) {
            blendStart=y;elapsed=0;blending=true;started=true;maxStep=0;
            wasActive=active;session=active?id:0;
        }
        // The fast follow filter survives both handovers. On disappearance retain
        // its last valid yokai target; exit blends from the actual applied offset.
        if(active)lastTarget=target;
        filtered=lastTarget+(filtered-lastTarget)*std::exp(-dt/.02f);
        const float previous=y;
        if(blending) {
            // First handover frame is exactly continuous, including a new press
            // interrupting an unfinished exit. Subsequent frames ease toward the
            // moving yokai target (enter) or player anchor (exit).
            if(!started)elapsed+=dt;
            float t=(std::min)(elapsed/.4f,1.f),w=t*t*(3-2*t);
            y=blendStart+((active?filtered:0)-blendStart)*w;
            if(t==1){blending=false;finished=true;}
        } else y=active?filtered:0;
        maxStep=(std::max)(maxStep,std::fabs(y-previous));v=0;return y;
    }
};
}
