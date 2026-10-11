// Private offline compute-PSO shadow lookup. No patch installer or game calls.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace compute_shadow {
using Record = std::array<uint8_t, 24>;
struct Counts { uint64_t stock=0, predicted=0, equal=0, mismatch=0, suppressed=0; };
// One instance per cache lifetime, serialized by the same lock as native lookup.
// Mutation scope begins BEFORE releases and ends AFTER mutation; nested lookups
// remain stock-only. Lifetime identity must be supplied by a future proven hook.
class Shadow {
    struct Slot { bool valid=false; Record record{}; };
    std::array<Slot,16> slots_{};
    uintptr_t cache_=0;
    uint64_t lifetime_=0;
    uint64_t revision_=0;
    unsigned mutationDepth_=0;
    void Clear() { for (auto& s:slots_) s.valid=false; }
    static bool KeyEqual(const Record& a,const Record& b) {
        return !std::memcmp(a.data(),b.data(),4) && !std::memcmp(a.data()+8,b.data()+8,8);
    }
public:
    Counts counts{};
    void Bind(uintptr_t cache,uint64_t lifetime) {
        Clear(); ++revision_; cache_=cache; lifetime_=lifetime; mutationDepth_=0;
    }
    void BeginMutation() { Clear(); ++revision_; ++mutationDepth_; }
    void EndMutation() { Clear(); ++revision_; if (mutationDepth_) --mutationDepth_; }
    void Retire() { Clear(); ++revision_; cache_=0; lifetime_=0; mutationDepth_=0; }
    template<class Stock> bool Find(uintptr_t cache,uint64_t lifetime,Record& inout,Stock stock) {
        const Record input=inout;
        uint32_t hash=0; std::memcpy(&hash,input.data(),4);
        Slot& s=slots_[(hash^(hash>>12))&15];
        const bool admitted=cache_ && cache==cache_ && lifetime==lifetime_ && !mutationDepth_;
        const uint64_t revision=revision_;
        const bool predicted=admitted && s.valid && KeyEqual(input,s.record);
        const Record prediction=s.record;
        ++counts.stock;
        const bool result=stock(inout); // always native result/output, even on mismatch
        if (!admitted || revision!=revision_) { ++counts.suppressed; return result; }
        if (predicted) {
            ++counts.predicted;
            if (result && prediction==inout) ++counts.equal;
            else ++counts.mismatch;
        }
        s.valid=result;
        if (result) s.record=inout;
        return result;
    }
};
}
