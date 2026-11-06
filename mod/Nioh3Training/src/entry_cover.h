// Bounded native-entry presentation only. Never pauses mission scripts or changes save state.
#pragma once
#include <stdint.h>
typedef struct { int active, covered, extended; uint32_t started; } EntryCover;
enum { EC_WAIT=0, EC_SHOW=1, EC_HIDE=2, EC_TIMEOUT=3, EC_END=4, EC_HARD_MS=90000u, EC_SAFETY_MS=180000u };
static void EcBegin(EntryCover* c, uint32_t now) { c->active=1; c->covered=0; c->extended=0; c->started=now; }
static int EcStep(EntryCover* c, uint32_t now, int cancel, int fight, int intro, int hold) {
    if (!c->active) return EC_WAIT;
    // hold = game still loading, or Fight now still pending. The 90 s cap yields; 180 s never does.
    // Cancel and a live fallback fight still end on this tick.
    const uint32_t age = now - c->started;
    if (cancel || fight || age >= EC_SAFETY_MS || (age >= EC_HARD_MS && !hold)) {
        c->active=c->covered=0;
        return cancel || fight ? EC_END : EC_TIMEOUT;
    }
    if (intro && c->covered) { c->covered=0; return EC_HIDE; }
    if (!intro && !c->covered) { c->covered=1; return EC_SHOW; }
    return EC_WAIT;
}
