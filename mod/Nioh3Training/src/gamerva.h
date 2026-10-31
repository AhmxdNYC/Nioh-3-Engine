// gamerva.h (Nioh3Training, C) - every fixed game address goes through Grva(rva) (tools/check_fixed_rvas.py enforces it).
// Port of mod/Nioh3EngineFix/src/gamerva.{h,cpp}: rva is the v2.02 RVA; on the known build (TimeDateStamp + SizeOfImage match) Grva()
// returns it unchanged - zero cost, no behaviour change. On another build the RVA's byte pattern (rvas.json -> rvas_gen.h, made by
// tools/gen_rvas.py) is looked up in the running image and the RVA on THIS build is returned; 0 if the pattern is missing / not
// unique, or the RVA is not in the table (never guess). Kinds:
//   GRV_CODE    the bytes at rva - offset match `pattern` exactly once in the executable sections -> rva = hit + offset
//   GRV_GLOBAL  data slot: rip-relative anchor instruction found by pattern; rva = anchor + insnLen + disp32 + addend
//   GRV_IAT     import slot dll!func (or dll!#ordinal): found in the import directory by name (no pattern)
//   GRV_NONE    no unique pattern exists: known build only, 0 elsewhere
// Included once, by training.c, right after <windows.h>. GrInit(B, Log) in DllMain; GrVerify() once .text is decrypted.
#pragma once
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rvas_gen.h"

static int g_gr_known = 1;
static uintptr_t g_gr_base;
static uint32_t g_gr_hi;
static void (*g_gr_log)(const char*, ...);
static struct { const uint8_t* p; size_t n; } g_gr_exec[16];
static int g_gr_nexec;
static volatile LONG g_gr_cache[GR_COUNT];  // resolved rva + 1 (0 = not tried, 1 = failed); a race only repeats the same lookup

typedef struct { uint8_t b[160]; uint8_t m[160]; size_t n; } GrPat;
static int GrParse(const char* s, GrPat* p) {
    p->n = 0;
    for (const char* c = s; *c && p->n < sizeof p->b;) {
        while (*c == ' ') ++c;
        if (!*c) break;
        if (c[0] == '?') { p->b[p->n] = 0; p->m[p->n++] = 0; }
        else { char h[3] = {c[0], c[1], 0}; p->b[p->n] = (uint8_t)strtoul(h, 0, 16); p->m[p->n++] = 1; }
        c += 2;
    }
    return p->n > 0;
}
static __attribute__((noinline)) int GrMatchAt(const uint8_t* q, const GrPat* p, size_t skipLo, size_t skipHi) {
    __try {
        for (size_t i = 0; i < p->n; ++i)
            if (p->m[i] && !(i >= skipLo && i < skipHi) && q[i] != p->b[i]) return 0;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static void GrAnchor(const GrPat* p, size_t* off, size_t* len) {  // longest literal run = memchr anchor
    *off = *len = 0;
    for (size_t i = 0, cur = 0, st = 0; i < p->n; ++i) {
        if (!p->m[i]) { cur = 0; continue; }
        if (cur == 0) st = i;
        if (++cur > *len) { *len = cur; *off = st; }
    }
}
// matches in the executable sections (stops at 2); *hit = first match
static __attribute__((noinline)) int GrScan(const GrPat* p, const uint8_t** hit) {
    size_t ao, al;
    GrAnchor(p, &ao, &al);
    if (!al) return -1;
    int n = 0;
    for (int k = 0; k < g_gr_nexec; ++k) {
        const uint8_t* lo = g_gr_exec[k].p;
        const uint8_t* hi = lo + g_gr_exec[k].n;
        __try {
            for (const uint8_t* cur = lo + ao; cur + (p->n - ao) <= hi;) {
                const uint8_t* f = (const uint8_t*)memchr(cur, p->b[ao], (size_t)(hi - cur));
                if (!f || f + (p->n - ao) > hi) break;
                const uint8_t* st = f - ao;
                if (st >= lo && !memcmp(f, p->b + ao, al) && GrMatchAt(st, p, 0, 0)) {
                    if (n++ == 0) *hit = st;
                    if (n >= 2) return n;
                }
                cur = f + 1;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    }
    return n;
}
static const IMAGE_NT_HEADERS64* GrNt(void) { return (const IMAGE_NT_HEADERS64*)(g_gr_base + ((const IMAGE_DOS_HEADER*)g_gr_base)->e_lfanew); }
static uint32_t GrIatSlot(const char* name) {  // "KERNEL32.dll!ReadFile" or "XINPUT1_4.dll!#2"
    const char* bang = strchr(name, '!');
    if (!bang) return 0;
    const IMAGE_DATA_DIRECTORY dir = GrNt()->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    for (const IMAGE_IMPORT_DESCRIPTOR* d = (const IMAGE_IMPORT_DESCRIPTOR*)(g_gr_base + dir.VirtualAddress); d->Name; ++d) {
        const char* dn = (const char*)(g_gr_base + d->Name);
        if (_strnicmp(dn, name, (size_t)(bang - name)) || dn[bang - name]) continue;
        const IMAGE_THUNK_DATA64* ilt = (const IMAGE_THUNK_DATA64*)(g_gr_base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (uint32_t k = 0; ilt[k].u1.AddressOfData; ++k) {
            if (IMAGE_SNAP_BY_ORDINAL64(ilt[k].u1.Ordinal)) {
                if (bang[1] == '#' && IMAGE_ORDINAL64(ilt[k].u1.Ordinal) == strtoul(bang + 2, 0, 10)) return d->FirstThunk + 8 * k;
                continue;
            }
            const IMAGE_IMPORT_BY_NAME* ibn = (const IMAGE_IMPORT_BY_NAME*)(g_gr_base + (uint32_t)ilt[k].u1.AddressOfData);
            if (!strcmp((const char*)ibn->Name, bang + 1)) return d->FirstThunk + 8 * k;
        }
    }
    return 0;
}
static uint32_t GrExpected(const GrDef* d) { return (d->kind == GRV_GLOBAL ? d->anchor : d->rva) - d->offset; }  // v2.02 match start
static uint32_t GrResolveOne(int i) {
    const GrDef* d = &k_grdefs[i];
    if (d->kind == GRV_IAT) return GrIatSlot(d->iat);
    if (d->kind == GRV_NONE) return 0;
    GrPat p; const uint8_t* hit = 0;
    if (!GrParse(d->pattern, &p) || GrScan(&p, &hit) != 1) return 0;
    const uintptr_t site = (uintptr_t)hit + d->offset;
    if (d->kind == GRV_CODE) return (uint32_t)(site - g_gr_base);
    int32_t disp = 0;
    memcpy(&disp, (const void*)(site + d->dispOff), 4);
    const int64_t r = (int64_t)(site - g_gr_base) + d->insnLen + disp + d->addend;
    return r > 0 && (uint64_t)r < g_gr_hi ? (uint32_t)r : 0;
}
static int GrIndex(uint32_t rva) {  // k_grdefs is sorted by rva
    int lo = 0, hi = GR_COUNT - 1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (k_grdefs[mid].rva == rva) return mid; if (k_grdefs[mid].rva < rva) lo = mid + 1; else hi = mid - 1; }
    return -1;
}
static uint32_t Gr_Resolve(uint32_t rva) {
    const int i = GrIndex(rva);
    if (i < 0) return 0;  // not in rvas.json: never guess on a foreign build
    LONG c = g_gr_cache[i];
    if (!c) { c = (LONG)GrResolveOne(i) + 1; g_gr_cache[i] = c; }
    return (uint32_t)c - 1;
}
static inline uint32_t Grva(uint32_t rva) { return g_gr_known ? rva : Gr_Resolve(rva); }

// DllMain: right after the image base is known (PE header only; .text may still be encrypted).
static void GrInit(uintptr_t base, void (*log)(const char*, ...)) {
    g_gr_base = base; g_gr_log = log;
    const IMAGE_NT_HEADERS64* nt = GrNt();
    g_gr_hi = nt->OptionalHeader.SizeOfImage;
    const IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
    for (int k = 0; k < nt->FileHeader.NumberOfSections && g_gr_nexec < 16; ++k)
        if (sh[k].Characteristics & IMAGE_SCN_MEM_EXECUTE) { g_gr_exec[g_gr_nexec].p = (const uint8_t*)(base + sh[k].VirtualAddress); g_gr_exec[g_gr_nexec++].n = sh[k].Misc.VirtualSize; }
    g_gr_known = nt->FileHeader.TimeDateStamp == GR_BUILD_TIMESTAMP && nt->OptionalHeader.SizeOfImage == GR_BUILD_SIZE_OF_IMAGE;
}
// Once .text is decrypted: logs "game RVAs (rvas.json, N entries): ..."; returns the number of entries NOT found at their expected
// place (known build) / not resolved (other build, which also fills the cache so later lookups are a table read).
static int GrVerify(void) {
    if (!g_gr_base) return -1;
    const DWORD t0 = GetTickCount();
    int ok = 0, iat = 0, none = 0, bad = 0, hooked = 0;
    char miss[512] = "";
    for (int i = 0; i < GR_COUNT; ++i) {
        const GrDef* d = &k_grdefs[i];
        int good = 0; GrPat p;
        if (d->kind == GRV_NONE) { ++none; continue; }
        if (!g_gr_known) good = Gr_Resolve(d->rva) != 0;
        else if (d->kind == GRV_IAT) { good = GrIatSlot(d->iat) == d->rva; iat += good; }
        else {
            const uint8_t* at = (const uint8_t*)(g_gr_base + GrExpected(d));
            if (GrParse(d->pattern, &p) && GrMatchAt(at, &p, 0, 0)) good = 1;
            else if (d->kind == GRV_CODE && GrParse(d->pattern, &p) && (at[d->offset] == 0xE9 || at[d->offset] == 0xCC || (at[d->offset] == 0xFF && at[d->offset + 1] == 0x25)) && GrMatchAt(at, &p, (size_t)d->offset, (size_t)d->offset + 16)) { good = 1; ++hooked; }  // a hook already sits on the site
        }
        if (good) ++ok;
        else { ++bad; size_t l = strlen(miss); if (l < sizeof miss - 12) snprintf(miss + l, sizeof miss - l, " %X", d->rva); }
    }
    if (g_gr_log)
        g_gr_log("game RVAs (rvas.json, %d entries): %d %s (%d with a hook on them, %d imports by name), %d missing%s%s, %d without a pattern (known build only) [%lu ms]",
                 GR_COUNT, ok, g_gr_known ? "verified at their v2.02 place" : "resolved by pattern on this build", hooked, iat, bad, bad ? ":" : "", miss, none, GetTickCount() - t0);
    return bad;
}
