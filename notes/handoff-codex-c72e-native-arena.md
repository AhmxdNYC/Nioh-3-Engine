# RC72E shrine-first diagnosis and private native-start pilot

2026-10-11. User wants the first controllable moment in the boss arena. This is
preparation, **not a validated instant-rematch fix**. Installed game/configuration
and user progress were untouched; recording PID18068 remains reserved.

## Observed run

Installed BossRematch1.4.0 selected `Fight now (in the arena)` for RC72E, Enenra /
Great Tengu / Jakotsu-baba, Sacred Mountain Defiled. At15:46:29 it requested C72E
start335814 (shrine43m away), armed door538668029 and falsely logged that the
mission starts at the fight. It landed15:46:47, requested the gate15:46:48 and
entered BossBattleMode15:46:52 at207073,6026,142372. Zero warps; eventual fight
PASS does **not** prove the first landing was in the arena.

Exact v4 source b2cfe097: `CanArena`1498 accepts a door gate without arena
coordinates; `ExecDirect`685 repeats it. At736 warp=false without arena data,
742 still arms the gate, and764 incorrectly claims a fight start. No actual
spawn override was armed. This was not an unbeaten-boss restriction or an old
version. Label/readiness correction alone does not meet the user's request.

## Native route found

`notes/rematch-fight-index.csv:3093` contains C72E native start334140, flags0/sub0,
207629,6027,142678,yaw40.6. It is4.593m from catalogue boss333550. The catalogue
selector deprioritizes points under8m rather than establishing safe arena starts;
it selected shrine335814 instead. Do not globally remove that filter: close
points can be post-fight/intro locations requiring individual verification.

Read-only raw-script audit found destination207072.90625,6026.490234375,
142371.828125,yaw38.70000076293945: OmazinaiBeforeRTM call0xef64 passes this
tuple; helper Player::SetPos0xedbe applies it. This matches the observed fight
position. Script SHA256:
`c7273a0639ec540df82517a6ff39837aa4efdf194270031e4bc8373a5f0d9302`.
Sequence: gate wait0xef38, RTM36600 request0xef70, load0xef88, play0xefa0,
CreateAndWait0xefac, BossBattleMode0xefcc. No direct script reference to334140 was
found; this static evidence does not prove that requesting it starts the fight.
Explicit post-boss retry334024 differs and has flags0x200.

Root independently checked the raw-script hash at
`C:/Users/Ahmad Hamza/nioh3-engine/data/extract/lfm/archive_09/A7/8EDB00EEC40D7D36314759FACC771E04E11248488F124C47B7A273F2F95F6E`.
Read-only getter audit:3B271C/3B2678 use native334140's exact +40h position
207628.53125,6027.47998,142677.921875 and +4Ch yaw40.61. Gadget adjustment requires
nonzero +0Ch; it is0, so alternate +50/+60 transforms do not affect this path.
Scenario word+110h can instead select DataMgr placement, an unresolved runtime
condition. The CSV positions used for metadata are rounded, not record writes.
Control catalogue hash matches the currently installed table byte for byte.

Current capture logic records the position **before** that intro load, which is
the shrine here. ApplyArenas blindly accepts it when placement333550 matches.
The current session produced such a matching line. Old capture import must be
isolated; otherwise it can override the correct arena target on next launch.

## Concrete prepared pilot

`tools/rematch/prepare_c72e_native_pilot.py` validates exact source records and
creates external A/B catalogues, overlay keys, an empty capture table and SHA256
manifest. It never installs, launches, sends input, edits saves or modifies the
default catalogue. It refuses output below a directory containing Nioh3.exe and
requires a new output directory. Unrelated catalogue bytes, including BOM/EOL,
are retained exactly. B changes only8 cells in RC72E: native start and metadata.

Generated kit: `D:/nioh3-re/rematch-pilots/C72E-native334140-20261011`.
Candidate SHA256:
`4025b610a6babf9e7d3b18f3b3af2aab69c1edc5f2a0baf4519d30ce71895c24`.
Both overlays use FightNow1/Warp0/Spawn0/ArenaCapture0/SelfTest empty and an empty
isolated Arenas file. A retains shrine335814; B requests334140. Keep the existing
door gate armed: setting `atFight=true` would suppress it. No spawn-record writes
or arena warp are allowed in this first pilot. These are overlay keys to merge
into a backed-up INI, **not a replacement full user INI**.

Lightweight AST/data/isolation/mutation/BOM checks:15 PASS. No compiled build,
runtime trial, save-restoration acceptance, first-frame comparison or FPS gain.

## Next controlled validation

After the user finishes recording, use existing guard/lock, fresh entry-save and
configuration backup, and the same installed1.4.0 ASI in A and B. No shared PC /
Ally account overlap. Apply only the pilot overlay and request RC72E manually.
Verify missionC72E/retry334140, actual first controllable position in the arena
**before** the gate request, then correct encounter and combat. Record both
first-land and fight time; F PASS/zero warps alone is insufficient. Native gate /
intro remains; no cutscene-bypass claim. Check death/retry, exit and exact entry
file/save restoration for this benchmark-only trial, separately from user play.
Stop on wrong landing, missing boss, online/co-op, crash or restore failure.

If the native point fails, the exact script destination is the second candidate
for v4's spawn override. That route stays gated by the known SpawnRestore
ownership/partial-write hazard; repair and prove it before adding mutations.
Neither variant is deployed or published. No universal instant-boss claim.
