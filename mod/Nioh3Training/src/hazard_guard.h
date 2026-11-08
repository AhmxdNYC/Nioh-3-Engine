// #70720: intercept native world hazards before HP/death/reward side effects.
// No boss/move/map lists. Ownership and native death state decide protection.
typedef struct { uint64_t actor,stats; unsigned generation; DWORD since,last; int pending,attempts; } HgRecord;
static HgRecord g_hg[N3T_SLOTS];
static unsigned g_hg_generation[N3T_SLOTS];
static int HgSlot(uint64_t a) {
    if(!a || a==PlayerChar() || a==g_kill_act || a==g_fin_live || !InSp(a)) return -1;
    for(int s=0;s<N3T_SLOTS;s++) if(g_slot_act[s]==a) {
        uint64_t st=0,hp=0,mx=0; int dead=0;
        if(!Rd64(a+0x3A0,&st) || !st || ActorDeadNow(a,&dead,&hp,&mx) || !hp || !mx) return -1;
        return s;
    }
    return -1;
}
static void HgMark(int s,uint64_t a) {
    uint64_t st=0; if(!Rd64(a+0x3A0,&st) || !st) return;
    HgRecord* r=&g_hg[s];
    if(r->pending && r->actor==a && r->stats==st && r->generation==g_hg_generation[s]) return;
    DWORD now=GetTickCount();
    if(r->actor!=a || r->stats!=st || r->generation!=g_hg_generation[s] || now-r->last>2000) {
        r->since=now;r->attempts=0;
    }
    r->actor=a;r->stats=st;r->generation=g_hg_generation[s];r->last=now;r->pending=1;
    Log("HAZARD boss slot %d actor %p: native world-death prevented before rewards",s,(void*)a);
}
#define HG_LATCH_UNSET 0x7F // nonzero like the "prior latch" AF910 tests; native writes only 0/1
typedef uint64_t (*HgUpdateFn)(uint64_t,uint64_t);
typedef uint64_t (*HgKillFn)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
static HgUpdateFn g_hg_update;
static HgKillFn g_hg_kill;
static __attribute__((noinline)) uint64_t HgUpdate(uint64_t a,uint64_t context) {
    int s=HgSlot(a); uint8_t prior=0;
    if(s>=0) {
        // AF910's hazard branch and kill-volume branch both test the prior latch.
        // It recalculates this latch from native world data before returning.
        if(*(uint8_t*)(a+0x89B)) HgMark(s,a);
        prior=*(uint8_t*)(a+0x89A);
        *(uint8_t*)(a+0x89A)=HG_LATCH_UNSET;
    }
    uint64_t result=g_hg_update(a,context);
    if(s>=0) {
        // AF910 returns early on a just-spawned actor (+852 set, 4032C4) without touching the latch.
        // An unchanged sentinel means "not recalculated", not a hazard (#300 Ally: boss removed every 2 s).
        if(*(uint8_t*)(a+0x89A)==HG_LATCH_UNSET) *(uint8_t*)(a+0x89A)=prior;
        else if(HgSlot(a)==s && *(uint8_t*)(a+0x89A)) HgMark(s,a);
    }
    return result;
}
static __attribute__((noinline)) uint64_t HgMovementKill(uint64_t c,uint64_t p2,uint64_t p3,uint64_t p4,
    uint64_t p5,uint64_t p6,uint64_t p7,uint64_t p8,uint64_t p9) {
    uint64_t a=0;int s=-1;
    if(Rd64(c,&a)) s=HgSlot(a);
    if(s>=0) { HgMark(s,a);return 0; }
    return g_hg_kill(c,p2,p3,p4,p5,p6,p7,p8,p9);
}
// Native fall damage (#300, Ally): after a long fall the landing action applies a stored self damage
// through 678636 (mode 3), outside AF910 and the movement kills. Only a lethal hit counts as a world death:
// falls the boss survives and combat hits (329890) keep their native result.
static __attribute__((noinline)) uint64_t HgFallDamage(uint64_t c,uint64_t p2,uint64_t p3,uint64_t p4,
    uint64_t p5,uint64_t p6,uint64_t p7,uint64_t p8,uint64_t p9) {
    uint64_t a=0,st=0,hp=0;int s=-1;
    if(Rd64(c,&a)) s=HgSlot(a);
    if(s>=0 && Rd64(a+0x3A0,&st) && st && Rd64(st+0x58,&hp) && (int64_t)(int32_t)p2>=(int64_t)hp) { HgMark(s,a);return 0; }
    return g_hg_kill(c,p2,p3,p4,p5,p6,p7,p8,p9);
}
