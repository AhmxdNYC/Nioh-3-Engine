// #83530: reset only this page's mutable controls; never invoke travel, fight, refill or delete actions.
// Included inside rematch UI namespace after Items; the caller owns confirmation and row rendering.
constexpr int kItPageDefaults = -69;
bool PageHasDefaults(int page) {
    return (page >= uSettingFirst && page < uSettingFirst + kConfigGroupN) ||
        page == uPlayer || page == uFightOpts || page == uBossVitals || page == uBoss ||
        page == uMoves || page == uMoveCat ||
        (page == uRematch && BossFightItemsAvailable()) ||
        (page == uGate && !WavesGateNow() && GateMode() == 1 && BossFightItemsAvailable()) ||
        (page == uPracticeHub && TrainingExport("Nioh3Training_PracticeFreeze")) ||
        (page == uTools && SpawnedCount() && TrainingExport("Nioh3Training_StanceTimer"))
#if BR_SOULCORE
        || page == uSoulTune
#endif
        ;
}
template <size_t N> bool ResetPracticeStats(const int (&ids)[N]) {
    if (!PstAvailable()) return true; // unavailable controls are not shown
    bool ok = true;
    for (int id : ids) if (PstSet(id, PstDefault(id)) < 0) ok = false;
    return ok;
}
bool ResetPageDefaults() {
    const int page = g_uiPages.page;
    if (!PageHasDefaults(page)) return false;
    bool ok = true;
    if (page >= uSettingFirst && page < uSettingFirst + kConfigGroupN) {
        for (int i = 0; i < kConfigN; ++i) {
            const auto& d = kConfigSettings[i];
            if (d.group == page - uSettingFirst && d.kind != setting_ui::Kind::Manual)
                g_configValues.values[(size_t)i] = {d.initial, true};
        }
        SaveUiSettings(); // same async persistence and restart semantics as an individual edit
        snprintf(g_msg, sizeof g_msg, "Page defaults restored. Startup preferences apply after restarting the game.");
        return true;
    }
    if (page == uPlayer) {
        using Set = int (*)(int,int);
        if (const auto set = (Set)TrainingExport("Nioh3Training_SetPlayerCantDie")) ok = set(1,1) >= 0;
        const int ids[] = {PS_PLAYER_INF_KI,PS_PLAYER_INF_NIN,PS_PLAYER_INF_SOUL,PS_DMG_DEALT,PS_KI_DEALT,PS_PLAYER_SPEED};
        ok = ResetPracticeStats(ids) && ok;
#if BR_SOULCORE
        bosslog::SoulCoreFormsSet(1,nullptr);
        bosslog::FormCoreReset();
#endif
    }
#if BR_SOULCORE
    else if (page == uSoulTune) bosslog::SoulTuneReset();
#endif
    else if (page == uFightOpts || page == uRematch || page == uGate) {
        using Death = void (*)(int,int,int,int);
        using NoFin = void (*)(int,int);
        using Tuning = int (*)(int,int,int);
        if (const auto set = (Death)TrainingExport("Nioh3Training_SetDeathCfg")) set(0,1,0,1);
        if (const auto set = (NoFin)TrainingExport("Nioh3Training_SetNoFinisher")) set(0,1);
        if (const auto set = (Tuning)TrainingExport("Nioh3Training_SetTuning")) {
            if (set(0,6000,1) < 0) ok = false;
            if (set(1,1000,1) < 0) ok = false;
        }
    }
    else if (page == uPracticeHub) {
        ok = PfzCall(0) >= 0; // release freeze only; never Step frame or Reset fight
    }
    else if (page == uTools) {
        g_stSec = 3;
        using Timer = int (*)(int,int);
        if (const auto timer = (Timer)TrainingExport("Nioh3Training_StanceTimer")) timer(0,0); // unarmed shipped state
    }
    else if (page == uBossVitals) {
        if (BvAvailable()) {
            if (BvSet(g_bvSlot,BV_INF_HP,0) < 0) ok = false;
            if (BvSet(g_bvSlot,BV_INF_KI,0) < 0) ok = false;
            if (BvSet(g_bvSlot,BV_HP_PCT,100) < 0) ok = false;
            if (BvSet(g_bvSlot,BV_KI_PCT,100) < 0) ok = false;
        }
        const int ids[] = {PS_DMG_TAKEN,PS_BOSS_KI_DRAIN,PS_ONE_HIT_KI,PS_HOLD_KI_BREAK,PS_HOLD_KI_V2,
                           PS_KI_FLOOR,PS_KI_LOG,PS_NEG_KI_METER,PS_BOSS_SPEED};
        ok = ResetPracticeStats(ids) && ok;
    }
    else if (page == uBoss) {
        const uint32_t chr = SlotChr(g_bossSlot);
        if (!chr) { BossGone(); return false; }
        using SlotSet = int (*)(int,int);
        using GapSet = int (*)(uint32_t,int,int);
        using LastSet = int (*)(uint32_t,int);
        if (const auto set = (LastSet)TrainingExport("Nioh3Training_SetRepeatLast")) set(chr,0); // -1 means successfully stopped, not an error
        if (const auto set = (SlotSet)TrainingExport("Nioh3Training_SetSlotStance")) if (set(g_bossSlot,GateMode()==1?0:1) <= 0) ok = false;
        if (const auto set = (SlotSet)TrainingExport("Nioh3Training_SetSlotAggro")) if (set(g_bossSlot,0) < 0) ok = false;
        if (const auto set = (GapSet)TrainingExport("Nioh3Training_SetRepeatGapChr")) if (set(chr,1000,1) < 0) ok = false;
    }
    else if (page == uMoves) {
        using LastSet = int (*)(uint32_t,int);
        const uint32_t chr = MoveBoss();
        if (chr) if (const auto set = (LastSet)TrainingExport("Nioh3Training_SetRepeatLast")) set(chr,0); // -1 means successfully stopped, not an error
    }
    else if (page == uMoveCat) {
        const uint32_t chr = MoveBoss();
        using Allow = int (*)(uint32_t,uint32_t,int,int);
        using Repeat = int (*)(uint32_t,uint32_t,int);
        const auto allow = (Allow)TrainingExport("Nioh3Training_SetMoveAllowed");
        const auto repeat = (Repeat)TrainingExport("Nioh3Training_SetMoveRepeat");
        for (int i = 0; chr && i < N3_MOVE_COUNT; ++i) {
            const N3Move& m = kN3Moves[i];
            if (m.chr != chr || N3MoveProtected(m.id)) continue;
            const bool sequence = g_moveCat == kMcSeq;
            if (sequence ? !SeqHead(chr,m.id) : N3MoveCat(&m) != g_moveCat) continue;
            if (MoveRepeating(m) && repeat) if (repeat(chr,m.id,0) <= 0) ok = false;
            if (!sequence && allow && MoveAllowed(m) == 0) if (allow(chr,m.id,1,1) <= 0) ok = false;
        }
    }
    snprintf(g_msg, sizeof g_msg, ok ? "Page defaults restored. Session-only controls stay session-only." :
        "Some page defaults could not be applied; check that the boss is still active.");
    return ok;
}
