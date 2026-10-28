#include <stdio.h>
#include <assert.h>
#include "../src/action_records.h"
#include "../../move_catalog.h"
int main(int argc,char** argv) {
    assert(argc==2); FILE* f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    char* text=(char*)malloc((size_t)size+1);assert(text);assert(fread(text,1,(size_t)size,f)==(size_t)size);fclose(f);text[size]=0;
    ArTable table={0};assert(ArLoad(&table,text)==11476);free(text);
    const ArRecord* r=0;
    assert(ArFind(&table,0x4e6e,1050,0xe999d024,&r)==1&&r->hits==2&&!strcmp(r->damage,"250/275"));
    assert(ArFind(&table,0x4e4e,1030,0xd049c7ec,&r)==1&&r->hits==3&&!strcmp(r->damage,"85/85/240"));
    // pc #41677 accuracy run: a zero-damage catch is 0 hits (zero_power kept), two windows of one hit id are 2 hits, and only the
    // continuation the move really plays is added (4E5C > 4E60 > 4E62; the AI-request links 4E4E > 4E52 / 4E55 > 4E59 are not)
    assert(ArFind(&table,0x4e4e,1012,0x0f6bedd8,&r)==1&&r->hits==0&&r->zero&&!strcmp(r->damage,""));
    assert(ArFind(&table,0x4e59,1011,0x457ecdff,&r)==1&&r->hits==2&&!strcmp(r->damage,"175/180"));
    assert(ArFind(&table,0x4e5c,1020,0x6d9cd061,&r)==1&&r->hits==3&&!strcmp(r->damage,"140/150/205"));
    assert(ArFind(&table,0x4e4e,1000,0x1af425df,&r)==1&&r->hits==1&&!strcmp(r->damage,"200"));
    assert(ArFind(&table,0x4e55,1010,0x44487b20,&r)==1&&r->hits==1&&!strcmp(r->damage,"250"));
    assert(ArFind(&table,0x4e4e,1050,0xe999d024,&r)==0); // same action id is NOT an identity
    assert(ArFind(&table,0x1f8f,265,0x8a13868a,&r)==-1&&!r);
    int known=0,ambiguous=0;
    for(int i=0;i<table.n;++i){ArRecord* x=&table.rows[i];int found=ArFind(&table,x->id,x->motion,x->hash,&r);known+=found==1;ambiguous+=found==-1;}
    assert(known==11474&&ambiguous==2);
    char bad[]="boss\taction_id\tmotion\tanim_hash\thit_count\tdamage_per_hit\tzero_power\nx\t1\t2\t3\t2\t10\t0\n";
    assert(ArLoad(&table,bad)==-1&&table.n==11476); // no partial publication
    char overflow[]="boss\taction_id\tmotion\tanim_hash\thit_count\tdamage_per_hit\tzero_power\nx\t65536\t2\t3\t0\t\t0\n";
    assert(ArLoad(&table,overflow)==-1);
    assert(ArDamage("10|20/30",2)&&!ArDamage("10/",1)&&ArDamage("",0));
    AmTable* moves=(AmTable*)calloc(1,sizeof(AmTable));assert(moves);moves->n=2;
    moves->m[0].id=0x4e4e;moves->m[0].motion=1012;moves->m[0].anim_hash=0x0f6bedd8;moves->m[0].kind='M';
    moves->m[1].id=0xeeee;moves->m[1].hits=7; // stale estimate must not survive an unmatched join
    ArApply(&table,moves);assert(moves->m[0].record_known&&moves->m[0].zero_power&&moves->m[0].hits==0&&moves->m[0].kind=='M');
    assert(!moves->m[1].record_known&&!moves->m[1].hits);
    N3CatInit(1);assert(g_n3fa_n==0); // no hand hit-count fallback, even with legacy names enabled
    N3CatMerge(999999,moves->m,moves->n,"test");
    assert(!N3CatFactFind(999999,0x4e4e)); // a known 0-hit catch is no hit fact (the menu does not list it under '1 hit')
    const N3ActionRecord* record=N3CatRecordFind(999999,0x4e4e);assert(record&&record->known&&record->zero&&!record->hits&&!strcmp(record->damage,""));
    assert(!N3CatRecordFind(999999,0xeeee));
    free(moves);free(table.rows);
    puts("PASS: 11476 rows, 11474 unambiguous aliases, 2 conflicting rows; 3 known examples, 5 pc #41677 drill moves, missing/malformed/overflow, zero power, catalogue propagation and no hand facts");
    return 0;
}
