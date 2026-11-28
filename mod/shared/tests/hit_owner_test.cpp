// TASK #142938: projectile / effect colliders count for the character that fired them. cl /EHsc /nologo hit_owner_test.cpp && hit_owner_test
#include "../hit_owner.h"
#include <cstdio>
#include <map>
static std::map<uint64_t, uint64_t> M;
static bool rd(uint64_t a, uint64_t* o) { auto it = M.find(a); if (it == M.end()) return false; *o = it->second; return true; }
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
int main() {
    const uint64_t seal = 0x1000, helper = 0x2000, player = 0x3000, proj = 0x4000, fx = 0x5000, fxp = 0x5800, other = 0x6000, ctl = 0x7000;
    M[seal] = 0x140000000ull;          // type 0 character
    M[helper] = 0x140000010ull;        // type 0
    M[player] = 0x140000020ull;        // type 0
    M[proj] = 0x140000034ull; M[proj + 0x260] = seal;    // type 4 projectile fired by the seal
    M[fx] = 0x140000042ull; M[fx + 0x240] = fxp; M[fxp + 0x690] = helper;  // type 2 effect of the helper
    M[other] = 0x140000054ull; M[other + 0x260] = player;  // the player's projectile
    M[ctl] = 0x140000083ull; M[ctl + 0x1E0] = seal;       // type 3 controller -> character
    const uint64_t proj2 = 0x8000; M[proj2] = 0x140000094ull; M[proj2 + 0x260] = ctl;  // type 4 -> type 3 -> seal
    auto mine = [&](uint64_t a) { return a == seal || a == helper; };
    bool via = false;
    CHECK(HitOwnerChar(seal, rd) == seal);
    CHECK(HitOwnerMine(seal, rd, mine, &via) == seal && !via);
    CHECK(HitOwnerMine(proj, rd, mine, &via) == seal && via);       // the flame/projectile case: was dropped before
    CHECK(HitOwnerMine(fx, rd, mine, &via) == helper && via);
    CHECK(HitOwnerMine(proj2, rd, mine, &via) == seal && via);
    CHECK(HitOwnerMine(other, rd, mine, &via) == 0);                 // the player's own projectile never counts
    CHECK(HitOwnerMine(player, rd, mine, &via) == 0);
    CHECK(HitOwnerMine(0x9999, rd, mine, &via) == 0);                // unreadable
    CHECK(HitOwnerMine(0, rd, mine, &via) == 0);
    std::printf(fails ? "hit_owner FAIL %d\n" : "hit_owner PASS\n", fails);
    return fails ? 1 : 0;
}
