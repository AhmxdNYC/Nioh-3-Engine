// sites.cpp - patch-resilient site resolution. Every site of src/sites_gen.h is found by:
//   * kNative: the script native table {char* name, fn*} (located from the string "Mission::BossBattleMode"); name -> function.
//     The head bytes (relocated prologue) must still equal the v2.02 bytes, or the hook would relocate unknown code.
//   * everything else: the wildcard pattern. Known build (timestamp + SizeOfImage match): checked at the expected RVA first
//     (fast path); otherwise, or if that fails, the executable sections are scanned and the pattern must match EXACTLY once.
//   * kGlobal: target = anchor instruction + insnLen + disp32 (+ addend).
// A site already hooked by another module (E9 rel32 + NOPs over the head) still resolves: the head is skipped in the match and
// hooks.cpp chains onto it.
#include "addon.h"
#include "sites_gen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace addon {

Image g_img;
int SiteIndex(const char* name);

namespace {

struct Res {
    uintptr_t addr = 0;
    const char* why = nullptr;  // failure
    bool hookedByOther = false;
};
Res g_res[sites::kCount];
std::string g_why[sites::kCount];

struct Pat {
    std::vector<uint8_t> b;
    std::vector<uint8_t> m;  // 1 = literal
};
Pat Parse(const char* s) {
    Pat p;
    for (const char* c = s; *c;) {
        while (*c == ' ') ++c;
        if (!*c) break;
        if (c[0] == '?') {
            p.b.push_back(0);
            p.m.push_back(0);
            c += 2;
        } else {
            p.b.push_back((uint8_t)strtoul(std::string(c, 2).c_str(), nullptr, 16));
            p.m.push_back(1);
            c += 2;
        }
    }
    return p;
}
bool MatchAt(const uint8_t* p, const Pat& pt, size_t skipHead = 0) {
    for (size_t i = skipHead; i < pt.b.size(); ++i)
        if (pt.m[i] && p[i] != pt.b[i]) return false;
    return true;
}

struct Sec {
    uintptr_t a;
    size_t n;
};
std::vector<Sec> g_exec;

void Sections() {
    const auto* dos = (const IMAGE_DOS_HEADER*)g_img.base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_img.base + dos->e_lfanew);
    const auto* sh = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) g_exec.push_back({g_img.base + sh[i].VirtualAddress, sh[i].Misc.VirtualSize});
}

// raw search of a literal run in [lo, hi) (SEH-guarded, POD only); returns hi when not found
const uint8_t* FindRun(const uint8_t* lo, const uint8_t* hi, const uint8_t* nb, size_t n, bool* fault) {
    __try {
        for (const uint8_t* cur = lo; cur + n <= hi;) {
            const uint8_t* f = (const uint8_t*)memchr(cur, nb[0], (size_t)(hi - cur) - n + 1);
            if (!f) return hi;
            if (!memcmp(f, nb, n)) return f;
            cur = f + 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *fault = true;
    }
    return hi;
}

// count matches in the executable sections (stops at 2); anchor = the longest literal run
int Scan(const Pat& pt, uintptr_t& hit) {
    size_t best = 0, blen = 0;
    for (size_t i = 0; i < pt.b.size();) {
        if (!pt.m[i]) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < pt.b.size() && pt.m[j]) ++j;
        if (j - i > blen) {
            blen = j - i;
            best = i;
        }
        i = j;
    }
    if (blen < 3) return -1;
    const uint8_t* nb = pt.b.data() + best;
    int n = 0;
    for (const Sec& s : g_exec) {
        const uint8_t* lo = (const uint8_t*)s.a;
        const uint8_t* hi = lo + s.n;
        for (const uint8_t* cur = lo + best;;) {
            bool fault = false;
            const uint8_t* f = FindRun(cur, hi, nb, blen, &fault);
            if (fault) return -2;
            if (f == hi) break;
            const uint8_t* st = f - best;
            if (st >= lo && st + pt.b.size() <= hi && MatchAt(st, pt)) {
                if (++n == 1) hit = (uintptr_t)st;
                if (n >= 2) return n;
            }
            cur = f + 1;
        }
    }
    return n;
}

// script native table: name -> fn
std::unordered_map<std::string, uintptr_t> g_natives;
const char* g_nativeWhy = nullptr;

bool Readable(uintptr_t a) {
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery((void*)a, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
           (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) &&
           !(mbi.Protect & PAGE_GUARD);
}
// search the whole image (readable regions) for bytes; align = required alignment of the hit
uintptr_t FindInImage(const void* needle, size_t n, size_t align, uintptr_t from = 0) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!from) from = g_img.base;
    for (uintptr_t a = from; a < g_img.end;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = std::min<uintptr_t>(rb + mbi.RegionSize, g_img.end);
        if (Readable(rb)) {
            const uint8_t* lo = (const uint8_t*)std::max(rb, from);
            const uint8_t* hi = (const uint8_t*)re;
            const uint8_t* nb = (const uint8_t*)needle;
            for (const uint8_t* cur = lo;;) {
                bool fault = false;
                const uint8_t* f = FindRun(cur, hi, nb, n, &fault);
                if (fault || f == hi) break;
                if (((uintptr_t)f % align) == 0) return (uintptr_t)f;
                cur = f + 1;
            }
        }
        if (re <= a) break;
        a = re;
    }
    return 0;
}
bool InImg(uint64_t v) { return v > g_img.base && v < g_img.end; }
bool NameAt(uint64_t p, char* out, size_t cap) {
    __try {
        size_t i = 0;
        for (; i + 1 < cap; ++i) {
            const char c = *(const char*)(p + i);
            if (!c) break;
            if (c < 0x20 || c > 0x7E) return false;
            out[i] = c;
        }
        out[i] = 0;
        return i > 2 && strstr(out, "::") != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void LoadNatives() {
    static const char key[] = "Mission::BossBattleMode";  // sizeof includes the terminating NUL
    uintptr_t s = 0;
    for (uintptr_t from = 0;;) {
        s = FindInImage(key, sizeof(key), 1, from);
        uint8_t prev = 1;
        if (!s || (Rd(s - 1, prev) && prev == 0)) break;
        from = s + 1;
    }
    if (!s) {
        g_nativeWhy = "script native table: name string not found";
        return;
    }
    const uint64_t va = s;
    const uintptr_t p0 = FindInImage(&va, 8, 8);
    if (!p0) {
        g_nativeWhy = "script native table: no pointer to the name";
        return;
    }
    uintptr_t p = p0;
    char nm[128];
    for (int k = 0; k < 4096; ++k) {  // walk back to the first entry
        uint64_t n = 0, f = 0;
        if (!Rd(p - 16, n) || !Rd(p - 8, f) || !InImg(n) || !InImg(f) || !NameAt(n, nm, sizeof(nm))) break;
        p -= 16;
    }
    for (int k = 0; k < 8192; ++k, p += 16) {
        uint64_t n = 0, f = 0;
        if (!Rd(p, n) || !Rd(p + 8, f) || !InImg(n) || !InImg(f) || !NameAt(n, nm, sizeof(nm))) break;
        g_natives.emplace(nm, (uintptr_t)f);
    }
    if (g_natives.size() < 100) g_nativeWhy = "script native table: too few entries";
}

void Fail(int i, const char* fmt, const char* nm) {
    char b[160];
    snprintf(b, sizeof(b), fmt, nm);
    g_why[i] = b;
    g_res[i].why = g_why[i].c_str();
}

bool HeadHooked(uintptr_t p, size_t head) {  // E9 rel32 + NOPs over the head (another module's entry hook)
    uint8_t b[16] = {};
    if (head < 5 || head > 16 || !Copy(b, p, head) || b[0] != 0xE9) return false;
    for (size_t i = 5; i < head; ++i)
        if (b[i] != 0x90) return false;
    return true;
}

void ResolveOne(int i) {
    const sites::Def& d = sites::kDefs[i];
    Res& r = g_res[i];
    const Pat pt = Parse(d.pattern);
    if (d.kind == sites::kNative) {
        if (g_nativeWhy) {
            g_why[i] = g_nativeWhy;
            r.why = g_why[i].c_str();
            return;
        }
        auto it = g_natives.find(d.native);
        if (it == g_natives.end()) return Fail(i, "native %s not in the table", d.native);
        uint8_t cur[16] = {};
        if (!Copy(cur, it->second, pt.b.size())) return Fail(i, "native %s unreadable", d.native);
        if (MatchAt(cur, pt)) {
            r.addr = it->second;
            return;
        }
        if (HeadHooked(it->second, d.head)) {
            r.addr = it->second;
            r.hookedByOther = true;
            return;
        }
        return Fail(i, "head bytes of %s differ (game patch: re-derive)", d.native);
    }
    uintptr_t at = 0;
    // fast path: the known build, at the expected RVA (also accepts another module's hook over the head)
    if (g_img.knownBuild) {
        const uintptr_t exp = g_img.base + (d.kind == sites::kGlobal ? d.anchor : d.rva) - d.offset;
        uint8_t buf[128];
        if (pt.b.size() <= sizeof(buf) && Copy(buf, exp, pt.b.size())) {
            if (MatchAt(buf, pt)) at = exp;
            else if (d.kind == sites::kHook && MatchAt(buf, pt, d.head) && HeadHooked(exp + d.offset, d.head)) {
                at = exp;
                r.hookedByOther = true;
            }
        }
    }
    if (!at) {
        uintptr_t hit = 0;
        const int n = Scan(pt, hit);
        if (n == 0) return Fail(i, "pattern not found: %s", d.name);
        if (n >= 2) return Fail(i, "pattern matches twice: %s", d.name);
        if (n < 0) return Fail(i, "pattern scan failed: %s", d.name);
        at = hit;
    }
    const uintptr_t site = at + d.offset;
    if (d.kind == sites::kGlobal) {
        int32_t disp = 0;
        if (!Rd(site + d.dispOff, disp)) return Fail(i, "anchor unreadable: %s", d.name);
        r.addr = (uintptr_t)((int64_t)site + d.insnLen + disp + d.addend);
        if (!InImg(r.addr)) {
            r.addr = 0;
            return Fail(i, "global outside the image: %s", d.name);
        }
        return;
    }
    r.addr = site;
}

}  // namespace

// SteamStub decrypts .text after the ASI loads: poll three sentinel patterns (never touch the stub)
bool CodeReady() {
    if (g_exec.empty()) Sections();
    static const char* const kSent[] = {"PerFrameTickCall", "fn:MissionRequestById", "chk:EmakiRecordGet_0x34"};
    for (const char* nm : kSent) {
        int i = SiteIndex(nm);
        if (i < 0) return false;
        const sites::Def& d = sites::kDefs[i];
        const Pat pt = Parse(d.pattern);
        if (g_img.knownBuild) {
            uint8_t buf[128];
            if (pt.b.size() > sizeof(buf) || !Copy(buf, g_img.base + d.rva - d.offset, pt.b.size()) || !MatchAt(buf, pt)) return false;
        } else {
            uintptr_t hit = 0;
            if (Scan(pt, hit) != 1) return false;
        }
    }
    return true;
}

void ResolveAll() {
    if (g_exec.empty()) Sections();
    LoadNatives();
    for (int i = 0; i < sites::kCount; ++i) ResolveOne(i);
    // one line per group
    const char* groups[16];
    int ng = 0;
    for (int i = 0; i < sites::kCount; ++i) {
        bool have = false;
        for (int k = 0; k < ng; ++k) have |= !strcmp(groups[k], sites::kDefs[i].group);
        if (!have && ng < 16) groups[ng++] = sites::kDefs[i].group;
    }
    for (int k = 0; k < ng; ++k) {
        int n = 0, ok = 0, other = 0, moved = 0;
        const char* why = nullptr;
        for (int i = 0; i < sites::kCount; ++i) {
            if (strcmp(sites::kDefs[i].group, groups[k])) continue;
            ++n;
            if (g_res[i].addr) {
                ++ok;
                other += g_res[i].hookedByOther;
                const sites::Def& d = sites::kDefs[i];
                const uint32_t exp = d.kind == sites::kGlobal ? d.rva : d.rva;
                moved += (g_res[i].addr - g_img.base) != exp;
            } else if (!why) {
                why = g_res[i].why;
            }
        }
        Log("Sites %-7s %d/%d resolved%s%s%s%s", groups[k], ok, n, moved ? " (moved vs v2.02: pattern scan)" : "",
            other ? " (some heads already hooked by another module: chained)" : "", why ? " - first failure: " : "", why ? why : "");
    }
}

uintptr_t Site(const char* name) {
    for (int i = 0; i < sites::kCount; ++i)
        if (!strcmp(sites::kDefs[i].name, name)) return g_res[i].addr;
    return 0;
}

bool GroupOk(const char* group, const char** why) {
    for (int i = 0; i < sites::kCount; ++i) {
        if (strcmp(sites::kDefs[i].group, group)) continue;
        if (!g_res[i].addr) {
            if (why) *why = g_res[i].why ? g_res[i].why : "unresolved";
            return false;
        }
    }
    return true;
}

int SiteIndex(const char* name) {
    for (int i = 0; i < sites::kCount; ++i)
        if (!strcmp(sites::kDefs[i].name, name)) return i;
    return -1;
}
bool SiteHookedByOther(int i) { return i >= 0 && g_res[i].hookedByOther; }

}  // namespace addon
