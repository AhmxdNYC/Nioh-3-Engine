// Idle-hold liveness. ActorDeadNow() answers "is this actor dead?" (nonzero = dead), NOT "was the read ok":
// the old test `ActorDeadNow(...) && !dead` was false for every living actor, so IDLEHOLD stopped "boss defeated".
// An unreadable HP block leaves max == 0, which still fails closed here.
static int IhAlive(uint64_t actor) {
    int dead = 0; uint64_t hp = 0, max = 0;
    if (ActorDeadNow(actor, &dead, &hp, &max)) return 0;
    return !dead && max && hp > 1;
}
