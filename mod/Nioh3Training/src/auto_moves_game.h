// pc2 #23907: runtime boss move list, game side (logic: auto_moves.h). Read only, nothing is written to the game. [Debug] AutoMoves (default 1).
// The boss's current action object P = [[[actor+0x3B0]+8]+0xE8] (autobind_game.h) leads to its file's loaded action data (AmLoadImg below); the
// rows and hitbox rows are copied, rebuilt into a file image (AmBuildImage) and parsed (AmParse). The loaded data is NOT one contiguous file image
// (checked live: no file header in the 1 MB below the rows, the sub-tables are separate allocations, sub-table 0 is parsed away). Each new
// descriptor is another file (a boss uses its own file plus shared ones); the biggest one is the boss's own move list. Only struct offsets, no
// RVAs, no names, no hand tables. The proof line compares the result with the old hand headers (auto_moves_cmp.h).
#pragma once
#include "auto_moves.h"
#include "auto_moves_cmp.h"
#include "action_records_game.h"
static int g_cfg_am = 1;
enum { AM_FILES = 48, AM_SPF = 8, AM_CHRS = 64 };
typedef struct { uint64_t base; int n; AmTable* t; } AmImg;
static AmImg g_ami[AM_FILES]; static int g_ami_n;
static struct { uint32_t chr; int img; } g_amc[AM_CHRS]; static volatile LONG g_amc_n;  // chr -> its own file (final)
static struct { uint64_t a; uint32_t chr; DWORD t0, tNext, tLast; int done, nf, miss; int f[AM_SPF]; } g_ams[MAXSP];
static void N3CatMergeAuto(uint32_t chr, const AmTable* t);  // move_catalog.h: the runtime list becomes the boss's move catalogue
static __attribute__((noinline)) int AmRd(uint64_t a, void* out, uint32_t n) { __try { memcpy(out, (const void*)a, n); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static int AmImgOf(uint64_t x) { for (int i = 0; i < g_ami_n; ++i) if (g_ami[i].base == x) return i; return -1; }
// Loaded action data of one file, from the current action object P (proven live on Yoritomo 2026-10-22: every byte of the 173 rows and the 182
// hitbox rows equals the extracted file, captures/pc2-automoves/run1):
//   P (action object) : u32 +0x00 action id, +0x20 its 336-byte row, +0x30 X (the file's loaded descriptor, shared by all its actions),
//                       +0x40 hitbox row pointer array, u16 +0x48 first, u16 +0x4A count (= the file's packed hitbox list; the game's per-tick
//                       slot walk reads exactly these: count word [R+0x4A], RVA 0x35A220)
//   X                 : +0x88 action object array (stride found by matching row pointers), +0x90 row pointer array, u16 +0x98 action count
static int AmLoadImg(uint64_t P) {
    uint64_t X = 0, A = 0, RP = 0; uint16_t n = 0;
    if (!Rd64(P + 0x30, &X) || !X) return -1;
    { const int i = AmImgOf(X); if (i >= 0) return i; }
    if (g_ami_n >= AM_FILES) return -1;
    if (!Rd64(X + 0x88, &A) || !A || !Rd64(X + 0x90, &RP) || !RP || !AmRd(X + 0x98, &n, 2) || !n || n > AM_MAXMOVES) {
        Log("AUTOMOVES: action object %p: descriptor %p unreadable (actions %p, rows %p, n %u)", (void*)P, (void*)X, (void*)A, (void*)RP, n); return -1;
    }
    static uint64_t rp[AM_MAXMOVES]; static uint8_t rows[AM_MAXMOVES * AM_ROW]; static uint16_t first[AM_MAXMOVES], cnt[AM_MAXMOVES];
    if (!AmRd(RP, rp, 8u * n)) return -1;
    uint64_t stride = 0, r0 = 0;
    if (Rd64(A + 0x20, &r0) && r0 == rp[0]) {
        if (n == 1) stride = 0xF0;
        else for (uint64_t sd = 8; sd <= 0x800 && !stride; sd += 8) { uint64_t r1 = 0; if (Rd64(A + sd + 0x20, &r1) && r1 == rp[1]) stride = sd; }
    }
    if (!stride) { Log("AUTOMOVES: descriptor %p: action objects %p do not match the row pointers", (void*)X, (void*)A); return -1; }
    uint64_t hbArr = 0; int nhb = 0, odd = 0;
    for (int k = 0; k < n; ++k) {
        const uint64_t o = A + (uint64_t)k * stride; uint64_t rk = 0, h = 0; uint16_t fc[2] = {0, 0};
        if (!Rd64(o + 0x20, &rk) || rk != rp[k] || !AmRd(rp[k], rows + (size_t)k * AM_ROW, AM_ROW)) { Log("AUTOMOVES: descriptor %p: action %d unreadable", (void*)X, k); return -1; }
        Rd64(o + 0x40, &h); AmRd(o + 0x48, fc, 4);
        first[k] = fc[0]; cnt[k] = fc[1];
        if (cnt[k]) {
            if (!hbArr) hbArr = h;
            if (h != hbArr || cnt[k] > AM_MAXROWS) { ++odd; cnt[k] = 0; continue; }
            if (first[k] + cnt[k] > nhb) nhb = first[k] + cnt[k];
        }
    }
    uint8_t* hb = nhb ? (uint8_t*)malloc((size_t)nhb * AM_HB) : 0;
    for (int i = 0; i < nhb; ++i) {
        uint64_t q = 0;
        if (!hb || !Rd64(hbArr + 8u * (uint64_t)i, &q) || !q || !AmRd(q, hb + (size_t)i * AM_HB, AM_HB)) { Log("AUTOMOVES: descriptor %p: hitbox row %d unreadable", (void*)X, i); free(hb); return -1; }
    }
    const uint32_t cap = AmImageSize(n, nhb, cnt); uint8_t* img = (uint8_t*)malloc(cap); AmTable* t = (AmTable*)malloc(sizeof(AmTable));
    const uint32_t size = img && t ? AmBuildImage(img, cap, rows, n, hb, nhb, first, cnt) : 0;
    const int na = size ? AmParse(img, size, t) : 0;
    free(img); free(hb);
    if (na != n) { Log("AUTOMOVES: descriptor %p: rebuilt image did not parse (%d of %u actions)", (void*)X, na, n); free(t); return -1; }
    ArApply(&g_action_records, t); // archive records replace the old window estimate before any catalogue/export publication
    uint32_t pid = 0; AmRd(P, &pid, 4);
    Log("AUTOMOVES: loaded action data %p: %u actions (objects %p stride 0x%llX, first row %p), %d hitbox rows (array %p)%s; current action 0x%X",
        (void*)X, n, (void*)A, (unsigned long long)stride, (void*)rp[0], nhb, (void*)hbArr, odd ? " [some actions use another hitbox array: hits unknown]" : "", pid);
    g_ami[g_ami_n].base = X; g_ami[g_ami_n].n = n; g_ami[g_ami_n].t = t;
    return g_ami_n++;
}
// Own file of chr (0 = not known yet). Entries are only appended.
static const AmTable* AmFor(uint32_t chr) {
    if (!g_cfg_am) return 0;
    for (int i = (int)g_amc_n - 1; i >= 0; --i) if (g_amc[i].chr == chr) return g_ami[g_amc[i].img].t;
    return 0;
}
static void AmFinal(int s, const char* why) {
    if (g_ams[s].done) return;
    g_ams[s].done = 1;
    int best = -1; for (int k = 0; k < g_ams[s].nf; ++k) if (best < 0 || g_ami[g_ams[s].f[k]].n > g_ami[best].n) best = g_ams[s].f[k];
    if (best < 0) { Log("AUTOMOVES spawn #%d chr %u: FINAL none (no action data found, %d misses) [%s]", s, g_ams[s].chr, g_ams[s].miss, why); return; }
    const uint32_t chr = g_ams[s].chr;
    if (g_ams[s].nf > 1 && g_ami_n < AM_FILES) {  // a boss that uses several files (humans: own file + the shared CP_HUMAN ones): the move list is their union
        AmTable* u = (AmTable*)malloc(sizeof(AmTable));
        if (u) {
            *u = *g_ami[best].t;
            for (int k = 0; k < g_ams[s].nf; ++k) { const AmTable* f = g_ami[g_ams[s].f[k]].t; if (f == g_ami[best].t) continue;
                for (int i = 0; i < f->n && u->n < AM_MAXMOVES; ++i) if (AmFind(u, f->m[i].id) < 0) u->m[u->n++] = f->m[i]; }
            g_ami[g_ami_n].base = 0; g_ami[g_ami_n].n = u->n; g_ami[g_ami_n].t = u; best = g_ami_n++;
        }
    }
    const AmTable* t = g_ami[best].t;
    int atk = 0, blk = 0, great = 0, hk = 0, links = 0, seq = 0, idle = 0, mov = 0;
    for (int i = 0; i < t->n; ++i) { atk += t->m[i].attack; blk += t->m[i].block; great += t->m[i].great; hk += t->m[i].hits > 0; links += t->m[i].link; seq += AmSeqHead(t, i); idle += t->m[i].kind == 'I'; mov += t->m[i].kind == 'M'; }
    Log("AUTOMOVES spawn #%d chr %u: FINAL action data %p (0 = union of the files seen): %d moves (attack %d, attack block %d, unblockable %d, hit count known %d, movement %d, idle %d), "
        "%d next links, %d sequences; %d file(s) seen [%s]", s, chr, (void*)g_ami[best].base, t->n, atk, blk, great, hk, mov, idle, links, seq, g_ams[s].nf, why);
    // the proof compares with the hand tables' ORIGINAL contents (move_catalog.h kN3MovesHand ..., apart from the live catalogue)
    if (!AmFor(chr) && AmcHasChr(chr)) { char line[1400]; AmCmp c; AmCompare(t, chr, &c); AmCmpText(&c, line, sizeof line); Log("AUTOMOVES PROOF chr %u runtime vs hand: %s", chr, line); }
    else if (!AmFor(chr)) Log("AUTOMOVES chr %u: no hand data for this boss: the move list is game data only", chr);
    char sb[900]; size_t o = 0; sb[0] = 0;
    for (int i = 0; i < t->n && o + 60 < sizeof sb; ++i) if (AmSeqHead(t, i)) {
        int ids[8], lp; const int k = AmSeqChain(t, i, ids, 8, &lp);
        o += (size_t)snprintf(sb + o, sizeof sb - o, " [");
        for (int q = 0; q < k; ++q) o += (size_t)snprintf(sb + o, sizeof sb - o, "%s%X", q ? ">" : "", t->m[ids[q]].id);
        o += (size_t)snprintf(sb + o, sizeof sb - o, "%s]", lp ? " loop" : "");
    }
    Log("AUTOMOVES chr %u sequences:%s", chr, sb[0] ? sb : " none");
    { const int k = AmFind(t, 0x4E6E); if (k >= 0) Log("AUTOMOVES chr %u action 0x4E6E: hits %u (windows %u), unblockable %u, attack block %u", chr, t->m[k].hits, t->m[k].nwin, t->m[k].great, t->m[k].block); }
    if (!AmFor(chr) && g_amc_n < AM_CHRS) { g_amc[g_amc_n].chr = chr; g_amc[g_amc_n].img = best; InterlockedIncrement(&g_amc_n); N3CatMergeAuto(chr, t); }
}
static void AmTick(void) {
    if (!g_cfg_am) return;
    const DWORD now = GetTickCount();
    for (int s = 0; s < MAXSP; ++s) {
        const uint64_t a = s < g_nsp ? g_sp_actor[s] : 0;
        if (a != g_ams[s].a || (a && g_sp_chr[s] != g_ams[s].chr)) {
            if (g_ams[s].a && !g_ams[s].done && g_ams[s].nf) AmFinal(s, "despawned/replaced");
            memset(&g_ams[s], 0, sizeof g_ams[s]); g_ams[s].a = a; g_ams[s].t0 = g_ams[s].tLast = now; g_ams[s].chr = a ? g_sp_chr[s] : 0;
        }
        if (!a || g_ams[s].done || now < g_ams[s].tNext) continue;
        g_ams[s].tNext = now + 250;
        uint64_t ai = 0, ctrl = 0, p = 0, x = 0;
        if (Rd64(a + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl && Rd64(ctrl + 0xE8, &p) && p >= 0x10000 && Rd64(p + 0x30, &x) && x) {
            int known = AmImgOf(x);
            if (known < 0 && g_ams[s].miss < 4) { known = AmLoadImg(p); if (known < 0) ++g_ams[s].miss; }
            if (known >= 0) {
                int have = 0; for (int k = 0; k < g_ams[s].nf; ++k) have |= g_ams[s].f[k] == known;
                if (!have && g_ams[s].nf < AM_SPF) {
                    g_ams[s].f[g_ams[s].nf++] = known; g_ams[s].tLast = now;
                    Log("AUTOMOVES spawn #%d chr %u: uses action data %p (%d actions)", s, g_ams[s].chr, (void*)g_ami[known].base, g_ami[known].n);
                }
            }
        }
        const DWORD age = now - g_ams[s].t0;
        if (age > 30000) AmFinal(s, "timeout");
        else if (age > 8000 && g_ams[s].nf && now - g_ams[s].tLast > 6000) AmFinal(s, "stable");
    }
}
