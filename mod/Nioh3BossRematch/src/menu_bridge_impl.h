// Included inside ui.cpp's private namespace. Only HostedTick runs Ui* methods.
namespace mb = rematch_menu_bridge;
SRWLOCK g_bridgeLock = SRWLOCK_INIT;
mb::Queue<> g_bridgeQueue;
mb::Snapshot g_bridgeSnapshot{};
uint64_t g_bridgeHost = 0, g_bridgeSession = 0, g_bridgeNext = 0;
bool g_bridgeClose = false;
std::atomic<bool> g_bridgeAvailable{false};
struct BridgeTryLock {
    bool held = TryAcquireSRWLockExclusive(&g_bridgeLock) != FALSE;
    ~BridgeTryLock() { if (held) ReleaseSRWLockExclusive(&g_bridgeLock); }
};
int BridgeAttach(uint64_t host, uint64_t* session) {
    if (!host || !session) return mb::InvalidArgument;
    if (!g_bridgeAvailable.load()) return mb::Unavailable;
    BridgeTryLock lock; if (!lock.held) return mb::Busy;
    if (g_bridgeHost && g_bridgeHost != host) return mb::Busy;
    if (!g_bridgeHost) {
        g_bridgeHost = host; g_bridgeSession = ++g_bridgeNext;
        g_bridgeQueue.Clear(); g_bridgeClose = true;
        g_bridgeSnapshot = {};
        g_bridgeSnapshot.size = sizeof(g_bridgeSnapshot); g_bridgeSnapshot.version = mb::kVersion;
        g_bridgeSnapshot.session = g_bridgeSession; g_bridgeSnapshot.generation = 1;
        g_hostRequested.store(true); // input hook becomes passthrough before HostedReady
    }
    *session = g_bridgeSession;
    return mb::Accepted;
}
int BridgeDetach(uint64_t session) {
    BridgeTryLock lock; if (!lock.held) return mb::Busy;
    if (!session || session != g_bridgeSession) return mb::StaleSession;
    g_bridgeHost = g_bridgeSession = 0; g_bridgeQueue.Clear(); g_bridgeClose = true;
    return mb::Accepted; // remain passthrough; never resurrect a second input owner
}
int BridgePost(uint64_t session, uint64_t generation, uint32_t action) {
    if (!mb::ValidAction(action)) return mb::InvalidArgument;
    BridgeTryLock lock; if (!lock.held) return mb::Busy;
    if (!session || session != g_bridgeSession) return mb::StaleSession;
    if (action == mb::CloseMenu) { g_bridgeClose = true; g_bridgeQueue.Clear(); return mb::Accepted; }
    if (g_bridgeClose) return mb::Busy; // don't queue a reopen behind a pending close
    if (!(g_bridgeSnapshot.flags & mb::HostedReady)) return mb::Unavailable;
    if (generation != g_bridgeSnapshot.generation) return mb::StaleGeneration;
    return g_bridgeQueue.Push({session, generation, action}) ? mb::Accepted : mb::Busy;
}
int BridgeSnapshot(uint64_t session, mb::Snapshot* out, uint32_t bytes) {
    if (!out || bytes != sizeof(*out) || out->size != sizeof(*out) || out->version != mb::kVersion) return mb::BadVersion;
    BridgeTryLock lock; if (!lock.held) return mb::Busy;
    if (!session || session != g_bridgeSession) return mb::StaleSession;
    *out = g_bridgeSnapshot;
    return mb::Accepted;
}
int BridgePresentCover(uint64_t session,uint64_t token) {
    BridgeTryLock lock;if(!lock.held)return mb::Busy;
    if(!session||session!=g_bridgeSession)return mb::StaleSession;
    if(g_bridgeClose || !(g_bridgeSnapshot.flags&mb::CoverActive))return mb::Unavailable;
    if(!token||token!=g_bridgeSnapshot.coverToken)return mb::StaleGeneration;
    rematch::CoverPresented(token);return mb::Accepted;
}
void PublishHostedModel() {
    rematch::UiRow rows[mb::kMaxRows]{};
    mb::Row next[mb::kMaxRows]{};
    char footer[sizeof(g_bridgeSnapshot.footer)]{};
    const uint32_t count = (uint32_t)rematch::UiFill(rows, mb::kMaxRows, footer, sizeof(footer));
    for (uint32_t i = 0; i < count; ++i) {
        memcpy(next[i].text, rows[i].text, sizeof(rows[i].text));
        next[i].color = rows[i].color; next[i].selected = rows[i].sel ? 1u : 0u;
    }
    if (count != g_bridgeSnapshot.rowCount || memcmp(next, g_bridgeSnapshot.rows, sizeof(next)) ||
        memcmp(footer, g_bridgeSnapshot.footer, sizeof(footer))) ++g_bridgeSnapshot.generation;
    g_bridgeSnapshot.rowCount = count;
    memcpy(g_bridgeSnapshot.rows, next, sizeof(next));
    memcpy(g_bridgeSnapshot.footer, footer, sizeof(footer));
    g_bridgeSnapshot.toast[0] = 0;
    rematch::Toast(g_bridgeSnapshot.toast, sizeof(g_bridgeSnapshot.toast), g_bridgeSnapshot.toastColor);
}
void HostedTick() {
    // This is the existing single UI worker. Never execute from the DX12 thread.
    AcquireSRWLockExclusive(&g_bridgeLock);
    const mb::Snapshot published = g_bridgeSnapshot;
    if (g_wnd) ShowWindow(g_wnd, SW_HIDE);
    if (!(g_bridgeSnapshot.flags & mb::HostedReady) || g_bridgeClose) {
        rematch::CoverCancel(g_bridgeSnapshot.coverToken);
        Toggle(false); g_padEdges.store(0); g_toggleOwner.store(-1); g_owner.store(-1);
        g_latchEpoch.fetch_add(1); g_bridgeQueue.Clear(); g_bridgeClose = false;
        ++g_bridgeSnapshot.generation;
    }
    // Scans can reorder the boss list without an input. Validate the displayed
    // model before consuming an action, not merely the last action counter.
    if (g_bridgeSession && g_open.load()) PublishHostedModel();
    bool closeRequested = false;
    mb::QueuedAction action{};
    if (g_bridgeQueue.Pop(action) && action.session == g_bridgeSession && action.generation == g_bridgeSnapshot.generation) {
        if (action.action == mb::OpenMenu) Toggle(true);
        else if (g_open.load()) {
            switch (action.action) {
                case mb::Up: rematch::UiMove(-1); break;
                case mb::Down: rematch::UiMove(1); break;
                case mb::Left: rematch::UiChange(-1); break;
                case mb::Right: rematch::UiChange(1); break;
                case mb::PageUp: rematch::UiMove(-10); break;
                case mb::PageDown: rematch::UiMove(10); break;
                case mb::Accept: closeRequested = rematch::UiActHosted(); break;
                case mb::Back: closeRequested = rematch::UiBack(); break;
                default: break;
            }
        }
        if (closeRequested) Toggle(false);
        ++g_bridgeSnapshot.generation;
    }
    if (g_bridgeSession) {
        g_bridgeSnapshot.flags = mb::HostedReady | (g_open.load() ? mb::Open : 0u) | (closeRequested ? mb::CloseRequested : 0u);
        if (g_open.load() || closeRequested) PublishHostedModel();
        rematch::CoverInfo cover{};
        if(rematch::CoverSnapshot(cover)) {
            if(g_bridgeSnapshot.coverPhase!=cover.phase || g_bridgeSnapshot.coverToken!=cover.token)++g_bridgeSnapshot.generation;
            g_bridgeSnapshot.coverPhase=cover.phase;g_bridgeSnapshot.coverToken=cover.token;g_bridgeSnapshot.coverDeadline=cover.deadline;
            memcpy(g_bridgeSnapshot.coverReason,cover.reason,sizeof(cover.reason));
            if(cover.phase>=1&&cover.phase<=3)g_bridgeSnapshot.flags|=mb::CoverActive;
        }
        else {
            // A failed try-lock is unknown, never a transition to inactive.
            // Retain the last coherent publication and its heartbeat; permanent
            // contention therefore expires the existing host freshness lease.
            // Consumed actions stay consumed and their generation stays invalidated.
            const uint64_t generation = g_bridgeSnapshot.generation;
            g_bridgeSnapshot = published;
            g_bridgeSnapshot.generation = generation;
            ReleaseSRWLockExclusive(&g_bridgeLock);
            return;
        }
        g_bridgeSnapshot.workerTickMs = GetTickCount64();
    }
    ReleaseSRWLockExclusive(&g_bridgeLock);
}
DWORD WINAPI HostedThread(void*) {
    for (;;) { HostedTick(); Sleep(33); }
}
