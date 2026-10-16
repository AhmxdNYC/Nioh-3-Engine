# Ally load phases: active LoadIsLoadEnd work vs. stale G9 attribution, the cover, and the tip-confirm wait (2026-10-12)

Static work and log digests only. The game was not run, and no FPS or load-time gain is claimed.

New tool: `py -I tools/loading/ally_load_phases.py <engine.log> [--rematch rematch.log] [--markers seq/index.txt]
[--marker NAME=HH:MM:SS] [--json out.json]`. Fixture test: `py -I tools/loading/test_ally_load_phases.py` (ALL OK).
Every number carries a source tag: `log` (printed by the ASI), `derived` (arithmetic on log values, 0.1 s print
rounding), `rematch log`, `external marker`, or `unknown`. An unknown is never filled in.

## Independent analyzer correction (Codex)

The chronology below reproduces evidence253, but the old causal labels are narrowed:
`work end` means **last observed LoadIsLoadEnd evaluation**, not completion of all
resource work. The remaining credited gate durations and uncredited residual are
wall accounting, not function execution/resource-service timings. Earliest gate
failure does not exclude earlier uncredited evaluations. G9 per-character zero
returns and aggregate counts cannot prove a native pass or its caller; the analyzer
now reports those observations without `pass1` or `no_eval_gap_after_pass1` claims.

A cover interval now needs unique ordered same-token begin, Present ACK, positive
native-post attempt and matching terminal attempt. It is a logged lifecycle interval,
not continuous rendering or native tip-page visibility proof. Generic markers remain
chronological annotations. Explicitly declare a recorded confirm using
`--confirm-marker NAME=HH:MM:SS` or `--markers index.txt --confirm-name TIP-A`.
This classification remains external evidence, not a native confirmation receipt.

Independent replay: `D:/nioh3-re/deliveries/ally-cover-redo-evidence253/` engine.log,
rematch.log and seq/index.txt produce load2 window102.6s, last evaluation+20.7s,
remaining81.9s. Ordered token1/attempt1 ACK/post/terminal at rematch.log29/30/41
split that tail into68.7s,11.1s to external TIP-A00:54:35,2.0s to clear.
171MB of ReadProbe reads remain after its own endpoint; **20.7s is not total
native resource time**, and68.7s is not a measured optimization gain.
Fixtures exercise missing/wrong ACK, wrong end token/attempt, reused token,
overlapping begin, untyped marker and three-G9-evaluation ambiguity. No game run.

## 1. Three probe facts the tool corrects for (sources)

1. **Stale tail.** `FlushEval` (loadstats.cpp:145-157) runs at the clear (LogGates, :552). It credits
   [last eval -> clear] to the last eval's first failing gate. So "holder: G9 LoadCharacterReady 82 s" is time after
   the work ended.
2. **G9 polarity is inverted** (loadstats.cpp:70 `nonzeroPasses = true`). In LoadIsLoadEnd 3B3424:
   - LoadCharacterReady 3B39AC returning 0 just advances the character loop (3B3562 -> 3B3543).
   - The end of the loop returns 1 (3B358A).
   - A nonzero return starts the dt60 ready-wait instead (acc 45C84A4 vs float 3BD4220, 3B356E-3B3588).
   - A logged "G9 failed" is a per-character zero observation; the aggregate does not prove the enclosing evaluator returned true. "0 full-pass evals" is an artefact of the
     inverted polarity.
3. **There is an unhooked check between G8 and G9.** After G8 (3B3506), the eval fails if 46518 && LoaderBusy 3B2970.
   3B2970 tests `[475A420]+248h || +249h`. +249h is manager A busy, written by ResourceLoadIdlePoll 1FEE28 from the
   pending lists +200h/+210h, which is the same layout as LoadStats manager A. The writer of +248h is not
   identified. The tool reports these evals as "uncredited". In the logs they end with "loader A last".

**Mission loads pass LoadIsLoadEnd twice:**
- Pass 1 is MissionLoadStep 223AF50. After it passes, it runs the mission-start setup (3B3B1C, GameMain+654h = 1, ...).
- Pass 2 is state 8, 223C4A8, which goes on to state 9.
- G9 "first-failing evals == 2" is consistent with this path but does not independently identify those passes. Between them, G5 ObjectListAnyNotReady and the loader
  drain hold.

## 2. Per-load results (actual private Ally logs)

| log / load | window | pre-gate | active (work end) | G8 | pass1 -> end | uncredited drain | post-eval tail | log "holder" |
|---|---|---|---|---|---|---|---|---|
| 1.7.19 L1 title->Continue | 115.8 | 30.3 | 63.3 (+93.6) | 46.95 | 16.2 | 1.4 | 22.2 | G8 46.95 |
| 1.7.19 L2 title->Continue | 298.5 | 25.5 | 67.6 (+93.1) | 45.52 | 14.2 | 0.8 | 205.4 | G9 208.12 |
| ev251 #1 title->Continue | 44.5 | 10.3 | 28.5 (+38.8) | 17.26 | 6.0 | 0.5 | 5.7 | G8 17.26 |
| ev251 #2 Fight now 0x1CBC | 66.6 | 4.1 | 16.6 (+20.7) | 11.21 | 5.4 | 2.0 | 45.9 | G9 46.17 |
| ev251 #3 boss intro (script) | 5.7 | 0.0 | 5.7 (+5.7) | 1.90 | 2.9 | 0.7 | 0.0 | G8 1.90 |
| ev251 #4 death/exit in mission | 125.9 | 13.0 | 27.8 (+40.8) | 17.85 | 6.2 | 0.8 | 85.1 | G9 86.98 |
| ev253 #1 title->Continue | 89.6 | 6.6 | 27.1 (+33.7) | 16.24 | 5.9 | 1.0 | 55.9 | G9 57.31 |
| **ev253 #2 Fight now 0x1CBC** | **102.6** | 4.8 | **15.9 (+20.7)** | **11.62** | 4.3 | 1.5 | **81.9** | **G9 82.03** |
| ev253 #3 boss intro (script) | 5.7 | 0.0 | 5.7 (+5.7) | 2.11 | 3.0 | 0.6 | 0.0 | G8 2.11 |

All values are in seconds. After subtracting the stale tail, the largest remaining credited wall interval is **G8 StageAreasObjectsReady**; this is not measured resource-service time.

During the tail, manager A has 0 in flight. Residual reads continue at 100-230 MB over the tail, mostly in its first
5 s (scene streaming behind the page).

### The reported "102.6 s, 82 s G9" Fight-now load (ev253 #2), tail split

| segment | s | source |
|---|---|---|
| set -> work end (last LoadIsLoadEnd eval) | 20.7 | log |
| work end -> BossRematch cover timeout ("Transition timed out", phase 5) | **68.7** | rematch.log:41 |
| cover gone -> TIP-A press | 11.1 | seq/index.txt marker (1 s resolution; external) |
| press -> 58A clear | 2.0 | log |

The seq luma corroborates this: it stays at about 16.5 (the cover) until 54:23.7 and is about 45.9 (the page) from
54:26.6.

In ev251 #2 the same cover stayed up 29.2 s after the work end, until it was cancelled by a menu close.

ev253 #1 and ev251 #1 are title loads with no cover. Their tails are plain tip-page waits; with no press marker they
stay `unknown` as confirm time.

The 3.5 GB memory-pressure hypothesis has no support here: no phase needs it.

## 3. Fight now: why the cover deadlocks (production fix prepared, not applied)

- The cover's `bossReady` (codex-settings-menu-v4 `rematch.cpp:1636`) needs boss actor 65092 present and algo-ready.
- 65092 is created by the 0x1CBC script inside the **second** load. The chain is: BOSSAREABOX 66599 -> StartWarp ->
  Pause::SetFlag(1) -> Player::SetPos to 334650 -> RTM::Request 31648 -> Character::Create 65092 -> Pause::SetFlag(0)
  -> BossBattleMode. The cover can only see 65092 after that.
- That second load starts only after load #2's 58A clear. The clear needs the tip-page confirm (state 0Ch-0Fh,
  notes/auto-continue-load.md). The page is under the cover.
- With `AutoContinueLoad = 0` (logged at start in both runs), the cover can only end at its 90 s deadline
  (`transition_cover.h:16,21`).

Prepared fix: `tools/loading/cover_tip/`.
- `tip_page_state.h`: a passive read of page `[[45C84A8]+158h]`, +71Bh load done, +717h prompt shown. Each offset was
  checked in Nioh3.dump.exe: 1D010E4 / 1CECB24. The vtable is not used, because the ctor 1CD1DA8 stores two.
- `test_tip_page_state.cpp`: MSVC /W4 /WX, ALL OK.
- `cover_tip_release.diff`: against v4 e9941b4. `git apply --check` passes; it is not applied.
- Behaviour: while the cover is waiting for the boss and the native page waits for the confirm, the cover is ended as
  Cancelled ("native request continues"), so the player sees the page.
- **Historical opportunity estimate (ev253): 68.7 s between last evaluation and cover terminal log; removal was not measured.** It is not a load speedup, and the native time is
  unchanged.
- Equivalent without code: `AutoContinueLoad = 1` answers the page through the game's own path. This is a settings
  decision for root.

## 4. Smaller native entry: rejected

0x1CBC has four shipped start points: 67982 (default, row 0), 67971, 68174 and 67064. All are shrines or the default,
and none has flag 0x40 (boss retry) or 0x200. Source: notes/rematch-fight-index.csv:995-998.

- **Entry choice cannot shrink the load.** MissionRequestById 2239600 copies region byte record+15Ch (0x4B) into the
  request. Mission plus region selects the areas. The start point is consulted only in 223B8F0, *after* G8
  (WarpPointFind 137DCC -> grid cell 3A1700 -> streaming centre). The spawn override already moves that same
  position.
- **Death retry and Continue reuse the same path.** Both go through the +14h start point -> MissionRequestById ->
  sequencer 5A29F0.
- **The boss-intro load (#3, 5.7 s) is required.** It is script-owned: Pause::SetFlag 23A763C / StopFlagSet 23A7681
  (by elimination of all 53 StopFlagSet sites; not runtime-proven). It loads the areas around 334650 (30 m deeper),
  RTM 31648 and boss 65092, none of which exist before the trigger. A start point at 334650 would sit outside
  BOSSAREABOX, so the fight would never start.
- **PC comparison:** the historical PC sequence took0.73s; this comparison alone does not identify the Ally bottleneck as streaming speed.

## 5. Next native candidate (one)

**What G5 waits for after pass 1 of the mission load: 4.3-5.4 s per Fight now, 16.2 s on the title load in L1.**

- **Probe:** at the first G9 call (pass 1, MissionLoadStep 223AF50), log:
  - manager A pending/in-flight;
  - while G5 fails, the first object of `[45B9E60]` with +2C8h == 0 (ObjectListAnyNotReady 13335C): its type and
    resource handle, and whether its resource request was issued before or after pass 1.
- **If after pass 1:** these are mission-start spawns whose resources could be requested during G8. That overlap is
  the production fix to design.
- **If before pass 1:** it is the same read latency as G8 (notes §8 RC/SR/AQ), and there is nothing new to do.
