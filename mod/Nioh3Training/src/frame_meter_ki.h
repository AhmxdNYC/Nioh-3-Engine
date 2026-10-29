// Per-frame Ki history uses the SAME clock/view/freeze boundaries as the boss row.
#pragma once
#include "frame_meter.h"
#include "negki_meter.h"
#include <string.h>
typedef struct { uint8_t cell[FM_CELLS], frozen[FM_CELLS]; int64_t last; uint8_t prev; int frozenN; } FmKi;
static inline void FmKiReset(FmKi* k) { memset(k,0,sizeof *k); k->last=-1; }
static inline void FmKiSample(FmKi* k,const FmMeter* boss,double t,uint32_t act,int negative) {
    // Called BEFORE the boss sample, so the old run's boundary is still available.
    if(boss->on && act!=boss->act && boss->nw) {
        int64_t from=boss->c0;
        if(from<boss->lastCell-(FM_CELLS-1)) from=boss->lastCell-(FM_CELLS-1);
        if(from<0) from=0;
        k->frozenN=0;
        for(int64_t g=from;g<=boss->lastCell && k->frozenN<FM_CELLS;++g)
            k->frozen[k->frozenN++]=k->cell[g%FM_CELLS];
    }
    const int64_t g=FmCellOf(t);
    if(g<k->last) return;
    if(g==k->last) { k->cell[g%FM_CELLS]|=(uint8_t)!!negative; k->prev=(uint8_t)!!negative; return; }
    int64_t from=k->last+1; if(from<g-FM_CELLS) from=g-FM_CELLS;
    for(int64_t i=from;i<g;++i) k->cell[i%FM_CELLS]=k->last<0?0:k->prev;
    k->cell[g%FM_CELLS]=(uint8_t)!!negative; k->prev=(uint8_t)!!negative; k->last=g;
}
static inline int FmKiView(const FmKi* k,const FmMeter* boss,int i) {
    if(!(boss->on && boss->nw) && boss->haveLast) return i<k->frozenN?k->frozen[i]:0;
    const int64_t g=FmViewStart(boss)+i;
    return g<0 || g>k->last?0:k->cell[g%FM_CELLS];
}
static inline int FmKiEligible(int enabled,int human,int readOk,int dead,float cur,float secondary) {
    return enabled && human && readOk && !dead && NkmNeg(cur,secondary,0)<0.0f;
}
