// Ceiling experiment: no pass/UI exclusions. Cache only scalar capability data;
// reset refreshes it from the actual list device, never the dummy install device.
int g_vrsMode = 0;
bool g_vrsMotionGuard = false;
bool VrsSelectiveTracking() { return g_vrsMode == 3 || (g_vrsMode && g_vrsMotionGuard); }
struct VrsSignature {
    vrsselect::Meta meta; volatile LONG64 coarse=0, fine=0; volatile LONG reasons=0;
};
VrsSignature g_vrsSig[256]; unsigned g_vrsSigN=0; SRWLOCK g_vrsSigLock=SRWLOCK_INIT;
int VrsSignatureIndex(const vrsselect::Meta& m) {
    AcquireSRWLockExclusive(&g_vrsSigLock);
    int found=-1;
    for (unsigned i=0;i<g_vrsSigN;++i) if (g_vrsSig[i].meta.count==m.count &&
        !memcmp(g_vrsSig[i].meta.formats,m.formats,sizeof m.formats)) { found=(int)i; break; }
    if (found<0 && g_vrsSigN<256) { found=(int)g_vrsSigN++; g_vrsSig[found].meta=m; }
    ReleaseSRWLockExclusive(&g_vrsSigLock); return found;
}
void VrsReportSignatures() {
    if (!VrsSelectiveTracking()) return;
    AcquireSRWLockShared(&g_vrsSigLock);
    for (unsigned i=0;i<g_vrsSigN;++i) {
        const auto& q=g_vrsSig[i]; if (!q.coarse && !q.fine) continue;
        const auto& m=q.meta;
        LOG("VRS signature n=%u rt=%u,%u,%u,%u,%u,%u,%u,%u tagged2x2=%lld fine1x1=%lld rejectOR=0x%lX",
            m.count,m.formats[0],m.formats[1],m.formats[2],m.formats[3],m.formats[4],m.formats[5],m.formats[6],m.formats[7],q.coarse,q.fine,q.reasons);
    }
    ReleaseSRWLockShared(&g_vrsSigLock);
}
volatile LONG64 g_vrsLists = 0, g_vrsDraws = 0, g_vrsIndirect = 0, g_vrsMiss = 0;
struct VrsEntry { void* list = nullptr; D3D12_SHADING_RATE rate = D3D12_SHADING_RATE_1X1; bool supported = false;
    vrsselect::Meta meta; unsigned boundCount=0; bool bindingKnown=false; int signature=-1;
};
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
void VrsBindPso(VrsEntry& e, uintptr_t pso) {
    const auto previous=e.meta; const int previousSignature=e.signature;
    e.meta={};
    if (pso) {
        UINT bytes=sizeof e.meta;
        if (FAILED(reinterpret_cast<ID3D12PipelineState*>(pso)->GetPrivateData(vrsselect::kMeta,&bytes,&e.meta)) ||
            bytes!=sizeof e.meta || e.meta.version!=1 || e.meta.count>8) e.meta={};
    }
    if (previousSignature>=0 && previous.count==e.meta.count &&
        !memcmp(previous.formats,e.meta.formats,sizeof previous.formats)) e.signature=previousSignature;
    else e.signature=VrsSignatureIndex(e.meta);
}
void VrsTrack(void* self, int set, int slot, uintptr_t a1) {
    if (!VrsSelectiveTracking() || t_vrsBusy) return;
    if (slot==2 || slot==9) {
        auto& e=t_vrs[((uintptr_t)self>>4)&15]; if (e.list==self) e={}; return;
    }
    if (slot!=11 && slot!=25 && slot!=46 && slot!=68 && slot!=69 && slot!=27 && slot!=75) return;
    auto& e=VrsCaps(self,set,false);
    if (slot==25) VrsBindPso(e,a1);
    else if (slot==11) { e.bindingKnown=false; VrsBindPso(e,a1); }
    else if (slot==46 || slot==68) { e.boundCount=(unsigned)a1; e.bindingKnown=a1<=8; }
    else { e.bindingKnown=false; e.meta={}; e.signature=VrsSignatureIndex(e.meta); }
}
void VrsApply(void* self, int set, bool reset, bool indirect = false) {
    if (!g_vrsMode || t_vrsBusy) return;
    t_vrsBusy = true;
    auto& e = VrsCaps(self, set, reset);
    if (e.supported) {
        const D3D12_SHADING_RATE_COMBINER pass[2] = {
            D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH};
        using RateFn = void(STDMETHODCALLTYPE*)(void*, D3D12_SHADING_RATE, const D3D12_SHADING_RATE_COMBINER*);
        D3D12_SHADING_RATE rate=e.rate; unsigned why=0;
        if (VrsSelectiveTracking()) {
            if (reset) rate=D3D12_SHADING_RATE_1X1;
            else {
                why = g_vrsMode==3 ? vrsselect::RejectDraw(e.meta,e.bindingKnown,e.boundCount,g_vrsMotionGuard) :
                    ((e.meta.reject & vrsselect::Unknown) || !e.bindingKnown ? vrsselect::Unknown :
                     (e.meta.count>=3 && e.meta.motion ? vrsselect::Motion : 0));
                rate=why ? D3D12_SHADING_RATE_1X1 : e.rate;
                if (e.signature<0) e.signature=VrsSignatureIndex(e.meta);
                if (e.signature>=0) {
                    auto& q=g_vrsSig[e.signature];
                    InterlockedIncrement64(why ? &q.fine : &q.coarse);
                    InterlockedOr(&q.reasons,(LONG)why);
                }
            }
        }
        reinterpret_cast<RateFn>((set ? gpuopt_orig1 : gpuopt_orig0)[77])(self, rate, pass);
        if (reset || rate != D3D12_SHADING_RATE_1X1) InterlockedIncrement64(reset ? &g_vrsLists : indirect ? &g_vrsIndirect : &g_vrsDraws);
    } else InterlockedIncrement64(&g_vrsMiss);
    t_vrsBusy = false;
}
template<int S> HRESULT STDMETHODCALLTYPE VrsReset(ID3D12GraphicsCommandList* list,
        ID3D12CommandAllocator* allocator, ID3D12PipelineState* pso) {
    // Use the normal thunk so flushing, profiler and ShaderHot still run; preserve HRESULT.
    using Fn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
    HRESULT hr = reinterpret_cast<Fn>((S ? gpuopt_thunks1 : gpuopt_thunks0)[10])(list, allocator, pso);
    if (SUCCEEDED(hr)) {
        VrsApply(list, S, true);
        if (VrsSelectiveTracking()) VrsBindPso(VrsCaps(list,S,false),reinterpret_cast<uintptr_t>(pso));
    }
    return hr;
}
