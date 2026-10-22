// user #14150 / #14267 'Infinite health' (Player page, [Summon] PlayerCantDie): what a Training / Boss Match session does with the player's HP.
// Pure decision, no game access (tests/player_hp_test.c). Native NoDeath (SessionNoDeath) keeps the HP at 1 at worst, so 'dead' = the floor.
//   ON  (cantdie 1): any HP below max -> refill at once (the player never loses HP; 0 HP never happens).
//   OFF (cantdie 0): normal damage; HP at the floor (<= 1 or <= 1 % of max) -> beaten -> TrainTick respawns in the arena after
//                    [Summon] BeatenRespawnMs (default 0 = at once; was a fixed 2 s).
// #14267 live (19:46, cantdie OFF): the toggle took effect at once (read every tick), but the player HP stayed 2658/2658 for the whole test,
// so there was never a floor to react to; with the old ON (heal only at the floor) ON and OFF looked the same until the floor.
enum { PH_NONE = 0, PH_REFILL = 1, PH_BEATEN = 2 };
static int PhFloor(uint64_t c, uint64_t m) { return m && (c <= 1 || c * 100 <= m); }
static int PhDecide(int cantdie, int nodeath, int beaten, int lost, uint64_t c, uint64_t m) {
    if (!m || beaten || lost) return PH_NONE;
    if (cantdie && nodeath) return c < m ? PH_REFILL : PH_NONE;
    return PhFloor(c, m) ? PH_BEATEN : PH_NONE;
}
// user #22918: the per-tick refill above only ran for a Practice (g_tr) session; in a Boss Match / arena fight (g_bm / g_ar) nothing healed the player (live 03:12 HB lines:
// HP 1969 -> 1280 -> 591 -> 0 vs Yamagata, never refilled), and even in Practice one hit larger than the HP left kills before the next tick. So with Infinite health ON
// the HP damage itself is cut to 0 when the victim is the player and any session is live (hook 787268 at 329890), and the Boss Match tick refills a hurt, living player.
static int PhDamageCut(int cantdie, int session, int victimIsPlayer, int dmg) { return cantdie && session && victimIsPlayer && dmg > 0 ? 0 : dmg; }
static int PhMatchRefill(int cantdie, int session, uint64_t c, uint64_t m) { return cantdie && session && m && c > 0 && c < m; }
