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
constexpr size_t kEventCapacity = 64;
EntryEvent g_events[kEventCapacity];
size_t g_eventHead = 0, g_eventSize = 0;
uint64_t g_eventSequence = 0;
std::atomic<uint64_t> g_eventContention{0}, g_eventCapacityLoss{0};
void Record(EntryKind kind, std::atomic<uint64_t>& count) {
    if (!Live()) return;
    count.fetch_add(1, std::memory_order_relaxed);
    // Never wait on a game entry; pump and queries share this local copy lock.
    if (!TryAcquireSRWLockExclusive(&g_snapshotLock)) {
        g_eventContention.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    EntryEvent event;
    event.sequence = ++g_eventSequence; // publication order, not native entry order
    event.monotonicMs = GetTickCount64();
    event.kind = kind;
    event.observation = g_snapshot;
    if (g_eventSize == kEventCapacity) {
        g_eventHead = (g_eventHead + 1) % kEventCapacity;
        --g_eventSize;
        g_eventCapacityLoss.fetch_add(1, std::memory_order_relaxed);
    }
    g_events[(g_eventHead + g_eventSize) % kEventCapacity] = event;
    ++g_eventSize;
    ReleaseSRWLockExclusive(&g_snapshotLock);
}
bool g_seenMission = false, g_missionGap = false;
uint32_t g_lastMission = 0;
uintptr_t g_missionSite = 0, g_gameMainSite = 0, g_playModeSite = 0, g_netSite = 0;
bool g_loadingGap = false, g_playModeGap = false, g_connectionGap = false;
bool g_seenLoading = false, g_seenSession = false;
uint8_t g_lastLoading = 0, g_lastConnection = 0;
int32_t g_lastPlayMode = 0;
uint32_t g_lastSessionMission = 0;
template<class T> bool ReadScalar(uintptr_t site, uint64_t offset, T& out) {
    uint64_t object = 0;
    T value{};
    if (!Rd(site, object) || object < 0x10000 || object > UINT64_MAX - offset ||
        !Rd(object + offset, value)) return false;
    out = value;
    return true;
}
bool Gap(bool valid, bool skipped, bool& pending) {
    const bool gap = pending || skipped || !valid;
    pending = !valid;
    return gap;
}
void Create(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    Record(EntryKind::Create, g_create);
}
void Begin(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    Record(EntryKind::Begin, g_begin);
}
void End(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    Record(EntryKind::End, g_end);
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
    g_gameMainSite = Site("g:GameMain");
    g_playModeSite = Site("g:PlayMode");
    g_netSite = Site("g:NetMgr");
    g_prepared = true;
    Log("Training observer: prepared (mission/retry/loading/session scalars; no readiness verdict)");
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
    next.publicationLosses = g_snapshotLosses.load(std::memory_order_relaxed);
    const bool skipped = next.publicationLosses != g_snapshot.publicationLosses;
    // Independent guarded reads: unknown never silently becomes offline/not loading.
    next.loadingValid = ReadScalar(g_gameMainSite, 0x58A, next.loading);
    next.playModeValid = ReadScalar(g_playModeSite, 0x34, next.playMode);
    next.connectionValid = ReadScalar(g_netSite, 0x1DC, next.connection);
    next.loadingGap = Gap(next.loadingValid, skipped, g_loadingGap);
    next.playModeGap = Gap(next.playModeValid, skipped, g_playModeGap);
    next.connectionGap = Gap(next.connectionValid, skipped, g_connectionGap);
    // Resolved CORE global and two fixed, guarded hops. No retained game pointer.
    uint64_t manager = 0, state = 0;
    if (Rd(g_missionSite, manager) && manager >= 0x10000 && manager <= UINT64_MAX - 8 &&
        Rd(manager + 8, state) && state >= 0x10000 && state <= UINT64_MAX - 0x28) {
        uint32_t value = 0;
        if (Rd(state + 0x28, value)) { next.missionValid = true; next.mission = value; }
        value = 0;
        if (Rd(state + 0x14, value)) { next.retryValid = true; next.retry = value; }
    }
    next.missionGap = g_missionGap || skipped || !next.missionValid;
    if (next.missionValid) {
        if (!g_seenMission || next.mission != g_lastMission) ++next.missionEpoch;
        g_seenMission = true;
        g_lastMission = next.mission;
        g_missionGap = false;
    } else {
        g_missionGap = true;
    }
    next.loadGeneration = g_snapshot.loadGeneration;
    if (next.loadingValid) {
        if (!g_seenLoading || next.loadingGap || next.loading != g_lastLoading)
            ++next.loadGeneration;
        g_seenLoading = true;
        g_lastLoading = next.loading;
    }
    // A separately readable tuple, not an atomic native session snapshot.
    next.sessionValid = next.missionValid && next.playModeValid && next.connectionValid;
    next.sessionGap = next.missionGap || next.playModeGap || next.connectionGap;
    next.sessionGeneration = g_snapshot.sessionGeneration;
    if (next.sessionValid) {
        if (!g_seenSession || next.sessionGap || next.mission != g_lastSessionMission ||
            next.playMode != g_lastPlayMode || next.connection != g_lastConnection)
            ++next.sessionGeneration;
        g_seenSession = true;
        g_lastSessionMission = next.mission;
        g_lastPlayMode = next.playMode;
        g_lastConnection = next.connection;
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
size_t Events(EntryEvent* out, size_t capacity) {
    if (!out || !capacity || !Live()) return 0;
    AcquireSRWLockShared(&g_snapshotLock);
    const size_t n = capacity < g_eventSize ? capacity : g_eventSize;
    for (size_t i = 0; i < n; ++i) out[i] = g_events[(g_eventHead + i) % kEventCapacity];
    ReleaseSRWLockShared(&g_snapshotLock);
    return n;
}
EventLossCounts EventLosses() {
    return {g_eventContention.load(std::memory_order_relaxed),
            g_eventCapacityLoss.load(std::memory_order_relaxed)};
}
uint64_t SnapshotLosses() { return g_snapshotLosses.load(std::memory_order_relaxed); }
EntryCounts Counts() {
    return {g_create.load(std::memory_order_relaxed), g_begin.load(std::memory_order_relaxed),
            g_end.load(std::memory_order_relaxed), g_pumps.load(std::memory_order_relaxed)};
}
} // namespace addon::training
