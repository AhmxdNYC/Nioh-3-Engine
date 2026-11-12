// Read-only discovery subjects. Borrowed actors never enter the owned spawn registry.
#pragma once
enum { META_ACTORS = MAXSP + N3T_SLOTS };
static uint64_t MetadataActor(int i, uint32_t* chr) {
    *chr = 0;
    if (i < 0 || i >= META_ACTORS) return 0;
    if (i < MAXSP) {
        if (i >= g_nsp || !g_sp_actor[i]) return 0;
        *chr = g_sp_chr[i]; return g_sp_actor[i];
    }
    const int s = i - MAXSP;
    if (!g_slot_native[s] || !NativeSlotAlive(s)) return 0;
    // Defensive de-duplication if ownership changes during a session.
    for (int k = 0; k < g_nsp; ++k) if (g_sp_actor[k] == g_slot_act[s]) return 0;
    for (int k = 0; k < s; ++k)
        if (g_slot_native[k] && g_slot_act[k] == g_slot_act[s] && NativeSlotAlive(k)) return 0;
    *chr = g_slot_chr[s]; return g_slot_act[s];
}
