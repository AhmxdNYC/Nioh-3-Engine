#pragma once
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include "meshletwait.h"
// Same native predicate. No cached ring pointer, generation, or retained waiter.
namespace drainwait {
struct State {
    meshletwait::WaitFn wait=nullptr;
    meshletwait::WakeFn wake=nullptr;
    bool enabled=false;
    volatile LONG failed=0, rounds=0, wakes=0, timeouts=0, errors=0;
    static bool Busy(const uint8_t* ring) {
        const auto* p=reinterpret_cast<const volatile uint32_t*>(ring);
        const uint32_t read=p[0x28/4];
        return p[0x24/4]!=read || p[0x30/4]!=0;
    }
    bool Poll(uint8_t* ring) {
        if (!enabled || !wait || !wake || InterlockedCompareExchange(&failed,0,0)) return Busy(ring);
        for(unsigned i=0;i<64;++i) { if(!Busy(ring)) return false; _mm_pause(); }
        auto* count=reinterpret_cast<volatile LONG*>(ring+0x30);
        LONG expected=*count;
        // Producer reserves write before incrementing count. No completion wake
        // exists yet in that gap; keep native outer polling instead of sleeping.
        if(expected!=0 && Busy(ring)) {
            InterlockedIncrement(&rounds);
            if(!wait(count,&expected,sizeof(expected),1)) {
                if(GetLastError()==ERROR_TIMEOUT) InterlockedIncrement(&timeouts);
                else { InterlockedIncrement(&errors); InterlockedExchange(&failed,1); }
            }
        }
        // Timeout/spurious/error never mean complete. Full predicate every time.
        return Busy(ring);
    }
    void Complete(uint8_t* ring) {
        auto* count=reinterpret_cast<volatile LONG*>(ring+0x30);
        InterlockedDecrement(count); // displaced native lock dec, after replay
        // Always signal: count-based waiter registration introduces lost wakes.
        if(enabled && wake) { wake(const_cast<LONG*>(count)); InterlockedIncrement(&wakes); }
    }
};
}
