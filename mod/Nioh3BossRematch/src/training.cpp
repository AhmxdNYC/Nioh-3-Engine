// Default-OFF read-only observer. Entry handlers never dereference arguments.
#include "addon.h"
#include <atomic>
namespace addon::training {
namespace {
bool g_prepared = false;
std::atomic<bool> g_live{false};
std::atomic<uint64_t> g_create{0}, g_begin{0}, g_end{0}, g_pumps{0};
std::atomic<uint64_t> g_snapshotLosses{0};
SRWLOCK g_snapshotLock = SRWLOCK_INIT;
MissionSnapshot g_snapshot;
bool g_seenMission = false, g_missionGap = false;
uint32_t g_lastMission = 0;
uintptr_t g_missionSite = 0;
void Create(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (Live()) g_create.fetch_add(1, std::memory_order_relaxed);
}
void Begin(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (Live()) g_begin.fetch_add(1, std::memory_order_relaxed);
}
void End(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (Live()) g_end.fetch_add(1, std::memory_order_relaxed);
}
}
bool Prepare() {
    if (g_prepared) return true; // startup registration is idempotent
    if (!IniInt(L"Training", L"Observe", 0)) return false;
    if (!g_img.knownBuild) {
        Log("Training entries: off - unknown build");
        return false;
    }
    const char* why = nullptr;
    if (!GroupOk("CORE", &why) || !GroupOk("FIGHT", &why) || !GroupOk("PUMP", &why)) {
        Log("Training entries: off - %s", why ? why : "?");
        return false;
    }
    OnEntry("Character::Create", &Create);
    OnEntry("BossBattleMode", &Begin);
    OnEntry("BossBattleModeEnd", &End);
    g_missionSite = Site("g:MissionMgr");
    g_prepared = true;
    Log("Training observer: prepared (mission/retry samples; session/load telemetry unavailable)");
    return true;
}
bool Live() { return g_live.load(std::memory_order_acquire); }
void SetPumpLive(bool installed) {
    g_live.store(installed && g_prepared, std::memory_order_release);
}
void Pump() {
    if (!Live()) return;
    g_pumps.fetch_add(1, std::memory_order_relaxed);
    if (!TryAcquireSRWLockExclusive(&g_snapshotLock)) {
        g_snapshotLosses.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    MissionSnapshot next;
    next.sequence = g_snapshot.sequence + 1;
    next.monotonicMs = GetTickCount64();
    next.missionEpoch = g_snapshot.missionEpoch;
    // Resolved CORE global and two fixed, guarded hops. No retained game pointer.
    uint64_t manager = 0, state = 0;
    if (Rd(g_missionSite, manager) && manager >= 0x10000 && manager <= UINT64_MAX - 8 &&
        Rd(manager + 8, state) && state >= 0x10000 && state <= UINT64_MAX - 0x28) {
        uint32_t value = 0;
        if (Rd(state + 0x28, value)) { next.missionValid = true; next.mission = value; }
        value = 0;
        if (Rd(state + 0x14, value)) { next.retryValid = true; next.retry = value; }
    }
    next.missionGap = g_missionGap || !next.missionValid;
    if (next.missionValid) {
        if (!g_seenMission || next.mission != g_lastMission) ++next.missionEpoch;
        g_seenMission = true;
        g_lastMission = next.mission;
        g_missionGap = false;
    } else {
        g_missionGap = true;
    }
    g_snapshot = next;
    ReleaseSRWLockExclusive(&g_snapshotLock);
}
bool Snapshot(MissionSnapshot& out) {
    out = {};
    if (!Live()) return false;
    AcquireSRWLockShared(&g_snapshotLock);
    out = g_snapshot;
    ReleaseSRWLockShared(&g_snapshotLock);
    return out.sequence != 0;
}
uint64_t SnapshotLosses() { return g_snapshotLosses.load(std::memory_order_relaxed); }
EntryCounts Counts() {
    return {g_create.load(std::memory_order_relaxed), g_begin.load(std::memory_order_relaxed),
            g_end.load(std::memory_order_relaxed), g_pumps.load(std::memory_order_relaxed)};
}
} // namespace addon::training
