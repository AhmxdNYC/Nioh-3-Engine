// Offline-tested lifecycle core. NOT wired to native creation/deletion yet.
// All calls require one serialized game-thread adapter. Proof flags must be
// established by that adapter; elapsed time and pointer equality are not proof.
#ifndef N3_OWNED_REGISTRY_H
#define N3_OWNED_REGISTRY_H
#include <stdint.h>
#include <string.h>
#define OR_CAP 2
typedef enum { OR_FREE, OR_LOADING, OR_ISSUED, OR_LIVE, OR_REMOVING,
               OR_UNKNOWN } OrState;
typedef struct {
    uint64_t epoch, address, incarnation;
    uint32_t handle, character, owner;
} OrIdentity;
typedef struct {
    uint64_t token, generation;
    OrState state;
    OrIdentity id;
    unsigned observed, delete_attempted, observer, hud_pending;
} OrJob;
typedef struct {
    OrJob jobs[OR_CAP];
    uint64_t serial, epoch, generation, acquired, released;
    unsigned clearing, world_lost, baseline_no_death, player_no_death;
} OrRegistry;
static inline void OrInit(OrRegistry *r, uint64_t epoch, unsigned baseline) {
    memset(r, 0, sizeof *r); r->epoch = epoch; r->generation = 1;
    r->baseline_no_death = r->player_no_death = !!baseline;
}
static inline OrJob *OrFind(OrRegistry *r, uint64_t token) {
    if (token) for (unsigned i = 0; i < OR_CAP; ++i)
        if (r->jobs[i].state && r->jobs[i].token == token) return &r->jobs[i];
    return 0;
}
static inline int OrSame(OrIdentity a, OrIdentity b) {
    return a.epoch == b.epoch && a.address == b.address &&
        a.incarnation == b.incarnation && a.handle == b.handle &&
        a.character == b.character && a.owner == b.owner;
}
static inline void OrRetire(OrRegistry *r, OrJob *j) {
    ++r->released; memset(j, 0, sizeof *j);
}
// Accounting is logical consumer pins, NOT permission to call native Release.
// Admission proof includes a distinct reserved native owner slot and resource
// capacity. Pin acquisition must succeed before this function is called.
static inline uint64_t OrAdd(OrRegistry *r, uint32_t chr, uint32_t owner,
                             int stable, int admission_proven) {
    OrJob *free_job = 0;
    if (r->clearing || r->world_lost || !stable || !admission_proven ||
        !chr || !owner || r->serial == UINT64_MAX) return 0;
    for (unsigned i = 0; i < OR_CAP; ++i) {
        OrJob *j = &r->jobs[i];
        if (!j->state) free_job = j;
        else if (j->id.owner == owner) return 0;
    }
    if (!free_job) return 0;
    free_job->state = OR_LOADING; free_job->token = ++r->serial;
    free_job->generation = r->generation;
    free_job->id.epoch = r->epoch; free_job->id.character = chr;
    free_job->id.owner = owner; ++r->acquired; r->player_no_death = 1;
    return free_job->token;
}
static inline int OrIssue(OrRegistry *r, uint64_t token, uint32_t handle) {
    OrJob *j = OrFind(r, token);
    if (!j || r->clearing || r->world_lost || j->state != OR_LOADING || !handle) return 0;
    for (unsigned i = 0; i < OR_CAP; ++i) {
        OrJob *x = &r->jobs[i];
        if (x->state && ((x->id.handle && !x->observed) || x->id.handle == handle)) return 0;
    }
    j->id.handle = handle; j->state = OR_ISSUED; return 1;
}
static inline void OrTimeout(OrRegistry *r, uint64_t token) {
    OrJob *j = OrFind(r, token); if (!j || r->world_lost) return;
    if (j->state == OR_LOADING) OrRetire(r, j);
    else if (j->state == OR_ISSUED) j->state = OR_UNKNOWN;
}
static inline int OrReject(OrRegistry *r, uint64_t token, int native_rejection_proven) {
    OrJob *j = OrFind(r, token);
    if (!j || r->world_lost || !native_rejection_proven || j->observed ||
        (j->state != OR_ISSUED && j->state != OR_UNKNOWN)) return 0;
    OrRetire(r, j); return 1;
}
static inline int OrObserve(OrRegistry *r, uint64_t token, OrIdentity id,
                            int provenance_proven) {
    OrJob *j = OrFind(r, token);
    if (!j || r->world_lost || !provenance_proven || j->observed ||
        (j->state != OR_ISSUED && j->state != OR_UNKNOWN) ||
        !id.address || !id.incarnation || id.epoch != j->id.epoch ||
        id.handle != j->id.handle || id.character != j->id.character || id.owner != j->id.owner) return 0;
    for (unsigned i = 0; i < OR_CAP; ++i)
        if (r->jobs[i].observed && r->jobs[i].id.address == id.address) return 0;
    j->id = id; j->observed = 1; j->state = OR_LIVE;
    j->observer = !r->clearing; return 1;
}
// Caller must quiesce native/hook readers BEFORE clear/remove, and explicitly
// mark HUD obligations before detaching wrappers. This core owns no pointers.
static inline void OrClear(OrRegistry *r) {
    if (r->clearing) return;
    r->clearing = 1; ++r->generation;
    for (unsigned i = 0; i < OR_CAP; ++i) {
        OrJob *j = &r->jobs[i]; j->observer = 0;
        if (j->state == OR_LOADING) OrRetire(r, j);
    }
}
// Returns authorization for ONE delete attempt; mark indeterminate before the
// native call so exceptions cannot cause a repeat. Fresh incarnation required.
static inline int OrRemove(OrRegistry *r, uint64_t token, OrIdentity fresh) {
    OrJob *j = OrFind(r, token);
    if (!j || r->world_lost || j->state != OR_LIVE || !j->observed ||
        j->delete_attempted || !OrSame(j->id, fresh)) return 0;
    j->delete_attempted = 1; j->observer = 0; j->state = OR_UNKNOWN; return 1;
}
static inline void OrDeleteReturned(OrRegistry *r, uint64_t token) {
    OrJob *j = OrFind(r, token);
    if (j && !r->world_lost && j->delete_attempted && j->state == OR_UNKNOWN) j->state = OR_REMOVING;
}
static inline int OrAbsent(OrRegistry *r, uint64_t token, int complete_scan,
                           int exact_identity_present, int owner_released) {
    OrJob *j = OrFind(r, token);
    if (!j || r->world_lost || !j->observed || !complete_scan ||
        exact_identity_present || !owner_released || j->hud_pending) return 0;
    OrRetire(r, j); return 1;
}
static inline int OrCallback(OrRegistry *r, uint64_t token, uint64_t generation, uint64_t epoch) {
    OrJob *j = OrFind(r, token);
    return j && !r->clearing && !r->world_lost && j->observer && j->state == OR_LIVE &&
        generation == r->generation && generation == j->generation && epoch == r->epoch;
}
static inline int OrFinishClear(OrRegistry *r) {
    if (!r->clearing || r->world_lost) return 0;
    for (unsigned i = 0; i < OR_CAP; ++i) if (r->jobs[i].state) return 0;
    r->player_no_death = r->baseline_no_death; r->clearing = 0; return 1;
}
static inline void OrLoseWorld(OrRegistry *r) {
    OrClear(r); r->world_lost = 1;
    for (unsigned i = 0; i < OR_CAP; ++i)
        if (r->jobs[i].state) r->jobs[i].state = OR_UNKNOWN;
}
#endif
