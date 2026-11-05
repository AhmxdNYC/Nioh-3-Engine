static void HgTick(void) {
    const uint8_t* gs=GameState();
    if(!gs || gs[2]) {memset(g_hg,0,sizeof g_hg);return;}
    for(int s=0;s<N3T_SLOTS;s++) {
        HgRecord* r=&g_hg[s];if(!r->pending) continue;
        uint64_t a=g_slot_act[s],st=0;
        if(!a || a!=r->actor || !InSp(a) || !Rd64(a+0x3A0,&st) || st!=r->stats ||
            r->generation!=g_hg_generation[s] || HgSlot(a)!=s) {memset(r,0,sizeof *r);continue;}
        if(GetTickCount()-r->since>2000) {
            r->pending=0;
            Log("HAZARD recovery timeout: removing actor %p without kill/reward",(void*)a);
            if(g_tr.active && ActorByHandle(g_bm.id)==a) {FightFinish(FL_QUIT);TrainRetry("native hazard recovery timeout");}
            else for(int i=0;i<g_nsp;i++) if(g_sp_actor[i]==a) {Despawn(i);break;}
            continue;
        }
        if(g_slot_sp_ok[s] && BossPlace(a,g_slot_sp[s],g_slot_sp[s][3],"native hazard recovery")) {
            r->pending=0;Log("HAZARD recovered boss %p without death/reward",(void*)a);
        }
    }
}
