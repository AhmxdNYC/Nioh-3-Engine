#include "compute_pso_shadow.h"
#include <cstdio>
#include <cstdlib>
using namespace compute_shadow;
unsigned checks=0;
void check(bool ok,const char* what) { ++checks; if(!ok) { std::printf("FAIL %s\n",what); std::exit(1); } }
Record record(uint32_t hash,uint32_t pad,uint64_t shader,uint64_t pso) {
    Record r{}; std::memcpy(r.data(),&hash,4); std::memcpy(r.data()+4,&pad,4);
    std::memcpy(r.data()+8,&shader,8); std::memcpy(r.data()+16,&pso,8); return r;
}
int main() {
    Shadow s; s.Bind(1,10); unsigned calls=0;
    Record current=record(7,123,55,100), query{};
    bool hit=true;
    auto stock=[&](Record& out) { ++calls; if(hit) out=current; return hit; };
    auto lookup=[&](uint64_t shader=55,uintptr_t cache=1,uint64_t life=10) {
        query=record(7,999,shader,0xdead); const Record before=query;
        const unsigned n=calls; const bool result=s.Find(cache,life,query,stock);
        check(calls==n+1,"exactly one stock call"); check(result==hit,"stock boolean preserved");
        check(query==(hit?current:before),"all24 bytes or untouched miss preserved");
    };
    lookup(); lookup(); check(s.counts.equal==1,"padding ignored for key, complete output compared");
    current=record(7,123,66,200); lookup(66); check(s.counts.predicted==1,"equal hash different shader is not a hit");
    lookup(66); check(s.counts.equal==2,"collision refilled");
    current=record(7,456,66,200); lookup(66); check(s.counts.mismatch==1,"padding-only output mismatch detected");
    current=record(7,456,66,300); lookup(66); check(s.counts.mismatch==2,"same root/count replacement detected without hooks");
    hit=false; lookup(66); check(s.counts.mismatch==3,"stale predicted hit against native miss detected");
    lookup(66); check(s.counts.mismatch==3,"miss never cached"); hit=true;
    const auto bad=s.counts.mismatch;
    // Model native insert/remove/trim/clear/reinit under an admitted mutation scope.
    for(unsigned operation=0;operation<6;++operation) {
        lookup(66); s.BeginMutation(); current=record(7,operation,66,400+operation);
        lookup(66); s.BeginMutation(); lookup(66); s.EndMutation(); lookup(66);
        s.EndMutation(); lookup(66); lookup(66);
    }
    check(s.counts.mismatch==bad,"nested mutation/reentrant lookup remains stock-only");
    const auto predicted=s.counts.predicted;
    lookup(66,2,10); lookup(66,1,11);
    check(s.counts.predicted==predicted,"foreign cache/lifetime never predicts");
    s.Retire(); current=record(7,0,55,999); lookup();
    s.Bind(1,11); lookup(55,1,11); lookup(55,1,11);
    check(s.counts.mismatch==bad,"same-address recreation and shader reuse invalidate");
    Shadow other; other.Bind(2,1); Record q=record(7,0,55,0);
    other.Find(2,1,q,stock); check(other.counts.stock==1,"separate cache has separate observer state");
    check(s.counts.stock+other.counts.stock==calls,"no lookup served from shadow");
    // Native calls can reenter a serialized observer: scope changes during
    // the stock lookup must invalidate the outer lookup's admission too.
    for(unsigned action=0;action<3;++action) {
        Shadow reentrant; reentrant.Bind(9,1);
        Record value=record(3,0,77,10), q2=value;
        auto plain=[&](Record& out) { out=value; return true; };
        reentrant.Find(9,1,q2,plain);
        const auto before=reentrant.counts.predicted;
        auto mutate=[&](Record& out) {
            out=value;
            if(action==0) { reentrant.BeginMutation(); reentrant.EndMutation(); }
            if(action==1) reentrant.Retire();
            if(action==2) reentrant.Bind(9,1); // even reused caller token is a new binding
            return true;
        };
        check(reentrant.Find(9,1,q2,mutate) && q2==value,"reentrant stock result retained");
        check(reentrant.counts.predicted==before,"scope changed inside stock suppresses outer comparison");
        value=record(3,0,77,20);
        reentrant.Find(9,1,q2,plain);
        check(reentrant.counts.mismatch==0,"scope changed inside stock prevents stale refill");
    }
    std::printf("PASS %u checks; stock=%llu predicted=%llu equal=%llu expected_mismatches=%llu suppressed=%llu\n",checks,
        (unsigned long long)s.counts.stock,(unsigned long long)s.counts.predicted,(unsigned long long)s.counts.equal,
        (unsigned long long)s.counts.mismatch,(unsigned long long)s.counts.suppressed);
}
