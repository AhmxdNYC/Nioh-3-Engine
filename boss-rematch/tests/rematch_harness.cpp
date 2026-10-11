// rematch_harness.cpp - offline tests of the add-on (no game): rematch_core.h (catalogue, verified merge, E1 map redirect /
// CRC restore, E lifecycle incl. retry and timeout) and hooks.cpp (entry thunk with 2 handlers, args + volatile registers +
// xmm preserved, chaining onto another module's E9 hook, the pump call retarget with chaining). Built by run_rematch_harness.py.
#include "../src/addon.h"
#include "../src/rematch_core.h"
#include "../src/sites_gen.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int g_fail = 0, g_ok = 0;
#define CHECK(c, m)                                \
    do {                                           \
        if (c) ++g_ok;                             \
        else {                                     \
            ++g_fail;                              \
            printf("FAIL %s (line %d)\n", m, __LINE__); \
        }                                          \
    } while (0)

// ---- stubs for hooks.cpp ----
namespace addon {
Image g_img;
static uintptr_t g_fakeSite[sites::kCount];
static bool g_fakeOther[sites::kCount];
void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
    printf("\n");
}
bool Copy(void* d, uint64_t s, size_t n) {
    memcpy(d, (const void*)s, n);
    return true;
}
int SiteIndex(const char* name) {
    for (int i = 0; i < sites::kCount; ++i)
        if (!strcmp(sites::kDefs[i].name, name)) return i;
    return -1;
}
uintptr_t Site(const char* name) {
    const int i = SiteIndex(name);
    return i < 0 ? 0 : g_fakeSite[i];
}
bool SiteHookedByOther(int i) { return g_fakeOther[i]; }
bool GroupOk(const char* group, const char** why) {
    for (int i = 0; i < sites::kCount; ++i)
        if (!strcmp(sites::kDefs[i].group, group) && !g_fakeSite[i] && sites::kDefs[i].kind != sites::kNative && sites::kDefs[i].kind != sites::kHook) {
            if (why) *why = "unresolved";
        }
    return true;
}
}  // namespace addon
#include "../src/hooks.cpp"

static bool WrPlain(void* d, const void* s, size_t n) {
    memcpy(d, s, n);
    return true;
}

// ---- handler recorders ----
static uint64_t g_a[4], g_b[4];
static int g_calls1 = 0, g_calls2 = 0, g_ticks = 0;
static void H1(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t*) {
    g_a[0] = a, g_a[1] = b, g_a[2] = c, g_a[3] = d;
    ++g_calls1;
}
static void H2(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t*) {
    g_b[0] = a, g_b[1] = b, g_b[2] = c, g_b[3] = d;
    ++g_calls2;
}
static void Tick() { ++g_ticks; }

int main(int argc, char** argv) {
    // ---------------- catalogue ----------------
    std::string cat;
    if (argc > 1) {
        std::ifstream f(argv[1], std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        cat = ss.str();
    }
    std::vector<rcore::Row> rows;
    const int n = rcore::ParseCatalogue(cat, rows);
    int c[5] = {};
    for (auto& r : rows) c[r.route]++;
    printf("catalogue: %d rows (R %d H %d D %d E %d X %d)\n", n, c[0], c[1], c[2], c[3], c[4]);
    CHECK(n > 100, "catalogue parsed");
    CHECK(c[rcore::kE] == 20, "20 Picture Scroll rows");
    CHECK(c[rcore::kH] == 3, "3 hidden duels");
    CHECK(c[rcore::kX] > 0, "X rows kept with a reason");
    bool reasons = true, eidx = true, mid = true;
    for (auto& r : rows) {
        if (r.route == rcore::kX && !r.reason[0]) reasons = false;
        if (r.route == rcore::kE && (r.emakiIndex < 0 || r.emakiIndex > 19)) eidx = false;
        if (r.route <= rcore::kD && !r.missionId) mid = false;
    }
    CHECK(reasons, "every X row has a reason");
    CHECK(eidx, "E rows have record indexes 0..19");
    CHECK(mid, "R/H/D rows have mission ids");
    const rcore::Row* r52 = nullptr;
    for (auto& r : rows)
        if (r.missionId == 0x34) r52 = &r;
    CHECK(r52 && r52->Verified() && r52->placement == 76781, "row 0x34 Jakotsu-baba verified with placement 76781");
    // verified merge
    {
        std::vector<rcore::Row> v = rows;
        const char* e0 = nullptr;
        for (auto& r : v)
            if (r.route == rcore::kE) {
                e0 = r.id;
                break;
            }
        std::string res = std::string("id\tverified\tdate\n") + e0 + "\t1\t2026-10-08\nNOPE\t1\tx\n";
        CHECK(rcore::ApplyVerified(res, v) == 1, "verified file marks 1 row");
        bool ok = false;
        for (auto& r : v)
            if (!strcmp(r.id, e0)) ok = r.Verified() && !r.verifiedCat;
        CHECK(ok, "the E row is verified by the run file only");
    }
    // Fight now (E1 straight to the boss): start points next to the boss, R / H / D rows only
    {
        const rcore::Row *y = nullptr, *j = nullptr;
        int nf = 0, nfe = 0, nbad = 0;
        for (auto& r : rows) {
            if (!strcmp(r.id, "R1CBC")) y = &r;
            if (!strcmp(r.id, "R0034")) j = &r;
            nf += r.fightStart != 0;
            nfe += r.fightStart != 0 && (r.route == rcore::kE || r.route == rcore::kX);
            nbad += r.fightStart != 0 && (!r.fightConf[0] || !r.fightKind[0] || r.fightDist < 0 || r.fightDist > 120);
        }
        printf("fight now: %d rows with a start point\n", nf);
        CHECK(nf >= 60, "fight start points for the replayable rows");
        CHECK(nfe == 0, "no fight start on E / X rows (E rows keep their Emaki start point in stage_mission)");
        CHECK(nbad == 0, "every fight row has kind, confidence and a distance <= 120 m");
        CHECK(y && y->fightStart == 67064 && !strcmp(y->fightKind, "shrine") && y->fightDist == 35, "Yamagata Masakage (0x1CBC): shrine 67064, 35 m");
        CHECK(j && j->fightStart == 77032 && !strcmp(j->fightConf, "high") && (int)j->warp[2] == 238947,
              "Jakotsu-baba (0x34): boss retry point 77032, high, spawn z 238947");
        uint8_t slot[0x58] = {};
        const uint32_t mis = 0x34, sp = 77032;
        const uint64_t mode = 2;
        memcpy(slot + 4, &mis, 4);
        memcpy(slot + 0xC, &sp, 4);
        memcpy(slot + 0x14, &mode, 8);
        CHECK(rcore::FightSlotOk(slot, 0x34, 77032), "queued slot with mission, start point and mode 2 accepted");
        CHECK(!rcore::FightSlotOk(slot, 0x34, 72636), "a different start point (the default) is reported");
        CHECK(!rcore::FightSlotOk(slot, 0x1CBC, 77032), "a different mission is reported");
        CHECK(!rcore::FightSlotOk(slot, 0x34, 0), "start point 0 is never a fight start");
        // v2: arena points
        int nar = 0, nat = 0;
        for (auto& r : rows) {
            nar += r.arenaOk;
            nat += r.atFight;
        }
        printf("fight now v2: %d rows with an arena point, %d start at the fight\n", nar, nat);
        CHECK(y && y->arenaOk && y->arenaBox == 66599 && !strcmp(y->arenaConf, "high") && y->arena[0] > 337000 && y->arena[0] < 337898 &&
                  y->arena[1] > 10100 && y->arena[1] < 10400,
              "Yamagata: arena point inside BOSSAREABOX 66599 (x < 337898, the box's gate edge), high (trace)");
        CHECK(nar >= 10 && nat >= 20, "arena points + start-at-fight rows");
        {
            const float post[3] = {334650, 10100, 206713}, pre[3] = {337042, 10100, 206691}, shrine[3] = {339660, 10192, 206266};
            CHECK(y && y->boxOk && rcore::InTriggerBox(*y, y->arena) && rcore::InTriggerBox(*y, pre), "Yamagata: warp point and the captured trigger point are inside BOSSAREABOX");
            CHECK(y && !rcore::InTriggerBox(*y, post) && !rcore::InTriggerBox(*y, shrine), "Yamagata: the post-intro position and the shrine are outside it");
            // 1.4.0 SPAWN: shrine 67064's shipped record spawns at 339659.9 10191.8 206266.3 yaw -145.5, flags 4 (arrival camera)
            const float rec[4] = {339659.9f, 10191.8f, 206266.3f, -145.5f}, other[4] = {343783, 11114, 198363, -90};
            CHECK(y && rcore::SpawnRecordMatches(*y, rec) && !rcore::SpawnRecordMatches(*y, other), "spawn: only the catalogue's start point record is patched");
            CHECK(rcore::SpawnFlags(4) == 0 && rcore::SpawnFlags(0x44) == 0x40 && rcore::SpawnFlags(0x40) == 0x40, "spawn: only the arrival-camera bit is cleared");
            const float landed[3] = {339660, 10195, 206265};  // 1.3.3 log: the player on the first frame at shrine 67064
            CHECK(y && rcore::AtStartPoint(*y, landed) && !rcore::AtStartPoint(*y, y->arena) && !rcore::AtStartPoint(*y, post),
                  "spawn: at the shrine = override did not take (warp fallback); the arena point / the post-intro spot = spawned in the arena");
            int nsp = 0;
            for (auto& r : rows) nsp += r.arenaOk && r.fightStart && !r.atFight && !rcore::AtStartPoint(r, r.arena);
            printf("spawn override: %d rows spawn more than 5 m from their start point\n", nsp);
            CHECK(nsp >= 10, "spawn: most arena rows are more than 5 m from their start point (a closer one keeps the plain warp)");
        }
        const rcore::Row* hz = nullptr;
        for (auto& r : rows)
            if (!strcmp(r.id, "RC362")) hz = &r;
        CHECK(hz && hz->atFight && hz->FightNowReady(), "Hanzo (Demon's Regimen): one start point at the fight = ready without a warp");
        std::vector<rcore::Row> v = rows;
        const std::string cap = "# x\nmission\tplacements\tx\ty\tz\twhen\tsrc\n0x0034\t76770 76781 1\t204600\t12390\t237500\t2026-10-09_20:00\tbefore-load\n"
                                "0x0034\t5 6\t1\t2\t3\tx\tfight-start\n0x1CBC\t65092\t334650\t10100\t206713\t2026-10-09_20:08\n";
        CHECK(rcore::ApplyArenas(cap, v) == 1, "a capture applies to the row whose placement was created before the fight; 1.2.0 lines (no src) ignored");
        const rcore::Row* jc = nullptr;
        for (auto& r : v)
            if (!strcmp(r.id, "R0034")) jc = &r;
        CHECK(jc && jc->arenaOk && !strcmp(jc->arenaConf, "captured") && jc->arena[0] == 204600.0f && jc->arena[1] == 12440.0f,
              "captured point overrides the catalogue (+0.5 m on y)");
    }
    CHECK(rcore::ParseCatalogue("garbage\n", rows) == -1, "a file without the header is rejected");

    // ---------------- E1 map redirect ----------------
    {
        uint8_t map[41 * 8];
        memset(map, 0, sizeof(map));
        const uint8_t empty = 0xFF;
        for (int i = 0; i < 41; ++i) map[i * 8] = empty;
        const uint8_t bytes[20] = {0xD4, 0x8E, 0xA3, 0xD1, 0x0B, 0x79, 0x2D, 0x37, 0x57, 0x2C, 0xD8, 0x74, 0xEE, 0x63, 0x08, 0xB9, 0x40, 0x45, 0x64, 0xFB};
        for (uint32_t i = 0; i < 20; ++i) {
            const int slot = (int)((bytes[i] * 7u) % 41);
            int s = slot;
            while (map[s * 8] != empty) s = (s + 1) % 41;
            map[s * 8] = bytes[i];
            memcpy(&map[s * 8 + 4], &i, 4);
        }
        uint8_t orig[sizeof(map)];
        memcpy(orig, map, sizeof(map));
        static rcore::MapPatch mp;
        CHECK(rcore::MapRedirect(mp, map, 41, empty, 7, 20, &WrPlain), "redirect applied");
        bool all = true;
        int valid = 0;
        for (int i = 0; i < 41; ++i) {
            if (map[i * 8] == empty) {
                all &= !memcmp(&map[i * 8], &orig[i * 8], 8);
                continue;
            }
            ++valid;
            uint32_t idx = 0;
            memcpy(&idx, &map[i * 8 + 4], 4);
            all &= idx == 7 && map[i * 8] == orig[i * 8];
        }
        CHECK(all && valid == 20, "every valid entry -> record 7, keys and empty slots untouched");
        CHECK(!rcore::MapRedirect(mp, map, 41, empty, 3, 20, &WrPlain), "no second redirect while active");
        CHECK(rcore::MapRestore(mp, &WrPlain) == 0 && !memcmp(map, orig, sizeof(map)), "restore: CRC ok, bytes identical");
        CHECK(rcore::MapRestore(mp, &WrPlain) == -1, "restore twice = nothing active");
        CHECK(rcore::MapRedirect(mp, map, 41, empty, 2, 20, &WrPlain), "redirect again");
        map[5 * 8 + 1] ^= 1;  // somebody else writes a pad byte
        CHECK(rcore::MapRestore(mp, &WrPlain) == 1 && !memcmp(map, orig, sizeof(map)), "restore after a foreign write: reported, restored");
        CHECK(!rcore::MapRedirect(mp, map, 41, empty, 20, 20, &WrPlain), "target out of range refused");
    }
    // ---------------- E lifecycle ----------------
    {
        rcore::ETrack t;
        t.st = rcore::kEArmed;
        t.armedAt = 1000;
        CHECK(rcore::ETick(t, 2000, false, false) == 0, "armed, waiting");
        CHECK(rcore::ETick(t, 60000, false, true) == 0, "no timeout while loading");
        CHECK(rcore::ETick(t, 61000, true, true) == 1 && t.st == rcore::kEIn, "in mission");
        CHECK(rcore::ETick(t, 62000, false, true) == 0, "retry reload (loading, flag 0) keeps the patch");
        CHECK(rcore::ETick(t, 63000, true, false) == 0, "back in after the retry");
        CHECK(rcore::ETick(t, 64000, false, false) == 0 && rcore::ETick(t, 66000, false, false) == 0, "exit pending (< 5 s)");
        CHECK(rcore::ETick(t, 69500, false, false) == 2 && t.st == rcore::kEIdle, "exit after 5 s -> restore");
        t.st = rcore::kEArmed;
        t.armedAt = 100000;
        CHECK(rcore::ETick(t, 131000, false, false) == 3 && t.st == rcore::kEIdle, "armed 30 s without a load -> timeout restore");
    }
    // ---------------- hooks.cpp: entry thunk ----------------
    {
        uint8_t* code = (uint8_t*)VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        addon::g_img.base = (uintptr_t)code;
        addon::g_img.end = (uintptr_t)code + 0x10000;
        // fake BossBattleMode (head 5 = 48 89 5C 24 10 mov [rsp+10h],rbx): returns rcx + rdx*2 + r8*3 + r9*4, keeps r10/r11/xmm
        const uint8_t fn[] = {0x48, 0x89, 0x5C, 0x24, 0x10,                    // mov [rsp+10h],rbx  (relocated head)
                              0x48, 0x8D, 0x04, 0x51,                          // lea rax,[rcx+rdx*2]
                              0x4B, 0x8D, 0x04, 0x40, 0x48, 0x01, 0xC0,        // placeholder (rewritten below)
                              0xC3};
        (void)fn;
        // simpler body: rax = rcx + rdx; rax += r8; rax += r9; ret
        const uint8_t body[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x8D, 0x04, 0x11, 0x4C, 0x01, 0xC0, 0x4C, 0x01, 0xC8, 0xC3};
        uint8_t* f1 = code + 0x100;
        memcpy(f1, body, sizeof(body));
        const int iBB = addon::SiteIndex("BossBattleMode");
        CHECK(iBB >= 0 && sites::kDefs[iBB].head == 5, "BossBattleMode head 5");
        addon::g_fakeSite[iBB] = (uintptr_t)f1;
        addon::OnEntry("BossBattleMode", &H1);
        addon::OnEntry("BossBattleMode", &H2);
        const char* why = nullptr;
        CHECK(addon::InstallGroup("FIGHT", &why), why ? why : "install FIGHT");
        CHECK(f1[0] == 0xE9, "entry now jumps to the thunk");
        auto call = (uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t))f1;
        const uint64_t r = call(1, 20, 300, 4000);
        CHECK(r == 4321, "original runs after the handlers (relocated head + jmp back)");
        CHECK(g_calls1 == 1 && g_calls2 == 1, "both handlers ran once");
        CHECK(g_a[0] == 1 && g_a[1] == 20 && g_a[2] == 300 && g_a[3] == 4000 && !memcmp(g_a, g_b, sizeof(g_a)), "handlers got rcx rdx r8 r9");
        // chained: another module already hooked a second fake (E9 -> its stub that runs the original head and jumps back)
        uint8_t* f2 = code + 0x200;
        memcpy(f2, body, sizeof(body));
        uint8_t* other = code + 0x400;  // other module's thunk: relocated head + jmp f2+5
        memcpy(other, body, 5);
        other[5] = 0xE9;
        const int32_t back = (int32_t)((f2 + 5) - (other + 10));
        memcpy(other + 6, &back, 4);
        f2[0] = 0xE9;
        const int32_t to = (int32_t)(other - (f2 + 5));
        memcpy(f2 + 1, &to, 4);
        const int iCr = addon::SiteIndex("Character::Create");
        CHECK(sites::kDefs[iCr].head == 5, "Character::Create head 5");
        addon::g_fakeSite[iCr] = (uintptr_t)f2;
        addon::g_fakeOther[iCr] = true;
        addon::OnEntry("Character::Create", &H1);
        CHECK(addon::InstallGroup("FIGHT", &why), why ? why : "install chained");
        int32_t now = 0;
        memcpy(&now, f2 + 1, 4);
        CHECK(f2[0] == 0xE9 && f2 + 5 + now != other, "chained: rel32 rewritten to our thunk");
        const uint64_t r2 = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t))f2)(5, 6, 7, 8);
        CHECK(r2 == 26 && g_calls1 == 2 && g_a[0] == 5, "chained: our handler, then the other module's thunk, then the original");
        // ---------------- pump ----------------
        // a caller: sub rsp,28h; call callee; add rsp,28h; ret ; callee: mov eax, 77; ret
        uint8_t* callee = code + 0x600;
        const uint8_t cb[] = {0xB8, 77, 0, 0, 0, 0xC3};
        memcpy(callee, cb, sizeof(cb));
        uint8_t* caller = code + 0x700;
        const uint8_t pre[] = {0x48, 0x83, 0xEC, 0x28};
        memcpy(caller, pre, 4);
        caller[4] = 0xE8;
        int32_t rc = (int32_t)(callee - (caller + 9));
        memcpy(caller + 5, &rc, 4);
        const uint8_t post[] = {0x48, 0x83, 0xC4, 0x28, 0xC3};
        memcpy(caller + 9, post, sizeof(post));
        addon::g_fakeSite[addon::SiteIndex("PerFrameTickCall")] = (uintptr_t)(caller + 4);
        CHECK(addon::InstallPump(&Tick, &why), why ? why : "pump");
        const int rr = ((int (*)())caller)();
        CHECK(rr == 77 && g_ticks == 1, "pump: tick first, then the original callee (rax intact)");
        ((int (*)())caller)();
        CHECK(g_ticks == 2, "pump ticks every call");
    }
    printf("%d checks, %d failed\n", g_ok + g_fail, g_fail);
    if (!g_fail) printf("ALL PASSED\n");
    return g_fail ? 1 : 0;
}
