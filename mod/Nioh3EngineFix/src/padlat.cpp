// padlat.cpp - PadLatency ([Debug] PadLatency = 0 by default, 1.7.35): see padlat.h and notes\pad-latency.md.
// Threads: the game's input worker runs HkXInputGetState; osd's Present-hook thread runs OnPresent; "PadLatency gpu" waits
// for our fence; "PadLatency report" drains every feed every 100 ms, matches and logs. Nothing here changes what the game
// reads or when: the hook returns the original result and state untouched.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <xinput.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "padlat.h"
#include "padlat_core.h"

namespace padlat {
namespace {

void (*g_log)(const char* fmt, ...) = nullptr;
#define PLOG(...) \
    do {          \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

StartInfo g_info;
int64_t g_freq = 1;
volatile LONG g_on = 0;
int g_reportSec = 30;

Ring<InputRec, 4096> g_in;
Ring<PresentRec, 1024> g_pr;
Ring<GpuRec, 1024> g_gr;

// ---- XInputGetState hook ---------------------------------------------------------------------------------------------
using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetState_t g_xiOrig = nullptr;
PadTracker g_trk;
volatile LONG g_trkBusy = 0;
volatile LONG64 g_reads = 0, g_changes = 0;

inline int64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return q.QuadPart;
}

DWORD WINAPI HkXInputGetState(DWORD idx, XINPUT_STATE* st) {
    const int64_t t0 = Qpc();
    const DWORD r = g_xiOrig(idx, st);
    const int64_t t1 = Qpc();
    if (InterlockedCompareExchange(&g_trkBusy, 1, 0) == 0) {  // a second reading thread (never seen) just isn't tracked
        __try {
            InterlockedIncrement64(&g_reads);
            if (r == ERROR_SUCCESS && st) {
                Pad p;
                p.buttons = st->Gamepad.wButtons;
                p.lt = st->Gamepad.bLeftTrigger;
                p.rt = st->Gamepad.bRightTrigger;
                p.lx = st->Gamepad.sThumbLX;
                p.ly = st->Gamepad.sThumbLY;
                p.rx = st->Gamepad.sThumbRX;
                p.ry = st->Gamepad.sThumbRY;
                if (g_trk.OnRead(t0, t1, idx, st->dwPacketNumber, p, [](const InputRec& x) { g_in.Push(x); }))
                    InterlockedIncrement64(&g_changes);
            } else {
                // not connected: still part of the poll round (the game asked), no state
                g_trk.OnRead(t0, t1, 4u, 0, Pad{}, [](const InputRec& x) { g_in.Push(x); });
                g_trk.OnDisconnected(idx);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        InterlockedExchange(&g_trkBusy, 0);
    }
    return r;
}

std::string ModuleOf(const void* p) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)p, &m))
        return "?";
    wchar_t w[MAX_PATH] = {};
    GetModuleFileNameW(m, w, MAX_PATH);
    const wchar_t* b = wcsrchr(w, L'\\');
    char a[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, b ? b + 1 : w, -1, a, (int)sizeof(a), nullptr, nullptr);
    char buf[MAX_PATH * 2 + 32];
    snprintf(buf, sizeof(buf), "%s+%llX", a, (unsigned long long)((const uint8_t*)p - (const uint8_t*)m));
    return buf;
}

// the game's IAT slot for XInputGetState (import by name, or ordinal 2 of xinput1_x) - same lookup as menu.cpp
void** FindXInputSlot(uint8_t* base) {
    __try {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return nullptr;
        for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dd.VirtualAddress); d->Name; ++d) {
            const char* dll = reinterpret_cast<const char*>(base + d->Name);
            if (_strnicmp(dll, "xinput", 6) != 0 || !d->OriginalFirstThunk || !d->FirstThunk) continue;
            const bool ord2 = _strnicmp(dll, "xinput1_", 8) == 0;
            auto names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->OriginalFirstThunk);
            auto iat = reinterpret_cast<void**>(base + d->FirstThunk);
            for (int i = 0; names[i].u1.AddressOfData; ++i) {
                const ULONGLONG e = names[i].u1.AddressOfData;
                if (IMAGE_SNAP_BY_ORDINAL64(e)) {
                    if (ord2 && IMAGE_ORDINAL64(e) == 2) return &iat[i];
                } else if (!strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + e)->Name, "XInputGetState")) {
                    return &iat[i];
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

bool HookSlot(void** slot, uint8_t* hostBase) {
    void* cur = *slot;
    HMODULE m = nullptr;
    if (!cur || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)cur, &m) ||
        (uint8_t*)m == hostBase) {
        PLOG("PadLatency: XInputGetState IAT slot %p holds %p, which is not inside a loaded DLL - not hooked", (void*)slot, cur);
        return false;
    }
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
        PLOG("PadLatency: VirtualProtect on the XInputGetState IAT slot failed (%lu)", GetLastError());
        return false;
    }
    g_xiOrig = reinterpret_cast<XInputGetState_t>(cur);  // published before the slot so the hook never sees a null original
    MemoryBarrier();
    InterlockedExchangePointer(slot, (void*)&HkXInputGetState);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}

// ---- GPU completion: our fence on the game's queue ------------------------------------------------------------------
ID3D12Device* g_dev = nullptr;   // device of the first queue (identity only)
ID3D12Fence* g_fence = nullptr;
HANDLE g_fenceEv = nullptr;
volatile LONG64 g_fenceVal = 0;  // last value signalled (present thread)
volatile LONG g_gpuState = 0;    // 0 not tried, 1 running, 2 failed / off
bool g_otherDevLogged = false;

DWORD WINAPI GpuThread(LPVOID) {
    uint64_t next = 1;
    for (;;) {
        if (g_gpuState != 1) return 0;
        if ((uint64_t)g_fenceVal < next) {  // nothing signalled yet (title / loading without presents)
            Sleep(50);
            continue;
        }
        const uint64_t done0 = g_fence->GetCompletedValue();
        if (done0 == UINT64_MAX) {
            PLOG("PadLatency: fence reports device removed - GPU-done timing off");
            InterlockedExchange(&g_gpuState, 2);
            return 0;
        }
        if (done0 < next) {
            if (FAILED(g_fence->SetEventOnCompletion(next, g_fenceEv))) {
                PLOG("PadLatency: SetEventOnCompletion failed - GPU-done timing off");
                InterlockedExchange(&g_gpuState, 2);
                return 0;
            }
            if (WaitForSingleObject(g_fenceEv, 500) != WAIT_OBJECT_0) continue;
        }
        const int64_t q = Qpc();
        const uint64_t done = g_fence->GetCompletedValue();
        if (done == UINT64_MAX || done < next) continue;
        g_gr.Push(GpuRec{done, q});
        next = done + 1;
    }
}

bool CreateFenceOn(ID3D12CommandQueue* q) {
    ID3D12Device* d = nullptr;
    if (FAILED(q->GetDevice(IID_PPV_ARGS(&d))) || !d) return false;
    ID3D12Fence* f = nullptr;
    const HRESULT hr = d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f));
    if (FAILED(hr) || !f) {
        PLOG("PadLatency: CreateFence failed (hr %08lX) - GPU-done timing off", (unsigned long)hr);
        d->Release();
        return false;
    }
    g_fenceEv = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEv) {
        f->Release();
        d->Release();
        return false;
    }
    g_dev = d;  // kept (one reference) for the session: identity check of later queues
    g_fence = f;
    InterlockedExchange(&g_gpuState, 1);
    if (HANDLE h = CreateThread(nullptr, 0, GpuThread, nullptr, 0, nullptr)) CloseHandle(h);
    else {
        InterlockedExchange(&g_gpuState, 2);
        return false;
    }
    PLOG("PadLatency: GPU-done fence on the game's present queue %p (one Signal per presented frame)", (void*)q);
    return true;
}

// ---- report thread --------------------------------------------------------------------------------------------------
Matcher g_m;  // report thread only (large: histograms)
uint64_t g_tsCursor = 0;
uint64_t g_tsLost = 0;

void DrainTs(int64_t now) {
    if (!g_info.tsRing || !g_info.tsRingIdx) return;
    const uint64_t idx = *g_info.tsRingIdx;
    if (idx - g_tsCursor > 4000) {  // fell a lap behind (never at a 100 ms drain): skip to the readable part
        g_tsLost += idx - 4000 - g_tsCursor;
        g_tsCursor = idx - 4000;
    }
    const int64_t stale = now - 5 * g_freq;
    while (g_tsCursor < idx) {
        const size_t s = (size_t)(g_tsCursor & 4095) * 2;
        const int64_t q = g_info.tsRing[s];
        const int id = (int)(g_info.tsRing[s + 1] & 0xFF);
        if (q <= stale || q > now + g_freq || id < 1 || id > 6) {  // reserved but not written yet (or torn): next drain,
            if (idx - g_tsCursor > 64) { ++g_tsLost; ++g_tsCursor; continue; }  // unless it is clearly abandoned
            break;
        }
        g_m.AddStamp(q, id);
        ++g_tsCursor;
    }
}

const char* Src() {
    static char b[200];
    snprintf(b, sizeof(b), "present=%s, gpu=%s, frames=%s",
             g_m.lastPresentSeen ? "DXGI Present hook" : "TS P stamp (no Present hook data)",
             g_gpuState == 1 ? "own fence" : "n/a",
             !g_info.tsRing ? "none (TS not installed)" : (g_info.tsOn && *g_info.tsOn) ? "TS ring" : "TS switched off");
    return b;
}

void LogWindow(int sec) {
    Window& w = g_m.win;
    const double p50 = Pct(w.inPresent, 50), p95 = Pct(w.inPresent, 95), p99 = Pct(w.inPresent, 99);
    const double g50 = Pct(w.inGpu, 50), g95 = Pct(w.inGpu, 95), g99 = Pct(w.inGpu, 99);
    const double b50 = Pct(w.inPresentButtons, 50);
    const double i50 = Pct(w.poll, 50), i95 = Pct(w.poll, 95), i99 = Pct(w.poll, 99);
    PLOG("PadLatency %ds: changes %u (buttons %u, triggers %u, sticks %u), matched %u, unmatched %u | input->present p50 %.2f p95 %.2f "
         "p99 %.2f ms (buttons p50 %.2f) | input->GPU-done p50 %.2f p95 %.2f p99 %.2f ms (n %u, missing %u) | poll interval p50 %.2f "
         "p95 %.2f p99 %.2f ms (%llu rounds, %.1f reads/round)",
         sec, w.events, w.buttons, w.triggers, w.sticks, w.matched, w.unmatched, p50, p95, p99, b50, g50, g95, g99,
         (unsigned)w.inGpu.size(), w.gpuMissing, i50, i95, i99, (unsigned long long)w.rounds,
         w.rounds ? (double)w.reads / (double)w.rounds : 0.0);
    PLOG("PadLatency total: changes %llu (buttons %llu, triggers %llu, sticks %llu), matched %llu, unmatched %llu | input->present p50 %.2f "
         "p95 %.2f p99 %.2f ms (buttons p50 %.2f) | input->GPU-done p50 %.2f p95 %.2f p99 %.2f ms (n %llu, missing %llu) | poll interval "
         "p50 %.2f p95 %.2f p99 %.2f ms (%llu rounds) | %s",
         (unsigned long long)g_m.totEvents, (unsigned long long)g_m.totButtons, (unsigned long long)g_m.totTriggers,
         (unsigned long long)g_m.totSticks, (unsigned long long)g_m.totMatched, (unsigned long long)g_m.totUnmatched,
         g_m.hPresent.Pct(50), g_m.hPresent.Pct(95), g_m.hPresent.Pct(99), g_m.hPresentButtons.Pct(50), g_m.hGpu.Pct(50),
         g_m.hGpu.Pct(95), g_m.hGpu.Pct(99), (unsigned long long)g_m.hGpu.n, (unsigned long long)g_m.totGpuMissing,
         g_m.hPoll.Pct(50), g_m.hPoll.Pct(95), g_m.hPoll.Pct(99), (unsigned long long)g_m.totRounds, Src());
    if (g_in.lost || g_pr.lost || g_gr.lost || g_tsLost)
        PLOG("PadLatency: records lost (input %llu, present %llu, gpu %llu, ts %llu)", (unsigned long long)g_in.lost,
             (unsigned long long)g_pr.lost, (unsigned long long)g_gr.lost, (unsigned long long)g_tsLost);
    w.Clear();
}

DWORD WINAPI ReportThread(LPVOID) {
    g_m.freq = g_freq;
    const int64_t expire = 2 * g_freq, margin = g_freq / 33;  // 2 s to complete a chain; feeds complete up to now - 30 ms
    if (g_info.tsRingIdx) g_tsCursor = *g_info.tsRingIdx;
    ULONGLONG lastReport = GetTickCount64();
    for (;;) {
        Sleep(100);
        __try {
            const int64_t now = Qpc();
            g_in.Drain([](const InputRec& r) { g_m.AddInput(r); });
            g_pr.Drain([](const PresentRec& r) { g_m.AddPresent(r); });
            g_gr.Drain([](const GpuRec& r) { g_m.AddGpu(r); });
            DrainTs(now);
            g_m.Resolve(now - margin, expire);
            const ULONGLONG t = GetTickCount64();
            if (t - lastReport >= (ULONGLONG)g_reportSec * 1000) {
                LogWindow((int)((t - lastReport + 500) / 1000));
                lastReport = t;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            PLOG("PadLatency: exception %08lX in the report thread - reports stop (the hooks keep passing through)", GetExceptionCode());
            InterlockedExchange(&g_on, 0);
            return 0;
        }
    }
}

}  // namespace

bool Wanted(const wchar_t* iniPath) { return iniPath && GetPrivateProfileIntW(L"Debug", L"PadLatency", 0, iniPath) != 0; }

bool WantsPresentHook() { return g_on != 0; }

void OnPresent(ID3D12CommandQueue* q, int64_t now) {
    if (!g_on) return;
    uint64_t v = 0;
    __try {
        if (q && g_gpuState == 0) {
            if (!CreateFenceOn(q)) InterlockedExchange(&g_gpuState, 2);
        }
        if (q && g_gpuState == 1) {
            ID3D12Device* d = nullptr;
            bool same = SUCCEEDED(q->GetDevice(IID_PPV_ARGS(&d))) && d == g_dev;
            if (d) d->Release();
            if (same) {
                const uint64_t nv = (uint64_t)g_fenceVal + 1;
                if (SUCCEEDED(q->Signal(g_fence, nv))) {
                    InterlockedExchange64(&g_fenceVal, (LONG64)nv);
                    v = nv;
                }
            } else if (!g_otherDevLogged) {
                g_otherDevLogged = true;
                PLOG("PadLatency: the presenting queue belongs to another device - no GPU-done timing for its frames");
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_gpuState, 2);
        PLOG("PadLatency: exception %08lX in the Present path - GPU-done timing off", GetExceptionCode());
    }
    g_pr.Push(PresentRec{now, v});
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!Wanted(info.iniPath)) return;
    g_info = info;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart ? f.QuadPart : 1;
    g_trk.gapTicks = g_freq / 500;  // 2 ms between reads = a new poll round
    const int profRs = (int)GetPrivateProfileIntW(L"Profiler", L"ReportSec", 30, info.iniPath);
    g_reportSec = (int)GetPrivateProfileIntW(L"Debug", L"PadLatencyReportSec", 0, info.iniPath);
    if (g_reportSec <= 0) g_reportSec = profRs > 0 ? profRs : 30;
    if (g_reportSec < 5) g_reportSec = 5;
    if (g_reportSec > 600) g_reportSec = 600;
    void** slot = info.base ? FindXInputSlot(info.base) : nullptr;
    if (!slot) {
        PLOG("PadLatency: no XInputGetState import in Nioh3.exe - off");
        return;
    }
    if (!HookSlot(slot, info.base)) return;
    PLOG("PadLatency: hooked the game's XInputGetState (IAT Nioh3.exe+%llX, chained to %s); frame hand-off: %s; report every %d s",
         (unsigned long long)((uint8_t*)slot - info.base), ModuleOf((void*)g_xiOrig).c_str(),
         info.tsRing ? "TS ring C->L->W->P" : "NONE (TS probes not installed: input->present n/a, poll interval only)", g_reportSec);
    InterlockedExchange(&g_on, 1);
    if (HANDLE h = CreateThread(nullptr, 0, ReportThread, nullptr, 0, nullptr)) CloseHandle(h);
    else PLOG("PadLatency: report thread not started (%lu)", GetLastError());
}

bool TestHookSlot(void** slot, uint8_t* hostBase) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart ? f.QuadPart : 1;
    g_trk.gapTicks = g_freq / 500;
    return HookSlot(slot, hostBase);
}

void TestCounters(uint64_t* reads, uint64_t* changes) {
    if (reads) *reads = (uint64_t)g_reads;
    if (changes) *changes = (uint64_t)g_changes;
}

}  // namespace padlat
