// Approved #40210 / a62fa590 geometry, 35px native icon. History d-pad only;
// left-stick direction arrows remain in their original separate column.
#pragma once
#include "hint_shapes.h"
#include <vector>
namespace inputhist_dpad {
struct P { float x,y; };
static const P cross[]={{13,3},{22,3},{23,4},{23,12},{31,12},{32,13},{32,22},{31,23},{23,23},{23,31},{22,32},{13,32},{12,31},{12,23},{4,23},{3,22},{3,13},{4,12},{12,12},{12,4}};
static const P key[]={{14,3},{21,3},{22,4},{22,11.5f},{20,14},{15,14},{13,11.5f},{13,4}};
inline float Sd(const P* p,int n,float x,float y) {
    bool inside=false;float dist=1e9f;
    for(int i=0,j=n-1;i<n;j=i++) {
        if(((p[i].y>y)!=(p[j].y>y)) && x<(p[j].x-p[i].x)*(y-p[i].y)/(p[j].y-p[i].y)+p[i].x)inside=!inside;
        dist=std::min(dist,hint_shapes::Seg(x,y,p[j].x,p[j].y,p[i].x,p[i].y));
    }
    return inside?-dist:dist;
}
inline uint32_t Sample(int style,unsigned mask,float x,float y) {
    constexpr uint32_t base=0xff413733, edge=0xffaa9d96, lit=0xfff5f2f1;
    uint32_t c=0;
    if(!style) {float d=Sd(cross,20,x,y);if(d<=.625f)c=d>=-.625f?edge:base;}
    static const unsigned bits[]={1,8,2,4}; // XInput up/right/down/left
    for(int arm=0;arm<4;++arm) {
        float u=x-17.5f,v=y-17.5f;
        for(int r=0;r<arm;++r){float old=u;u=v;v=-old;}
        u+=17.5f;v+=17.5f;
        if(style) {float d=Sd(key,8,u,v);if(d<=.575f)c=d>=-.575f?edge:(mask&bits[arm]?lit:base);}
        else if((mask&bits[arm]) && u>=13.25f && u<=21.75f && v>=4.25f && v<=13.f)c=lit;
    }
    return c;
}
struct Run { int x,y,w;uint32_t c; };
struct Cache { float size=0;std::vector<Run> runs; };
inline void Draw(int style,unsigned mask,float bx,float by,float size) {
    // Present serializes OSD draws. Cache 2 skins x 16 held masks; never rasterize each frame.
    static Cache cache[2][16];
    Cache& entry=cache[style?1:0][mask&15];
    if(entry.size!=size) {
    entry.runs.clear();entry.size=size;
    const int n=(int)std::ceil(size);
    for(int y=0;y<n;++y) {
        int start=0;uint32_t run=0;
        for(int x=0;x<=n;++x) {
            uint32_t c=0;
            if(x<n) {
                unsigned a=0,r=0,g=0,b=0;
                for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx) {
                    uint32_t p=Sample(style,mask,(x+(sx+.5f)/4)*35.f/size,(y+(sy+.5f)/4)*35.f/size);
                    if(p){++a;r+=p&255;g+=(p>>8)&255;b+=(p>>16)&255;}
                }
                if(a)c=(((a*255+8)/16)<<24)|(((b+a/2)/a)<<16)|(((g+a/2)/a)<<8)|((r+a/2)/a);
            }
            if(c!=run || x==n) {if(run)entry.runs.push_back({start,y,x-start,run});start=x;run=c;}
        }
    }
    }
    for(const Run& r:entry.runs)osd::DrawRect(bx+r.x,by+r.y,(float)r.w,1,r.c);
}
}
