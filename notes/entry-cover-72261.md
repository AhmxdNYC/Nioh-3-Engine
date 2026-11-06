# Fight now loading cover — 72261

Base origin/cx-pc/door-3rows 71ea9f0b. Human R3513 evidence: request to load completion about 12 s, then about 7 s before FIGHT start. Native-first returned before the old EntryLsBegin path, so Training owned the landing but never held its existing EngineFix entry screen.

The accepted native-entry callback now starts the existing Nioh3EngineFix_LoadScreen presentation. Native and fallback entries retain one cover lease until the fight starts; request/load/landing/door readiness alone cannot release it. A fallback history row opens before actor creation, so it waits for route 4 (live fallback actor) instead. Generic fade settling cannot end the lease. Existing EntryLoadScreen setting remains respected; no EngineFix or BR change is required.

A 90-second wall-clock limit includes all loading, pauses and intro time. Refusal, failed/cancelled entry and return also release. New event-scene IDs, compared with the entry baseline, temporarily reveal the native intro without stopping/clearing any scene; if the scene ends before the fight, the cover resumes. No mission, boss or map list is used.

While covered, controller buttons have a renewable 300 ms mask. A separate renewable 500 ms lease uses the existing player-identity HP damage hook without changing Infinite health or any saved preference. Healing and other actors are unchanged, and the native hazard-respawn bypass retains priority. Cover is refused if the protection hook cannot install. A final 500 ms grace after release covers the next render presentation. Fallback boss hold now treats this cover as not visible, preserving its normal post-cover AI grace. This does not claim all keyboard/analog input is suppressed or that every non-HP grab/hit reaction is prevented.

Offline evidence:
- entry_cover_test.py compiles the actual cover adapter and shared clock policy: supplied native timing, premature ready actor, intro reveal/resume, fallback history-open versus live actor, timeout and tick wrap, refusal/failure/return, unavailable protection, and finite damage grace all pass.
- native_entry_test.py and native_entry_return_test.py pass with added presentation stubs.
- all 17 existing Training harnesses pass; player_hp_entry_test additionally proves cover protection with Infinite health OFF, unchanged preference, and normal damage after expiry. Existing missing arena generator and three extracted action fixtures remain skipped.
- Training build passes. No install, game process, save or game lock touched.

Live gate owned by Claude: on R3513 verify the screen remains continuous from native load through warp/door setup, the actual intro is visible, gameplay appears at FIGHT start, and the player suffers no hidden HP loss or hit/grab reaction (test Infinite health OFF too). Check a fallback row: history-open must not release early, then live actor releases with boss grace. A no-fight case must release by 90 s without stuck input; native return must still work. Capture clip/screenshots and send via the standard Discord media tool. Event-scene identity versus visible cinematic timing, render-frame continuity, and hidden hit reactions remain unproven until that run.
