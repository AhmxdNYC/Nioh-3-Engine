// replayprof_detail.h - RPF internals shared by replayprof.cpp and tests\replayprof_harness.cpp (sizes must match
// replayprof.asm: RP_SETS, RP_SLOTS, RP_GROUPS, RP_OPS).
#pragma once
#include <cstddef>
#include <cstdint>

namespace replayprof {
namespace detail {

constexpr int kSets = 10, kSlots = 128, kIds = kSets * kSlots, kGroups = 24, kOps = 186, kOpNone = 185;
enum Kind { kKindList = 0, kKindDevice, kKindQueue, kKindFence, kKindResource };
enum Group {
    kGNone = 0, kGState, kGRoot, kGPso, kGBarrier, kGDraw, kGCopy, kGQuery, kGLife, kGDescCopy, kGViewCreate, kGDevOther,
    kGEcl, kGQueueSync, kGFence, kGMap, kGResOther, kGCom, kGWait, kGSleep, kGListOther
};
extern const char* const kGroupNames[kGroups];

int GroupOf(int kind, const char* methodName);
const char* const* KindNames(int kind, int& n);

// Hooks vtable `vt` (first min(slots, names, 128) slots) as a new set; returns the set index (an already hooked vtable
// returns its set), -1 on failure.
int HookVtable(const char* label, int kind, void** vt, int slots);
void RehookVtables(bool on);
int TableSwap(bool on);
int ImportSwap(bool on);
void Switch(bool on);
// harness only: table / loop return address / wait return address / WaitForSingleObject import slot, D3D12 timing on/off
void TestSetup(uintptr_t table, uintptr_t loopRet, uintptr_t waitRet, void** wfsoSlot, int api);

struct Counters {
    uint64_t n[kOps], incl[kOps], child[kOps], other[185];
    uint64_t id[kIds * 2], mat[kOps * kGroups * 2];
    uint64_t outer, idle, idleN;
    uint64_t psoRedT, psoRedN;
    uint32_t tid;
};
Counters Read();
const char* IdName(int id, char* b, size_t n);
int Report(const Counters& a, const Counters& b, double msPerTick, double sec, double pairTicks, int top, int topApi,
           void (*log)(const char*, ...));

}  // namespace detail
}  // namespace replayprof
