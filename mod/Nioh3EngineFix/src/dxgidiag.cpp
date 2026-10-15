// Opt-in observer. No COM/allocator/file I/O under the capture lock; no exception handler.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "dxgidiag.h"
#include "osd.h"
#include <d3d12.h>
#include <cstdio>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>
#include <new>
#include <winver.h>

namespace dxgidiag {
namespace {
std::atomic<bool> g_enabled{false};
bool g_passthrough=false;
int g_mode=0;
Store* g_store=nullptr;
SRWLOCK g_lock=SRWLOCK_INIT;
HANDLE g_file=INVALID_HANDLE_VALUE, g_mapping=nullptr, g_wake=nullptr;
volatile LONG64 g_calls=0, g_dropped=0, g_firstQpc=0, g_sampleQpc=0;
volatile LONG g_removed=0;
std::wstring g_prefix;
void (*g_log)(const char*,...)=nullptr;
uint64_t Now() { LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart; }
void Save(Event e, bool trace=true) {
    if(!Enabled()) return;
    e.qpc=Now(); e.tid=GetCurrentThreadId();
    if(!TryAcquireSRWLockExclusive(&g_lock)) { InterlockedIncrement64(&g_dropped); return; }
    Append(*g_store,e,trace);
    // Publish all record bytes before unlock/worker flush. Offline readers must also validate checksum.
    MemoryBarrier();
    ReleaseSRWLockExclusive(&g_lock);
    if((e.hasHr && e.hr<0) || e.stage==Fault) SetEvent(g_wake);
}
const char* StageName(uint32_t s) {
    switch(s) { case Enter:return "enter"; case Overlay:return "overlay"; case Filter:return "filter";
    case Original:return "original"; case Returned:return "return"; case Retry:return "retry";
    case Finish:return "finish"; case Setup:return "setup"; case Fault:return "CPU-fault";
    case Removed:return "removed-reason"; case Queue:return "queue"; default:return "unknown"; }
}
const char* MethodName(uint32_t m) {
    switch(m) { case Present:return "Present"; case Present1:return "Present1"; case ColorSpace:return "SetColorSpace1";
    case Install:return "InstallHooks"; case Factory:return "dummy-factory"; case Device:return "dummy-device";
    case CommandQueue:return "dummy-queue"; case SwapChain:return "dummy-chain"; case Hook:return "hook-slot";
    default:return "unknown"; }
}
const char* HrName(int32_t hr) {
    switch(static_cast<uint32_t>(hr)) {
    case 0:return "S_OK"; case 0x087a0001:return "DXGI_STATUS_OCCLUDED";
    case 0x887a0001:return "DXGI_ERROR_INVALID_CALL"; case 0x887a0005:return "DXGI_ERROR_DEVICE_REMOVED";
    case 0x887a0006:return "DXGI_ERROR_DEVICE_HUNG"; case 0x887a0007:return "DXGI_ERROR_DEVICE_RESET";
    case 0x887a000a:return "DXGI_ERROR_WAS_STILL_DRAWING"; default:return hr<0 ? "failure" : "success/status";
    }
}
std::string Module(uint64_t address) {
    HMODULE h=nullptr; wchar_t path[MAX_PATH]{}; char name[MAX_PATH]{};
    if(!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
             reinterpret_cast<LPCWSTR>(address),&h) || !GetModuleFileNameW(h,path,MAX_PATH)) return "unknown";
    const wchar_t* base=wcsrchr(path,L'\\'); base=base ? base+1 : path;
    WideCharToMultiByte(CP_UTF8,0,base,-1,name,MAX_PATH,nullptr,nullptr);
    char out[MAX_PATH+32]{}; snprintf(out,sizeof(out),"%s+0x%llX",name,address-reinterpret_cast<uint64_t>(h));
    return out;
}
std::string Describe(const Record& r) {
    if(!Valid(r)) return "none/unavailable";
    const Event& e=r.event; char out[512]{};
    snprintf(out,sizeof(out),"seq=%llu call=%llu %s/%s tid=%u depth=%u chain=%llX SI=%u flags=%08X hr=%s%08X(%s)",
        e.seq,e.call,MethodName(e.method),StageName(e.stage),e.tid,e.depth,e.chain,e.sync,e.flags,
        e.hasHr ? "" : "unavailable/",static_cast<uint32_t>(e.hr),e.hasHr ? HrName(e.hr) : "no result");
    return out;
}
bool WriteFlushed(const std::wstring& name, const std::string& text) {
    // Replace only a fully written + flushed temporary file: a crash preserves the prior report.
    const std::wstring tmp=name+L".tmp";
    HANDLE h=CreateFileW(tmp.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD n=0; bool ok=WriteFile(h,text.data(),static_cast<DWORD>(text.size()),&n,nullptr) && n==text.size();
    ok=FlushFileBuffers(h) && ok; CloseHandle(h);
    return ok && MoveFileExW(tmp.c_str(),name.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
std::string Version(HMODULE module) {
    // Resolve version APIs only in the enabled worker; no new import/library dependency when off.
    static HMODULE version=LoadLibraryW(L"version.dll");
    using SizeFn=DWORD(WINAPI*)(LPCWSTR,LPDWORD);
    using InfoFn=BOOL(WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID);
    using QueryFn=BOOL(WINAPI*)(LPCVOID,LPCWSTR,LPVOID*,PUINT);
    static auto sizeFn=version ? reinterpret_cast<SizeFn>(GetProcAddress(version,"GetFileVersionInfoSizeW")) : nullptr;
    static auto infoFn=version ? reinterpret_cast<InfoFn>(GetProcAddress(version,"GetFileVersionInfoW")) : nullptr;
    static auto queryFn=version ? reinterpret_cast<QueryFn>(GetProcAddress(version,"VerQueryValueW")) : nullptr;
    wchar_t path[MAX_PATH]{}; DWORD unused=0;
    if(!sizeFn || !infoFn || !queryFn || !GetModuleFileNameW(module,path,MAX_PATH)) return "version-unavailable";
    DWORD bytes=sizeFn(path,&unused);
    if(!bytes || bytes>1024*1024) return "version-unavailable";
    std::vector<unsigned char> data(bytes);
    if(!infoFn(path,0,bytes,data.data())) return "version-unavailable";
    VS_FIXEDFILEINFO* info=nullptr; UINT length=0;
    if(!queryFn(data.data(),L"\\",reinterpret_cast<void**>(&info),&length) || length<sizeof(*info) || info->dwSignature!=0xfeef04bd) return "version-unavailable";
    char out[80]{};
    snprintf(out,sizeof(out),"%u.%u.%u.%u",HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS));
    return out;
}
std::string Modules() {
    const wchar_t* names[]={L"dxgi.dll",L"d3d12.dll",L"sl.interposer.dll",L"sl.dlss_g.dll",L"nvngx_dlssg.dll",L"OptiScaler.dll",L"dxgi_proxy.dll"};
    static HMODULE previous[7]{};
    static std::string versions[7];
    std::string out="Modules (presence != FG activation): ";
    for(unsigned i=0;i<7;++i) {
        char utf8[80]{}; WideCharToMultiByte(CP_UTF8,0,names[i],-1,utf8,80,nullptr,nullptr);
        HMODULE module=GetModuleHandleW(names[i]);
        if(module!=previous[i]) { previous[i]=module; versions[i]=module ? Version(module) : ""; }
        out+=utf8; out+="="; out+=module ? versions[i] : "absent"; out+=" ";
    }
    return out;
}

DWORD WINAPI Worker(void*) {
    // Static storage is worker-owned, no large stack copy and no heap work in hooks.
    static Store snap;
    for(;;) {
        WaitForSingleObject(g_wake,250);
        AcquireSRWLockExclusive(&g_lock);
        memcpy(&snap,g_store,sizeof(snap));
        ReleaseSRWLockExclusive(&g_lock);
        const bool flushed=FlushViewOfFile(g_store,sizeof(Store)) && FlushFileBuffers(g_file);
        const Record* pending=nullptr; unsigned active=0;
        for(const auto& p:snap.pending) if(Valid(p)) { ++active; if(!pending || p.event.seq>pending->event.seq) pending=&p; }
        char counts[512]{};
        snprintf(counts,sizeof(counts),"Calls=%llu failed-original-returns=%llu nested=%llu suppressed=%llu overwritten=%llu dropped=%lld pendingLost=%llu",
            snap.calls,snap.failures,snap.nested,snap.suppressed,snap.overwritten,InterlockedCompareExchange64(&g_dropped,0,0),snap.pendingLost);
        std::string report=snap.header;
        report+="\n"+Modules();
        report+="\n"+std::string(snap.metadata[0] ? snap.metadata : "GPU=not observed yet\nFirst chain=not observed yet");
        const Record* queue=nullptr;
        for(const auto& r:snap.ring) if(Valid(r) && r.event.stage==Queue && (!queue || r.event.seq>queue->event.seq)) queue=&r;
        char queueText[200]{};
        if(queue) snprintf(queueText,sizeof(queueText),"; queue=%llX chain=%llX source=%s",queue->event.target,queue->event.chain,queue->event.extra==2 ? "heuristic" : "GetDevice");
        report+="\nCreation=unobserved; generation=unknown; lifecycle-hook coverage=none"+std::string(queue ? queueText : "; queue=unavailable");
        report+="\n"+std::string(counts);
        report+="\nLast entry: "+Describe(snap.entry);
        report+="\nLast return: "+Describe(snap.returned);
        report+="\nLast failure: "+Describe(snap.failure);
        report+="\nDeviceRemovedReason: "+std::string(Valid(snap.removed) && !snap.removed.event.extra ? "GetDevice query FAILED: " : "")+Describe(snap.removed);
        report+="\nPending snapshot="+std::to_string(active)+"; newest: "+(pending ? Describe(*pending) : "none");
        report+="\nLast entry target="+Module(snap.entry.event.target)+" caller="+Module(snap.entry.event.caller);
        report+="\nCapture=partial/live; mapped flush="+std::string(flushed ? "ok" : "FAILED")+"; recover .bin after crash; latest unflushed events may be missing";
        report+="\nTrigger=<startup/FG toggle/resize/gameplay>; exact error=<paste here>\n";
        const bool reportOk=WriteFlushed(g_prefix+L"-report.txt",report);
        std::vector<Record> records;
        for(const auto& r:snap.ring) if(Valid(r)) records.push_back(r);
        std::sort(records.begin(),records.end(),[](const Record& a,const Record& b){return a.event.seq<b.event.seq;});
        std::string detail=report;
        for(const auto& r:records) {
            char extra[256]{};
            snprintf(extra,sizeof(extra)," qpc=%llu dirty=%u scroll=%u extra=%llX target=%s\n",r.event.qpc,r.event.dirty,r.event.scroll,r.event.extra,Module(r.event.target).c_str());
            const std::string line=Describe(r)+extra;
            if(detail.size()+line.size()>65536) break;
            detail+=line;
        }
        const bool detailOk=WriteFlushed(g_prefix+L"-detail.txt",detail);
        static bool warned=false;
        if((!reportOk || !detailOk || !flushed) && !warned) {
            warned=true;
            if(g_log) g_log("DXGI-DIAG: output/flush failed; use .bin recovery if available; capture may be partial");
        }
    }
}
void Metadata(IDXGISwapChain* sc) {
    // Once per session, on a live chain. Never dereference a cached queue or chain.
    static volatile LONG captured=0;
    if(InterlockedCompareExchange(&captured,1,0)!=0) return;
    DXGI_SWAP_CHAIN_DESC sd{};
    const HRESULT descHr=sc->GetDesc(&sd);
    ID3D12Device* dev=nullptr;
    const HRESULT devHr=sc->GetDevice(IID_PPV_ARGS(&dev));
    LUID luid{}; HRESULT adapterHr=E_NOINTERFACE, driverHr=E_NOINTERFACE;
    DXGI_ADAPTER_DESC1 ad{}; LARGE_INTEGER driver{};
    if(SUCCEEDED(devHr) && dev) {
        luid=dev->GetAdapterLuid(); dev->Release();
        using CreateFn=HRESULT(WINAPI*)(REFIID,void**);
        const auto create=reinterpret_cast<CreateFn>(GetProcAddress(GetModuleHandleW(L"dxgi.dll"),"CreateDXGIFactory1"));
        IDXGIFactory4* factory=nullptr;
        adapterHr=create ? create(IID_PPV_ARGS(&factory)) : E_NOINTERFACE;
        if(SUCCEEDED(adapterHr) && factory) {
            IDXGIAdapter1* adapter=nullptr;
            adapterHr=factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&adapter));
            if(SUCCEEDED(adapterHr) && adapter) {
                adapterHr=adapter->GetDesc1(&ad);
                driverHr=adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver);
                adapter->Release();
            }
            factory->Release();
        }
    }
    char gpu[256]{}; WideCharToMultiByte(CP_UTF8,0,ad.Description,-1,gpu,sizeof(gpu),nullptr,nullptr);
    char text[2048]{};
    snprintf(text,sizeof(text),"GPU=%s vendor=%04X device=%04X LUID=%08X:%08X driver=%llX driverHR=%08X adapterHR=%08X deviceHR=%08X\nFirst chain=%p %ux%u fmt=%u buffers=%u effect=%u flags=%08X windowed=%d descHR=%08X; queue=see detail (unknown until observed)",
        gpu[0]?gpu:"unavailable",ad.VendorId,ad.DeviceId,static_cast<uint32_t>(luid.HighPart),luid.LowPart,driver.QuadPart,
        static_cast<uint32_t>(driverHr),static_cast<uint32_t>(adapterHr),static_cast<uint32_t>(devHr),sc,
        sd.BufferDesc.Width,sd.BufferDesc.Height,sd.BufferDesc.Format,sd.BufferCount,sd.SwapEffect,sd.Flags,sd.Windowed,static_cast<uint32_t>(descHr));
    AcquireSRWLockExclusive(&g_lock); memcpy(g_store->metadata,text,sizeof(text)); ReleaseSRWLockExclusive(&g_lock);
}
}
bool Enabled() { return g_enabled.load(std::memory_order_acquire); }
bool Passthrough() { return Enabled() && g_passthrough; }
void Start(const osd::StartupInfo& info) {
    if(!info.iniPath || Enabled()) return;
    g_mode=static_cast<int>(GetPrivateProfileIntW(L"Debug",L"DxgiDiag",0,info.iniPath));
    if(g_mode!=1 && g_mode!=2) return;
    g_log=info.log;
    g_passthrough=GetPrivateProfileIntW(L"Debug",L"DxgiDiagPresentPassthrough",0,info.iniPath)!=0;
    g_prefix=info.iniPath;
    const auto slash=g_prefix.find_last_of(L"\\/");
    g_prefix=slash==std::wstring::npos ? L"" : g_prefix.substr(0,slash+1);
    g_prefix+=L"Nioh3EngineFix-dxgi";
    g_file=CreateFileW((g_prefix+L".bin").c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(g_file!=INVALID_HANDLE_VALUE) g_mapping=CreateFileMappingW(g_file,nullptr,PAGE_READWRITE,0,sizeof(Store),nullptr);
    if(g_mapping) g_store=static_cast<Store*>(MapViewOfFile(g_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Store)));
    g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!g_store || !g_wake) {
        if(g_log) g_log("DXGI-DIAG: initialization failed (%lu); diagnostics and passthrough disabled",GetLastError());
        if(g_store) UnmapViewOfFile(g_store);
        if(g_mapping) CloseHandle(g_mapping);
        if(g_file!=INVALID_HANDLE_VALUE) CloseHandle(g_file);
        if(g_wake) CloseHandle(g_wake);
        return;
    }
    // Store is process-lifetime mapped storage; hooks/worker are never unloaded by this mod.
    new(g_store) Store{};
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq); g_store->frequency=freq.QuadPart;
    auto setting=[&](const wchar_t* section,const wchar_t* key,int def) { return static_cast<int>(GetPrivateProfileIntW(section,key,def,info.iniPath)); };
    snprintf(g_store->header,sizeof(g_store->header),"DXGI-DIAG v1 mod=%s build=" __DATE__ "-" __TIME__ " FG=unknown mode=%d passthrough=%d OS=%s\nSettings: applied=%d skipped=%d(%s) VRR=%d ini:VrrPresent=%d MaxFrameLatency=%d FrameOverlap=%d; passthrough skips overlay/latency/filter only; other patches remain active",
        info.version?info.version:"unknown",g_mode,g_passthrough,info.wine?info.wine:"Windows",info.applied,info.skipped,info.skippedNames?info.skippedNames:"",info.vrr,
        setting(L"Latency",L"VrrPresent",0),setting(L"Latency",L"MaxFrameLatency",0),setting(L"GpuOpt",L"FrameOverlap",0));
    FlushViewOfFile(g_store,sizeof(Store)); FlushFileBuffers(g_file);
    HANDLE thread=CreateThread(nullptr,0,Worker,nullptr,CREATE_SUSPENDED,nullptr);
    if(!thread) {
        if(g_log) g_log("DXGI-DIAG: worker creation failed; diagnostics and passthrough disabled");
        UnmapViewOfFile(g_store); CloseHandle(g_mapping); CloseHandle(g_file); CloseHandle(g_wake); return;
    }
    g_enabled.store(true,std::memory_order_release);
    ResumeThread(thread); CloseHandle(thread);
    if(g_log) g_log("DXGI-DIAG: mode=%d passthrough=%d; Nioh3EngineFix-dxgi-report.txt + .bin beside INI; restart to change",g_mode,g_passthrough);
}
Call Begin(IDXGISwapChain* sc, Method method, UINT sync, UINT flags, uint32_t depth,const void* target,const void* caller,const DXGI_PRESENT_PARAMETERS* params) {
    Call c;
    if(!Enabled()) return c;
    c.event.call=InterlockedIncrement64(&g_calls); c.event.chain=reinterpret_cast<uint64_t>(sc);
    c.event.target=reinterpret_cast<uint64_t>(target); c.event.caller=reinterpret_cast<uint64_t>(caller);
    c.event.method=method; c.event.depth=depth; c.event.sync=sync; c.event.flags=flags;
    const auto now=static_cast<LONG64>(Now());
    const LONG64 first=InterlockedCompareExchange64(&g_firstQpc,now,0);
    const auto sampled=InterlockedCompareExchange64(&g_sampleQpc,0,0);
    const bool tick=now-sampled>=static_cast<LONG64>(g_store->frequency) && InterlockedCompareExchange64(&g_sampleQpc,now,sampled)==sampled;
    c.trace=c.event.call<=16 || tick || (g_mode==2 && (!first || now-first<static_cast<LONG64>(30*g_store->frequency)));
    c.event.stage=Enter; Save(c.event,c.trace); // before any diagnostic COM queries too
    if(params) { c.event.dirty=params->DirtyRectsCount; c.event.scroll=params->pScrollRect!=nullptr; }
    Metadata(sc);
    return c;
}
void Mark(Call& c,Stage stage,UINT sync,UINT flags) {
    if(!c.event.call) return;
    c.event.stage=stage; c.event.sync=sync; c.event.flags=flags; c.event.hasHr=0; c.event.hr=0;
    Save(c.event,c.trace);
}
void Result(Call& c,HRESULT hr,IDXGISwapChain* sc) {
    if(!c.event.call) return;
    c.event.stage=Returned; c.event.hr=hr; c.event.hasHr=1; Save(c.event,c.trace);
    if((hr==DXGI_ERROR_DEVICE_REMOVED || hr==DXGI_ERROR_DEVICE_RESET) && InterlockedCompareExchange(&g_removed,1,0)==0) {
        ID3D12Device* dev=nullptr; HRESULT reason=sc->GetDevice(IID_PPV_ARGS(&dev));
        const bool got=SUCCEEDED(reason) && dev;
        if(got) { reason=dev->GetDeviceRemovedReason(); dev->Release(); }
        Event e=c.event; e.stage=Removed; e.hr=reason; e.extra=got ? 1 : 0; Save(e);
    }
}
void End(Call& c) { if(c.event.call) { c.event.stage=Finish; c.event.hasHr=0; Save(c.event,c.trace); } }
void Note(Method method,Stage stage,const void* object,const void* target,HRESULT hr,bool hasHr,uint64_t extra) {
    if(!Enabled()) return;
    Event e; e.method=method; e.stage=stage; e.chain=reinterpret_cast<uint64_t>(object);
    e.target=reinterpret_cast<uint64_t>(target); e.hr=hr; e.hasHr=hasHr; e.extra=extra; Save(e);
}
void QueueInfo(const void* chain,const void* queue,bool fallback) {
    Note(Present,Queue,chain,queue,S_OK,false,fallback ? 2 : 1);
}
}
