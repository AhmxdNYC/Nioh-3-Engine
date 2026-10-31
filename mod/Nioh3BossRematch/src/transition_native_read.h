#pragma once
#include <cstddef>
#include "gamerva.h"
#include <cstdint>
#include <cstring>
#include <limits>

// Passive, game-thread-only observations. Caller validates native code anchors
// and its attempt/mission epoch. Equality is not a lifetime lock or ABA proof.
namespace transition_native_read {
using Read = bool (*)(void*, uint64_t, void*, size_t);
inline bool Bytes(Read read, void* ctx, uint64_t p, void* out, size_t n) {
    return read && p && n && n <= UINT64_MAX - p && read(ctx, p, out, n);
}
template<class T> inline bool At(Read read, void* ctx, uint64_t p, uint64_t offset, T& value) {
    return p && offset <= UINT64_MAX - p && Bytes(read, ctx, p + offset, &value, sizeof(value));
}
struct Control {
    bool known = false, clear = false;
    uint64_t manager = 0;
};
struct Actor {
    bool known = false, present = false, algoReady = false;
    uint32_t placement = 0;
    uint64_t wrapper = 0, live = 0, component = 0, algo = 0;
};
struct Life { bool known = false, notDead = false; };
struct Player { bool known=false,notDead=false; uint64_t live=0; };
namespace detail {
struct ControlSample {
    uint64_t manager = 0, primary = 0, count = 0, begin = 0, end = 0;
    uint32_t aggregate = 0;
    uint8_t reason = 0, pairs[512]{}, secondary[4096]{};
};
inline bool ControlOnce(uint64_t base, Read read, void* ctx, ControlSample& s) {
    if (!At(read,ctx,base,0x45BBD58,s.manager) || !s.manager ||
        !At(read,ctx,s.manager,0x18,s.primary) || !At(read,ctx,s.manager,0x20,s.count) ||
        !At(read,ctx,s.manager,0,s.begin) || !At(read,ctx,s.manager,8,s.end) ||
        !At(read,ctx,s.manager,0x30,s.reason) || !At(read,ctx,s.manager,0x34,s.aggregate)) return false;
    if (!s.count || s.count > 256 || s.end < s.begin || (s.end-s.begin)%16 || s.end-s.begin > sizeof(s.secondary)) return false;
    if (!Bytes(read,ctx,s.primary,s.pairs,size_t(s.count*2))) return false;
    if (s.end != s.begin && !Bytes(read,ctx,s.begin,s.secondary,size_t(s.end-s.begin))) return false;
    bool found = false;
    for (size_t i=0;i<s.count;++i) {
        if (s.pairs[2*i+1]>1 || (i && s.pairs[2*i]<=s.pairs[2*i-2])) return false;
        found |= s.pairs[2*i]==0x48;
    }
    return found;
}
struct Node { uint64_t index=0, wrapper=0; uint32_t key=0; };
struct ActorSample {
    uint64_t outer=0, context=0, map=0, table=0, count=0;
    uint64_t wrapper=0, live=0, component=0, algo=0;
    int32_t active=0;
    uint32_t visited=0;
    Node nodes[24]{};
};
inline bool ActorOnce(uint64_t base, uint32_t placement, Read read, void* ctx, ActorSample& s) {
    if (!placement || !At(read,ctx,base,0x45B9E40,s.outer) || !s.outer ||
        !At(read,ctx,s.outer,0,s.context) || !s.context ||
        !At(read,ctx,s.context,0x48,s.map) || !s.map ||
        !At(read,ctx,s.map,0,s.table) || !At(read,ctx,s.map,8,s.count)) return false;
    if (s.count>65536 || (s.count && (!s.table || s.count>(UINT64_MAX-s.table)/16))) return false;
    uint64_t lo=0,hi=s.count;
    auto node = [&](uint64_t index, Node& n) {
        n.index=index;
        return At(read,ctx,s.table,index*16,n.key) && At(read,ctx,s.table,index*16+8,n.wrapper);
    };
    while (lo<hi) {
        if (s.visited>=22) return false;
        Node& n=s.nodes[s.visited++]; const uint64_t mid=lo+(hi-lo)/2;
        if (!node(mid,n)) return false;
        if (n.key<placement) lo=mid+1; else hi=mid;
    }
    if (lo==s.count) return true;
    Node& n=s.nodes[s.visited++];
    if (!node(lo,n)) return false;
    if (n.key!=placement) return true;
    s.wrapper=n.wrapper;
    if (!s.wrapper) return true;
    if (!At(read,ctx,s.wrapper,0,s.live)) return false;
    if (!s.live) return true;
    if (!At(read,ctx,s.live,0x3B0,s.component)) return false;
    if (!s.component) return true;
    if (!At(read,ctx,s.component,0x50,s.algo)) return false;
    if (!s.algo) return true;
    return At(read,ctx,s.algo,0x1574,s.active);
}
struct LifeSample {
    ActorSample actor{};
    uint64_t statusObject=0,state=0,vtable=0,getter=0,primary=0,data=0,secondary=0,secondaryData=0,bits=0;
    uint32_t kind=0,primaryFlags=0,secondaryFlags=0;
    uint16_t characterId=0;
    int32_t status=0,deadCount=0;
};
inline bool LifeOnce(uint64_t base,uint32_t placement,const Actor& expected,Read read,void* ctx,LifeSample& s) {
    if (!expected.known || expected.placement!=placement || !expected.present || !expected.live ||
        !ActorOnce(base,placement,read,ctx,s.actor) || s.actor.wrapper!=expected.wrapper ||
        s.actor.live!=expected.live || s.actor.component!=expected.component || s.actor.algo!=expected.algo ||
        !s.actor.component) return false;
    if (!At(read,ctx,s.actor.live,0,s.kind) ||
        !At(read,ctx,s.actor.live,0x3A0,s.statusObject) || !s.statusObject ||
        !At(read,ctx,s.statusObject,0x48,s.status)) return false;
    if (s.status==1) return true;
    if (!At(read,ctx,s.actor.component,8,s.state) || !s.state ||
        !At(read,ctx,s.state,0,s.vtable) || !s.vtable ||
        !At(read,ctx,s.vtable,0xC0,s.getter) || base>UINT64_MAX-0x2E9934 || s.getter!=base+Grva(0x2E9934) ||
        !At(read,ctx,s.state,0xE8,s.primary) || !s.primary ||
        !At(read,ctx,s.primary,0x20,s.data) || !s.data ||
        !At(read,ctx,s.data,0x18,s.primaryFlags)) return false;
    if (s.primaryFlags&0x100) return true; // Native condition35 bypasses the remaining dead checks.
    // Native special-kind branch needs an additional effect-list predicate only
    // for character4E52. Other character IDs use the ordinary supported path.
    if ((s.kind&0xFFFFFFF0u)==0x930FE0u &&
        (!At(read,ctx,s.data,0xC,s.characterId) || s.characterId==0x4E52)) return false;
    if (!At(read,ctx,s.state,0x118,s.secondary)) return false;
    if (s.secondary && (!At(read,ctx,s.secondary,0x20,s.secondaryData) || !s.secondaryData ||
        !At(read,ctx,s.secondaryData,0x18,s.secondaryFlags))) return false;
    if (s.secondaryFlags&0x100) return true;
    return At(read,ctx,s.state,0xA8,s.bits) && At(read,ctx,s.actor.live,0x8D4,s.deadCount);
}
}
inline Control ReadControl(uint64_t base, Read read, void* ctx) {
    detail::ControlSample a{},b{};
    std::memset(&a,0,sizeof(a)); std::memset(&b,0,sizeof(b));
    if (!detail::ControlOnce(base,read,ctx,a) || !detail::ControlOnce(base,read,ctx,b) ||
        std::memcmp(&a,&b,sizeof(a))) return {};
    Control out; out.known=true; out.manager=a.manager; out.clear=a.aggregate==0;
    for (size_t i=0;i<a.count;++i) if (a.pairs[2*i]==0x48 && a.pairs[2*i+1]) out.clear=false;
    for (size_t i=0;i<a.end-a.begin;i+=16) if (a.secondary[i+8]==0x48) out.clear=false;
    return out;
}
inline Actor ReadActor(uint64_t base, uint32_t placement, Read read, void* ctx) {
    detail::ActorSample a{},b{};
    std::memset(&a,0,sizeof(a)); std::memset(&b,0,sizeof(b));
    if (!detail::ActorOnce(base,placement,read,ctx,a) || !detail::ActorOnce(base,placement,read,ctx,b) ||
        std::memcmp(&a,&b,sizeof(a))) return {};
    Actor out; out.known=true; out.placement=placement; out.wrapper=a.wrapper; out.live=a.live;
    out.component=a.component; out.algo=a.algo; out.present=a.live!=0;
    out.algoReady=a.live && a.component && a.algo && a.active==1;
    return out; // Explicitly not an alive/HP/controller-response predicate.
}
inline Life ReadLife(uint64_t base,uint32_t placement,const Actor& expected,Read read,void* ctx) {
    detail::LifeSample a{},b{};
    std::memset(&a,0,sizeof(a)); std::memset(&b,0,sizeof(b));
    if (!detail::LifeOnce(base,placement,expected,read,ctx,a) ||
        !detail::LifeOnce(base,placement,expected,read,ctx,b) || std::memcmp(&a,&b,sizeof(a))) return {};
    Life out;out.known=true;
    out.notDead=a.status!=1 && ((a.primaryFlags&0x100) || (a.secondaryFlags&0x100) ||
        (!(a.bits&(uint64_t(1)<<46)) && a.deadCount<1));
    return out; // Supported subset of native DEAD predicate inversion, not HP or ABA proof.
}
inline Player ReadPlayer(uint64_t base,Read read,void* ctx) {
    struct Sample { uint64_t table=0,live=0,statusObject=0;int32_t status=0; } a{},b{};
    std::memset(&a,0,sizeof(a));std::memset(&b,0,sizeof(b));
    auto sample=[&](Sample& s) {
        return At(read,ctx,base,0x4745348,s.table) && s.table &&
            At(read,ctx,s.table,0xCD*0x18,s.live) && s.live &&
            At(read,ctx,s.live,0x3A0,s.statusObject) && s.statusObject &&
            At(read,ctx,s.statusObject,0x48,s.status);
    };
    if (!sample(a) || !sample(b) || std::memcmp(&a,&b,sizeof(a))) return {};
    Player out;out.known=true;out.notDead=a.status!=1;out.live=a.live;return out;
}
}
