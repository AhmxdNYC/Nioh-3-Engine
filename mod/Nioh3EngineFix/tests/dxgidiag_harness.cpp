#include "../src/dxgidiag_core.h"
#include <cassert>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
using namespace dxgidiag;
int main(int argc,char** argv) {
    // Opaque Present1 parameter pointer is captured unchanged by the same closure used in production.
    int parameter=123, invokes=0, refusals=0, returns=0;
    const int* observed=nullptr;
    auto observe=[&](Stage stage,uint32_t,uint32_t,int32_t) { if(stage==Returned) ++returns; };
    auto refuse=[&](int32_t hr) { assert(static_cast<uint32_t>(hr)==0x887a0001u); ++refusals; };
    auto original=[&](uint32_t si,uint32_t flags) -> int32_t {
        ++invokes; observed=&parameter;
        assert(si==1 && flags==0x200);
        return 0x087a0001; // DXGI_STATUS_OCCLUDED must not become S_OK or failure
    };
    assert(Forward(1,0x200,1,0x200,false,original,observe,refuse)==0x087a0001);
    assert(invokes==1 && returns==1 && !refusals && observed==&parameter);
    invokes=returns=0;
    auto retry=[&](uint32_t si,uint32_t flags) -> int32_t {
        if(++invokes==1) { assert(si==0 && flags==0x200); return static_cast<int32_t>(0x887a0001u); }
        assert(si==1 && flags==0); return 0;
    };
    assert(Forward(1,0,0,0x200,true,retry,observe,refuse)==0);
    assert(invokes==2 && returns==2 && refusals==1);
    invokes=0;
    auto noRetry=[&](uint32_t,uint32_t) -> int32_t { ++invokes; return static_cast<int32_t>(0x887a0001u); };
    assert(static_cast<uint32_t>(Forward(1,0,1,0,false,noRetry,observe,refuse))==0x887a0001u);
    assert(invokes==1); // original INVALID_CALL without rewrite is never retried
    static Store s;
    static_assert(sizeof(Store)==40464,"decoder layout");
    Event e; e.call=1; e.method=Present1; e.stage=Enter; e.depth=1;
    Append(s,e,true);
    e.call=2; e.method=Present; e.depth=2; Append(s,e,true);
    assert(s.calls==2 && s.nested==1 && Valid(s.pending[1]) && Valid(s.pending[2]));
    e.stage=Returned; e.hasHr=1; e.hr=0x087a0001; Append(s,e,true);
    assert(s.failures==0 && s.returned.event.hr==0x087a0001); // positive status preserved
    e.stage=Finish; Append(s,e,true);
    assert(!Valid(s.pending[2]) && Valid(s.pending[1])); // nested return does not erase outer pending
    e.call=1; e.stage=Returned; e.hr=static_cast<int32_t>(0x887a0001); Append(s,e,false);
    assert(s.failures==1 && Valid(s.failure)); // failure retained even after sampled tracing expires
    e.stage=Retry; e.hasHr=0; Append(s,e,false);
    e.stage=Returned; e.hasHr=1; e.hr=0; Append(s,e,false);
    assert(s.returned.event.hr==0 && s.failure.event.hr<0); // retry success retains first failed result
    Record torn=s.failure; torn.event.sync ^= 1;
    assert(!Valid(torn)); torn=s.failure; torn.commit=0; assert(!Valid(torn));
    std::mutex lock;
    std::vector<std::thread> threads;
    for(unsigned t=0;t<8;++t) threads.emplace_back([&,t] {
        for(unsigned i=0;i<10000;++i) {
            Event v; v.call=100+t; v.stage=Original;
            std::lock_guard<std::mutex> hold(lock); Append(s,v,true);
        }
    });
    for(auto& t:threads)t.join();
    assert(s.overwritten>79000);
    for(const auto& r:s.ring) assert(Valid(r));
    e.call=65; e.stage=Enter; e.hasHr=0; Append(s,e,true);
    assert(s.pendingLost>0 && s.pending[1].event.call==65); // bounded outstanding-call overflow explicit
    if(argc>1) {
        std::snprintf(s.header,sizeof(s.header),"DXGI-DIAG v1 test\nSettings=test");
        std::snprintf(s.metadata,sizeof(s.metadata),"GPU=test\nFirst chain=test");
        FILE* f=std::fopen(argv[1],"wb"); assert(f); assert(std::fwrite(&s,sizeof(s),1,f)==1); std::fclose(f);
    }
    std::puts("PASS: production forwarding arguments/status/retry, nesting, positive status, retry failure retention, torn records, 80000 concurrent writes, overflow, pending collision");
}
