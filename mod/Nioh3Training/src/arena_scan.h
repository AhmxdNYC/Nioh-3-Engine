// arena_scan.h - #23436/#23447/#23451/#23479: the arena table derived in-process from the game's own data files at startup.
// Pure C (stdio + malloc + math), no game addresses: reads <game>/archive (archive_NN.bin + lfm_order_NN.bin bundles, loose XX/<hash> files),
// the LFM id table, the mission table (0x198-byte records), per mission its enemy tables (+0x54/+0x58, 0x138 B), PlacementBox table (+0x5C,
// 0xA0 B), start point table (+0x6C, 0xA4 B) and script (+0x50, trigger boxes tested with Player::IsEnter* / Wait_Invade* / Loop_*).
// A C port of tools/n3lfm.py + tools/auto_arena.py + rematch_catalog.pick_fight_start / pick_arena: same rules, same rows (checked offline by
// mod/Nioh3Training/tests/arena_scan_test.c against the Python generator). The known file paths are only the fast path: when they fail (a patch moved them) the
// tables are found again by structure. Every archive_*.bin present is indexed, so DLC bundles are seen without a code change.
// Host: define AS_LOG(fmt, ...) before including (default: nothing). Entry: AsScan(gameDir, cb, ctx) -> rows found (< 0 = data not readable).
#ifndef ARENA_SCAN_H
#define ARENA_SCAN_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#ifndef AS_LOG
#define AS_LOG(...) ((void)0)
#endif

// ---------------------------------------------------------------- inflate (RFC 1951, stored / fixed / dynamic blocks)
typedef struct { const uint8_t* in; size_t inLen, pos; uint32_t bit, nbit; uint8_t* out; size_t outLen, outCap; int err; } AsInf;
typedef struct { uint16_t count[16], sym[320]; } AsHuff;
static uint32_t AsBits(AsInf* s, int n) {
    while (s->nbit < (uint32_t)n) { if (s->pos >= s->inLen) { s->err = 1; return 0; } s->bit |= (uint32_t)s->in[s->pos++] << s->nbit; s->nbit += 8; }
    uint32_t v = s->bit & ((1u << n) - 1); s->bit >>= n; s->nbit -= n; return v;
}
static int AsBuild(AsHuff* h, const uint8_t* len, int n) {
    uint16_t offs[16]; memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; ++i) h->count[len[i]]++;
    h->count[0] = 0; offs[1] = 0;
    for (int i = 1; i < 15; ++i) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; ++i) if (len[i]) h->sym[offs[len[i]]++] = (uint16_t)i;
    return 0;
}
static int AsDecode(AsInf* s, const AsHuff* h) {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; ++l) {
        code |= (int)AsBits(s, 1); if (s->err) return -1;
        int c = h->count[l];
        if (code - c < first) return h->sym[index + (code - first)];
        index += c; first += c; first <<= 1; code <<= 1;
    }
    s->err = 1; return -1;
}
static void AsPut(AsInf* s, uint8_t b) { if (s->outLen < s->outCap) s->out[s->outLen] = b; s->outLen++; }
static int AsCodes(AsInf* s, const AsHuff* lc, const AsHuff* dc) {
    static const uint16_t lb[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t le[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t db[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t de[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = AsDecode(s, lc); if (sym < 0 || s->err) return -1;
        if (sym < 256) { AsPut(s, (uint8_t)sym); if (s->outLen >= s->outCap) return 1; continue; }
        if (sym == 256) return 0;
        sym -= 257; if (sym >= 29) return -1;
        int len = lb[sym] + (int)AsBits(s, le[sym]);
        int ds = AsDecode(s, dc); if (ds < 0 || ds >= 30) return -1;
        size_t dist = db[ds] + AsBits(s, de[ds]); if (s->err || dist > s->outLen) return -1;
        for (int i = 0; i < len; ++i) { size_t from = s->outLen - dist; AsPut(s, from < s->outCap ? s->out[from] : 0); }
        if (s->outLen >= s->outCap) return 1;
    }
}
// raw deflate -> out (stops early once outCap bytes are produced: return 1); 0 = stream end, -1 = error. *used = input bytes consumed.
static int AsInflate(const uint8_t* in, size_t inLen, uint8_t* out, size_t outCap, size_t* outLen, size_t* used) {
    AsInf s; memset(&s, 0, sizeof s); s.in = in; s.inLen = inLen; s.out = out; s.outCap = outCap;
    static AsHuff fl, fd; static int fixedOk;
    if (!fixedOk) { uint8_t l[320]; int i = 0; for (; i < 144; ++i) l[i] = 8; for (; i < 256; ++i) l[i] = 9; for (; i < 280; ++i) l[i] = 7; for (; i < 288; ++i) l[i] = 8; AsBuild(&fl, l, 288); for (i = 0; i < 30; ++i) l[i] = 5; AsBuild(&fd, l, 30); fixedOk = 1; }
    int last, r = 0;
    do {
        last = (int)AsBits(&s, 1); int type = (int)AsBits(&s, 2); if (s.err) return -1;
        if (type == 0) {
            s.bit = 0; s.nbit = 0;
            if (s.pos + 4 > s.inLen) return -1;
            unsigned n = s.in[s.pos] | (s.in[s.pos + 1] << 8); s.pos += 4;
            if (s.pos + n > s.inLen) return -1;
            for (unsigned i = 0; i < n; ++i) { AsPut(&s, s.in[s.pos++]); if (s.outLen >= s.outCap) { r = 1; break; } }
            if (r) break;
        } else if (type == 1) {
            r = AsCodes(&s, &fl, &fd); if (r) break;
        } else if (type == 2) {
            static const uint8_t ord[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            int nlen = (int)AsBits(&s, 5) + 257, ndist = (int)AsBits(&s, 5) + 1, ncode = (int)AsBits(&s, 4) + 4;
            uint8_t l[320] = {0}; AsHuff lc, dc;
            if (s.err || nlen > 286 || ndist > 30) return -1;
            for (int i = 0; i < ncode; ++i) l[ord[i]] = (uint8_t)AsBits(&s, 3);
            AsBuild(&lc, l, 19);
            int idx = 0;
            while (idx < nlen + ndist) {
                int sym = AsDecode(&s, &lc); if (sym < 0 || s.err) return -1;
                if (sym < 16) { l[idx++] = (uint8_t)sym; continue; }
                int rep, val = 0;
                if (sym == 16) { if (!idx) return -1; val = l[idx - 1]; rep = 3 + (int)AsBits(&s, 2); }
                else if (sym == 17) rep = 3 + (int)AsBits(&s, 3);
                else rep = 11 + (int)AsBits(&s, 7);
                if (idx + rep > nlen + ndist) return -1;
                while (rep--) l[idx++] = (uint8_t)val;
            }
            AsBuild(&lc, l, nlen); AsBuild(&dc, l + nlen, ndist);
            r = AsCodes(&s, &lc, &dc); if (r) break;
        } else return -1;
    } while (!last);
    *outLen = s.outLen < outCap ? s.outLen : outCap; *used = s.pos;
    return r < 0 ? -1 : r;
}

// ---------------------------------------------------------------- LFM namespace (n3lfm.py)
typedef struct { char path[68]; int arc; uint64_t off, usize, csize; uint32_t comp; } AsEnt;   // arc -1 = loose file (path on disk in AsLfm.loose)
typedef struct { char root[512]; AsEnt* e; int n, cap; char (*arcPath)[540]; int narc; } AsLfm;
static int AsCmpEnt(const void* a, const void* b) { return strcmp(((const AsEnt*)a)->path, ((const AsEnt*)b)->path); }
static void AsUpper(char* s) { for (; *s; ++s) if (*s >= 'a' && *s <= 'z') *s -= 32; }
static AsEnt* AsAdd(AsLfm* L) { if (L->n == L->cap) { L->cap = L->cap ? L->cap * 2 : 8192; L->e = (AsEnt*)realloc(L->e, L->cap * sizeof(AsEnt)); } memset(&L->e[L->n], 0, sizeof(AsEnt)); return &L->e[L->n++]; }
static uint8_t* AsReadFile(const char* p, size_t* n) {
    FILE* f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* b = sz > 0 ? (uint8_t*)malloc((size_t)sz) : 0;
    if (b && fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); b = 0; }
    fclose(f); *n = b ? (size_t)sz : 0; return b;
}
#ifdef _WIN32
#define AS_SEP "\\"
static int AsListLoose(AsLfm* L) {   // archive\XX\<62 hex>
    char pat[600]; snprintf(pat, sizeof pat, "%s\\??", L->root); WIN32_FIND_DATAA d; HANDLE h = FindFirstFileA(pat, &d); int n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || strlen(d.cFileName) != 2) continue;
        char sub[600]; snprintf(sub, sizeof sub, "%s\\%s\\*", L->root, d.cFileName); WIN32_FIND_DATAA f; HANDLE g = FindFirstFileA(sub, &f);
        if (g == INVALID_HANDLE_VALUE) continue;
        do { if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue; AsEnt* e = AsAdd(L); snprintf(e->path, sizeof e->path, "/%s/%s", d.cFileName, f.cFileName); AsUpper(e->path); e->arc = -1; ++n; } while (FindNextFileA(g, &f));
        FindClose(g);
    } while (FindNextFileA(h, &d));
    FindClose(h); return n;
}
#else
#include <dirent.h>
#define AS_SEP "/"
static int AsListLoose(AsLfm* L) {
    DIR* d = opendir(L->root); struct dirent* x; int n = 0; if (!d) return 0;
    while ((x = readdir(d))) {
        if (strlen(x->d_name) != 2 || x->d_name[0] == '.') continue;
        char sub[600]; snprintf(sub, sizeof sub, "%s/%s", L->root, x->d_name); DIR* g = opendir(sub); struct dirent* y; if (!g) continue;
        while ((y = readdir(g))) { if (y->d_name[0] == '.') continue; AsEnt* e = AsAdd(L); snprintf(e->path, sizeof e->path, "/%s/%s", x->d_name, y->d_name); AsUpper(e->path); e->arc = -1; ++n; }
        closedir(g);
    }
    closedir(d); return n;
}
#endif
static void AsLfmFree(AsLfm* L) { free(L->e); free(L->arcPath); memset(L, 0, sizeof *L); }
static int AsLfmOpen(AsLfm* L, const char* game) {
    memset(L, 0, sizeof *L); snprintf(L->root, sizeof L->root, "%s" AS_SEP "archive", game);
    L->arcPath = (char(*)[540])calloc(100, 540);
    for (int nn = 0; nn < 100; ++nn) {   // archive_00 .. archive_99 (DLC bundles add new numbers)
        char ap[540], op[540]; snprintf(ap, sizeof ap, "%s" AS_SEP "archive_%02d.bin", L->root, nn); snprintf(op, sizeof op, "%s" AS_SEP "lfm_order_%02d.bin", L->root, nn);
        FILE* f = fopen(ap, "rb"); if (!f) continue;
        uint8_t head[0x20]; size_t on = 0; uint8_t* o = 0;
        if (fread(head, 1, 0x20, f) != 0x20 || memcmp(head, "LFMB", 4) || !(o = AsReadFile(op, &on)) || on < 0x14 || memcmp(o, "LFMO", 4)) { fclose(f); free(o); continue; }
        uint32_t cnt; memcpy(&cnt, head + 8, 4);
        uint8_t* tab = (uint8_t*)malloc((size_t)cnt * 0x20 + 1);
        if (!tab || fread(tab, 1, (size_t)cnt * 0x20, f) != (size_t)cnt * 0x20) { fclose(f); free(o); free(tab); continue; }
        fclose(f);
        int ai = L->narc++; snprintf(L->arcPath[ai], 540, "%s", ap);
        uint32_t ocnt, toff; memcpy(&ocnt, o + 8, 4); memcpy(&toff, o + 0x10, 4);
        for (uint32_t k = 0; k < ocnt && toff + 12 * (k + 1) <= on; ++k) {
            uint32_t idx, so; memcpy(&idx, o + toff + 12 * k + 4, 4); memcpy(&so, o + toff + 12 * k + 8, 4);
            if (idx >= cnt || so >= on) continue;
            AsEnt* e = AsAdd(L); snprintf(e->path, sizeof e->path, "%.*s", (int)strnlen((const char*)o + so, on - so), (const char*)o + so); AsUpper(e->path);
            e->arc = ai; memcpy(&e->off, tab + 0x20 * idx, 8); memcpy(&e->usize, tab + 0x20 * idx + 8, 8); memcpy(&e->csize, tab + 0x20 * idx + 16, 8); memcpy(&e->comp, tab + 0x20 * idx + 24, 4);
        }
        free(o); free(tab);
    }
    int loose = AsListLoose(L);
    if (!L->n) return 0;
    qsort(L->e, L->n, sizeof(AsEnt), AsCmpEnt);
    // a loose file overrides a bundled entry of the same path: keep the last of equal paths with arc -1 preferred
    int w = 0;
    for (int i = 0; i < L->n; ++i) {
        if (w && !strcmp(L->e[w - 1].path, L->e[i].path)) { if (L->e[i].arc == -1 || L->e[w - 1].arc != -1) L->e[w - 1] = L->e[i]; continue; }
        L->e[w++] = L->e[i];
    }
    L->n = w;
    AS_LOG("ARENASCAN: %d LFM entries (%d bundles, %d loose files) in %s", L->n, L->narc, loose, L->root);
    return L->n;
}
static AsEnt* AsFind(AsLfm* L, const char* path) {
    AsEnt k; snprintf(k.path, sizeof k.path, "%s", path); AsUpper(k.path);
    return (AsEnt*)bsearch(&k, L->e, L->n, sizeof(AsEnt), AsCmpEnt);
}
// entry -> bytes (malloc). cap > 0: stop after cap bytes (header sniffing). *n = bytes returned.
static uint8_t* AsGetEnt(AsLfm* L, const AsEnt* e, size_t cap, size_t* n) {
    *n = 0;
    if (e->arc < 0) {
        char p[700]; snprintf(p, sizeof p, "%s" AS_SEP "%.2s" AS_SEP "%s", L->root, e->path + 1, e->path + 4);
        if (cap) { FILE* f = fopen(p, "rb"); if (!f) return 0; uint8_t* b = (uint8_t*)malloc(cap); size_t got = b ? fread(b, 1, cap, f) : 0; fclose(f); *n = got; return b; }
        size_t sz; uint8_t* b = AsReadFile(p, &sz); if (!b) return 0;
        *n = sz; return b;
    }
    size_t want = cap && cap < e->usize ? cap : (size_t)e->usize;
    if (!e->usize || e->usize > (512u << 20)) return 0;
    FILE* f = fopen(L->arcPath[e->arc], "rb"); if (!f) return 0;
    size_t clen = e->comp ? (size_t)e->csize : want;
    if (e->comp && cap) clen = clen < 4096 + cap * 2 ? clen : 4096 + cap * 2;
    uint8_t* c = (uint8_t*)malloc(clen + 1), *out = (uint8_t*)malloc(want + 1);
#ifdef _WIN32
    _fseeki64(f, (long long)e->off, SEEK_SET);
#else
    fseeko(f, (off_t)e->off, SEEK_SET);
#endif
    size_t got = c ? fread(c, 1, clen, f) : 0; fclose(f);
    if (!c || !out || got < (e->comp ? 4 : want)) { free(c); free(out); return 0; }
    if (!e->comp) { memcpy(out, c, want); free(c); *n = want; return out; }
    size_t p = 0, ol = 0;
    while (ol < want && p + 4 <= got) {
        uint32_t hdr; memcpy(&hdr, c + p, 4);
        if (p + 5 < got && c[p + 4] == 0x78) {
            size_t produced = 0, used = 0;
            int r = AsInflate(c + p + 6, got - p - 6, out + ol, want - ol, &produced, &used);
            ol += produced;
            if (r < 0) break;
            if (r == 1) break;            // cap reached
            p += 4 + 2 + used + 4;        // chunk header, zlib header, deflate data, adler32
        } else {                          // stored chunk: hdr = raw size
            size_t m = (hdr & 0x7FFF) ? (hdr & 0x7FFF) : (0x4000 < want - ol ? 0x4000 : want - ol);
            if (p + 4 + m > got) m = got - p - 4;
            if (m > want - ol) m = want - ol;
            memcpy(out + ol, c + p + 4, m); ol += m; p += 4 + m;
        }
        p = (p + 15) & ~(size_t)15;
    }
    free(c);
    if (ol < want) { free(out); return 0; }
    *n = want; return out;
}
static uint8_t* AsGet(AsLfm* L, const char* path, size_t* n) { AsEnt* e = path ? AsFind(L, path) : 0; *n = 0; return e ? AsGetEnt(L, e, 0, n) : 0; }

// ---------------------------------------------------------------- tables
#define AS_MAGIC 0x20042200u
#define AS_MREC 0x198
#define AS_EREC 0x138
#define AS_BREC 0xA0
#define AS_SREC 0xA4
static uint32_t AsU32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static float AsF32(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }
static uint32_t AsTab(const uint8_t* d, size_t n, size_t stride) {
    if (!d || n < 8 || AsU32(d) != AS_MAGIC) return 0;
    uint32_t c = AsU32(d + 4); return c && 8 + (uint64_t)c * stride <= n ? c : 0;
}
typedef struct { uint32_t id; char path[68]; } AsId;
typedef struct { AsId* v; int n; } AsIdTab;
static int AsCmpId(const void* a, const void* b) { uint32_t x = ((const AsId*)a)->id, y = ((const AsId*)b)->id; return x < y ? -1 : x > y; }
static const char* AsIdPath(const AsIdTab* t, uint32_t id) { AsId k; k.id = id; AsId* r = t->v ? (AsId*)bsearch(&k, t->v, t->n, sizeof(AsId), AsCmpId) : 0; return r ? r->path : 0; }
static int AsIsHexPath(const char* s) {   // XX/<62 hex>
    if (strlen(s) != 65 || s[2] != '/') return 0;
    for (int i = 0; i < 65; ++i) { if (i == 2) continue; char c = s[i]; if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return 0; }
    return 1;
}
static int AsParseIdTab(const uint8_t* d, size_t n, AsIdTab* t) {
    t->v = 0; t->n = 0;
    if (!d || n < 16 || AsU32(d) != 0) return 0;
    uint32_t c = AsU32(d + 4); if (c < 100 || c > 1000000 || 8 + (uint64_t)c * 8 > n) return 0;
    t->v = (AsId*)malloc(c * sizeof(AsId)); int good = 0;
    for (uint32_t i = 0; i < c; ++i) {
        size_t e = 8 + (size_t)i * 8; int32_t o; memcpy(&o, d + e + 4, 4);
        if ((int64_t)e + o < 0 || e + o >= n) { free(t->v); t->v = 0; return 0; }
        const char* s = (const char*)d + e + o; size_t l = strnlen(s, n - e - o); if (e + o + l >= n) { free(t->v); t->v = 0; return 0; }
        t->v[i].id = AsU32(d + e); snprintf(t->v[i].path, sizeof t->v[i].path, "/%.*s", (int)(l < 66 ? l : 66), s); AsUpper(t->v[i].path);
        if (i < 200 && AsIsHexPath(s)) ++good;
    }
    t->n = (int)c;
    if (good < (c < 150 ? (int)c : 150)) { free(t->v); t->v = 0; t->n = 0; return 0; }
    qsort(t->v, t->n, sizeof(AsId), AsCmpId);
    return 1;
}
static int AsMissionsOk(const uint8_t* d, size_t n, const AsIdTab* E) {
    uint32_t c = AsTab(d, n, AS_MREC); if (c < 20 || c > 4096) return 0;
    uint32_t ok = 0;
    for (uint32_t i = 0; i < c; ++i) { const uint8_t* r = d + 8 + (size_t)i * AS_MREC; ok += AsIdPath(E, AsU32(r + 0x50)) && AsIdPath(E, AsU32(r + 0x54)); }
    return ok >= c / 3;
}

// ---------------------------------------------------------------- per mission data (auto_arena.mission_data)
typedef struct { double pos[3], yaw, ext[3]; uint32_t id, flags, sub, pid, chr; uint8_t fl; char name[128]; } AsPt;
typedef struct { uint32_t mid, flags, nameId; uint8_t kind, world, shipped; AsPt *en, *st, *box, *trig; int nen, nst, nbox, ntrig; } AsMis;
static double AsR0(double v) { char b[64]; snprintf(b, sizeof b, "%.0f", v); return strtod(b, 0); }   // the Python f0 / f1 rounding
static double AsR1(double v) { char b[64]; snprintf(b, sizeof b, "%.1f", v); return strtod(b, 0); }
static void AsName(char* dst, const uint8_t* src, size_t max) {   // rematch_catalog.ascii_(latin1)[:60]: non-ASCII -> '?', stripped
    char t[512]; size_t n = 0;
    for (; n < max && n < sizeof t - 1 && src[n]; ++n) t[n] = (src[n] >= 32 && src[n] < 127) ? (char)src[n] : '?';
    t[n] = 0; char* a = t; while (*a == ' ') ++a; size_t l = strlen(a); while (l && a[l - 1] == ' ') a[--l] = 0;
    snprintf(dst, 61, "%s", a);
}
static void* AsPush(void* arr, int* n, size_t sz) { arr = realloc(arr, (*n + 1) * sz); memset((char*)arr + *n * sz, 0, sz); ++*n; return arr; }
static int AsSetHas(const int64_t* v, int n, int64_t x) { for (int i = 0; i < n; ++i) if (v[i] == x) return 1; return 0; }
// script -> the int constants pushed (op 0x29) in the 5 ops before a native call whose name starts with Player::IsEnter / Function::Wait_Invade /
// Function::Loop_ and contains Box or Invade (rematch_catalog._script_box_calls, 'out' part)
static int AsScriptTrig(const uint8_t* d, size_t n, int64_t** outv) {
    *outv = 0; int nout = 0;
    if (!d || n < 4) return 0;
    size_t q = 1 + d[0]; if (q + 2 > n) return 0;
    unsigned cnt = d[q] | (d[q + 1] << 8); q += 2;
    const char** names = (const char**)calloc(cnt + 1, sizeof(char*)); uint16_t* nlen = (uint16_t*)calloc(cnt + 1, 2);
    for (unsigned i = 0; i < cnt; ++i) { if (q + 6 > n) goto done; unsigned l = d[q] | (d[q + 1] << 8); if (q + 6 + l > n) goto done; names[i] = (const char*)d + q + 6; nlen[i] = (uint16_t)l; q += 6 + l + 1; }
    if (q + 2 > n) goto done;
    unsigned ns = d[q] | (d[q + 1] << 8); q += 2;
    static int32_t symOf[65536]; for (int i = 0; i < 65536; ++i) symOf[i] = -1;
    for (unsigned i = 0; i < ns; ++i) { if (q + 6 * i + 6 > n) goto done; unsigned a = d[q + 6 * i] | (d[q + 6 * i + 1] << 8), b = d[q + 6 * i + 2] | (d[q + 6 * i + 3] << 8); symOf[b] = (int32_t)a; }
    q += 6 * (size_t)ns;
    // string pool end: the first run of >= 3 printable/high bytes ending in NUL, then runs <= 5 bytes apart
    size_t pe = q, i = q; int first = 1;
    while (i < n) {
        int cls = d[i] >= 0x20 && d[i] != 0x7F;
        if (!cls) { ++i; continue; }
        size_t rs = i; while (i < n && d[i] >= 0x20 && d[i] != 0x7F) ++i;
        if (i < n && d[i] == 0 && i - rs >= 3) {
            if (!first && rs - pe > 5) break;
            pe = i + 1; first = 0; ++i;
        }
    }
    {
        size_t k = pe; while (k + 1 < n && !(d[k] == 0x35 && d[k + 1] == 0)) ++k;
        if (k + 1 >= n) goto done;
        i = k;
    }
    {
        // op walker with a ring of the last 5 ops
        int ro[5] = {0}; int32_t rv[5] = {0}; int nr = 0;
        while (i + 2 <= n) {
            unsigned o = d[i] | (d[i + 1] << 8); size_t ln;
            if (o == 0x1b || o == 0x1f || o == 0x3b || o == 0x3a || o == 0x01 || o == 0x2f || o == 0x50 || o == 0x51) ln = 2;
            else if (o == 0x29 || o == 0x2d || o == 0x22 || o == 0x23 || o == 0x24 || o == 0x30 || o == 0x31 || o == 0x05 || o == 0x11 || o == 0x3e || o == 0x3d) ln = 4;
            else if (o == 0x35 || o == 0x2c || o == 0x2e) { if (i + 4 > n) break; ln = 2 + (size_t)(o == 0x35 ? 8 : 4) * (d[i + 2] | (d[i + 3] << 8)); }
            else ln = 0;
            if (o > 0x5f || i + 2 + ln > n) break;
            const uint8_t* x = d + i + 2;
            if ((o == 0x1b || o == 0x1f) && ln == 2) {
                unsigned v = x[0] | (x[1] << 8); int32_t si = symOf[v];
                if (si >= 0 && (unsigned)si < cnt && names[si]) {
                    char nm[256]; snprintf(nm, sizeof nm, "%.*s", nlen[si] < 255 ? nlen[si] : 255, names[si]);
                    if ((!strncmp(nm, "Player::IsEnter", 15) || !strncmp(nm, "Function::Wait_Invade", 21) || !strncmp(nm, "Function::Loop_", 15)) && (strstr(nm, "Box") || strstr(nm, "Invade"))) {
                        for (int j = 0; j < nr && j < 5; ++j) { int s = (nr - 1 - j) % 5; if (ro[s] == 0x29 && !AsSetHas(*outv, nout, rv[s])) { *outv = (int64_t*)realloc(*outv, (nout + 1) * sizeof(int64_t)); (*outv)[nout++] = rv[s]; } }
                    }
                }
            }
            int s = nr % 5; ro[s] = (int)o; rv[s] = ln == 4 ? (int32_t)AsU32(x) : 0; if (ln != 4 && o == 0x29) ro[s] = -1; ++nr;
            i += 2 + ln;
        }
    }
done:
    free(names); free(nlen); return nout;
}
static const char* const AS_BAD_TRIG[] = {"BGM", "JUKE", "ITEM", "GUEST", "INV", "SILENCE", "CLEAR", "VOICE", "TALK", "NOKATANA", "NO_KATANA", 0};
static int AsHasAny(const char* u, const char* const* k) { for (; *k; ++k) if (strstr(u, *k)) return 1; return 0; }
static void AsUpperCopy(char* d, const char* s, size_t cap) { snprintf(d, cap, "%s", s); AsUpper(d); }
static void AsEnemies(AsMis* m, const uint8_t* et, size_t n) {
    uint32_t c = AsTab(et, n, AS_EREC); if (!c) return;
    size_t pool = 8 + (size_t)c * AS_EREC;
    for (uint32_t j = 0; j < c; ++j) {
        const uint8_t* e = et + 8 + (size_t)j * AS_EREC;
        m->en = (AsPt*)AsPush(m->en, &m->nen, sizeof(AsPt)); AsPt* p = &m->en[m->nen - 1];
        uint32_t no = AsU32(e + 0x134);
        if (pool + no < n) { size_t l = strnlen((const char*)et + pool + no, n - pool - no); snprintf(p->name, sizeof p->name, "%.*s", (int)(l < 127 ? l : 127), (const char*)et + pool + no); }
        p->pid = AsU32(e); p->chr = AsU32(e + 4); p->fl = e[0x9C];
        for (int k = 0; k < 3; ++k) p->pos[k] = AsR0(AsF32(e + 0x40 + 4 * k));
        p->yaw = AsR1(AsF32(e + 0x4C));
    }
}
static void AsMission(AsLfm* L, const AsIdTab* E, const uint8_t* r, AsMis* m) {
    memset(m, 0, sizeof *m);
    m->mid = r[0x158] | (r[0x159] << 8); m->kind = r[0x15A]; m->world = r[0x15C]; m->flags = AsU32(r + 0x40); m->nameId = AsU32(r + 0x70);
    uint32_t ids[8]; for (int k = 0; k < 8; ++k) ids[k] = AsU32(r + 0x50 + 4 * k);
    const char* sp = AsIdPath(E, ids[0]);
    m->shipped = sp != 0; for (int k = 1; k < 7; ++k) if (ids[k] && !AsIdPath(E, ids[k])) m->shipped = 0;
    size_t n; uint8_t* d;
    for (int k = 1; k <= 2; ++k) { d = AsGet(L, AsIdPath(E, ids[k]), &n); AsEnemies(m, d, n); free(d); }
    d = AsGet(L, AsIdPath(E, ids[7]), &n);
    for (uint32_t j = 0, c = AsTab(d, n, AS_SREC); j < c; ++j) {
        const uint8_t* s = d + 8 + (size_t)j * AS_SREC; m->st = (AsPt*)AsPush(m->st, &m->nst, sizeof(AsPt)); AsPt* p = &m->st[m->nst - 1];
        p->id = AsU32(s); p->flags = AsU32(s + 8); p->sub = AsU32(s + 0xC);
        for (int k = 0; k < 3; ++k) p->pos[k] = AsR0(AsF32(s + 0x40 + 4 * k));
        p->yaw = AsR1(AsF32(s + 0x4C));
    }
    free(d);
    int64_t* trig = 0; int ntrig = 0;
    d = AsGet(L, sp, &n); if (d) { ntrig = AsScriptTrig(d, n, &trig); free(d); }
    d = AsGet(L, AsIdPath(E, ids[3]), &n);
    uint32_t c = AsTab(d, n, AS_BREC);
    size_t pool = 8 + (size_t)c * AS_BREC;
    for (uint32_t j = 0; j < c; ++j) {
        const uint8_t* b = d + 8 + (size_t)j * AS_BREC; uint32_t no = AsU32(b + 0x9C);
        if (pool + no >= n) break;
        char nm[64], u[600]; size_t l = strnlen((const char*)d + pool + no, n - pool - no);
        char raw[512]; snprintf(raw, sizeof raw, "%.*s", (int)(l < 511 ? l : 511), (const char*)d + pool + no);
        AsUpperCopy(u, raw, sizeof u); AsName(nm, (const uint8_t*)raw, sizeof raw);
        AsPt v; memset(&v, 0, sizeof v); v.id = AsU32(b);
        for (int k = 0; k < 3; ++k) { v.pos[k] = AsR0(AsF32(b + 0x10 + 4 * k)); v.ext[k] = AsR0(AsF32(b + 0x28 + 4 * k)); }
        v.yaw = AsR1(AsF32(b + 0x20)); snprintf(v.name, sizeof v.name, "%s", nm);
        if (AsSetHas(trig, ntrig, (int64_t)v.id)) { m->trig = (AsPt*)AsPush(m->trig, &m->ntrig, sizeof(AsPt)); m->trig[m->ntrig - 1] = v; }
        static const char* const ab[] = {"AREA", "BOX", "BARRIER", 0}, *const bj[] = {"BGM", "JUKE", "ITEM", 0};
        if (strstr(u, "BOSS") && AsHasAny(u, ab) && !AsHasAny(u, bj)) { m->box = (AsPt*)AsPush(m->box, &m->nbox, sizeof(AsPt)); m->box[m->nbox - 1] = v; }
    }
    free(d); free(trig);
}
static void AsMisFree(AsMis* m) { free(m->en); free(m->st); free(m->box); free(m->trig); }

// ---------------------------------------------------------------- picks (rematch_catalog.pick_fight_start / pick_arena, auto_arena.field_point)
static double AsDist(const double* a, const double* b) { double x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return sqrt(x * x + y * y + z * z) / 100.0; }
typedef struct { const AsPt* s; int kind; int conf; double dist; uint32_t box; } AsStart;   // kind 0 mission start, 1 retry, 2 shrine, 3 spawn, 4 gadget; conf 2 high 1 medium 0 low
static const double* g_as_bp;
static int AsCmpStart(const void* a, const void* b) {
    const AsPt* x = *(const AsPt* const*)a, *y = *(const AsPt* const*)b;
    int fx = fabs(x->pos[1] - g_as_bp[1]) > 500, fy = fabs(y->pos[1] - g_as_bp[1]) > 500; if (fx != fy) return fx - fy;
    double dx = AsDist(x->pos, g_as_bp), dy = AsDist(y->pos, g_as_bp); if (dx != dy) return dx < dy ? -1 : 1;
    return x->id < y->id ? -1 : x->id > y->id;
}
static int AsPickStart(const AsMis* m, const AsPt* boss, AsStart* o) {
    if (!m->nst) return 0;
    const double* bp = boss->pos; memset(o, 0, sizeof *o);
    if (m->nst == 1) { o->s = &m->st[0]; o->kind = 0; o->dist = AsDist(o->s->pos, bp); o->conf = o->dist <= 30 ? 2 : o->dist <= 80 ? 1 : 0; return 1; }
    const AsPt** ok = (const AsPt**)malloc(m->nst * sizeof(AsPt*)); int nok = 0;
    for (int i = 0; i < m->nst; ++i) if (!(m->st[i].flags & 0x200)) ok[nok++] = &m->st[i];
    g_as_bp = bp; qsort(ok, nok, sizeof(AsPt*), AsCmpStart);
    int found = 0;
    for (int i = 0; i < nok && !found; ++i) { double d = AsDist(ok[i]->pos, bp); if ((ok[i]->flags & 0x40) && d <= 40) { o->s = ok[i]; o->kind = 1; o->conf = 2; o->dist = d; found = 1; } }
    for (int i = 0; i < nok && !found; ++i) {
        double d = AsDist(ok[i]->pos, bp); uint32_t k = ok[i]->sub;
        if (d >= 8 && (((k == 0 || k == 908) && d <= 40) || ((k == 906 || k == 907 || k == 909) && d <= 60))) { o->s = ok[i]; o->kind = (k == 906 || k == 907 || k == 909) ? 2 : k == 0 ? 3 : 4; o->conf = 1; o->dist = d; found = 1; }
    }
    for (int i = 0; i < nok && !found; ++i) {
        double d = AsDist(ok[i]->pos, bp); uint32_t k = ok[i]->sub;
        if (d <= 120) { o->s = ok[i]; o->kind = (k == 906 || k == 907 || k == 909) ? 2 : k == 0 ? 3 : 4; o->conf = 0; o->dist = d; found = 1; }
    }
    free(ok); return found;
}
static int AsCmpBox(const AsPt* a, const AsPt* b, const double* bp) { double x = AsDist(a->pos, bp), y = AsDist(b->pos, bp); if (x != y) return x < y ? -1 : 1; return a->id < b->id ? -1 : a->id > b->id; }
static int AsPickArena(const AsMis* m, const AsPt* boss, const AsStart* f, double* out /* x y z yaw */) {
    const double* bp = boss->pos; const AsPt* best = 0; char u[80];
    for (int i = 0; i < m->ntrig; ++i) {
        AsUpperCopy(u, m->trig[i].name, sizeof u);
        if (strstr(u, "BOSS") && !AsHasAny(u, AS_BAD_TRIG) && AsDist(m->trig[i].pos, bp) <= 40 && (!best || AsCmpBox(&m->trig[i], best, bp) < 0)) best = &m->trig[i];
    }
    if (!best) for (int i = 0; i < m->nbox; ++i) {
        AsUpperCopy(u, m->box[i].name, sizeof u);
        if (AsDist(m->box[i].pos, bp) <= 40 && !AsHasAny(u, AS_BAD_TRIG) && (!best || AsCmpBox(&m->box[i], best, bp) < 0)) best = &m->box[i];
    }
    if (!best) return 0;
    const double* c = best->pos, *ext = best->ext, *sp = f->s->pos; double a = best->yaw * 3.14159265358979323846 / 180.0;
    double dx = sp[0] - c[0], dz = sp[2] - c[2];
    double lx = dx * cos(a) - dz * sin(a), lz = dx * sin(a) + dz * cos(a), hx = ext[0] / 2, hz = ext[2] / 2;
    double t = fmin(fmin(lx != 0 ? hx / fabs(lx) : 1e9, lz != 0 ? hz / fabs(lz) : 1e9), 1.0);
    double wx = c[0] + dx * t * 0.6, wz = c[2] + dz * t * 0.6;
    double hs = hypot(sp[0] - wx, sp[2] - wz), hb = hypot(bp[0] - wx, bp[2] - wz);
    double wy = (sp[1] * hb + bp[1] * hs) / (hs + hb != 0 ? hs + hb : 1) + 100;
    if (fabs(sp[1] - bp[1]) > 500) wy = bp[1] + 100;
    double lo = c[1] - ext[1] / 2, hi = c[1] + ext[1] / 2;
    if (!(lo <= wy && wy <= hi) && lo <= bp[1] + 100 && bp[1] + 100 <= hi) wy = bp[1] + 100;
    out[0] = wx; out[1] = wy; out[2] = wz; out[3] = atan2(bp[0] - wx, bp[2] - wz) * 180.0 / 3.14159265358979323846;
    return 1;
}
static uint32_t AsNearestStart(const AsMis* m, const double* pos) {
    const AsPt* best = 0; int anyOk = 0;
    for (int i = 0; i < m->nst; ++i) if (!(m->st[i].flags & 0x200)) anyOk = 1;
    for (int i = 0; i < m->nst; ++i) {
        if (anyOk && (m->st[i].flags & 0x200)) continue;
        if (!best) { best = &m->st[i]; continue; }
        double a = AsDist(m->st[i].pos, pos), b = AsDist(best->pos, pos);
        if (a < b || (a == b && m->st[i].id < best->id)) best = &m->st[i];
    }
    return best ? best->id : 0;
}

// ---------------------------------------------------------------- rows (auto_arena.build_rows without the committed-table carry-over)
typedef struct { uint32_t mission, start, chr, placement; uint8_t world; double p[4], b[3]; char kind[8], id[16]; } AsRow;
typedef void (*AsRowFn)(const AsRow* r, void* ctx);
static int AsValid(const AsPt* e) { return e->chr > 0 && e->chr < 0xFFFFFFFFu && e->pid > 0 && e->pid < 0xFFFFFFFFu && isfinite(e->pos[0]) && isfinite(e->pos[1]) && isfinite(e->pos[2]) && (e->pos[0] != 0 || e->pos[1] != 0 || e->pos[2] != 0); }
static int AsHasBossWord(const char* nm) { char u[160]; AsUpperCopy(u, nm, sizeof u); return strstr(u, "BOSS") != 0; }
static int AsIsBoss(const AsPt* e) { return AsValid(e) && ((e->fl & 4) || AsHasBossWord(e->name)); }
static const AsPt* AsOpponent(const AsMis* m) {
    const AsPt* one = 0; int nv = 0, nz = 0; const AsPt* z = 0;
    for (int i = 0; i < m->nen; ++i) {
        if (!AsValid(&m->en[i])) continue;
        ++nv; one = &m->en[i]; char u[160]; AsUpperCopy(u, m->en[i].name, sizeof u);
        if (!strstr(u, "ZAKO")) { ++nz; if (!z) z = &m->en[i]; }
    }
    if (nv == 1) return one;
    return nz == 1 && nv > 1 ? z : 0;
}
typedef struct { uint32_t* chr; int n; } AsChrSet;
static int AsChrHas(const AsChrSet* s, uint32_t c) { for (int i = 0; i < s->n; ++i) if (s->chr[i] == c) return 1; return 0; }
static void AsChrAdd(AsChrSet* s, uint32_t c) { if (!AsChrHas(s, c)) { s->chr = (uint32_t*)realloc(s->chr, (s->n + 1) * 4); s->chr[s->n++] = c; } }
static void AsEmit(const AsMis* m, const AsPt* e, const char* id, AsChrSet* have, AsRowFn cb, void* ctx) {
    AsRow r; memset(&r, 0, sizeof r); AsStart f; double ar[4];
    const AsPt* bp = e; for (int k = 0; k < m->nen; ++k) if (m->en[k].pid == e->pid) bp = &m->en[k];   // the generator's {pid: record} map keeps the last record of a pid
    int hasF = AsPickStart(m, bp, &f);
    if (hasF && f.kind != 0 && AsPickArena(m, bp, &f, ar)) hasF = 2;
    r.mission = m->mid; r.world = m->world; r.chr = e->chr; r.placement = e->pid; memcpy(r.b, e->pos, sizeof r.b); snprintf(r.id, sizeof r.id, "%s", id);
    if (m->kind == 2 || !hasF) {   // open-field region / no start point: 8 m in front of the boss along its facing, facing it
        double a = e->yaw * 3.14159265358979323846 / 180.0, x = e->pos[0] + 800 * sin(a), z = e->pos[2] + 800 * cos(a);
        r.p[0] = x; r.p[1] = e->pos[1] + 100; r.p[2] = z; r.p[3] = atan2(e->pos[0] - x, e->pos[2] - z) * 180.0 / 3.14159265358979323846;
        snprintf(r.kind, sizeof r.kind, "field"); r.start = AsNearestStart(m, e->pos);
    } else if (hasF == 2) {
        memcpy(r.p, ar, sizeof r.p); snprintf(r.kind, sizeof r.kind, "arena"); r.start = f.s->id;
    } else {
        memcpy(r.p, f.s->pos, 3 * sizeof(double)); r.p[3] = f.s->yaw; snprintf(r.kind, sizeof r.kind, "start"); r.start = f.s->id;
    }
    AsChrAdd(have, e->chr); cb(&r, ctx);
}
// gameDir = the folder holding 'archive'. Calls cb once per row in the generator's order. Returns the row count, -1 = no data.
static int AsScan(const char* gameDir, AsRowFn cb, void* ctx) {
#ifndef AS_TEST_NO_KNOWN   // test hook: force the structure search
    static const char* const KNOWN_IDTAB = "/AE/50B70535D0EDD19523951019C533295915AE7D4F65FE37E351E09D3858A8C5";
    static const char* const KNOWN_MIS = "/52/BD29D45533CFACBFA2D6EC80A3B9F40E07CBF9183727358B0B3C3ABE3E4FC2";
#else
    static const char* const KNOWN_IDTAB = "/00/X", *const KNOWN_MIS = "/00/X";
#endif
    AsLfm L; if (!AsLfmOpen(&L, gameDir)) { AS_LOG("ARENASCAN: no LFM archives under %s", gameDir); AsLfmFree(&L); return -1; }
    size_t n; uint8_t* d = AsGet(&L, KNOWN_IDTAB, &n); AsIdTab E; int ok = AsParseIdTab(d, n, &E); free(d);
    for (int i = 0; !ok && i < L.n; ++i) {   // by structure: u32 0, u32 count, 8-byte {id, rel offset} rows -> 'XX/<62 hex>' strings
        size_t hn; uint8_t* h = AsGetEnt(&L, &L.e[i], 16, &hn);
        int cand = h && hn >= 8 && AsU32(h) == 0 && AsU32(h + 4) >= 100; free(h);
        if (!cand) continue;
        d = AsGetEnt(&L, &L.e[i], 0, &n); ok = AsParseIdTab(d, n, &E); free(d);
        if (ok) AS_LOG("ARENASCAN: id table found by structure at %s (known path failed)", L.e[i].path);
    }
    if (!ok) { AS_LOG("ARENASCAN: no LFM id table"); AsLfmFree(&L); return -1; }
    uint8_t* md = AsGet(&L, KNOWN_MIS, &n); size_t mn = n;
    if (!AsMissionsOk(md, mn, &E)) {
        free(md); md = 0;
        for (int i = 0; i < L.n && !md; ++i) {
            if ((L.e[i].arc >= 0 && (L.e[i].usize < 8 || (L.e[i].usize - 8) % AS_MREC >= 64))) continue;
            size_t hn; uint8_t* h = AsGetEnt(&L, &L.e[i], 16, &hn); int cand = h && hn >= 8 && AsU32(h) == AS_MAGIC; free(h);
            if (!cand) continue;
            uint8_t* c = AsGetEnt(&L, &L.e[i], 0, &n);
            if (AsMissionsOk(c, n, &E)) { md = c; mn = n; AS_LOG("ARENASCAN: mission table found by structure at %s", L.e[i].path); } else free(c);
        }
    }
    if (!md) { AS_LOG("ARENASCAN: no mission table"); free(E.v); AsLfmFree(&L); return -1; }
    uint32_t nm = AsTab(md, mn, AS_MREC);
    AsMis* ms = (AsMis*)calloc(nm, sizeof(AsMis));
    for (uint32_t i = 0; i < nm; ++i) AsMission(&L, &E, md + 8 + (size_t)i * AS_MREC, &ms[i]);
    AsChrSet have = {0, 0}; int rows = 0;
    struct { AsRowFn cb; void* ctx; int* n; } cnt = {cb, ctx, &rows};
    (void)cnt;
#define AS_USABLE(m) ((m)->shipped && (m)->world && (m)->nen)
    for (uint32_t i = 0; i < nm; ++i) {   // 1. one row per startable (non-region) mission with a boss record (or its only opponent)
        AsMis* m = &ms[i]; if (!AS_USABLE(m) || m->kind == 2) continue;
        const AsPt* e = 0; char id[16];
        for (int k = 0; k < m->nen && !e; ++k) if (AsIsBoss(&m->en[k]) && (m->en[k].fl & 4) && AsHasBossWord(m->en[k].name)) e = &m->en[k];
        for (int k = 0; k < m->nen && !e; ++k) if (AsIsBoss(&m->en[k]) && (m->en[k].fl & 4)) e = &m->en[k];
        for (int k = 0; k < m->nen && !e; ++k) if (AsIsBoss(&m->en[k])) e = &m->en[k];
        if (!e) e = AsOpponent(m);
        if (!e) continue;
        snprintf(id, sizeof id, "%c%04X", (m->flags & 0x41) ? 'D' : 'R', m->mid);
        AsEmit(m, e, id, &have, cb, ctx); ++rows;
    }
    for (int pass = 0; pass < 2; ++pass)   // 2. every further boss character: missions, then open-field regions (id S<chr>)
        for (uint32_t i = 0; i < nm; ++i) {
            AsMis* m = &ms[i]; if (!AS_USABLE(m) || (m->kind == 2) != (pass == 1)) continue;
            for (int k = 0; k < m->nen; ++k) {
                const AsPt* e = &m->en[k]; char id[16];
                if (!AsIsBoss(e) || !(e->fl & 4) || AsChrHas(&have, e->chr)) continue;
                snprintf(id, sizeof id, "S%u", e->chr); AsEmit(m, e, id, &have, cb, ctx); ++rows;
            }
        }
#undef AS_USABLE
    AS_LOG("ARENASCAN: %u missions, %d rows derived from the game data", nm, rows);
    for (uint32_t i = 0; i < nm; ++i) AsMisFree(&ms[i]);
    free(ms); free(have.chr); free(md); free(E.v); AsLfmFree(&L);
    return rows;
}
#endif
