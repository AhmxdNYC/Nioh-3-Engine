// Portable on-disk schema. All fields fixed width; Windows x64 little endian.
#pragma once
#include <cstdint>
#include <cstring>
#include <atomic>
namespace dxgidiag {
constexpr uint32_t kSchema = 1, kRing = 256, kPending = 64;
enum Stage : uint32_t { Enter=1, Overlay, Filter, Original, Returned, Retry, Finish, Setup, Fault, Removed, Queue };
enum Method : uint32_t { Present=1, Present1, ColorSpace, Install, Factory, Device, CommandQueue, SwapChain, Hook };
struct Event {
    uint64_t seq=0, call=0, qpc=0, chain=0, target=0, caller=0;
    uint32_t tid=0, depth=0, method=0, stage=0, sync=0, flags=0, dirty=0, scroll=0;
    int32_t hr=0;
    uint32_t hasHr=0;
    uint64_t extra=0;
};
static_assert(sizeof(Event)==96, "disk schema");
struct Record { uint64_t commit=0; Event event{}; uint64_t checksum=0; };
struct Store {
    char magic[8]={'N','3','D','X','G','I','1',0};
    uint32_t schema=kSchema, bytes=sizeof(Store);
    uint64_t frequency=0, written=0, calls=0, failures=0, nested=0, suppressed=0, overwritten=0, pendingLost=0;
    char header[2048]{};
    char metadata[2048]{};
    Record entry{}, returned{}, failure{}, removed{};
    Record pending[kPending]{};
    Record ring[kRing]{};
};
static_assert(sizeof(Store)<65536, "bounded mapped file");
inline uint64_t Checksum(const Event& e) {
    const auto* p=reinterpret_cast<const unsigned char*>(&e);
    uint64_t h=14695981039346656037ull;
    for (unsigned i=0;i<sizeof(e);++i) h=(h^p[i])*1099511628211ull;
    return h;
}
inline bool Valid(const Record& r) { return r.commit && r.commit==r.event.seq && r.checksum==Checksum(r.event); }
// Caller serializes access; clearing commit first lets offline recovery reject torn writes.
inline void Commit(Record& r, const Event& e) {
    *reinterpret_cast<volatile uint64_t*>(&r.commit)=0;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    r.event=e;
    r.checksum=Checksum(e);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    *reinterpret_cast<volatile uint64_t*>(&r.commit)=e.seq;
}
inline void Append(Store& s, Event e, bool trace) {
    e.seq=++s.written;
    if (e.stage==Enter) {
        if(e.call) { ++s.calls; if(e.depth>1) ++s.nested; }
        Commit(s.entry,e);
    }
    if(e.stage==Returned) {
        Commit(s.returned,e);
        if(e.hasHr && e.hr<0) { if(e.call) ++s.failures; Commit(s.failure,e); }
    }
    if(e.stage==Removed) Commit(s.removed,e);
    if(e.call) {
        auto& p=s.pending[e.call%kPending];
        if(e.stage==Finish) { if(p.event.call==e.call) p.commit=0; }
        else if(e.stage!=Removed && e.stage!=Queue) {
            if(Valid(p) && p.event.call!=e.call) ++s.pendingLost;
            Commit(p,e);
        }
    }
    if(trace || (e.hasHr && e.hr<0) || e.stage==Fault || e.stage==Removed) {
        auto& r=s.ring[e.seq%kRing];
        if(Valid(r)) ++s.overwritten;
        Commit(r,e);
    } else ++s.suppressed;
}
// The production wrappers and fake call-through tests share this exact forwarding path.
// Observation cannot change arguments/results or add a retry; only the pre-existing rewrite refusal does.
template<class Invoke, class Observe, class Refuse>
int32_t Forward(uint32_t sync, uint32_t flags, uint32_t sentSync, uint32_t sentFlags,
                bool rewritten, Invoke invoke, Observe observe, Refuse refuse) {
    observe(Original,sentSync,sentFlags,0);
    int32_t hr=invoke(sentSync,sentFlags);
    observe(Returned,sentSync,sentFlags,hr);
    if(rewritten && static_cast<uint32_t>(hr)==0x887a0001u) {
        refuse(hr);
        observe(Retry,sync,flags,0);
        hr=invoke(sync,flags);
        observe(Returned,sync,flags,hr);
    }
    return hr;
}

}
