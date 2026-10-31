// gamerva.cpp - pattern lookup behind Grva() (gamerva.h) for builds other than v2.02. Table: src/rvas_gen.h (generated from
// rvas.json by tools/gen_rvas.py from a decrypted dump). Kinds:
//   code    the bytes at rva - offset match `pattern` exactly once in the executable sections -> rva = hit + offset
//   global  data slot: rip-relative anchor instruction found by pattern; rva = anchor + insnLen + disp32 + addend
//   iat     import slot dll!func: found in the game's import directory by name (no pattern)
//   none    no unique pattern exists (rvas.json "why"): known build only, 0 elsewhere
// Known build: Grva() never gets here; Verify() only checks every entry at its v2.02 place and logs the count.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gamerva.h"
#include "rvas_gen.h"

namespace gamerva {
namespace {

uintptr_t g_base = 0;
void (*g_log)(const char*, ...) = nullptr;
struct Sec {
    const uint8_t* p;
    size_t n;
};
std::vector<Sec> g_exec;
std::atomic<uint32_t> g_cache[rvas::kCount];  // resolved rva + 1 (0 = not tried, 1 = failed)
uint32_t g_lo = 0, g_hi = 0;                 // image range

struct Pat {
    uint8_t b[160];
    uint8_t m[160];  // 1 = literal
    size_t n = 0;
};
bool Parse(const char* s, Pat& p) {
    p.n = 0;
    for (const char* c = s; *c && p.n < sizeof p.b;) {
        while (*c == ' ') ++c;
        if (!*c) break;
        if (c[0] == '?') {
            p.b[p.n] = 0;
            p.m[p.n++] = 0;
        } else {
            char h[3] = {c[0], c[1], 0};
            p.b[p.n] = (uint8_t)strtoul(h, nullptr, 16);
            p.m[p.n++] = 1;
        }
        c += 2;
    }
    return p.n > 0;
}
bool MatchAt(const uint8_t* q, const Pat& p, size_t skipLo = 0, size_t skipHi = 0) {
    __try {
        for (size_t i = 0; i < p.n; ++i)
            if (p.m[i] && !(i >= skipLo && i < skipHi) && q[i] != p.b[i]) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// longest literal run = memchr anchor
void Anchor(const Pat& p, size_t& off, size_t& len) {
    off = len = 0;
    for (size_t i = 0, cur = 0, st = 0; i < p.n; ++i) {
        if (!p.m[i]) { cur = 0; continue; }
        if (cur == 0) st = i;
        if (++cur > len) { len = cur; off = st; }
    }
}
// matches in the executable sections (stops at 2); *hit = first match
int Scan(const Pat& p, const uint8_t** hit) {
    size_t ao, al;
    Anchor(p, ao, al);
    if (!al) return -1;
    int n = 0;
    for (const Sec& s : g_exec) {
        const uint8_t* lo = s.p;
        const uint8_t* hi = s.p + s.n;
        __try {
            for (const uint8_t* cur = lo + ao; cur + (p.n - ao) <= hi;) {
                const uint8_t* f = (const uint8_t*)memchr(cur, p.b[ao], (size_t)(hi - cur));
                if (!f || f + (p.n - ao) > hi) break;
                const uint8_t* st = f - ao;
                if (st >= lo && !memcmp(f, p.b + ao, al) && MatchAt(st, p)) {
                    if (n++ == 0) *hit = st;
                    if (n >= 2) return n;
                }
                cur = f + 1;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return -1;
        }
    }
    return n;
}

const IMAGE_NT_HEADERS64* Nt() {
    auto dos = (const IMAGE_DOS_HEADER*)g_base;
    return (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
}

uint32_t IatSlot(const char* name) {  // "KERNEL32.dll!ReadFile" or "XINPUT1_4.dll!#2"
    const char* bang = strchr(name, '!');
    if (!bang) return 0;
    const auto& dir = Nt()->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    for (auto d = (const IMAGE_IMPORT_DESCRIPTOR*)(g_base + dir.VirtualAddress); d->Name; ++d) {
        if (_strnicmp((const char*)(g_base + d->Name), name, (size_t)(bang - name)) || ((const char*)(g_base + d->Name))[bang - name]) continue;
        auto ilt = (const IMAGE_THUNK_DATA64*)(g_base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (uint32_t k = 0; ilt[k].u1.AddressOfData; ++k) {
            if (IMAGE_SNAP_BY_ORDINAL64(ilt[k].u1.Ordinal)) {  // "XINPUT1_4.dll!#2" = import by ordinal
                if (bang[1] == '#' && IMAGE_ORDINAL64(ilt[k].u1.Ordinal) == strtoul(bang + 2, nullptr, 10)) return d->FirstThunk + 8 * k;
                continue;
            }
            auto ibn = (const IMAGE_IMPORT_BY_NAME*)(g_base + (uint32_t)ilt[k].u1.AddressOfData);
            if (!strcmp((const char*)ibn->Name, bang + 1)) return d->FirstThunk + 8 * k;
        }
    }
    return 0;
}

// expected (v2.02) match start of an entry
uint32_t Expected(const rvas::Def& d) { return (d.kind == rvas::kGlobal ? d.anchor : d.rva) - d.offset; }

uint32_t ResolveOne(int i) {
    const rvas::Def& d = rvas::kDefs[i];
    if (d.kind == rvas::kIat) return IatSlot(d.iat);
    if (d.kind == rvas::kNone) return 0;
    Pat p;
    if (!Parse(d.pattern, p)) return 0;
    const uint8_t* hit = nullptr;
    if (Scan(p, &hit) != 1) return 0;
    const uintptr_t site = (uintptr_t)hit + d.offset;
    if (d.kind == rvas::kCode) return (uint32_t)(site - g_base);
    int32_t disp = 0;
    memcpy(&disp, (const void*)(site + d.dispOff), 4);
    const int64_t r = (int64_t)(site - g_base) + d.insnLen + disp + d.addend;
    return r > 0 && (uint64_t)r < g_hi ? (uint32_t)r : 0;
}

int Index(uint32_t rva) {
    const rvas::Def* e = rvas::kDefs + rvas::kCount;
    const rvas::Def* f = std::lower_bound(rvas::kDefs, e, rva, [](const rvas::Def& d, uint32_t v) { return d.rva < v; });
    return f != e && f->rva == rva ? (int)(f - rvas::kDefs) : -1;
}

uint32_t Resolve(uint32_t rva) {
    const int i = Index(rva);
    if (i < 0) return 0;  // not in rvas.json: never guess on a foreign build
    uint32_t c = g_cache[i].load(std::memory_order_acquire);
    if (!c) {
        c = ResolveOne(i) + 1;
        g_cache[i].store(c, std::memory_order_release);
    }
    return c - 1;
}

}  // namespace

void Init(uintptr_t base, bool known, void (*log)(const char*, ...)) {
    g_base = base;
    g_log = log;
    g_hi = Nt()->OptionalHeader.SizeOfImage;
    auto sh = IMAGE_FIRST_SECTION(Nt());
    for (int k = 0; k < Nt()->FileHeader.NumberOfSections; ++k)
        if (sh[k].Characteristics & IMAGE_SCN_MEM_EXECUTE) g_exec.push_back({(const uint8_t*)(base + sh[k].VirtualAddress), sh[k].Misc.VirtualSize});
    g_known = known;
    g_resolve = &Resolve;
    (void)g_lo;
}

int Verify() {
    if (!g_base) return -1;
    const DWORD t0 = GetTickCount();
    int ok = 0, hooked = 0, iat = 0, none = 0, bad = 0;
    char miss[512] = "";
    for (int i = 0; i < rvas::kCount; ++i) {
        const rvas::Def& d = rvas::kDefs[i];
        bool good = false;
        if (d.kind == rvas::kNone) {
            ++none;
            continue;
        }
        if (!g_known) {
            good = Resolve(d.rva) != 0;
        } else if (d.kind == rvas::kIat) {
            good = IatSlot(d.iat) == d.rva;
            iat += good;
        } else {
            Pat p;
            const uint8_t* at = (const uint8_t*)(g_base + Expected(d));
            if (Parse(d.pattern, p) && MatchAt(at, p)) {
                good = true;
            } else if (d.kind == rvas::kCode && Parse(d.pattern, p) && (at[d.offset] == 0xE9 || at[d.offset] == 0xCC || (at[d.offset] == 0xFF && at[d.offset + 1] == 0x25)) &&
                       MatchAt(at, p, d.offset, d.offset + 16)) {
                good = true;  // a hook (ours or another module's: jmp rel32 / int3 / jmp [rip] abs64) already sits on the site
                ++hooked;
            }
        }
        if (good) {
            ++ok;
        } else {
            ++bad;
            size_t l = strlen(miss);
            if (l < sizeof miss - 12) snprintf(miss + l, sizeof miss - l, " %X", d.rva);
        }
    }
    if (g_log)
        g_log("game RVAs (rvas.json, %d entries): %d %s (%d with a hook on them, %d imports by name), %d missing%s%s, %d without a "
              "pattern (known build only) [%lu ms]",
              rvas::kCount, ok, g_known ? "verified at their v2.02 place" : "resolved by pattern on this build", hooked, iat, bad,
              bad ? ":" : "", miss, none, GetTickCount() - t0);
    return bad;
}

}  // namespace gamerva
