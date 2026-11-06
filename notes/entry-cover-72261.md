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

## Follow-up #72442: false intro release
The full PC Training log contains an explicit off line at 23:48:14.341
(`native intro visible`), after warp at 23:48:14.339, then on at 23:48:18.123.
The gap is 3782 ms. Door opening at 23:48:17.360 occurs inside it. The provided
sheet shows ordinary player-at-door gameplay. This disproves the proposed
EngineFix load-end expiry: its LoadScreen flag persists until an off call.
Mission event-scene IDs are not proof of a visible boss cinematic.

The adapter no longer releases on event IDs. It retains the cover until FIGHT,
fallback actor readiness, cancellation/failure/Return, or the original 90-second
deadline. No per-boss data or EngineFix/BR change. HP lease and button mask stay.
Regression exercises a new event ID, door opening, and native load-state change
without allowing the screen to drop. Cover, native entry, native return and 17
Training harnesses pass; build exits 0. Real action fixtures remain unavailable.
Live gate: repeat the same R3513 video and R25E4 fallback. No claim that visuals
are fixed until that run. Intro frames occurring before the existing FIGHT
callback remain covered; event IDs cannot safely identify them. Verify the boss
intro in the live run and use a verified cinematic signal if it is obscured.

## Follow-up #73087: the 90 s cap fired during a live load
R3513 on TR 1b8b6345 ended the cover at +90 s while GAMESTATE[2] (stop flag 58A)
was still set. LoadStats #4 was 93.3 s. The 90 s cap now yields while that flag
is set, the mission-load sequencer is in state 1..15, or Fight now route is still
1 or 3. A 180 s safety cap always releases. Cancel, failure and Return still
release on the same tick. No row list. The cover still does not write the load flag.

The 84 s idle after loaders finished at +9.3 s is not a cover hold. EngineFix
LoadScreen only sets a draw flag (menu.cpp Nioh3EngineFix_LoadScreen). LoadGates
HkGate returns the original result (loadstats.cpp). XwPreload can keep LoadIsLoadEnd
gate G5 false until character +0x2C8 is set (training.c XwPreload), and that request
is not started or extended by the cover. Loaders had already finished, so that gate
does not explain the rest of the wait. Not proven: what kept 58A set for those 84 s.
