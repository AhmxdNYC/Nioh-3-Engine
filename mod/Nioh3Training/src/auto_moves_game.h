// pc2 #23907: runtime boss move list, game side (logic: auto_moves.h). Read only, nothing is written to the game. [Debug] AutoMoves (default 1).
// The boss's current action row R = [[[[actor+0x3B0]+8]+0xE8]+0x20] (proven by autobind_game.h) lies inside its loaded archive_01 action file. The file
// header (u32 16 + 16 x (offset, size)) is searched downward from R (AmFindBase: it must place R on a row boundary of sub-table 1), then the whole
// image is copied and parsed. Each new row outside the parsed images is another file (a boss uses its own file plus shared ones); the biggest one is
// the boss's own move list. Only struct offsets, no RVAs, no names, no hand tables. The proof line compares it with the old hand headers.
#pragma once
#include "auto_moves.h"
#include "auto_moves_cmp.h"
static int g_cfg_am = 1;
enum { AM_FILES = 48, AM_SPF = 8, AM_BACK = 1 << 20, AM_CHRS = 64 };
typedef struct { uint64_t base; uint32_t size; AmTable* t; } AmImg;
static AmImg g_ami[AM_FILES]; static int g_ami_n;
static struct { uint32_t chr; int img; } g_amc[AM_CHRS]; static volatile LONG g_amc_n;  // chr -> its own file (final)
static struct { uint64_t a; uint32_t chr; DWORD t0, tNext, tLast; int done, nf, miss; int f[AM_SPF]; } g_ams[MAXSP];
static __attribute__((noinline)) int AmRd(uint64_t a, void* out, uint32_t n) { __try { memcpy(out, (const void*)a, n); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static int AmImgOf(uint64_t row) { for (int i = 0; i < g_ami_n; ++i) if (row >= g_ami[i].base && row < g_ami[i].base + g_ami[i].size) return i; return -1; }
// 1 = row looks like an action row (constant words +0xE4 / +0xE6 = 1, notes/yoritomo-action-fields.md); used only for the diagnostic block start
static int AmRowLike(uint64_t r) { uint16_t w[2]; return AmRd(r + 0xE4, w, 4) && w[0] == 1 && w[1] == 1; }
static void AmDiag(uint64_t row) {
    uint64_t lo = row; int k = 0; while (k < 4000 && AmRowLike(lo - AM_ROW)) { lo -= AM_ROW; ++k; }
    uint32_t w[16] = {0}; AmRd(lo - 64, w, 64);
    MEMORY_BASIC_INFORMATION mb; memset(&mb, 0, sizeof mb); VirtualQuery((const void*)row, &mb, sizeof mb);
    Log("AUTOMOVES diag: row %p, row-like block start %p (%d rows before), alloc base %p (row - alloc = 0x%llX), region %p size 0x%llX; 64 B before block: "
        "%08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X", (void*)row, (void*)lo, k, mb.AllocationBase,
        (unsigned long long)(row - (uint64_t)mb.AllocationBase), mb.BaseAddress, (unsigned long long)mb.RegionSize,
        w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]);
}
// Find + parse the file that holds row. Returns the image index or -1.
static int AmLoadImg(uint64_t row) {
    int i = AmImgOf(row); if (i >= 0) return i;
    if (g_ami_n >= AM_FILES) return -1;
    static uint8_t buf[AM_BACK + AM_ROW];
    // copy [row - AM_BACK, row + AM_ROW) page by page from the top; stop at the first unreadable page (the header must be contiguous with the row)
    const uint64_t top = row + AM_ROW; uint64_t lo = top;
    while (top - lo < AM_BACK) {
        const uint64_t pg = (lo - 1) & ~0xFFFull, from = (top - pg > AM_BACK) ? top - AM_BACK : pg;
        if (!AmRd(from, buf + (from - (top - AM_BACK)), (uint32_t)(lo - from))) break;
        lo = from;
    }
    const uint64_t bufBase = top - AM_BACK; const uint8_t* b = buf + (lo - bufBase);
    const int64_t o = AmFindBase(b, lo, (uint32_t)(top - lo), row);
    if (o < 0) { Log("AUTOMOVES: no action-file header within 0x%llX bytes below row %p", (unsigned long long)(top - lo), (void*)row); AmDiag(row); return -1; }
    const uint64_t base = lo + (uint64_t)o; const uint32_t size = AmHeader(b + o, (uint32_t)(top - lo - (uint64_t)o) > 4096 ? 4096 : (uint32_t)(top - lo - (uint64_t)o));
    uint8_t* img = (uint8_t*)malloc(size); AmTable* t = (AmTable*)malloc(sizeof(AmTable));
    if (!img || !t || !AmRd(base, img, size) || AmParse(img, size, t) <= 0) {
        Log("AUTOMOVES: header at %p (row %p) but the image (%u bytes) did not read / parse", (void*)base, (void*)row, size);
        free(img); free(t); return -1;
    }
    uint16_t rid = 0; AmRd(row + 0xC, &rid, 2);
    const int rr = (int)((row - base - t->off[1]) / AM_ROW);
    MEMORY_BASIC_INFORMATION mb; memset(&mb, 0, sizeof mb); VirtualQuery((const void*)base, &mb, sizeof mb);
    Log("AUTOMOVES: action file image at %p (alloc base %p, +0x%llX), %u bytes, %d actions, %u hitbox rows; current row %p = row %d id 0x%X (table 0x%X); "
        "sub-tables (off,size): %X,%X %X,%X %X,%X %X,%X %X,%X %X,%X", (void*)base, mb.AllocationBase, (unsigned long long)(base - (uint64_t)mb.AllocationBase), size,
        t->n, t->len[2] / AM_HB, (void*)row, rr, rid, rr >= 0 && rr < t->n ? t->m[rr].id : 0xFFFF, t->off[0], t->len[0], t->off[1], t->len[1], t->off[2], t->len[2],
        t->off[3], t->len[3], t->off[5], t->len[5], t->off[7], t->len[7]);
    free(img);
    g_ami[g_ami_n].base = base; g_ami[g_ami_n].size = size; g_ami[g_ami_n].t = t;
    return g_ami_n++;
}
// Own file of chr (0 = not known yet). Read by the menu / move-list paths (any thread: entries are only appended).
static const AmTable* AmFor(uint32_t chr) {
    if (!g_cfg_am) return 0;
    for (int i = (int)g_amc_n - 1; i >= 0; --i) if (g_amc[i].chr == chr) return g_ami[g_amc[i].img].t;
    return 0;
}
static void AmFinal(int s, const char* why) {
    if (g_ams[s].done) return;
    g_ams[s].done = 1;
    int best = -1; for (int k = 0; k < g_ams[s].nf; ++k) if (best < 0 || g_ami[g_ams[s].f[k]].t->n > g_ami[best].t->n) best = g_ams[s].f[k];
    if (best < 0) { Log("AUTOMOVES spawn #%d chr %u: FINAL none (no action file found, %d misses) [%s]", s, g_ams[s].chr, g_ams[s].miss, why); return; }
    const AmTable* t = g_ami[best].t;
    if (!AmFor(g_ams[s].chr) && g_amc_n < AM_CHRS) { g_amc[g_amc_n].chr = g_ams[s].chr; g_amc[g_amc_n].img = best; InterlockedIncrement(&g_amc_n); }
    int atk = 0, blk = 0, great = 0, hk = 0, links = 0, seq = 0, idle = 0, mov = 0;
    for (int i = 0; i < t->n; ++i) { atk += t->m[i].attack; blk += t->m[i].block; great += t->m[i].great; hk += t->m[i].hits > 0; links += t->m[i].link; seq += AmSeqHead(t, i); idle += t->m[i].kind == 'I'; mov += t->m[i].kind == 'M'; }
    Log("AUTOMOVES spawn #%d chr %u: FINAL own file %p: %d moves (attack %d, attack block %d, unblockable %d, hit count known %d, movement %d, idle %d), "
        "%d next links, %d sequences; %d file(s) seen [%s]", s, g_ams[s].chr, (void*)g_ami[best].base, t->n, atk, blk, great, hk, mov, idle, links, seq, g_ams[s].nf, why);
    char line[1400]; AmCmp c; AmCompare(t, g_ams[s].chr, &c); AmCmpText(&c, line, sizeof line);
    if (N3MoveFind(g_ams[s].chr, 2) >= 0 || N3MoveFactsBoss(g_ams[s].chr)) Log("AUTOMOVES PROOF chr %u runtime vs hand: %s", g_ams[s].chr, line);
    else Log("AUTOMOVES chr %u: no hand data for this boss (runtime list only)", g_ams[s].chr);
    char sb[900]; size_t o = 0; sb[0] = 0;
    for (int i = 0; i < t->n && o + 60 < sizeof sb; ++i) if (AmSeqHead(t, i)) {
        int ids[8], lp; const int k = AmSeqChain(t, i, ids, 8, &lp);
        o += (size_t)snprintf(sb + o, sizeof sb - o, " [");
        for (int q = 0; q < k; ++q) o += (size_t)snprintf(sb + o, sizeof sb - o, "%s%X", q ? ">" : "", t->m[ids[q]].id);
        o += (size_t)snprintf(sb + o, sizeof sb - o, "%s]", lp ? " loop" : "");
    }
    Log("AUTOMOVES chr %u sequences:%s", g_ams[s].chr, sb[0] ? sb : " none");
    { const int k = AmFind(t, 0x4E6E); if (k >= 0) Log("AUTOMOVES chr %u action 0x4E6E: hits %u (windows %u), unblockable %u, attack block %u", g_ams[s].chr, t->m[k].hits, t->m[k].nwin, t->m[k].great, t->m[k].block); }
}
static void AmTick(void) {
    if (!g_cfg_am) return;
    const DWORD now = GetTickCount();
    for (int s = 0; s < MAXSP; ++s) {
        const uint64_t a = s < g_nsp ? g_sp_actor[s] : 0;
        if (a != g_ams[s].a || (a && g_sp_chr[s] != g_ams[s].chr)) {
            if (g_ams[s].a && !g_ams[s].done) AmFinal(s, "despawned/replaced");
            memset(&g_ams[s], 0, sizeof g_ams[s]); g_ams[s].a = a; g_ams[s].t0 = g_ams[s].tLast = now; g_ams[s].chr = a ? g_sp_chr[s] : 0;
        }
        if (!a || g_ams[s].done || now < g_ams[s].tNext) continue;
        g_ams[s].tNext = now + 250;
        uint64_t ai = 0, ctrl = 0, p = 0, row = 0;
        if (Rd64(a + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl && Rd64(ctrl + 0xE8, &p) && p && Rd64(p + 0x20, &row) && row >= 0x10000) {
            int known = AmImgOf(row);
            if (known < 0 && g_ams[s].miss < 4) { known = AmLoadImg(row); if (known < 0) ++g_ams[s].miss; }
            if (known >= 0) {
                int have = 0; for (int k = 0; k < g_ams[s].nf; ++k) have |= g_ams[s].f[k] == known;
                if (!have && g_ams[s].nf < AM_SPF) { g_ams[s].f[g_ams[s].nf++] = known; g_ams[s].tLast = now;
                    Log("AUTOMOVES spawn #%d chr %u: uses file %p (%d actions)", s, g_ams[s].chr, (void*)g_ami[known].base, g_ami[known].t->n); }
            }
        }
        const DWORD age = now - g_ams[s].t0;
        if (age > 30000) AmFinal(s, "timeout");
        else if (age > 8000 && g_ams[s].nf && now - g_ams[s].tLast > 6000) AmFinal(s, "stable");
    }
}
