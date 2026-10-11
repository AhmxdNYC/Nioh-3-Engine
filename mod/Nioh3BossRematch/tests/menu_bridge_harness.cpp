// Actual provider callbacks/worker with a small UI model. No game/window/thread.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "../../rematch_menu_bridge.h"
namespace fixture {
std::atomic<bool> g_open{false}, g_hostRequested{false};
std::atomic<uint32_t> g_padEdges{0};
std::atomic<int> g_toggleOwner{-1}, g_owner{-1};
std::atomic<unsigned> g_latchEpoch{0};
HWND g_wnd = nullptr;
int calls = 0, model = 0;
void Toggle(bool open) { g_open = open; }
namespace rematch {
struct UiRow { char text[112]; uint32_t color; bool sel; };
void UiMove(int n) { model += n; }
void UiChange(int n) { model += n; }
bool UiAct() { ++calls; return true; }
bool UiBack() { return true; }
int UiFill(UiRow* rows, int, char* footer, size_t size) {
    snprintf(rows[0].text, sizeof(rows[0].text), "model %d", model);
    rows[0].color = 123; rows[0].sel = true; snprintf(footer, size, "native rewards"); return 1;
}
bool Toast(char* out, size_t, uint32_t&) { out[0] = 0; return false; }
}
#include "../src/menu_bridge_impl.h"
}
#define CHECK(c) do { ++checks; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    using namespace fixture;
    int checks = 0; uint64_t session = 0, other = 0;
    CHECK(BridgeAttach(1, &session) == mb::Unavailable);
    g_bridgeAvailable = true;
    CHECK(BridgeAttach(1, &session) == mb::Accepted && session && g_hostRequested);
    CHECK(BridgeAttach(1, &other) == mb::Accepted && other == session);
    CHECK(BridgeAttach(2, &other) == mb::Busy);
    CHECK(BridgePost(session, 1, mb::OpenMenu) == mb::Busy); // attach's initial close barrier
    HostedTick();
    mb::Snapshot s{}; s.size = sizeof(s); s.version = mb::kVersion;
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted && mb::ValidSnapshot(s, session) && (s.flags & mb::HostedReady));
    AcquireSRWLockExclusive(&g_bridgeLock);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Busy);
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::Busy);
    ReleaseSRWLockExclusive(&g_bridgeLock);
    CHECK(BridgePost(session, s.generation, mb::OpenMenu) == mb::Accepted);
    HostedTick(); CHECK(g_open);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted && (s.flags & mb::Open));
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::Accepted);
    ++model; // async model change after display/queue but before worker dispatch
    HostedTick(); CHECK(calls == 0 && g_open);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted);
    for (int i = 0; i < 32; ++i) CHECK(BridgePost(session, s.generation, mb::Down) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::Down) == mb::Busy);
    CHECK(BridgePost(session, 0, mb::CloseMenu) == mb::Accepted);
    HostedTick(); CHECK(!g_open && !g_bridgeQueue.count && !calls);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::OpenMenu) == mb::Accepted);
    HostedTick(); CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::Accepted);
    HostedTick(); CHECK(calls == 1 && !g_open);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted && (s.flags & mb::CloseRequested));
    CHECK(BridgePost(session, s.generation - 1, mb::OpenMenu) == mb::StaleGeneration);
    CHECK(BridgeDetach(session) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::StaleSession);
    HostedTick(); CHECK(!g_open && g_hostRequested);
    CHECK(BridgeAttach(2, &other) == mb::Accepted && other != session);
    puts("PASS actual provider attach/queue/worker/close/stale model fixtures; no native game exercised");
    printf("%d checks\n", checks);
}
