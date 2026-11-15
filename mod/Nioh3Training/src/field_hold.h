#include <stdint.h>
// #89181: a long same-world warp reports stage state 5 while the destination cell is in the
// loaded set without the collision mark (StageLog '*', cell entry +0xE8). Hold the player
// on the target until that mark has been set, then summon. Never summon on the cell we
// left. After 20 s without the mark, go home and do not summon.
// Pure decision. starMs is how long the current cell has kept the mark.
enum { FH_HOLD = 0, FH_SUMMON = 1, FH_HOME = -1 };
static int FieldHoldStep(unsigned elapsedMs, uint32_t cellNow, int star, uint32_t cellAtWarp, unsigned starMs) {
    if (cellNow && cellNow != cellAtWarp && star == 1 && starMs >= 500) return FH_SUMMON;
    if (elapsedMs >= 20000) return FH_HOME;
    return FH_HOLD;
}
