static void PlayerHazardRecoverTick(void) {
    uint64_t player=PlayerChar(), hp=0, max=0;
    const uint8_t* gs=GameState();
    if (!g_cfg_cantdie || !player || !gs || gs[2]) { g_ph_hazard_actor=0; return; }
    if (g_ph_hazard_actor && g_ph_hazard_actor!=player) g_ph_hazard_actor=0;
    if (!PlayerNativeHazard(player) && g_ph_hazard_actor!=player) return;
    if (!PlayerHp(&hp,&max) || !max || !hp) return; // do not revive a native death already underway
    float p[4]; int native=PlayerNativeRecoveryPos(p);
    if (!native) {
        if (!g_tr.active || !g_tr.hasStart) return;
        memcpy(p,g_tr.pp,sizeof p);
        for(int i=0;i<4;i++) if(!isfinite(p[i])) return;
    }
    // RecoverFull clears native hazard/death/controller state (204E038 -> 6028A4).
    // Warp resets the physics object and stream center, then reset the dead camera.
    g_ph_hazard_actor=player; // retain intent if Warp fails after RecoverFull clears native flags
    PlayerRecoverFull();
    int ok=PlayerRecoveryWarp(p);
    if(ok>0) g_ph_hazard_actor=0;
    Log("INFHP RECOVERY native-hazard actor %p source %s pos %.0f %.0f %.0f warp %d",(void*)player,
        native ? "game recovery position" : "round start",p[0],p[1],p[2],ok);
}
