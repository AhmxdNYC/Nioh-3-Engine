// Included in menu.cpp's private namespace. Resolver work stays off Present.
namespace rmb = rematch_menu_bridge;
rmb::Api g_rematchApi{};
std::atomic<uint64_t> g_rematchSession{0};
std::atomic<bool> g_rematchResolverStarted{false};
std::atomic<uint64_t> g_rematchRendererTick{0};
rmb::Snapshot g_rematchView{};
bool g_rematchPage = false, g_rematchAwait = false, g_rematchOpened = false;
bool g_rematchClosePending = false;
uint64_t g_rematchSentGeneration = 0;
uint32_t g_rematchLastAction = rmb::OpenMenu;
ULONGLONG g_rematchFreshAt = 0;
ULONGLONG g_rematchAwaitAt = 0;
DWORD WINAPI ResolveRematchHost(void*) {
    const uint64_t host = (uint64_t)&g_rematchApi;
    for (;;) {
        const uint64_t render = g_rematchRendererTick.load();
        if (!render || GetTickCount64() - render >= 1500) { Sleep(1000); continue; }
        HMODULE module = GetModuleHandleW(L"Nioh3BossRematch.asi");
        if (module) {
            auto query = reinterpret_cast<rmb::GetApi>(GetProcAddress(module, rmb::kExport));
            rmb::Api api{};
            if (query && query(rmb::kVersion, sizeof(api), &api) == rmb::Accepted && rmb::ValidApi(api)) {
                uint64_t session = 0;
                if (api.Attach(host, &session) == rmb::Accepted && session) {
                    // Provider is an ASI resident for process lifetime. Pin it
                    // before publishing callbacks to the renderer.
                    HMODULE pinned = nullptr;
                    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"Nioh3BossRematch.asi", &pinned)) {
                        g_rematchApi = api;
                        g_rematchSession.store(session, std::memory_order_release);
                        return 0;
                    }
                    api.Detach(session);
                }
            }
        }
        Sleep(1000);
    }
}
void StartRematchHostResolver() {
    if (g_rematchResolverStarted.exchange(true)) return;
    if (HANDLE th = CreateThread(nullptr, 0, ResolveRematchHost, nullptr, 0, nullptr)) CloseHandle(th);
    else g_rematchResolverStarted.store(false);
}
bool RefreshRematchHost(ULONGLONG now) {
    const uint64_t session = g_rematchSession.load(std::memory_order_acquire);
    if (!session) return false;
    if (g_rematchClosePending && g_rematchApi.Post(session, 0, rmb::CloseMenu) == rmb::Accepted) g_rematchClosePending = false;
    rmb::Snapshot fresh{}; fresh.size = sizeof(fresh); fresh.version = rmb::kVersion;
    if (g_rematchApi.CopySnapshot(session, &fresh, sizeof(fresh)) == rmb::Accepted && rmb::ValidSnapshot(fresh, session)) {
        g_rematchView = fresh; g_rematchFreshAt = fresh.workerTickMs;
        if (g_rematchAwait && fresh.generation != g_rematchSentGeneration) g_rematchAwait = false;
        if (fresh.flags & rmb::Open) g_rematchOpened = true;
    }
    return g_rematchFreshAt && now >= g_rematchFreshAt && now - g_rematchFreshAt < 1500 &&
        (!g_rematchAwait || now - g_rematchAwaitAt < 1500) && (g_rematchView.flags & rmb::HostedReady);
}
bool PostRematchAction(uint32_t action) {
    const uint64_t session = g_rematchSession.load(std::memory_order_acquire);
    if (!session) return false;
    if (action != rmb::CloseMenu && (g_rematchClosePending || g_rematchAwait || !(g_rematchView.flags & rmb::HostedReady))) return false;
    const int result = g_rematchApi.Post(session, g_rematchView.generation, action);
    if (result != rmb::Accepted) return false;
    g_rematchLastAction = action; g_rematchSentGeneration = g_rematchView.generation;
    g_rematchAwait = true;
    g_rematchAwaitAt = GetTickCount64();
    return true;
}
void CloseRematchHostPage() {
    if (g_rematchPage) g_rematchClosePending = !PostRematchAction(rmb::CloseMenu);
    g_rematchPage = g_rematchAwait = g_rematchOpened = false;
}
