// hooks.cpp - entry hooks (one thunk per site, any number of handlers) and the per-frame pump (call-site retarget).
// Thunk: push rcx, rdx, r8-r11; sub rsp 98h; save xmm0-5; Dispatch(rcx, rdx, r8, r9, entry rsp, site*); restore; then the
// relocated head + jmp back (fresh site) or jmp to the other module's thunk (chained site; it runs the original head itself).
// Volatile registers incl. r10/r11 and xmm0-5 are preserved (the 1.7.17 / 1.7.19 lessons of the perf ASI: LTCG callers keep
// values live across calls to functions the compiler knows do not write them).
#include "addon.h"
#include "sites_gen.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <intrin.h>

namespace addon {
int SiteIndex(const char* name);
bool SiteHookedByOther(int i);

namespace {

constexpr int kMaxSites = 96, kMaxHandlers = 4;
struct Slot {
    int site = -1;
    Handler h[kMaxHandlers] = {};
    int nh = 0;
    bool installed = false;
};
Slot g_slots[kMaxSites];
int g_nslots = 0;

uint8_t* g_thunks = nullptr;
size_t g_used = 0;
constexpr size_t kPage = 64 * 1024, kThunk = 192;

uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + kPage <= re && c + kPage <= to)
                if (void* p = VirtualAlloc((void*)c, kPage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
bool Thunks() {
    if (g_thunks) return true;
    const uintptr_t lo = g_img.base, hi = g_img.end;
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    g_thunks = AllocIn(hi, hi + reach);
    if (!g_thunks) g_thunks = AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
    return g_thunks != nullptr;
}

bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {  // straddles 16 bytes: park the head, write the tail, then the head
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}
bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

void Dispatch(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp, Slot* s) {
    for (int i = 0; i < s->nh; ++i) s->h[i](a, b, c, d, sp);
}

// 0 ok, else a reason
const char* InstallSlot(Slot& s) {
    const sites::Def& d = sites::kDefs[s.site];
    uint8_t* p = (uint8_t*)Site(d.name);
    if (!p) return "unresolved";
    if (!Thunks()) return "no executable page within 2 GB";
    if (g_used + kThunk > kPage) return "thunk page full";
    const bool chained = SiteHookedByOther(s.site);
    uint64_t prev = 0;
    uint8_t head[16] = {};
    if (!Copy(head, (uint64_t)p, d.head)) return "head unreadable";
    if (chained) {
        int32_t rel = 0;
        memcpy(&rel, head + 1, 4);
        prev = (uint64_t)(p + 5 + rel);
    }
    uint8_t* t = g_thunks + g_used;
    uint8_t* w = t;
    auto put = [&](std::initializer_list<uint8_t> b) {
        for (uint8_t x : b) *w++ = x;
    };
    auto put64 = [&](uint64_t v) {
        memcpy(w, &v, 8);
        w += 8;
    };
    put({0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});  // push rcx, rdx, r8, r9, r10, r11
    put({0x48, 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00});                    // sub rsp,98h (16-aligned at the call)
    for (uint8_t i = 0; i < 6; ++i) put({0xF3, 0x0F, 0x7F, (uint8_t)(0x44 | (i << 3)), 0x24, (uint8_t)(0x30 + 0x10 * i)});  // movdqu [rsp+30h+i*10h],xmm_i
    put({0x48, 0x8D, 0x84, 0x24, 0xC8, 0x00, 0x00, 0x00});  // lea rax,[rsp+0C8h] = entry rsp
    put({0x48, 0x89, 0x44, 0x24, 0x20});                    // mov [rsp+20h],rax  (arg 5)
    put({0x48, 0xB8});                                      // mov rax, imm64 slot
    put64((uint64_t)&s);
    put({0x48, 0x89, 0x44, 0x24, 0x28});  // mov [rsp+28h],rax  (arg 6)
    put({0x48, 0xB8});                    // mov rax, Dispatch
    put64((uint64_t)&Dispatch);
    put({0xFF, 0xD0});  // call rax
    for (uint8_t i = 0; i < 6; ++i) put({0xF3, 0x0F, 0x6F, (uint8_t)(0x44 | (i << 3)), 0x24, (uint8_t)(0x30 + 0x10 * i)});
    put({0x48, 0x81, 0xC4, 0x98, 0x00, 0x00, 0x00});                    // add rsp,98h
    put({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59});  // pop r11, r10, r9, r8, rdx, rcx
    uint64_t back = prev;
    if (!chained) {
        memcpy(w, head, d.head);  // relocated head (derive_sites.py proved: whole instructions, no rip-relative / branch)
        w += d.head;
        back = (uint64_t)(p + d.head);
    }
    put({0xFF, 0x25, 0, 0, 0, 0});  // jmp [rip+0]
    put64(back);
    FlushInstructionCache(GetCurrentProcess(), t, (size_t)(w - t));
    g_used += ((size_t)(w - t) + 15) & ~size_t(15);
    const intptr_t dd = (intptr_t)t - (intptr_t)(p + 5);
    if (dd < INT32_MIN || dd > INT32_MAX) return "thunk out of rel32 range";
    uint8_t nb[16];
    memset(nb, 0x90, sizeof(nb));
    nb[0] = 0xE9;
    const int32_t rel = (int32_t)dd;
    memcpy(nb + 1, &rel, 4);
    if (!Patch(p, nb, chained ? 5 : d.head)) return "atomic write failed";
    s.installed = true;
    return nullptr;
}

// ---- pump ----
void (*g_tick)() = nullptr;
volatile LONG g_inTick = 0;
void PumpTick() {
    if (InterlockedExchange(&g_inTick, 1)) return;  // never re-entered
    __try {
        g_tick();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static volatile LONG once = 0;
        if (!InterlockedExchange(&once, 1)) Log("Pump: exception in the tick (logged once; the game continues)");
    }
    InterlockedExchange(&g_inTick, 0);
}

}  // namespace

void OnEntry(const char* site, Handler h) {
    const int i = SiteIndex(site);
    if (i < 0) {
        Log("internal: OnEntry(%s): no such site", site);
        return;
    }
    for (int k = 0; k < g_nslots; ++k)
        if (g_slots[k].site == i) {
            for (int j = 0; j < g_slots[k].nh; ++j)
                if (g_slots[k].h[j] == h) return;
            if (g_slots[k].nh < kMaxHandlers) {
                if (g_slots[k].installed) {  // installed already: append is safe (single pointer store, count after)
                    g_slots[k].h[g_slots[k].nh] = h;
                    InterlockedIncrement((volatile LONG*)&g_slots[k].nh);
                } else {
                    g_slots[k].h[g_slots[k].nh++] = h;
                }
            }
            return;
        }
    if (g_nslots >= kMaxSites) return;
    Slot& s = g_slots[g_nslots++];
    s.site = i;
    s.h[s.nh++] = h;
}

bool InstallGroup(const char* group, const char** why) {
    if (!GroupOk(group, why)) return false;
    for (int k = 0; k < g_nslots; ++k) {
        Slot& s = g_slots[k];
        if (s.installed || strcmp(sites::kDefs[s.site].group, group)) continue;
        if (const char* r = InstallSlot(s)) {
            static char b[160];
            snprintf(b, sizeof(b), "%s: %s", sites::kDefs[s.site].name, r);
            if (why) *why = b;
            return false;
        }
    }
    return true;
}

bool InstallPump(void (*tick)(), const char** why) {
    if (!GroupOk("PUMP", why)) return false;
    uint8_t* p = (uint8_t*)Site("PerFrameTickCall");
    uint8_t b[5];
    if (!Copy(b, (uint64_t)p, 5) || b[0] != 0xE8) {
        if (why) *why = "PUMP call site is not a call";
        return false;
    }
    int32_t rel = 0;
    memcpy(&rel, b + 1, 4);
    const uint64_t next = (uint64_t)(p + 5 + rel);  // the original callee, or another module's thunk (chained)
    if (!Thunks() || g_used + kThunk > kPage) {
        if (why) *why = "no thunk room";
        return false;
    }
    g_tick = tick;
    uint8_t* t = g_thunks + g_used;
    uint8_t* w = t;
    auto put = [&](std::initializer_list<uint8_t> x) {
        for (uint8_t c : x) *w++ = c;
    };
    // the call site's own frame: rsp = 8 mod 16 at our entry (a call pushed the return address)
    put({0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x50});  // push rcx..r11, rax (7: 16-aligned)
    put({0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00});                          // sub rsp,80h
    for (uint8_t i = 0; i < 6; ++i) put({0xF3, 0x0F, 0x7F, (uint8_t)(0x44 | (i << 3)), 0x24, (uint8_t)(0x20 + 0x10 * i)});
    put({0x48, 0xB8});
    const uint64_t fn = (uint64_t)&PumpTick;
    memcpy(w, &fn, 8);
    w += 8;
    put({0xFF, 0xD0});
    for (uint8_t i = 0; i < 6; ++i) put({0xF3, 0x0F, 0x6F, (uint8_t)(0x44 | (i << 3)), 0x24, (uint8_t)(0x20 + 0x10 * i)});
    put({0x48, 0x81, 0xC4, 0x80, 0x00, 0x00, 0x00});
    put({0x58, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59});  // pop rax, r11..rcx
    put({0xFF, 0x25, 0, 0, 0, 0});
    memcpy(w, &next, 8);
    w += 8;
    FlushInstructionCache(GetCurrentProcess(), t, (size_t)(w - t));
    g_used += ((size_t)(w - t) + 15) & ~size_t(15);
    const intptr_t dd = (intptr_t)t - (intptr_t)(p + 5);
    if (dd < INT32_MIN || dd > INT32_MAX) {
        if (why) *why = "thunk out of rel32 range";
        return false;
    }
    uint8_t nb[5] = {0xE8};
    const int32_t r2 = (int32_t)dd;
    memcpy(nb + 1, &r2, 4);
    if (!Patch(p, nb, 5)) {
        if (why) *why = "atomic write failed";
        return false;
    }
    Log("Pump: per-frame call +%llX now runs the add-on tick first, then %s +%llX", (unsigned long long)((uintptr_t)p - g_img.base),
        (next >= g_img.base && next < g_img.end) ? "the original" : "ANOTHER MODULE's thunk (chained)",
        (unsigned long long)(next - g_img.base));
    return true;
}

// retarget an E8 rel32 call site (kind "call") to `fn` (same signature and ABI as the callee); *orig = the previous callee
bool InstallCallRetarget(const char* site, void* fn, void** orig, const char** why) {
    uint8_t* p = (uint8_t*)Site(site);
    uint8_t b[5];
    if (!p || !Copy(b, (uint64_t)p, 5) || b[0] != 0xE8) {
        if (why) *why = "call site not resolved / not a call";
        return false;
    }
    int32_t rel = 0;
    memcpy(&rel, b + 1, 4);
    *orig = (void*)(p + 5 + rel);
    if (!Thunks() || g_used + 16 > kPage) {
        if (why) *why = "no thunk room";
        return false;
    }
    uint8_t* t = g_thunks + g_used;
    const uint8_t jmp[6] = {0xFF, 0x25, 0, 0, 0, 0};  // jmp [rip+0] ; abs64
    memcpy(t, jmp, 6);
    const uint64_t f = (uint64_t)fn;
    memcpy(t + 6, &f, 8);
    FlushInstructionCache(GetCurrentProcess(), t, 14);
    g_used += 16;
    const intptr_t dd = (intptr_t)t - (intptr_t)(p + 5);
    if (dd < INT32_MIN || dd > INT32_MAX) {
        if (why) *why = "thunk out of rel32 range";
        return false;
    }
    uint8_t nb[5] = {0xE8};
    const int32_t r2 = (int32_t)dd;
    memcpy(nb + 1, &r2, 4);
    if (!Patch(p, nb, 5)) {
        if (why) *why = "atomic write failed";
        return false;
    }
    Log("Hook: call +%llX (%s) now goes through the add-on, then +%llX", (unsigned long long)((uintptr_t)p - g_img.base), site,
        (unsigned long long)((uintptr_t)*orig - g_img.base));
    return true;
}

}  // namespace addon
