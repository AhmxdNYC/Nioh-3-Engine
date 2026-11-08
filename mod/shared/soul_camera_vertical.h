#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace soulcamera {
constexpr float Limit = 400.f;
struct Target {
    uint64_t player=0, seal=0; double session=-1; float sealY=0, playerY=0;
    float Sample(uint64_t p,uint64_t s,double t,float sy,float py,bool valid) {
        if(!valid || !p || !s || !std::isfinite(sy) || !std::isfinite(py)) { *this={}; return 0; }
        if(player!=p || seal!=s || session!=t) { player=p;seal=s;session=t;sealY=sy;playerY=py;return 0; }
        // Native camera already follows the player: add only the missing vertical displacement.
        float y=(sy-sealY)-(py-playerY);
        if(std::fabs(y)<.5f)return 0;
        return (std::clamp)(y,-Limit,Limit);
    }
};
struct Spring {
    float y=0,v=0;
    float Step(float target,float dt) {
        if(!std::isfinite(target))target=0;
        target=(std::clamp)(target,-Limit,Limit);
        if(!std::isfinite(dt)||dt<=0)return y;
        dt=(std::min)(dt,.1f);
        // Exact critically damped integration for a fixed target, smoothTime=0.15s.
        constexpr float omega=2.f/.15f;
        const float e=std::exp(-omega*dt),d=y-target,c=v+omega*d;
        y=target+(d+c*dt)*e;v=(v-omega*c*dt)*e;
        if(y>Limit){y=Limit;v=(std::min)(v,0.f);}if(y<-Limit){y=-Limit;v=(std::max)(v,0.f);}
        if(target==0 && std::fabs(y)<.01f && std::fabs(v)<.01f){y=v=0;}
        return y;
    }
};
}
