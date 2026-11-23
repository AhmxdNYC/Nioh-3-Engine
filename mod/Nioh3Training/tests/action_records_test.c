#include <stdio.h>
#undef NDEBUG  // zig -O2 defines it; the asserts must run
#include <assert.h>
#include "../src/action_records.h"
#include "../src/auto_moves.h"
#include "../../move_catalog.h"
// human #121846: a real action file with no archive rows (open-world Giant Toad chr 210803, archive_01/98/A3954455...; not in git, looked up in
// data/extract like auto_moves_test.c, skipped when missing) must keep the hit counts of its own hitbox rows after ArApply.
static int ToadCheck(const ArTable* table) {
    static const char* roots[]={"../../data/extract","../../../nioh3-engine/data/extract","../../../../nioh3-engine/data/extract","../../../data/extract","data/extract",
                                "C:/nioh3-engine/data/extract"};
    char p[512]; const char* env=getenv("N3_EXTRACT"); FILE* f=0;
    for(int i=-1;i<(int)(sizeof roots/sizeof roots[0])&&!f;++i){if(i<0&&!env)continue;
        snprintf(p,sizeof p,"%s/lfm/archive_01/98/A39544553277A061AB34F7A3EB9B7A0FBFA834260AC9BA7CBEFD1C69D3E7EF",i<0?env:roots[i]);f=fopen(p,"rb");}
    if(!f){puts("SKIP Giant Toad: action file not extracted");return -1;}
    fseek(f,0,SEEK_END);long L=ftell(f);rewind(f);uint8_t* d=(uint8_t*)malloc((size_t)L);assert(d&&fread(d,1,(size_t)L,f)==(size_t)L);fclose(f);
    static AmTable t;const int na=AmParse(d,(uint32_t)L,&t);assert(na==126);
    int before=0,joined=0;for(int i=0;i<na;++i)before+=t.m[i].hits>0;
    ArApply(table,&t);int after=0;for(int i=0;i<na;++i){after+=t.m[i].hits>0;joined+=t.m[i].record_known;assert(t.m[i].record_known||t.m[i].hits==AmRuntimeHits(&t.m[i]));}
    const int k=AmFind(&t,0xC83);assert(k>=0&&t.m[k].hits==4);  // four hit windows in its own rows
    // human #123773 (668f1f12): 0x4E24 spawns object 47 (20047@174): type byte 1, no hitbox slot (+0x15 FF), no children, no components =
    // proven hit-free (AmObjMayHit 0), so its one own window counts. 20 -> 21 counted.
    const int s=AmFind(&t,0x4E24);assert(s>=0&&t.m[s].nwin==1&&t.m[s].hits==1&&!t.m[s].spawns&&!t.m[s].nohit);
    // human #123773: 0x0C88 slot 1 is a damaging untimed row (element 50) that no object or component names: dead, so its one timed window
    // counts. 21 -> 22 counted.
    const int u=AmFind(&t,0xC88);assert(u>=0&&t.m[u].nwin==1&&t.m[u].hits==1&&!t.m[u].untimed);
    const int z=AmFind(&t,0xC68);assert(z>=0&&t.m[z].attack&&t.m[z].nohit&&!t.m[z].hits);  // human #126246: known 0 hits
    printf("Giant Toad: %d actions, %d joined the archive, hit count known %d before ArApply, %d after\n",na,joined,before,after);
    assert(before==22&&after==22&&joined<=1);free(d);return after;
}
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
    assert(known==11446&&ambiguous==30); // the 1c272daf table (chain-aware regeneration); 11474 / 2 was the older table, hidden by NDEBUG
    char bad[]="boss\taction_id\tmotion\tanim_hash\thit_count\tdamage_per_hit\tzero_power\nx\t1\t2\t3\t2\t10\t0\n";
    assert(ArLoad(&table,bad)==-1&&table.n==11476); // no partial publication
    char overflow[]="boss\taction_id\tmotion\tanim_hash\thit_count\tdamage_per_hit\tzero_power\nx\t65536\t2\t3\t0\t\t0\n";
    assert(ArLoad(&table,overflow)==-1);
    assert(ArDamage("10|20/30",2)&&!ArDamage("10/",1)&&ArDamage("",0));
    AmTable* moves=(AmTable*)calloc(1,sizeof(AmTable));assert(moves);moves->n=6;
    moves->m[0].id=0x4e4e;moves->m[0].motion=1012;moves->m[0].anim_hash=0x0f6bedd8;moves->m[0].kind='M';
    moves->m[0].nwin=3; // the archive record wins over the runtime windows where it is known (a known zero stays zero)
    moves->m[1].id=0xeeee;moves->m[1].hits=7; // stale estimate must not survive an unmatched join (no hitbox windows: unknown)
    // human #121846: an action the archive does not cover keeps the hit windows of its own hitbox rows; a spawned / untimed row keeps it unknown
    moves->m[2].id=0xeeef;moves->m[2].nwin=2;moves->m[2].nrows=3;moves->m[2].fact=1;moves->m[2].attack=1;moves->m[2].kind='A';
    moves->m[3].id=0xeef0;moves->m[3].nwin=2;moves->m[3].nrows=3;moves->m[3].untimed=1;moves->m[3].proj=1;moves->m[3].fact=1;moves->m[3].attack=1;moves->m[3].kind='A';
    // human #126246: a known 0-hit attack keeps nohit; an archive count (chain hits) clears it
    moves->m[4].id=0xeef1;moves->m[4].nrows=0;moves->m[4].block=1;moves->m[4].nohit=1;moves->m[4].fact=1;moves->m[4].attack=1;moves->m[4].kind='A';
    moves->m[5].id=0x4e6e;moves->m[5].motion=1050;moves->m[5].anim_hash=0xe999d024;moves->m[5].nohit=1;moves->m[5].fact=1;moves->m[5].attack=1;moves->m[5].kind='A';
    ArApply(&table,moves);assert(moves->m[4].nohit&&!moves->m[4].hits&&moves->m[5].hits==2&&!moves->m[5].nohit);assert(moves->m[0].record_known&&moves->m[0].zero_power&&moves->m[0].hits==0&&moves->m[0].kind=='M');
    assert(!moves->m[1].record_known&&!moves->m[1].hits);
    assert(!moves->m[2].record_known&&moves->m[2].hits==2&&!moves->m[2].damage[0]);
    assert(!moves->m[3].record_known&&!moves->m[3].hits);
    N3CatInit(1);assert(g_n3fa_n==0); // no hand hit-count fallback, even with legacy names enabled
    N3CatMerge(999999,moves->m,moves->n,"test");
    assert(!N3CatFactFind(999999,0x4e4e)); // a known 0-hit catch is no hit fact (the menu does not list it under '1 hit')
    const N3ActionRecord* record=N3CatRecordFind(999999,0x4e4e);assert(record&&record->known&&record->zero&&!record->hits&&!strcmp(record->damage,""));
    assert(!N3CatRecordFind(999999,0xeeee));
    {const N3MoveFact* hf=N3CatFactFind(999999,0xeeef);assert(hf&&hf->hits==2&&!hf->proj);} // the menu groups it under '2 hits'
    {const N3MoveFact* uf=N3CatFactFind(999999,0xeef0);assert(uf&&!uf->hits&&!uf->nohit&&uf->proj);}  // human #126342: proj reaches the fact   // 'hits unknown'
    {const N3MoveFact* zf=N3CatFactFind(999999,0xeef1);assert(zf&&!zf->hits&&zf->nohit);}    // human #126246: '0 hits'
    {const N3MoveFact* cf=N3CatFactFind(999999,0x4e6e);assert(cf&&cf->hits==2&&!cf->nohit);}
    assert(!N3CatRecordFind(999999,0xeeef)); // runtime hits carry no archive damage record
    const int toad=ToadCheck(&table);
    free(moves);free(table.rows);
    printf("PASS: 11476 rows, 11446 unambiguous aliases, 30 conflicting rows; 3 known examples, 5 pc #41677 drill moves, missing/malformed/overflow, zero power, catalogue propagation, no hand facts, runtime hits for unjoined actions (#121846)%s\n",toad<0?" [Giant Toad file skipped]":", Giant Toad real file");
    return 0;
}
