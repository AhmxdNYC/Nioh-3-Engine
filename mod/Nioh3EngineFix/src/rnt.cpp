// rnt.cpp - render-node timer. See rnt.h and notes/node-timer-33584.md.
//
// v2.02, checked on the dump:
//   GameMain = [base+4759910], found by the pool-id getter 31A048 (it returns a pool id, not the pointer).
//   host     = [GameMain+530h]  (the object MainLoopFrame 1A74130 is called with)
//   graph    = [host+128h]      (FrameStep 3D1F20)
//   box      = [graph+28h]
//   first    = [box]            (FrameStepRender 112DD8 @112F25)
//   next     = [entry+38h]      (112F93)  -- this is the link, not the timer
//   stats    = [entry+40h]      (113292)  -- null in retail; 113204 then QPC-times the node
// The outside recipe's node+38h is JobInvokeTimed 1113B4 (a job object). Writing that on a
// render entry would replace the next pointer. This file does not do that.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "gamerva.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "rnt.h"

namespace rnt {
namespace {

constexpr uint32_t kRvaGameMain = 0x4759910;
constexpr uint32_t kRvaGetter = 0x31A048;
constexpr uint32_t kRvaStatsLoad = 0x113292;  // mov rdi, [rdi+40h]
constexpr uint32_t kRvaNext = 0x112F93;       // mov rsi, [rsi+38h]
constexpr uint32_t kRvaFirst = 0x112F25;      // mov rsi, [rsi]
constexpr uint32_t kRvaGraph = 0x3D1F81;      // mov rax, [rbx+128h]
constexpr uint32_t kRvaBox = 0x3D1F93;        // mov rax, [rax+28h]
constexpr int kCap = 512;
constexpr double kShowMs = 0.05;

void (*g_log)(const char*, ...) = nullptr;
#define RLOG(fmt, ...) do { if (g_log) g_log("RNT: " fmt, __VA_ARGS__); } while (0)

uint8_t* g_base = nullptr;
uintptr_t g_end = 0;
bool g_bytesOk = false;
Rec g_recs[kCap];
Slot g_slots[kCap];
int g_used = 0;
void* g_graph = nullptr;
volatile LONG g_armed = 0;

bool Read(const void* p, void* dst, size_t n) {
    __try {
        memcpy(dst, p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <class T>
bool Load(const void* p, T* out) {
    return Read(p, out, sizeof(T));
}

bool InImage(uintptr_t a) {
    return g_end > (uintptr_t)g_base && a >= (uintptr_t)g_base && a < g_end;
}

void CopyName(void* entry, char* dst) {
    dst[0] = 0;
    uint8_t* nameObj = nullptr;
    if (!Load((uint8_t*)entry + 0x28, &nameObj) || !nameObj) return;
    uint8_t* inner = nullptr;
    if (!Load(nameObj, &inner) || !inner) return;
    uint32_t n = 0, off = 0;
    if (!Load(inner + 0x10, &n) || !Load(inner + 0x14, &off)) return;
    if (n < 1 || n > 47 || off > 0x400) return;
    char tmp[48];
    if (!Read(inner + off, tmp, n)) return;
    for (uint32_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)tmp[i];
        if (c == 0) { n = i; break; }
        if (c < 32 || c > 126) return;
    }
    if (n == 0) return;
    memcpy(dst, tmp, n);
    dst[n] = 0;
}

uint32_t ExecRva(void* entry) {
    uint8_t* node = nullptr;
    if (!Load((uint8_t*)entry + 0x30, &node) || !node) return 0;
    uint8_t* vt = nullptr;
    if (!Load(node, &vt) || !vt) return 0;
    uint8_t* exec = nullptr;
    if (!Load(vt + 8, &exec) || !exec) return 0;
    uintptr_t a = (uintptr_t)exec;
    if (!InImage(a)) return 0;
    return (uint32_t)(a - (uintptr_t)g_base);
}

uint32_t TypeHash(void* entry) {
    uint8_t* td = nullptr;
    if (!Load((uint8_t*)entry + 0x20, &td) || !td) return 0;
    uint32_t h = 0;
    if (!Load(td + 0x0C, &h)) return 0;
    return h;
}

void InitRec(Rec* r) {
    memset(r, 0, sizeof(Rec));
    uint64_t ones = ~0ull;
    memcpy(r->b + 0x18, &ones, 8);
}

bool StoreStats(void* entry, Rec* rec) {
    __try {
        void** slot = (void**)((uint8_t*)entry + 0x40);
        if (*slot) return false;
        _ReadWriteBarrier();
        *slot = rec;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* LiveGraph() {
    uint8_t* O = nullptr;
    if (!Load(g_base + Grva(kRvaGameMain), &O) || !O) return nullptr;
    uint8_t* host = nullptr;
    if (!Load(O + 0x530, &host) || !host) return nullptr;
    void* graph = nullptr;
    if (!Load(host + 0x128, &graph)) return nullptr;
    return graph;
}

void* FirstOf(void* graph) {
    if (!graph) return nullptr;
    void* box = nullptr;
    if (!Load((uint8_t*)graph + 0x28, &box) || !box) return nullptr;
    void* first = nullptr;
    if (!Load(box, &first)) return nullptr;
    return first;
}

}  // namespace

bool BytesMatch(const uint8_t* image, size_t imageBytes) {
    auto has = [&](uint32_t rva, const uint8_t* sig, size_t n) -> bool {
        if ((size_t)rva + n > imageBytes) return false;
        return memcmp(image + rva, sig, n) == 0;
    };
    const uint8_t kGetterHead[] = {0x48, 0x8B, 0x05};
    const uint8_t kGetterTail[] = {0x48, 0x85, 0xC0, 0x74, 0x07, 0x8B, 0x80, 0xC0, 0x06, 0x00, 0x00, 0xC3};
    const uint8_t kStats[] = {0x48, 0x8B, 0x7F, 0x40};
    const uint8_t kNext[] = {0x48, 0x8B, 0x76, 0x38};
    const uint8_t kFirst[] = {0x48, 0x8B, 0x36};
    const uint8_t kGraph[] = {0x48, 0x8B, 0x83, 0x28, 0x01, 0x00, 0x00};
    const uint8_t kBox[] = {0x48, 0x8B, 0x40, 0x28};
    if (!has(kRvaGetter, kGetterHead, sizeof kGetterHead)) return false;
    if ((size_t)kRvaGetter + 7 + sizeof kGetterTail > imageBytes) return false;
    int32_t rel = 0;
    memcpy(&rel, image + kRvaGetter + 3, 4);
    const int64_t target = (int64_t)kRvaGetter + 7 + rel;
    if (target != (int64_t)kRvaGameMain) return false;
    if (!has(kRvaGetter + 7, kGetterTail, sizeof kGetterTail)) return false;
    return has(kRvaStatsLoad, kStats, sizeof kStats)
        && has(kRvaNext, kNext, sizeof kNext)
        && has(kRvaFirst, kFirst, sizeof kFirst)
        && has(kRvaGraph, kGraph, sizeof kGraph)
        && has(kRvaBox, kBox, sizeof kBox);
}

double MsFromTicks(double ticks, double ticksPerMs) {
    if (ticksPerMs <= 0.0 || ticks < 0.0) return -1.0;
    return ticks / ticksPerMs;
}

int FormatPass(char* dst, int dstN, double ms, double maxMs, uint64_t count, const char* name, uint32_t execRva) {
    if (!dst || dstN < 8) return 0;
    const char* nm = (name && name[0]) ? name : "?";
    return snprintf(dst, (size_t)dstN, "RNT: %.3f ms max %.3f n=%llu %s exec=%06X",
                    ms, maxMs, (unsigned long long)count, nm, execRva);
}

int ArmEntries(void* first, Rec* recs, Slot* slots, int cap, uintptr_t imageBase, uintptr_t imageEnd, int* stoppedEarly) {
    if (stoppedEarly) *stoppedEarly = 0;
    if (!first || !recs || !slots || cap <= 0) return 0;
    g_base = (uint8_t*)imageBase;
    g_end = imageEnd;
    int added = 0;
    void* seen[kCap];
    int nSeen = 0;
    void* cur = first;
    while (cur && added < cap) {
        for (int i = 0; i < nSeen; ++i) {
            if (seen[i] == cur) {
                if (stoppedEarly) *stoppedEarly = 1;
                return added;
            }
        }
        if (nSeen < kCap) seen[nSeen++] = cur;
        void* existing = nullptr;
        if (!Load((uint8_t*)cur + 0x40, &existing)) {
            if (stoppedEarly) *stoppedEarly = 1;
            return added;
        }
        if (!existing) {
            InitRec(&recs[added]);
            slots[added].name[0] = 0;
            CopyName(cur, slots[added].name);
            slots[added].execRva = ExecRva(cur);
            slots[added].typeHash = TypeHash(cur);
            if (!StoreStats(cur, &recs[added])) {
                if (stoppedEarly) *stoppedEarly = 1;
                return added;
            }
            ++added;
        }
        void* nxt = nullptr;
        if (!Load((uint8_t*)cur + 0x38, &nxt)) {
            if (stoppedEarly) *stoppedEarly = 1;
            return added;
        }
        cur = nxt;
    }
    if (cur && added >= cap && stoppedEarly) *stoppedEarly = 1;
    return added;
}

namespace {

struct Row { int idx; double ms; double maxMs; uint64_t count; };

bool ReadRow(int idx, double ticksPerMs, Row* row) {
    uint64_t mx = 0, mn = 0, n = 0;
    double mean = 0;
    if (!Load(g_recs[idx].b + 0x10, &mx)) return false;
    if (!Load(g_recs[idx].b + 0x18, &mn)) return false;
    if (!Load(g_recs[idx].b + 0x20, &n)) return false;
    if (!Load(g_recs[idx].b + 0x28, &mean)) return false;
    if (n == 0 || mn == ~0ull) return false;
    row->idx = idx;
    row->count = n;
    row->ms = MsFromTicks(mean, ticksPerMs);
    row->maxMs = MsFromTicks((double)mx, ticksPerMs);
    return row->ms >= 0.0;
}

void LogSecond(double ticksPerMs) {
    std::vector<Row> rows;
    rows.reserve((size_t)g_used);
    double sum = 0;
    for (int i = 0; i < g_used; ++i) {
        Row r{};
        if (!ReadRow(i, ticksPerMs, &r)) continue;
        sum += r.ms;
        rows.push_back(r);
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.ms > b.ms; });
    int shown = 0;
    for (const Row& r : rows) if (r.ms >= kShowMs) ++shown;
    const int capShow = 24;
    RLOG("sum %.3f ms nodes %d over %.2f ms %d (last-10 mean, top list only)",
         sum, g_used, kShowMs, shown);
    int printed = 0;
    char line[160];
    for (const Row& r : rows) {
        if (r.ms < kShowMs) break;
        if (printed >= capShow) break;
        const char* nm = g_slots[r.idx].name[0] ? g_slots[r.idx].name : "?";
        FormatPass(line, (int)sizeof line, r.ms, r.maxMs, r.count, nm, g_slots[r.idx].execRva);
        if (g_log) g_log("%s type=%08X", line, g_slots[r.idx].typeHash);
        ++printed;
    }
    if (shown > printed) RLOG("%d more passes over %.2f ms omitted", shown - printed, kShowMs);
}

int ArmLive() {
    void* graph = LiveGraph();
    if (!graph) return -1;
    void* first = FirstOf(graph);
    if (!first) return -1;
    int stopped = 0;
    int room = kCap - g_used;
    int added = 0;
    if (room > 0) {
        added = ArmEntries(first, g_recs + g_used, g_slots + g_used, room, (uintptr_t)g_base, g_end, &stopped);
        g_used += added;
    } else {
        stopped = 1;
    }
    g_graph = graph;
    g_armed = 1;
    RLOG("armed %d new, %d total, graph %p%s", added, g_used, graph, stopped ? " (list cut)" : "");
    return added;
}

DWORD WINAPI Thread(void*) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    const double ticksPerMs = (double)freq.QuadPart / 1000.0;
    bool down = false;
    DWORD lastLog = 0;
    for (;;) {
        Sleep(50);
        const bool now = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
        const bool press = now && !down;
        down = now;
        if (!g_armed) {
            if (!press) continue;
            if (ArmLive() < 0) RLOG("%s", "F11 pressed, render graph is not in memory yet");
            lastLog = GetTickCount();
            continue;
        }
        void* graph = LiveGraph();
        if (graph && graph != g_graph) {
            RLOG("%s", "graph changed, arming the new list");
            ArmLive();
        } else if (press) {
            ArmLive();
        }
        if (GetTickCount() - lastLog >= 1000) {
            LogSecond(ticksPerMs);
            lastLog = GetTickCount();
        }
    }
}

void StartImpl(const StartInfo& info) {
    g_log = info.log;
    if (!info.iniPath) return;
    const int on = (int)GetPrivateProfileIntW(L"Profiler", L"NodeTimer", 0, info.iniPath);
    if (on == 0) return;
    g_base = (uint8_t*)info.base;
    if (!g_base) {
        RLOG("%s", "off, no image base");
        return;
    }
    MEMORY_BASIC_INFORMATION mi{};
    if (VirtualQuery(g_base, &mi, sizeof mi) && mi.RegionSize) {
        // The image is many regions. Use the PE size when the header is readable.
        __try {
            auto* dos = (IMAGE_DOS_HEADER*)g_base;
            auto* nt = (IMAGE_NT_HEADERS*)(g_base + dos->e_lfanew);
            g_end = (uintptr_t)g_base + nt->OptionalHeader.SizeOfImage;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_end = (uintptr_t)g_base + mi.RegionSize;
        }
    }
    size_t bytes = (g_end > (uintptr_t)g_base) ? (size_t)(g_end - (uintptr_t)g_base) : 0;
    g_bytesOk = bytes && BytesMatch(g_base, bytes);
    if (!g_bytesOk) {
        RLOG("%s", "off, render-graph bytes are not v2.02");
        return;
    }
    if (!info.knownBuild) RLOG("%s", "bytes match; build flag was off");
    RLOG("%s", "armed key F11. Set nothing until then. Writes entry+40h only, no code patch.");
    HANDLE th = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr);
    if (th) CloseHandle(th);
    else RLOG("%s", "thread failed");
}

}  // namespace

void Start(const StartInfo& info) { StartImpl(info); }

}  // namespace rnt
