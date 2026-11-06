// Bounded native-entry presentation only. Never pauses mission scripts or changes save state.
#pragma once
#include <stdint.h>
typedef struct { int active, covered; uint32_t started; } EntryCover;
enum { EC_WAIT=0, EC_SHOW=1, EC_HIDE=2, EC_TIMEOUT=3, EC_END=4 };
static void EcBegin(EntryCover* c, uint32_t now) { c->active=1; c->covered=0; c->started=now; }
static int EcStep(EntryCover* c, uint32_t now, int cancel, int fight, int intro) {
    if (!c->active) return EC_WAIT;
    if (cancel || fight || now-c->started >= 90000u) {
        c->active=c->covered=0;
        return cancel || fight ? EC_END : EC_TIMEOUT;
    }
    if (intro && c->covered) { c->covered=0; return EC_HIDE; }
    if (!intro && !c->covered) { c->covered=1; return EC_SHOW; }
    return EC_WAIT;
}
