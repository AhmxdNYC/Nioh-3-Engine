// pc2 #23565: automatic boss action-file binding, game side (logic: autobind.h). Log + query only, nothing is written to the game.
// Proven in game (pc2, 2026-10-22): the boss's current action object P = [ctrl+0xE8] (ctrl = [[actor+0x3B0]+8]) has [P+0x20] = the action's 336-byte ROW inside
// the loaded archive_01 action file (u16 id at +0xC, u32 action object hash at +0xF0). Rows of a file are contiguous, so walking 0x150-byte steps from that row in
// both directions (rows whose hash is a hash of some action file) yields the file's hash set, matched against Nioh3Training_actionsets.tsv. Each new row pointer
// outside the scanned blocks is another block (a boss uses its own file plus shared ones); the biggest bound block is the boss's main file.
// Only struct offsets, no function RVAs, no names: a game update changes at most these offsets. [Debug] AutoBind=0 turns it off.
#pragma once
#include "autobind.h"
static AbTable g_ab; static int g_ab_state;  // 0 = not loaded, 1 = loaded, -1 = failed / off
static uint32_t* g_ab_hash; static int g_ab_nhash;  // sorted union of every file's hashes
static int g_cfg_ab = 1;
enum { AB_UNI = 8192, AB_BLK = 12, AB_STABLE_MS = 6000, AB_MIN_MS = 8000, AB_MAX_MS = 30000 };
static struct { uint64_t a; uint32_t chr; DWORD t0, tNext, tLast; int done, composedHuman, nblk; struct { uint64_t lo, hi; int rows, file, pct, cfile, cpct; uint32_t tiny[AB_CONTAIN_MIN]; } blk[AB_BLK]; int nuni; uint32_t uni[AB_UNI]; } g_abs[MAXSP];
static __attribute__((noinline)) int AbRd(uint64_t a, void* out, int n) { __try { memcpy(out, (const void*)a, (size_t)n); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static int AbKnown(uint32_t h) { int lo = 0, hi = g_ab_nhash - 1; while (lo <= hi) { const int m = (lo + hi) >> 1; if (g_ab_hash[m] == h) return 1; if (g_ab_hash[m] < h) lo = m + 1; else hi = m - 1; } return 0; }
static void AbLoadTable(void) {
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\Nioh3Training_actionsets.tsv", g_dir);
    FILE* f = fopen(path, "rb"); if (!f) { g_ab_state = -1; Log("AUTOBIND: %s not found, off", path); return; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1); if (!buf) { fclose(f); g_ab_state = -1; return; }
    n = (long)fread(buf, 1, (size_t)n, f); fclose(f); buf[n] = 0;
    const int nf = AbLoad(&g_ab, buf); free(buf);
    int tot = 0; for (int i = 0; i < g_ab.n; ++i) tot += g_ab.f[i].n;
    g_ab_hash = (uint32_t*)malloc((size_t)(tot + 1) * sizeof(uint32_t)); g_ab_nhash = 0;
    for (int i = 0; i < g_ab.n; ++i) for (int k = 0; k < g_ab.f[i].n; ++k) g_ab_hash[g_ab_nhash++] = g_ab.f[i].ids[k];
    qsort(g_ab_hash, (size_t)g_ab_nhash, sizeof(uint32_t), AbCmp32);
    int u = 0; for (int i = 0; i < g_ab_nhash; ++i) if (!u || g_ab_hash[u - 1] != g_ab_hash[i]) g_ab_hash[u++] = g_ab_hash[i];
    g_ab_nhash = u; g_ab_state = nf > 0 ? 1 : -1; Log("AUTOBIND: %d action files, %d distinct hashes loaded from %s", nf, u, path);
}
// Scan the contiguous row block around row pointer r0. Row valid = readable and its +0xF0 hash is a known action-file hash; up to 6 invalid rows are bridged.
static int AbScan(uint64_t r0, uint32_t* out, int cap, uint64_t* lo, uint64_t* hi) {
    int n = 0; *lo = *hi = r0;
    for (int dir = -1; dir <= 1; dir += 2) {
        int bad = 0;
        for (int k = (dir < 0 ? 1 : 0); k < 2500 && bad < 6; ++k) {
            const uint64_t r = r0 + (uint64_t)((int64_t)dir * k * 0x150); uint32_t h = 0;
            if (!AbRd(r + 0xF0, &h, 4)) break;
            if (AbOk(h) && AbKnown(h)) { bad = 0; if (n < cap) out[n++] = h; if (dir < 0 && r < *lo) *lo = r; if (dir > 0 && r > *hi) *hi = r; } else ++bad;
        }
    }
    qsort(out, (size_t)n, sizeof(uint32_t), AbCmp32);   // distinct hashes only (duplicates would push the Jaccard over 100 %)
    { int u = 0; for (int i = 0; i < n; ++i) if (!u || out[u - 1] != out[i]) out[u++] = out[i]; n = u; }
    return n;
}
// Bind = the largest block, if it matches a file (> 70 %, runner-up 15+ lower, not a shared CP_HUMAN_* file; the boss's own file is bigger than the shared side files). Re-scanned every 250 ms until no new
// block appeared for AB_STABLE_MS (after >= AB_MIN_MS), at most AB_MAX_MS; a boss that despawns / is replaced earlier is finalised with what was seen so far.
static void AbFinal(int s, const char* why) {
    if (g_abs[s].done) return;
    g_abs[s].done = 1;
    int best = -1; for (int k = 0; k < g_abs[s].nblk; ++k) if (best < 0 || g_abs[s].blk[k].rows > g_abs[s].blk[best].rows) best = k;
    if (best >= 0 && g_abs[s].blk[best].file < 0) best = -1;
    if (best < 0 && g_abs[s].nblk > 1 && g_abs[s].nuni) {   // pc2 #24074: biggest block unbound -> the boss file may be spread over several blocks: match the union by coverage
        uint32_t* u = g_abs[s].uni; int n = g_abs[s].nuni;
        qsort(u, (size_t)n, sizeof(uint32_t), AbCmp32); int d = 0; for (int i = 0; i < n; ++i) if (!d || u[d - 1] != u[i]) u[d++] = u[i];
        int cov = 0, i2 = 0; const int m = AbMatchUnion(&g_ab, u, d, &cov, &i2);
        if (m >= 0) { Log("AUTOBIND spawn #%d chr %u: FINAL %s (union of %d blocks, %d ids, %d%% of the file covered, runner-up %d ids) file %s [%s]", s, g_abs[s].chr, g_ab.f[m].chr, g_abs[s].nblk, d, cov, i2, g_ab.f[m].file, why); return; }
        Log("AUTOBIND spawn #%d chr %u: union of %d blocks (%d ids) matches no file (best coverage %d%%, runner-up %d ids)", s, g_abs[s].chr, g_abs[s].nblk, d, cov, i2);
    }   // the biggest block is the boss's own file: if it is unmatched / ambiguous / shared, stay unbound (a small bound side block is never the boss file)
    if (best < 0 && g_abs[s].nblk >= 1) {   // pc2 #24074: no own file: explain the blocks as fragments of known files (human-style boss: moves come from shared weapon files). #24140: also a lone block
        int expl = 0, rows = 0, top = -1, common = 0, weapon = 0; char names[400] = "";
        for (int k = 0; k < g_abs[s].nblk; ++k) {   // pc2 #24140: a block too small for containment (< AB_CONTAIN_MIN ids: 0-1 row weapon slices) is explained when its ids lie in a file another block was assigned to
            if (g_abs[s].blk[k].cfile >= 0 || g_abs[s].blk[k].rows >= AB_CONTAIN_MIN) continue;
            for (int j = 0; j < g_abs[s].nblk && g_abs[s].blk[k].cfile < 0; ++j) {
                const int c = g_abs[s].blk[j].cfile; if (c < 0) continue;
                int in = 1; for (int i = 0; i < g_abs[s].blk[k].rows; ++i) if (!AbHas(&g_ab.f[c], g_abs[s].blk[k].tiny[i])) in = 0;
                if (in) g_abs[s].blk[k].cfile = c;
            }
        }
        for (int k = 0; k < g_abs[s].nblk; ++k) {
            rows += g_abs[s].blk[k].rows; const int c = g_abs[s].blk[k].cfile; if (c < 0) continue;
            const int family=AbHumanFamily(g_ab.f[c].chr); common |= family==1; weapon |= family==2;
            expl += g_abs[s].blk[k].rows; if (top < 0 || g_abs[s].blk[k].rows > g_abs[s].blk[top].rows) top = k;
            char one[64]; snprintf(one, sizeof one, "%s%s(%d)", names[0] ? " + " : "", g_ab.f[c].chr, g_abs[s].blk[k].rows);
            if (strlen(names) + strlen(one) < sizeof names) strcat(names, one);
        }
        if (top>=0 && rows>0 && expl * 100 >= rows * 80) { g_abs[s].composedHuman=AbComposedHuman(0,common,weapon,expl,rows); Log("AUTOBIND spawn #%d chr %u: FINAL COMPOSED of known files (%d of %d rows explained, biggest block -> %s): %s [%s]", s, g_abs[s].chr, expl, rows, g_ab.f[g_abs[s].blk[top].cfile].chr, names, why); return; }
    }
    if (best >= 0) Log("AUTOBIND spawn #%d chr %u: FINAL %s (%d%%, %d rows) file %s; %d block(s) seen [%s]", s, g_abs[s].chr, g_ab.f[g_abs[s].blk[best].file].chr, g_abs[s].blk[best].pct, g_abs[s].blk[best].rows, g_ab.f[g_abs[s].blk[best].file].file, g_abs[s].nblk, why);
    else Log("AUTOBIND spawn #%d chr %u: FINAL UNBOUND (%d block(s) seen) [%s]", s, g_abs[s].chr, g_abs[s].nblk, why);
}
static void AbTick(void) {
    if (!g_cfg_ab || g_ab_state < 0) return;
    if (!g_ab_state) { AbLoadTable(); if (g_ab_state < 0) return; }
    const DWORD now = GetTickCount();
    for (int s = 0; s < MAXSP; ++s) {
        const uint64_t a = s < g_nsp ? g_sp_actor[s] : 0;
        if (a != g_abs[s].a || (a && g_sp_chr[s] != g_abs[s].chr)) {
            if (g_abs[s].a && g_abs[s].nblk) AbFinal(s, "despawned/replaced");
            memset(&g_abs[s], 0, sizeof g_abs[s]); g_abs[s].a = a; g_abs[s].t0 = g_abs[s].tLast = now; g_abs[s].chr = a ? g_sp_chr[s] : 0;
            if (a) Log("AUTOBIND spawn #%d: boss chr %u actor %p seen", s, g_sp_chr[s], (void*)a);
        }
        if (!a || g_abs[s].done || now < g_abs[s].tNext) continue;
        g_abs[s].tNext = now + 250;
        uint64_t ai = 0, ctrl = 0, p = 0, row = 0;
        if (Rd64(a + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl && Rd64(ctrl + 0xE8, &p) && p && Rd64(p + 0x20, &row) && row >= 0x10000) {
            int inb = 0; for (int k = 0; k < g_abs[s].nblk; ++k) if (row >= g_abs[s].blk[k].lo && row <= g_abs[s].blk[k].hi + 0x150) inb = 1;
            if (!inb && g_abs[s].nblk < AB_BLK) {
                static uint32_t hs[6000]; uint64_t lo, hi; const int n = AbScan(row, hs, 6000, &lo, &hi);
                int p1 = 0, p2 = 0; int m = AbMatch(&g_ab, hs, n, &p1, &p2); const int shared = m >= 0 && AbIsShared(&g_ab.f[m]); if (shared) m = -1;  // shared file -> unbound, per-actor action set stays in use
                const int k = g_abs[s].nblk++; g_abs[s].tLast = now;
                if (!shared) for (int i = 0; i < n && g_abs[s].nuni < AB_UNI; ++i) g_abs[s].uni[g_abs[s].nuni++] = hs[i];
                g_abs[s].blk[k].lo = lo; g_abs[s].blk[k].hi = hi; g_abs[s].blk[k].rows = n; g_abs[s].blk[k].file = m; g_abs[s].blk[k].pct = p1; g_abs[s].blk[k].cfile = AbBlockFile(&g_ab, hs, n, &g_abs[s].blk[k].cpct);
                for (int i = 0; i < n && i < AB_CONTAIN_MIN; ++i) g_abs[s].blk[k].tiny[i] = hs[i];
                uint16_t id = 0; AbRd(row + 0xC, &id, 2);
                Log("AUTOBIND spawn #%d chr %u: block %d at %p (row of action 0x%X): %d rows -> %s match %d%% (runner-up %d%%)%s%s", s, g_abs[s].chr, k, (void*)row, id, n,
                    m >= 0 ? g_ab.f[m].chr : "UNBOUND", p1, p2, m >= 0 ? " file " : "", m >= 0 ? g_ab.f[m].file : "");
            }
        }
        const DWORD age = now - g_abs[s].t0;
        if (age > AB_MAX_MS) AbFinal(s, "timeout");
        else if (age > AB_MIN_MS && g_abs[s].nblk && now - g_abs[s].tLast > AB_STABLE_MS) AbFinal(s, "stable");
    }
}

// Called only by the game-thread meter feed. Spawn index and Training slot are different namespaces.
static int FmHumanBoss(uint64_t actor,uint32_t chr) {
    if(!g_cfg_ab || g_ab_state!=1 || !actor) return 0;
    for(int i=0;i<MAXSP;++i) if(g_abs[i].a==actor && g_abs[i].chr==chr)
        return g_abs[i].done && g_abs[i].composedHuman;
    return 0;
}
