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
    float y=0,v=0,exitY=0,elapsed=0; bool wasActive=true;
    float Step(float target,float dt,bool active=true) {
        if(!std::isfinite(target))target=0;
        target=(std::clamp)(target,-Limit,Limit);
        if(!std::isfinite(dt)||dt<=0)return y;
        dt=(std::min)(dt,.1f);
        if(!active) {
            if(wasActive){exitY=y;elapsed=0;}
            elapsed+=dt;float t=(std::min)(elapsed/.2f,1.f);
            y=exitY*(1-t*t*(3-2*t));v=0;
        } else {
            // 20 ms time constant: unity gain, light lag with no overshoot.
            y=target+(y-target)*std::exp(-dt/.02f);v=0;
        }
        wasActive=active;return y;
    }
};
}
