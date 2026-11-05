// Ceiling experiment: no pass/UI exclusions. Cache only scalar capability data;
// reset refreshes it from the actual list device, never the dummy install device.
int g_vrsMode = 0;
volatile LONG64 g_vrsLists = 0, g_vrsDraws = 0, g_vrsIndirect = 0, g_vrsMiss = 0;
struct VrsEntry { void* list = nullptr; D3D12_SHADING_RATE rate = D3D12_SHADING_RATE_1X1; bool supported = false; };
thread_local VrsEntry t_vrs[16];
thread_local bool t_vrsBusy = false;
VrsEntry& VrsCaps(void* self, int set, bool refresh) {
    auto& e = t_vrs[((uintptr_t)self >> 4) & 15];
    if (!refresh && e.list == self) return e;
    e = {}; e.list = self;
    auto* list = static_cast<ID3D12GraphicsCommandList*>(self);
    if (g_vtSlots[set] < 79 || list->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT) return e;
    ID3D12Device* dev = nullptr;
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 opt{};
    HRESULT hr = list->GetDevice(__uuidof(ID3D12Device), (void**)&dev);
    if (SUCCEEDED(hr) && dev) {
        hr = dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &opt, sizeof opt);
        dev->Release();
    } else hr = E_NOINTERFACE;
    e.supported = SUCCEEDED(hr) && opt.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_1;
    e.rate = g_vrsMode == 2 && opt.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2 &&
        opt.AdditionalShadingRatesSupported ? D3D12_SHADING_RATE_4X4 : D3D12_SHADING_RATE_2X2;
    static volatile LONG reports = 0;
    if (InterlockedIncrement(&reports) <= 8)
        LOG("VRS ceiling actual-list=%p OPTIONS6=%08lX tier=%d additional=%d mode=%d rate=%s supported=%d",
            self, (unsigned long)hr, (int)opt.VariableShadingRateTier, (int)opt.AdditionalShadingRatesSupported,
            g_vrsMode, e.rate == D3D12_SHADING_RATE_4X4 ? "4x4" : "2x2", e.supported);
    return e;
}
void VrsApply(void* self, int set, bool reset, bool indirect = false) {
    if (!g_vrsMode || t_vrsBusy) return;
    t_vrsBusy = true;
    auto& e = VrsCaps(self, set, reset);
    if (e.supported) {
        const D3D12_SHADING_RATE_COMBINER pass[2] = {
            D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH};
        using RateFn = void(STDMETHODCALLTYPE*)(void*, D3D12_SHADING_RATE, const D3D12_SHADING_RATE_COMBINER*);
        reinterpret_cast<RateFn>((set ? gpuopt_orig1 : gpuopt_orig0)[77])(self, e.rate, pass);
        InterlockedIncrement64(reset ? &g_vrsLists : indirect ? &g_vrsIndirect : &g_vrsDraws);
    } else InterlockedIncrement64(&g_vrsMiss);
    t_vrsBusy = false;
}
template<int S> HRESULT STDMETHODCALLTYPE VrsReset(ID3D12GraphicsCommandList* list,
        ID3D12CommandAllocator* allocator, ID3D12PipelineState* pso) {
    // Use the normal thunk so flushing, profiler and ShaderHot still run; preserve HRESULT.
    using Fn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
    HRESULT hr = reinterpret_cast<Fn>((S ? gpuopt_thunks1 : gpuopt_thunks0)[10])(list, allocator, pso);
    if (SUCCEEDED(hr)) VrsApply(list, S, true);
    return hr;
}
