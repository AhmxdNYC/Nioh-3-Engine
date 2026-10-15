# Boss HP bar for CreateDynamic bosses + Nuribotoke dump (bridge 4768)

2026-10-14/15, Ally (Proton, 1920x1080 fullscreen/gamescope). Evidence /tmp/hpbar-4768 (screenshots g3 g4 m1 m2 q6, final-training.log).

## Root cause (runtime code dump, rdump command)
- ShowBossGauge 1D115D4 -> BossGaugeSetup 1D05824 -> AddBossGaugeRow 1CE7C6C(hud, id): FindWrapper 135B78(charMgr, id) = binary search of the sorted placement registry (c0+0x48, rows {id u32, wrapper*}), wrapper[0] = live actor, skip if null/dead (4892A8); row+4 = id, name text id = wrapper+0xD4 else actor->charParam+0xA0.
- A CreateDynamic handle (0x1000xxxx|slot) is NOT a registry id, so the gauge found nothing (or the owner placement's own actor, e.g. the Oni of Rajomon). That is why swapped/summoned bosses had no bar.

## Fix (training.c, FakeGauge*)
- Hook FindWrapper 135B78 (12-byte prologue 48 8B 09 45 33 C0 44 8B DA 48 85 C9). Private id 0x7FFF0001 returns a copy of the owner's wrapper with [0] = the spawned actor and +0xD4 = 0.
- After CreateDynamic (spawn/summon/arena), GaugeTick waits until the actor is in the table (~20 ms), then ShowBossGauge(0x7FFF0001). Hide on boss end/despawn. Commands: fgauge <handle> [owner], unfgauge.
- Live result: Ibaraki Doji (summon REA75 in Rajomon) and Nuribotoke (summon R401E in mission 0x401E) show the name + HP bar; the bar tracks HP (kill -> bar empty).
- Open: HideBossGauge (1C208BC, a queued event) does not remove the bar after the boss dies (same as with the old handle). Frame stays, empty, until the next mission/area load. Raw 1CEC2E0(hud,0) and ShowBossGauge(0) did not hide it either.

## Nuribotoke native diff (mission 0x401E, native load, then act 4087191)
- Registry holds 222 rows (placement 4087191 = Nuribotoke, char 392871, live=0 until activated). 26 actors in table before, 27 after `act 4087191` (boss live at 180761 23861 217416). `dump` run before and after: DUMP lines 115.. and 837.. in final-training.log (registry wrapper words, +0..0xFF of every actor).
- Summon in this mission: owner 4080158 (live registered actor) found at once, boss hostile, bar OK.

## HideBossGauge fix + death-sensor re-verify (bridge 4801, 2026-10-15, Ally)
- GaugeForceHide (training.c): the native HideBossGauge (queued event) is still called, then by hand: clear row vector hud+0x3C0 (1CEE8C8), hide the 3 row elements (type 0xF5) and 42 detail elements (0xF6) via 300F44 + 5D1D8C(elem,0), HUD hide 1CEC2E0(hud,0); repeated 700 ms later. hud = [[45C84A8]+0xA8]. Command: hidegauge.
- Live (Yoritomo R7B35, fnow, Proton 1920x1080 fullscreen): kill -> boss dead -> bar fully gone (k4.png); before: empty frame stayed. Evidence /tmp/hpbar-4801 (f1 bar shown, k2 bar at kill, k4 gone, s5/r1 gameplay).
- Death: player died at +48 s, actor left table -> 'actor gone, not dead' path, boss mode ended, no victory sting, bar hidden, no stale bar after Retry. After retry fnow again: win -> 'boss dead' -> victory path.
