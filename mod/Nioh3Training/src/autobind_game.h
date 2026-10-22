// pc2 #23565: automatic boss action-file binding, game side (logic: autobind.h). Log + query only, nothing is written to the game.
// For every Training boss slot, ~2.5 s after the boss appears: walk its action set (ctrl = [[actor+0x3B0]+8], set = [ctrl+0x178], groups = [set+idx*8],
// group array [group+0x190] / count u16 [group+0x198], entry = s16 key, u16 mapped action id, notes/boss-action-table-re.md), collect the mapped action ids and match them
// against Nioh3Training_actionsets.tsv. Only struct offsets are used, no function RVAs, so a game update changes at most these offsets (the entry stride is
// detected from the data: the stride whose +2 words are mostly ids that exist in some action file). Fallback / refinement: ids the boss is seen performing
// ([ctrl+0xE8] -> id) are added each tick, so the binding also works if the walk finds nothing. [Debug] AutoBind=0 turns it off.
#pragma once
#include "autobind.h"
static AbTable g_ab; static int g_ab_state;  // 0 = not loaded, 1 = loaded, -1 = failed / off
static uint8_t g_ab_known[8192];             // union of every file's ids (bit per id)
static int g_cfg_ab = 1;
static struct { uint64_t a; DWORD t0, tNext; int done, tries, nobs, file, pct; uint16_t obs[1024]; uint8_t seen[8192]; } g_abs[N3T_SLOTS];
static __attribute__((noinline)) int AbRd(uint64_t a, void* out, int n) { __try { memcpy(out, (const void*)a, (size_t)n); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static void AbAdd(int s, uint16_t id) { if (id == 0xFFFF || g_abs[s].nobs >= 1024 || (g_abs[s].seen[id >> 3] >> (id & 7) & 1)) return; g_abs[s].seen[id >> 3] |= (uint8_t)(1 << (id & 7)); g_abs[s].obs[g_abs[s].nobs++] = id; }
static void AbLoadTable(void) {
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\Nioh3Training_actionsets.tsv", g_dir);
    FILE* f = fopen(path, "rb"); if (!f) { g_ab_state = -1; Log("AUTOBIND: %s not found, off", path); return; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1); if (!buf) { fclose(f); g_ab_state = -1; return; }
    n = (long)fread(buf, 1, (size_t)n, f); fclose(f); buf[n] = 0;
    const int nf = AbLoad(&g_ab, buf); free(buf);
    for (int i = 0; i < g_ab.n; ++i) for (int k = 0; k < g_ab.f[i].n; ++k) g_ab_known[g_ab.f[i].ids[k] >> 3] |= (uint8_t)(1 << (g_ab.f[i].ids[k] & 7));
    g_ab_state = nf > 0 ? 1 : -1; Log("AUTOBIND: %d action files loaded from %s", nf, path);
}
// Walk the action set; returns the number of ids added. diag = log the layout (first attempt per boss).
static int AbWalk(int s, uint64_t set, int diag) {
    static const int strides[] = {4, 8, 12, 16, 20, 24, 32, 40, 48, 64};
    int added = 0;
    for (int idx = 0; idx < 0x30; ++idx) {
        uint64_t g = 0, arr = 0; uint16_t cnt = 0;
        if (!AbRd(set + (uint64_t)idx * 8, &g, 8) || g < 0x10000 || (g & 7)) continue;
        if (!AbRd(g + 0x190, &arr, 8) || !AbRd(g + 0x198, &cnt, 2) || arr < 0x10000 || cnt == 0 || cnt > 6000) continue;
        int bestS = 0, bestFrac = 0; const int m = cnt < 64 ? cnt : 64;
        for (unsigned k = 0; k < sizeof strides / sizeof *strides; ++k) {
            uint8_t blk[64 * 64]; const int S = strides[k]; if (m * S > (int)sizeof blk || !AbRd(arr, blk, m * S)) continue;
            int good = 0; for (int i = 0; i < m; ++i) { const uint16_t id = *(uint16_t*)(blk + i * S + 2); if (id == 0xFFFF || (g_ab_known[id >> 3] >> (id & 7) & 1)) ++good; }
            const int frac = good * 100 / m; if (frac > bestFrac) { bestFrac = frac; bestS = S; }
        }
        if (diag) {
            uint8_t raw[48] = {0}; AbRd(arr, raw, 48);
            Log("AUTOBIND slot %c group %d @%p cnt %u stride %d known %d%% first %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", 'A' + s, idx, (void*)g, cnt, bestS, bestFrac,
                raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9], raw[10], raw[11], raw[12], raw[13], raw[14], raw[15]);
        }
        if (bestS && bestFrac >= 80) for (int i = 0; i < cnt; ++i) { uint16_t id = 0; if (!AbRd(arr + (uint64_t)i * bestS + 2, &id, 2)) break; const int n0 = g_abs[s].nobs; AbAdd(s, id); added += g_abs[s].nobs - n0; }
    }
    return added;
}
static void AbTick(void) {
    if (!g_cfg_ab || g_ab_state < 0) return;
    if (!g_ab_state) { AbLoadTable(); if (g_ab_state < 0) return; }
    const DWORD now = GetTickCount();
    for (int s = 0; s < N3T_SLOTS; ++s) {
        const uint64_t a = BvActor(s);
        if (a != g_abs[s].a) { memset(&g_abs[s], 0, sizeof g_abs[s]); g_abs[s].a = a; g_abs[s].t0 = now; g_abs[s].file = -1; if (a) Log("AUTOBIND slot %c: boss chr %u actor %p seen", 'A' + s, g_slot_chr[s], (void*)a); }
        if (!a || g_abs[s].done) continue;
        uint64_t ai = 0, ctrl = 0, set = 0;
        if (!Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) continue;
        const uint32_t cur = ActIdOf(ctrl); if (cur < 0xFFFF) AbAdd(s, (uint16_t)cur);
        if (now - g_abs[s].t0 < 2500 || now < g_abs[s].tNext) continue;
        g_abs[s].tNext = now + 3000; const int first = g_abs[s].tries++ == 0;
        if (Rd64(ctrl + 0x178, &set) && set) AbWalk(s, set, first);
        int p = 0, p2 = 0; const int m = AbMatch(&g_ab, g_abs[s].obs, g_abs[s].nobs, &p, &p2);
        g_abs[s].file = m; g_abs[s].pct = p;
        if (m >= 0) Log("AUTOBIND slot %c chr %u: file %.12s... char %s (%d rows) match %d%% (runner-up %d%%) from %d ids, try %d", 'A' + s, g_slot_chr[s], g_ab.f[m].file, g_ab.f[m].chr, g_ab.f[m].rows, p, p2, g_abs[s].nobs, g_abs[s].tries);
        else Log("AUTOBIND slot %c chr %u: UNBOUND best %d%% from %d ids, try %d", 'A' + s, g_slot_chr[s], p, g_abs[s].nobs, g_abs[s].tries);
        if ((m >= 0 && p >= 90) || g_abs[s].tries >= 12) { g_abs[s].done = 1; Log("AUTOBIND slot %c chr %u: FINAL %s", 'A' + s, g_slot_chr[s], m >= 0 ? g_ab.f[m].chr : "UNBOUND"); }
    }
}
