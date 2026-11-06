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

## Follow-up #73087 / #73112: the cover was blocking the post-load tip
Do not extend the 90 s cap. R3513 and R0C12 both kept stop flag 58A until about
3.8 s after that cap (93.3 s and 93.4 s). Loaders had finished at +9.3 s.

The cover does not write 58A. EntryLs (training.c) refuses to turn the overlay off
while the cover is up, BhScreenVisible reports the screen hidden (boss AI only),
and EntryLsTick returns early. None of those call the load sequencer. EngineFix
LoadScreen is a draw flag. LoadGates returns the original gate result.

What does block the load end: while the cover is up, Training masks every pad
button (MaskGameButtons 0xFFFF). After LoadIsLoadEnd passes, sequencer state 0xB
sets the tip page's load-done byte and waits for ConfirmPressed before
UiLoadingScreenEnd. AutoContinueLoad defaults to 0. CoverTipAllowed was only the
rematch present-cover lease, which this entry screen does not start, so the tip
never got a press until the 90 s cap lifted the mask. The same tail on two
missions is that confirm plus the sequencer, not a second 90 s timer.

The 90 s cap stays. While this cover is up, Nioh3EngineFix_EntryTip answers that
one check. Buttons stay hidden and the overlay stays up. A missing export leaves
the old wait. Harness loads that finished in about 21 s are not proven here;
a machine whose tip is already answered (AutoContinueLoad 1, or the rematch
lease) would not wait for the mask.
