// Ceiling experiment: no pass/UI exclusions. Cache only scalar capability data;
// reset refreshes it from the actual list device, never the dummy install device.
// #79098: the five tuning values are read on every draw, so a menu change reaches the next draw. One aligned write each;
// a frame that sees old and new values mixed is legal, RSSetShadingRate may change between any two draws.
volatile int g_vrsMode = 0;
bool g_vrsMotionGuard = false;
volatile unsigned g_vrsWideExclude=0; volatile LONG64 g_vrsWideCalls[vrsselect::WClassN]{}, g_vrsWideCoarse=0, g_vrsWideFine=0;
volatile unsigned g_vrsLayouts=7; volatile int g_vrsRate=0; volatile bool g_vrsSkipSkinned=true, g_vrsKeepCharacters=true;
// Fixed at startup: which draw hooks exist and whether tracking slots are routed. A later Off must not unhook anything.
int g_vrsBootMode = 0; bool g_vrsBootTracking = false;
bool VrsSelectiveTracking() { return g_vrsMode == 3 || (g_vrsMode && (g_vrsMotionGuard || g_vrsWideExclude)); }
struct VrsSignature {
    vrsselect::Meta meta; volatile LONG64 coarse=0, fine=0, skinEvidence=0, skinRejected=0, characterRejected=0; volatile LONG reasons=0, legacyReasons=0;
};
VrsSignature g_vrsSig[256]; unsigned g_vrsSigN=0; SRWLOCK g_vrsSigLock=SRWLOCK_INIT;
volatile LONG64 g_vrsObserved=0, g_vrsTagged=0;
// Capture the first 200 intercepted draw calls between two Present boundaries,
// once per report interval. Render threads copy scalars; the watch thread logs.
struct VrsDrawSample { vrsselect::Meta meta; unsigned reason=0, bound=0; bool known=false, indirect=false; };
VrsDrawSample g_vrsSamples[248];
volatile LONG g_vrsSurfaceCount=0, g_vrsSurfaceLayouts[3]{};
SRWLOCK g_vrsSampleLock=SRWLOCK_INIT;
volatile LONG g_vrsSampleState=3, g_vrsSampleCount=0; // 3=arm, 1=record, 2=ready
unsigned long long g_vrsFrame=0, g_vrsSampleFrame=0;
void VrsDiagFrame() {
    if (!VrsSelectiveTracking()) return;
    AcquireSRWLockExclusive(&g_vrsSampleLock);
    ++g_vrsFrame;
    if (g_vrsSampleState==1) InterlockedExchange(&g_vrsSampleState,2);
    else if (g_vrsSampleState==3) {
        g_vrsSampleFrame=g_vrsFrame; InterlockedExchange(&g_vrsSampleCount,0);
        g_vrsSurfaceCount=0; for(auto& n:g_vrsSurfaceLayouts) n=0;
        InterlockedExchange(&g_vrsSampleState,1);
    }
    ReleaseSRWLockExclusive(&g_vrsSampleLock);
}
void VrsDiagDraw(const vrsselect::Meta& m,unsigned reason,unsigned bound,bool known,bool indirect) {
    if (InterlockedCompareExchange(&g_vrsSampleState,0,0)!=1) return;
    AcquireSRWLockShared(&g_vrsSampleLock);
    if (g_vrsSampleState==1) {
        LONG n=InterlockedIncrement(&g_vrsSampleCount)-1;
        if (n<200) g_vrsSamples[n]={m,reason,bound,known,indirect};
        // Reserve 16 records per surface layout so shadow passes cannot starve
        // character diagnostics. These are additional samples, not extra draws.
        if(m.shaderPolicy==2 && m.count>=6 && m.count<=8 &&
           InterlockedIncrement(&g_vrsSurfaceLayouts[m.count-6])<=16) {
            LONG surface=InterlockedIncrement(&g_vrsSurfaceCount)-1;
            g_vrsSamples[200+surface]={m,reason,bound,known,indirect};
        }
    }
    ReleaseSRWLockShared(&g_vrsSampleLock);
}
void VrsReportDraws() {
    VrsDrawSample copy[248]; LONG count=0, surfaces=0; unsigned long long frame=0;
    AcquireSRWLockExclusive(&g_vrsSampleLock);
    if (g_vrsSampleState==2) {
        count=g_vrsSampleCount<200 ? g_vrsSampleCount : 200; frame=g_vrsSampleFrame;
        memcpy(copy,g_vrsSamples,count*sizeof copy[0]);
        surfaces=g_vrsSurfaceCount; memcpy(copy+count,g_vrsSamples+200,surfaces*sizeof copy[0]);
        InterlockedExchange(&g_vrsSampleState,3);
    }
    ReleaseSRWLockExclusive(&g_vrsSampleLock);
    if (!count && !surfaces) return;
    LOG("VRS draw capture frame=%llu calls=%ld cap=200 surfaceSamples=%ld cap=16-per-layout; legacyShape bits: layout1 dsv2 depthOff4 cullNone8 GS16; shaderPolicy 0=unverified 1=verified 2=captured-layout",frame,count,surfaces);
    for(LONG i=0;i<count+surfaces;++i) {
        const auto& s=copy[i]; const auto& m=s.meta;
        LOG("VRS draw frame=%llu i=%ld surfaceSample=%u reject=0x%X legacyShape=0x%X shaderPolicy=%u n=%u rt=%u,%u,%u,%u,%u,%u,%u,%u dsv=%u depth=%u cull=%u gs=%u blendMask=0x%X bind=%u/%u indirect=%u skin=0x%X input=%u/%u vs=%08X%08X%08X%08X worldVs=%u",
            frame,i,(unsigned)(i>=count),s.reason,m.legacyShape,m.shaderPolicy,m.count,m.formats[0],m.formats[1],m.formats[2],m.formats[3],m.formats[4],m.formats[5],m.formats[6],m.formats[7],
            m.dsv,m.depth,m.cull,m.geometry,m.blendMask,s.known,s.bound,s.indirect,m.skin,m.inputKnown,m.inputCount,m.vsHash[0],m.vsHash[1],m.vsHash[2],m.vsHash[3],m.worldVs);
    }
}
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
    VrsReportDraws();
    const LONG64 total=InterlockedCompareExchange64(&g_vrsObserved,0,0);
    const LONG64 tagged=InterlockedCompareExchange64(&g_vrsTagged,0,0);
    LOG("VRS coverage mode=%d motion1x1=%d layouts=%u rate=%s skipSkinned=%d keepCharacters=%d tagged_calls=%lld total_calls=%lld (cumulative draw API calls, not expanded indirect draws)",g_vrsMode,g_vrsMotionGuard,g_vrsLayouts,vrsselect::RateName(vrsselect::SelectiveRate(g_vrsRate)),g_vrsSkipSkinned,g_vrsKeepCharacters,tagged,total);
    if (g_vrsWideExclude) {
        LONG64 n[vrsselect::WClassN]; for (unsigned b=0;b<vrsselect::WClassN;++b) n[b]=InterlockedCompareExchange64(&g_vrsWideCalls[b],0,0);
        LOG("VRS wide exclude=0x%X coarse=%lld fine=%lld classCalls unknown=%lld blend=%lld noDepth=%lld depthOnly=%lld forward=%lld motion=%lld skinned=%lld gbuffer=%lld",
            g_vrsWideExclude,InterlockedCompareExchange64(&g_vrsWideCoarse,0,0),InterlockedCompareExchange64(&g_vrsWideFine,0,0),n[0],n[1],n[2],n[3],n[4],n[5],n[6],n[7]);
    }
    AcquireSRWLockShared(&g_vrsSigLock);
    for (unsigned i=0;i<g_vrsSigN;++i) {
        const auto& q=g_vrsSig[i]; if (!q.coarse && !q.fine) continue;
        const auto& m=q.meta;
        const LONG64 coarse=InterlockedCompareExchange64(&g_vrsSig[i].coarse,0,0), fine=InterlockedCompareExchange64(&g_vrsSig[i].fine,0,0);
        LOG("VRS signature n=%u rt=%u,%u,%u,%u,%u,%u,%u,%u taggedCoarse=%lld total=%lld fine1x1=%lld rejectOR=0x%lX legacyShapeOR=0x%lX firstDSV=%u firstDepth=%u firstCull=%u firstGS=%u skinCalls=%lld skinRejected=%lld characterRejected=%lld",
            m.count,m.formats[0],m.formats[1],m.formats[2],m.formats[3],m.formats[4],m.formats[5],m.formats[6],m.formats[7],coarse,coarse+fine,fine,q.reasons,q.legacyReasons,m.dsv,m.depth,m.cull,m.geometry,InterlockedCompareExchange64(&g_vrsSig[i].skinEvidence,0,0),InterlockedCompareExchange64(&g_vrsSig[i].skinRejected,0,0),InterlockedCompareExchange64(&g_vrsSig[i].characterRejected,0,0));
    }
    ReleaseSRWLockShared(&g_vrsSigLock);
}
volatile LONG64 g_vrsLists = 0, g_vrsDraws = 0, g_vrsIndirect = 0, g_vrsMiss = 0;
struct VrsEntry { void* list = nullptr; bool fourByFour = false; bool supported = false;
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
    e.fourByFour = opt.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2 && opt.AdditionalShadingRatesSupported;
    static volatile LONG reports = 0;
    if (InterlockedIncrement(&reports) <= 8)
        LOG("VRS ceiling actual-list=%p OPTIONS6=%08lX tier=%d additional=%d mode=%d fourByFour=%d supported=%d",
            self, (unsigned long)hr, (int)opt.VariableShadingRateTier, (int)opt.AdditionalShadingRatesSupported,
            g_vrsMode, e.fourByFour, e.supported);
    return e;
}
inline D3D12_SHADING_RATE VrsRateNow(const VrsEntry& e) {
    const int mode = g_vrsMode;
    return mode == 3 ? vrsselect::SelectiveRate(g_vrsRate) : mode == 2 && e.fourByFour ? D3D12_SHADING_RATE_4X4 :
        mode ? D3D12_SHADING_RATE_2X2 : D3D12_SHADING_RATE_1X1;
}
void VrsBindPso(VrsEntry& e, uintptr_t pso) {
    const auto previous=e.meta; const int previousSignature=e.signature;
    e.meta={};
    if (pso) {
        UINT bytes=sizeof e.meta;
        if (FAILED(reinterpret_cast<ID3D12PipelineState*>(pso)->GetPrivateData(vrsselect::kMeta,&bytes,&e.meta)) ||
            bytes!=sizeof e.meta || e.meta.version!=4 || e.meta.count>8) e.meta={};
    }
    if (previousSignature>=0 && previous.count==e.meta.count &&
        !memcmp(previous.formats,e.meta.formats,sizeof previous.formats)) e.signature=previousSignature;
    else e.signature=VrsSignatureIndex(e.meta);
}
void VrsTrack(void* self, int set, int slot, uintptr_t a1) {
    // Keeps running through a live Off or whole-frame mode, so Selective comes back with the true PSO and targets.
    if ((!g_vrsBootTracking && !VrsSelectiveTracking()) || t_vrsBusy) return;
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
unsigned VrsWideWhy(const vrsselect::Meta& m) {
    const unsigned c=vrsselect::WideClasses(m);
    for (unsigned b=0;b<vrsselect::WClassN;++b) if (c & (1u<<b)) InterlockedIncrement64(&g_vrsWideCalls[b]);
    const unsigned why=(c & g_vrsWideExclude) << 16;
    InterlockedIncrement64(why ? &g_vrsWideFine : &g_vrsWideCoarse);
    return why;
}
void VrsApply(void* self, int set, bool reset, bool indirect = false) {
    // A live Off still answers Reset with an explicit 1x1, so the next recording cannot keep a coarse rate.
    if ((!g_vrsMode && !(reset && g_vrsBootMode)) || t_vrsBusy) return;
    t_vrsBusy = true;
    if (VrsSelectiveTracking() && !reset) InterlockedIncrement64(&g_vrsObserved);
    auto& e = VrsCaps(self, set, reset);
    if (e.supported) {
        const D3D12_SHADING_RATE_COMBINER pass[2] = {
            D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH};
        using RateFn = void(STDMETHODCALLTYPE*)(void*, D3D12_SHADING_RATE, const D3D12_SHADING_RATE_COMBINER*);
        const D3D12_SHADING_RATE coarse=VrsRateNow(e); D3D12_SHADING_RATE rate=coarse; unsigned why=0;
        if (VrsSelectiveTracking()) {
            if (reset) rate=D3D12_SHADING_RATE_1X1;
            else {
                why = g_vrsMode==3 ? vrsselect::RejectDraw(e.meta,e.bindingKnown,e.boundCount,g_vrsMotionGuard,g_vrsLayouts,g_vrsSkipSkinned,g_vrsKeepCharacters,indirect) :
                    g_vrsWideExclude ? VrsWideWhy(e.meta) :
                    ((e.meta.reject & vrsselect::Unknown) || !e.bindingKnown ? vrsselect::Unknown :
                     (e.meta.count>=3 && e.meta.motion ? vrsselect::Motion : 0));
                rate=why ? D3D12_SHADING_RATE_1X1 : coarse;
                VrsDiagDraw(e.meta,why,e.boundCount,e.bindingKnown,indirect);
                if (e.signature<0) e.signature=VrsSignatureIndex(e.meta);
                if (e.signature>=0) {
                    auto& q=g_vrsSig[e.signature];
                    InterlockedIncrement64(why ? &q.fine : &q.coarse);
                    if(e.meta.skin) InterlockedIncrement64(&q.skinEvidence);
                    if(why & vrsselect::Skinned) InterlockedIncrement64(&q.skinRejected);
                    if(why & vrsselect::Character) InterlockedIncrement64(&q.characterRejected);
                    InterlockedOr(&q.reasons,(LONG)why);
                    InterlockedOr(&q.legacyReasons,(LONG)e.meta.legacyShape);
                }
            }
        }
        reinterpret_cast<RateFn>((set ? gpuopt_orig1 : gpuopt_orig0)[77])(self, rate, pass);
        if (VrsSelectiveTracking() && !reset && rate!=D3D12_SHADING_RATE_1X1) InterlockedIncrement64(&g_vrsTagged);
        if (reset || rate != D3D12_SHADING_RATE_1X1) InterlockedIncrement64(reset ? &g_vrsLists : indirect ? &g_vrsIndirect : &g_vrsDraws);
    } else {
        InterlockedIncrement64(&g_vrsMiss);
        if (VrsSelectiveTracking() && !reset) VrsDiagDraw(e.meta,vrsselect::Unknown,e.boundCount,e.bindingKnown,indirect);
    }
    t_vrsBusy = false;
}
template<int S> HRESULT STDMETHODCALLTYPE VrsReset(ID3D12GraphicsCommandList* list,
        ID3D12CommandAllocator* allocator, ID3D12PipelineState* pso) {
    // Use the normal thunk so flushing, profiler and ShaderHot still run; preserve HRESULT.
    using Fn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);
    HRESULT hr = reinterpret_cast<Fn>((S ? gpuopt_thunks1 : gpuopt_thunks0)[10])(list, allocator, pso);
    if (SUCCEEDED(hr)) {
        VrsApply(list, S, true);
        if (g_vrsBootTracking || VrsSelectiveTracking()) VrsBindPso(VrsCaps(list,S,false),reinterpret_cast<uintptr_t>(pso));
    }
    return hr;
}
