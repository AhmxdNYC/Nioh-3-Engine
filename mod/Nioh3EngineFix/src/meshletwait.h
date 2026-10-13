#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

// Startup-only paired patch. The native caller still rechecks the completion byte.
// No waiter-count gate: every completion signals its exact address, eliminating
// enrollment-vs-clear races without another shared hot cache line.
namespace meshletwait {
inline bool KnownThread(DWORD id, DWORD self, const DWORD* ids, size_t count) {
    if (!id) return false;
    if (id == self) return true;
    for (size_t i = 0; i < count; ++i) if (id == ids[i]) return true;
    return false;
}
struct Site { uint8_t* address; const uint8_t* original; const uint8_t* replacement; size_t size; };
enum class Publication { Applied, Unchanged, Uncertain };
// Caller owns a verified frozen process. No allocation, logging, or loader work.
template<class Flush> Publication Publish(const Site* sites, size_t count, Flush flush) {
    for (size_t i = 0; i < count; ++i)
        if (memcmp(sites[i].address, sites[i].original, sites[i].size)) return Publication::Unchanged;
    for (size_t i = 0; i < count; ++i)
        memcpy(sites[i].address, sites[i].replacement, sites[i].size);
    bool verified = flush();
    for (size_t i = 0; i < count; ++i)
        if (memcmp(sites[i].address, sites[i].replacement, sites[i].size)) verified = false;
    if (verified) return Publication::Applied;
    for (size_t i = 0; i < count; ++i)
        memcpy(sites[i].address, sites[i].original, sites[i].size);
    verified = flush();
    for (size_t i = 0; i < count; ++i)
        if (memcmp(sites[i].address, sites[i].original, sites[i].size)) verified = false;
    return verified ? Publication::Unchanged : Publication::Uncertain;
}
using WaitFn = BOOL(WINAPI*)(volatile VOID*, PVOID, SIZE_T, DWORD);
using WakeFn = VOID(WINAPI*)(PVOID);
struct State {
    WaitFn wait = nullptr;
    WakeFn wake = nullptr;
    bool enabled = false;
    volatile LONG rounds = 0, timeouts = 0, wakes = 0, errors = 0;
    void Round(volatile uint8_t* flag) {
        if (!enabled || !wait || !wake) { Sleep(0); return; }
        // Native 256-iteration polling already precedes this call. Compare the
        // expected busy value inside WaitOnAddress; an earlier clear cannot sleep.
        uint8_t busy = *flag;
        if (!busy) return;
        InterlockedIncrement(&rounds);
        if (!wait(flag, &busy, 1, 1)) {
            if (GetLastError() == ERROR_TIMEOUT) InterlockedIncrement(&timeouts);
            else { InterlockedIncrement(&errors); Sleep(0); }
        }
        // Spurious wake/timeout is fine: native 3FD46 rechecks; never clear here.
    }
    void Complete(volatile uint8_t* flag) {
        *flag = 0; // displaced original instruction, AFTER the original job returns
        if (enabled && wait && wake) {
            MemoryBarrier();
            wake(const_cast<uint8_t*>(flag));
            InterlockedIncrement(&wakes);
        }
    }
};
}
