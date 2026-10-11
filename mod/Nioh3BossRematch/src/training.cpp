// Default-OFF read-only entry observer groundwork. Never dereferences arguments.
#include "addon.h"
#include <atomic>
namespace addon::training {
namespace {
bool g_prepared = false;
std::atomic<bool> g_live{false};
std::atomic<uint64_t> g_create{0}, g_begin{0}, g_end{0}, g_pumps{0};
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
    g_prepared = true;
    Log("Training entries: prepared (entry counts only; session/epoch telemetry unavailable)");
    return true;
}
bool Live() { return g_live.load(std::memory_order_acquire); }
void SetPumpLive(bool installed) {
    g_live.store(installed && g_prepared, std::memory_order_release);
}
void Pump() {
    if (Live()) g_pumps.fetch_add(1, std::memory_order_relaxed);
}
EntryCounts Counts() {
    return {g_create.load(std::memory_order_relaxed), g_begin.load(std::memory_order_relaxed),
            g_end.load(std::memory_order_relaxed), g_pumps.load(std::memory_order_relaxed)};
}
} // namespace addon::training
