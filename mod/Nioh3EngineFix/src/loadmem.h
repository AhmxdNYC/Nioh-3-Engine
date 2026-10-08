// loadmem.h - LM, the load-memory sweeper ([Memory] LM = 0 by default). notes\load-memory.md.
//
// Nioh 3 never decommits its 6 GiB arena: when a stage's pools are freed, the freed chunks go back on the engine heap's
// bins and their pages stay committed and dirty. On the ROG Ally (RAM = VRAM pool, zram swap) the next stage's
// allocations then push those dead pages to swap (class A load hitches). LM runs a sweeper thread that, every
// LM_PeriodMs, takes each engine heap's own mutex and calls VirtualAlloc(MEM_RESET) on the page-aligned interior of free
// chunks (payload >= LM_MinKB) that have been free and unchanged (same address and size) for >= 2 sweeps. The 4-qword
// chunk header and the next chunk's header are never touched (the range is rounded inwards), the allocator reads only
// headers of free chunks (proof + model: re\scripts_stutter\loadmem_check.py), so the allocator's behaviour is identical.
// Start-up self-test: 64 MiB scratch, touch, MEM_RESET, and the process working set must fall by >= 50 %; if it does not
// LM stays off and logs it (LM_Force = 1 skips the test). Log line every 30 s: "stats lm:".
//
// Ini ([Memory]): LM (0/1), LM_PeriodMs (1000), LM_MinKB (256), LM_Force (0), LM_BudgetUs (1000, max time one heap lock is
// held per bin walk), LM_SliceMB (128, max bytes one MEM_RESET call covers).
#pragma once
#include <cstddef>
#include <cstdint>

namespace loadmem {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;        // Nioh3.exe image base
    bool knownBuild = false;   // v2.02 only; other builds refuse
};

// Reads [Memory] LM. Returns false (and does nothing more) when it is 0 or the build is unknown.
bool Start(const StartInfo& info);
bool Enabled();

// "stats lm:" line; call from the 30 s stats thread. No-op when disabled.
void LogLine();

// ---- test hooks (tests\loadmem_harness.cpp); the game never uses these -------------------------------------------------
namespace testing {

struct Ops {
    int (*lock)(void* mtx) = nullptr;     // returns 0 on success (the game's _Mtx_lock)
    int (*unlock)(void* mtx) = nullptr;
    bool (*reset)(uintptr_t addr, size_t len) = nullptr;  // default: VirtualAlloc(addr, len, MEM_RESET, PAGE_READWRITE)
};
struct Cfg {
    size_t minBytes = 256 * 1024;
    uint32_t budgetUs = 1000;
    size_t sliceBytes = 128u << 20;
};
struct Stats {
    uint64_t chunks = 0, bytes = 0, lockWaits = 0, capped = 0, resetFails = 0, nodes = 0, badNodes = 0;
    uint64_t maxLockWaitUs = 0, maxHoldUs = 0;
};
struct HeapState;
HeapState* NewState(uintptr_t heap);
void DeleteState(HeapState* s);
void SetOps(const Ops& o);   // swap the lock / reset functions (harness)
void Sweep(HeapState* s, const Cfg& c, Stats& st);   // one sweep of one heap (the thread calls this once per period)
bool ValidHeap(uintptr_t heap);                      // layout sanity checks used by discovery

}  // namespace testing
}  // namespace loadmem
