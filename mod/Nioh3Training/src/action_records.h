// Generated archive facts, not observed fight results. Pure C parser / identity join.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "auto_moves_types.h"
enum { AR_CAP = 32768 };
typedef struct { uint32_t hash; uint16_t id, motion; uint8_t hits, zero; char damage[96]; } ArRecord;
typedef struct { ArRecord* rows; int n; } ArTable;
static int ArCmp(const void* a, const void* b) {
    const ArRecord* x=(const ArRecord*)a; const ArRecord* y=(const ArRecord*)b;
    if(x->hash!=y->hash) return x->hash<y->hash?-1:1;
    if(x->id!=y->id) return x->id<y->id?-1:1;
    return x->motion<y->motion?-1:x->motion>y->motion;
}
static int ArNumber(const char* s, int base, unsigned long max, unsigned long* out) {
    char* end; unsigned long v; if(!s[0] || s[0]=='-' || s[0]=='+')return 0;
    errno=0; v=strtoul(s,&end,base); if(errno || *end || v>max)return 0; *out=v;return 1;
}
static int ArDamage(const char* s, unsigned hits) {
    if(!hits)return !*s;
    unsigned groups=1; int digit=0;
    for(;*s;++s) { if(*s>='0'&&*s<='9')digit=1; else if((*s=='/'||*s=='|')&&digit) {groups+=*s=='/';digit=0;} else return 0; }
    return digit && groups==hits;
}
// Modifies text. Atomic replacement: malformed/oversized tables leave the caller's prior table untouched.
static int ArLoad(ArTable* table,char* text) {
    static const char header[]="boss\taction_id\tmotion\tanim_hash\thit_count\tdamage_per_hit\tzero_power";
    ArTable tmp={0}; char* p=text; int line=0;
    tmp.rows=(ArRecord*)calloc(AR_CAP,sizeof(ArRecord)); if(!tmp.rows)return -1;
    while(*p) {
        char* end=strchr(p,'\n'); if(end)*end=0;
        size_t len=strlen(p); if(len&&p[len-1]=='\r')p[--len]=0;
        if(!line++) { if(strcmp(p,header))goto bad; }
        else {
            char* col[7]; int nc=1; col[0]=p;
            for(char* q=p;*q;++q)if(*q=='\t'){*q=0;if(nc==7)goto bad;col[nc++]=q+1;}
            if(nc!=7 || !col[0][0] || tmp.n>=AR_CAP)goto bad;
            ArRecord* r=&tmp.rows[tmp.n]; unsigned long v;
            if(!ArNumber(col[1],0,65535,&v))goto bad; r->id=(uint16_t)v;
            if(!ArNumber(col[2],10,65535,&v))goto bad; r->motion=(uint16_t)v;
            if(!ArNumber(col[3],16,0xffffffffUL,&v))goto bad; r->hash=(uint32_t)v;
            if(!ArNumber(col[4],10,255,&v))goto bad; r->hits=(uint8_t)v;
            if(!ArNumber(col[6],10,1,&v))goto bad; r->zero=(uint8_t)v;
            if(strlen(col[5])>=sizeof r->damage || !ArDamage(col[5],r->hits))goto bad;
            strcpy(r->damage,col[5]); ++tmp.n;
        }
        if(!end)break; p=end+1;
    }
    if(!tmp.n)goto bad;
    qsort(tmp.rows,(size_t)tmp.n,sizeof(ArRecord),ArCmp);
    free(table->rows); *table=tmp;return tmp.n;
bad: free(tmp.rows); return -1;
}
// -1 ambiguous, 0 absent, 1 known (including known zero power / zero hits).
static int ArFind(const ArTable* t,uint16_t id,uint16_t motion,uint32_t hash,const ArRecord** out) {
    ArRecord key; memset(&key,0,sizeof key);key.id=id;key.motion=motion;key.hash=hash;*out=0;
    int lo=0,hi=t->n;
    while(lo<hi){int mid=lo+(hi-lo)/2;if(ArCmp(&t->rows[mid],&key)<0)lo=mid+1;else hi=mid;}
    if(lo==t->n || ArCmp(&t->rows[lo],&key))return 0;
    const ArRecord* r=&t->rows[lo];
    for(int i=lo+1;i<t->n&&!ArCmp(&t->rows[i],&key);++i)
        if(r->hits!=t->rows[i].hits || r->zero!=t->rows[i].zero || strcmp(r->damage,t->rows[i].damage))return -1;
    *out=r;return 1;
}
static void ArApply(const ArTable* records,AmTable* moves) {
    for(int i=0;i<moves->n;++i){
        AmMove* m=&moves->m[i];const ArRecord* r=0;
        int found=ArFind(records,m->id,m->motion,m->anim_hash,&r);
        m->hits=0;m->record_known=0;m->zero_power=0;m->damage[0]=0;
        if(found==1){m->record_known=1;m->hits=r->hits;m->zero_power=r->zero;strcpy(m->damage,r->damage);
            if(r->hits){m->fact=1;m->attack=1;if(m->kind!='I')m->kind='A';}}
    }
}
