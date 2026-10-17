# Boss-rematch loading-screen bypass: patch notes for pc (from pc2)

2026-10-14, pc2, read-only RE and design. No mod source edit, build, game launch, save or game-folder access (a bench was running).
This builds on `notes/boss-only-load-pc2-3271.md` (Task C), `notes/rematch-fast-restart-2026-10-13.md` and the training notes on
`origin/codex-combined-training-pilot`. It does not repeat their RE.

**Evidence tags.**
- **[D]** seen in a decompile or asm export: `C:/nioh3-re/bridge-files/task3271-decomp/*.c`, `decomp_batch1_*.txt` (B1), or
  `loader-decomp2.tgz` (13 C files plus `loader2_asm.txt`, cited as A:line).
- **[L]** seen in a pc2 live log from today: `C:/nioh3-re/task3886-boss-load/addon_launch1.log` (L:line), addon 1.4.13.
- **[N]** from a prior note, not re-checked here.
- **[I]** inferred.
- **[R]** reported by the requester and not found in any repo/bridge file on pc2. This covers the *training scriptcreate in-place
  restart 4.6 s on pc*.

## 1. Verdict

1. **Inside the loader, nothing can be bypassed safely.** Every dispatcher state and every step of MissionLoadStep either waits on a
   readiness predicate or creates mission state that later code reads (section 3). Forcing state 6, faking `LoadIsLoadEnd`, or
   skipping 223B8F0's area scheduling all leave a mission without required state. This stays rejected, as in Task C rank 5.
2. **Top recommendation: do not enter the loader at all for a repeat fight in the same arena.** Re-run the boss-start coroutine in
   place, under the same live mission script owner, by replaying the native call
   `94B04(manager, "sub_event_boss_battle_start", 0, owner)` that the game itself made at first entry. Ship it as
   `[Rematch] InPlaceRestart = 0` (default off), with strict gates and a fallback to the normal native request. The intro, gate
   rights, `Character::Create` and `BossBattleMode` all stay script-owned. The add-on only re-issues one native launch with
   arguments it captured from the game.
3. **Ship alongside, low risk:** `[Rematch] FastGate = 0` (default off). It removes about 5.5 s of the add-on's own post-load
   settle before the gate-right request [L]. It also needs an `AutoContinueLoad` check (section 5.3).
4. **The size of the possible gain on pc2 [L].** For Yoritomo R7B35, request to `BossBattleMode` takes **99-123 s**:
   - mission load 50-68 s;
   - add-on settle about 5.5 s;
   - script intro window 30-48 s.

   In-place restart removes the mission load and the settle. The predicted ON time is about the intro window plus 1 s. This is a
   prediction, not a measurement.

## 2. Where the time goes today (pc2, task 3886, Fight now with spawn override, no tip-page press)

| run (L) | request | first frame after load | load s | gate right asked | intro window (58A) | Create 375545 | BossBattleMode | total s |
|---|---|---|---|---|---|---|---|---|
| R7B35 A (L:29-47) | 11:28:37.752 | 11:29:45.981 | 68.2 | 11:29:51.499 | 11:29:52.26 -> 11:30:40.22 = 48.0 | (in window) | 11:30:40.224 | **122.5** |
| R7B35 B (L:95-117) | 11:37:33.371 | 11:38:36.939 | 63.6 | 11:38:42.451 | 11:38:43.21 -> 11:39:13.6 = 30.4 | 11:39:10.873 | 11:39:13.489 | **100.1** |
| R7B35 C (L:236-258) | 11:52:00.207 | 11:52:50.754 | 50.5 | 11:52:56.267 | 11:52:57.03 -> 11:53:39.5 = 42.5 | 11:53:37.632 | 11:53:39.444 | **99.2** |
| REA75 Ibaraki (L:170-187) | 11:46:01.074 | 11:46:35.569 | 34.5 | (no gate; box) | none: the fight begins at landing | 11:46:38.084 | 11:46:38.730 | **37.7** |

- **Repeating the same row is not faster than a cold load [L].** Runs B and C repeat A. So no scene residency survives a
  mission-request reload today. This is consistent with the owner drain `1C10570 -> 1C08D1C` [N].
- **The native in-mission reload is faster but still long [L].** L:119-122 shows a reload of 24.3 s (11:39:41.8 -> 11:40:06.1).
  It is probably the death retry; the cause was not logged.
- **These pc2 numbers are about 3-5x the earlier pc figures** (16.3 / 19.6 s, `rematch-fast-restart:9`). A concurrent CPU-task
  owner was coordinating on pc2 (`CPU3915-COORDINATION.txt`). Do not compare pc against pc2. Use ABBA order on one machine.
- **The add-on adds about 5.5 s per Fight now [L]** between the first frame and the gate request:
  - a 3 s landing settle (`rematch.cpp:941`);
  - `g_gateDue = t + 1000/2500` (`:958`);
  - then `GateTick` waits for the native gate poll.

## 3. (a) Stage list: dispatcher 5A29F0, 223B8F0 and MissionLoadStep 223AF50

### 3.1 Dispatcher `5A29F0`: state at controller+4, mirrored to global `45BA21C`

| st | handler | what it does | skippable? | state it must leave |
|---|---|---|---|---|
| 3 | `223B3F8` | Applies queue head to the mission tuple: +0 -> state+10 journey, +14 -> +24 mode, +4 -> +28 mission, +C -> +14 retry point; sets st 4 [N training-queue-consumer-proof] | No: every later reader uses the tuple | `[MissionMgr+8]` +10/+14/+24/+28 = head |
| 4 | `223B8F0` | Stream centre and area scheduling (3.2) [D B1] | No | Areas scheduled for the cell; controller+20 region ref; st 5 |
| 5 | `223AF50` MissionLoadStep | Polls until resident, then sets up the mission (3.3) [D] | No | Per-mission char/emaki/coop state; GameMain+654 = 1; st 6 |
| 6 | `223C53C` | Waits on another predicate, acts, then st 7 [N] | Unknown: body not exported | - |
| 8 | `223C4A8` | `LoadIsLoadEnd` pass 2, `219F3A0` idle, then st 9 [N loading-ally-phases s.1, auto-continue-load] | No: G1-G9 readiness | All characters/objects ready |
| 0B | `223BE08` | If head+47 tips byte: `TipPageSetLoadDone` 1D010E4 (+71B); `UiLoadingScreenEnd` [N auto-continue-load] | The page is skippable natively (AutoContinueLoad AC1); the state is not | Tip page "load done" |
| 0C-0E | `223BEA8` | Waits for the tip page to close (slot 2B +2C == 3) [N] | Yes, through the game's own confirm path (AC1 thunk) | Page closed |
| 0F | `223C308` | Clears G+58A after 30 (or 100) dt60 units [N] | **About 0.5-1.7 s fixed timer [I]**; not worth a patch | 58A = 0: the loading cover ends |
| 13 | `223AE90` | After timer 30.0, `223568C` retires the queue head if controller+11 [N] | No | Queue advanced |
| 14 | `5A2A90` | Next-request processing [N]; not exported | - | - |

**The fight-now request bytes** (queue head `45BC420`, slot 0) are seen in the log (L:243 `req[58]`) and read through the
decompile offsets [D+L]:

| offset | value | consequence |
|---|---|---|
| +4 | mission 0x7B35 | |
| +8 | region 0xB1 | |
| +C | start 374664 | |
| +14 | mode 2 | |
| +40..4F | `01 01 00 00 00 00 00 01 00 01 01 00 00 00 00 00` | see the next four rows |
| +42 = +43 = +45 = 0 | | 223B8F0 takes the default `3A1700 -> 9181D8 + 1FCECE4` path |
| +47 = 1 | | The tip page is armed in state 0B |
| +49 = 1 | | `8B9640(region)` writes stage+11F1 |
| +4C = +4D = 0 | | No loading-title text override |

The request copies the region byte record+15C. **The start point does not select a smaller area set**
[N loading-ally-phases s.4].

### 3.2 State 4 `223B8F0` [D B1:9-213]

1. Returns until `3B3A28(stage,0,-1)` (G8 `StageAreasObjectsReady`) passes and `61A914(0)` is nonzero (B1:46-58). The prior area set
   must be settled first.
2. Writes stage+0x530 = 1 (`puVar5+0x14c`, a dword index) and `*stage = 1`. Calls `3B3B1C`, `723C7C(0)` and `2239C3C` (B1:63-70).
3. Gets the start position.
   - If CharMgr+48 -> +110 == 0, `137DCC(head+C)` -> `3B271C`. This is the record the spawn override rewrites.
   - If head+18 is set, it also calls `1C203EC`.
   - Otherwise the position comes from a DataDb+188 record (B1:73-102).
4. Optional loading-title text ids, then `226601C`, `1FD5974` (B1:103-121).
5. Unless head+45 is set, the cell is `3A1700(region, pos)`, then area scheduling `1FCECE4` / `1FCEE1C` / `1FC4208`. MissionMgr+11
   = region (B1:122-143).
6. Region record -> controller+20 (B1:144-172); `3301BEC(..+16B6A0, 1)` (B1:173).
7. For region rows with flag 2, `217D624(ClearDb, id, 1)`. Region 'K' plus mode 1 sets clear flag 0xC1D9 through `1C1DB88`
   (B1:174-204). **These are persistent writes [D]; a bypass that skips st 4 changes save-visible flags.**
8. Mode 1: `1C08C70`, `1C2A784`; `A62110`; st 5 (B1:205-211).

### 3.3 State 5 `223AF50` MissionLoadStep [D 0x223af50.c]

| step | lines | what it does [D] (meaning [I]) | skippable? |
|---|---|---|---|
| gate | 35-38 | `1FC6674(stage+0x500)` and `LoadIsLoadEnd` pass 1; otherwise returns and stays in st 5 | No: G1-G9 readiness [N] |
| S1 | 39-44 | `3B3B1C` (mission-start setup, also called in st 4); **GameMain+654 = 1**; `3301BEC(..,0)` (pairs with st 4's `,1`) [I: a load-mode toggle]; `1CF7054` | No |
| S2 | 45-57 | Saves the emaki region byte CharMgr+2BF4; `1C252E4` [I: retry manager; it sits next to SetDecideRetry 1C251C8]; `E472E8`; clears coop slots +2FC8[2], +2FCC = -1; clears +305D | No: stale coop/retry state |
| S3a | 58-74 | No mission (head+4 == 0): region 0xC0 special `2219230` on the ClearDb slot; `702BBC` | n/a for rematch |
| S3b | 75-90 | `702CC0(mission)` [I: mission flag table, beside SetMissionFlag 70275C]; 0x6F24 special; build list `1BF64A4`; `20E50E8(head+20 params)`; **`MissionSetupEmaki`** | No |
| S4 | 91-102 | `1C29768(CharMgr+2820)`, `1C27898(CharMgr+2860, mission, 1, host)`; not emaki and host: `107C5F0(ClearDb, mission)` | No: per-mission character/clear state |
| S5 | 103-144 | Emaki only: ref +2AA8, `1C083B0`, `9D8AA8`, `21616A0`, `29D5B0`, `9E8CEC`, `E5A740(CharMgr+2BD0)` | n/a for story rows |
| S6 | 145-164 | `1CA4F8C` [I: gadget/rights, beside RightsRequest 1CA52C4]; coop slots rebuilt from `Net_GetCoopPlayerCountMinus1`; list freed | No |
| S7 | 166-176 | Region byte: CharMgr+26C5/+26C7, `1C28EC8`, `A619F0`; mission mode 1: `1C2A180` | No |
| exit | 177-179 | controller+8 = 0, +4 = 6, global = 6 | A forced jump omits S1-S7 |

**Where the wall time sits [N loading-ally-phases].**
- Pass 1 to the end is 4.3-5.4 s on Fight now loads, held by G5 `ObjectListAnyNotReady`.
- The largest credited pre-pass interval is G8 `StageAreasObjectsReady`.
- MissionLoadStep's own body is a one-frame setup. **Hooking or skipping it saves nothing measurable.**

## 4. (b) What an in-place restart (no loader) must keep or re-establish

| item | owner today | in-place requirement | how v1 handles it | evidence |
|---|---|---|---|---|
| Scene, collision, navmesh, placements | Loaded mission; owner drain only on teardown `1C10570` | Must still be resident: no load since capture, owner not being removed | Gates: same load generation; owner+134 == 0, +142 == 0 (no `mission_end`) | D A:1233-1272 (E22D10 reads +134/+142) |
| Mission tuple and queue | `[MissionMgr+8]` +10/+14/+28; queue `45BC420` | Unchanged; **no request pending** | Refuse if any slot has mission != 0 or byte+8 != 0 | D/L |
| Clear flag and clear db | Clear coroutine `sub_event_boss_clear_check` | Not rewound. Re-setting already-set flags on a later win [I idempotent] | No flag writes by the add-on | N rematch-fast-restart:63 |
| Loot and rewards | Clear coroutine plus drop natives (BOSSLOG observes only) | No double grant from one kill. The old clear task must be **finished** before re-entry. Ground drops picked up | Gate: the clear-task slot was released by the native tick (`95244` called from `282320`), plus `InPlaceSettleSec` quiet after the last reward/flag event | D A:911-922; N :71 |
| Old start/clear coroutines | VM manager `owner+8`, map at +40 | Same-name launch replaces the start node (`94D80 -> 95244`). Clear is a different name, so it must already have ended | As above; never call 95244/282320 ourselves | D A:741-797, 854-865 |
| Boss actor | Placement wrapper; `1180740` / `669110` | Create must allocate a fresh live boss. If wrapper[0] still holds the dead object, `669110` skips allocation | **Unverified: v1's main risk.** Watchdog W4 (section 5.2). No despawn/HP writes | N training-spawn-contract:40-46 |
| Boss gate / door rights | Start script `WaitWarpStart` / `EndWarpRights`; add-on `GateTick` | A fresh rights grant per generation | Re-arm the `g_gate*` state per in-place generation (the retry-generation fix) | N rematch-fast-restart:31,37 |
| Camera, fade, control | Start script: Pause::SetFlag, fade, RTM, SetPos | Script-owned: the same intro runs again | Nothing written. Watchdog: if pause/58A sticks, fall back to a native exit | N loading-ally-phases s.3 |
| Player state (HP, ki, elixirs, position) | Native retry respawns at the retry point | **Not restored in v1 [I]**: the player keeps post-fight state; the intro SetPos moves them | Documented limitation; no HP writes (policy) | N |
| Spawn override record | Add-on (1.4.x branch) | Untouched: there is no load | The existing restore on mission exit still applies | L:177, L:244 |
| Native stop flags | GameMain +588..+58D | The game's own `mission_end` launch avoids +588/+589/+58C/+58D | Require all six bytes +588..+58D == 0 before the call | D A:1240-1264 |
| Online / coop | - | Solo/offline only | Existing `Gates()` | - |

**Hard blocker to settle in phase 0 [L, old session].** A replay of mission 52 queued a native return **about 20 s after
BossBattleModeEnd** (`notes/session-2026-10-08-gamedata.md:115`, caller +22398BD).
- If the boss clear task itself queues the return, the queue-empty gate always refuses a post-win restart. The add-on must not
  delete queue records, so v1 would then have **no admissible trigger state**.
- The pc training run (4.6 s, [R]) must say which state it restarted from (post-win, post-death, or mid-fight). Port exactly that
  state.

## 5. (c) Patch notes

The base is `main`, addon 1.3.3. The installed 1.4.13 lives on a codex branch (`origin/codex-combined-cover-training`, with spawn
override / SPAWN sites). Apply the same hunks there. Regenerate `sites_gen.h` with `tools/derive_sites.py`, then run
`tests/test_sites.py`.

### 5.1 Sites (`sites.json`; group `INPLACE`; bytes from A, executable SHA256 `ce9dec99...eb02` [N])

| name | kind | RVA | head | pattern to verify | ABI |
|---|---|---|---|---|---|
| `ScriptLaunchNamed` | hook | 0x94B04 | 7 | `48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 70 20 55 48 8D 68 A1 48 81 EC C0 00 00 00` (A:741-747) | See below |
| `fn:ScriptLaunchNamed` | fn | 0x94B04 | 0 | same | `bool(__fastcall*)(void* mgr, const char* name, uint64_t, void* owner)` |
| `ScriptCtxReplace` | hook | 0x94D80 | 5 | `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 49 8B F9 48 8B DA 48 8B F1 E8` (A:854-861) | RCX = node slot wrapper (node+40); the new context goes to [RCX+8] |
| `ScriptVmRelease` | hook | 0x95244 | 6 | `40 53 48 83 EC 20 48 8B D9 48 8B 49 08 48 85 C9 75 06` (A:911-916) | RCX = slot wrapper; old ctx [RCX+8]; return address = `sp[0]` |
| `chk:TaskTickRelease` | check | 0x28241A | - | **pc must extract**: E8 at 28241A..282433 -> 95244 [N :71] | Defines the completion-caller range |

`ScriptLaunchNamed` ABI, seen in asm:
- RCX = manager, equal to `*(owner+8)` (E22D10 loads RCX = [RBX+8] at A:1239 and calls at A:1272 without changing RCX).
- RDX = `const char*` name (E22D10 passes `lea rdx,"mission_end"` at A:1266).
- R8 is overwritten before any read (A:749).
- R9 = owner, saved to RSI (A:761) and passed as the 5th argument to 94D80.
- Returns AL; E22D10 tests AL.
- All the heads are whole instructions with no rip-relative operands. The existing thunk in `hooks.cpp` (volatile GPRs and
  xmm0-5 saved) fits.

### 5.2 Code (pseudo-diff)

```diff
--- a/mod/Nioh3BossRematch/Nioh3BossRematch.ini
+++ b/mod/Nioh3BossRematch/Nioh3BossRematch.ini
@@ [Rematch]
+; Fight again (no load), experimental. After a WIN in a Fight now arena, 'Fight again' re-runs the boss's own start script in place
+; (the native call the game made at first entry). No mission load. Solo/offline; rows in InPlaceRows only. Falls back to the
+; normal Fight now when any gate fails. The player keeps current HP/items. 0 = off (default).
+InPlaceRestart = 0
+InPlaceRows = R7B35
+InPlaceSettleSec = 8
+; 1 = after a Fight now load, request the boss gate's right as soon as the boss start polls it (no fixed 3 s + 1-2.5 s settle).
+FastGate = 0
--- a/mod/Nioh3BossRematch/src/rematch.cpp
+++ b/mod/Nioh3BossRematch/src/rematch.cpp
@@ Install (near :1361)
+    g_cfg.inPlace = IniInt(L"Rematch", L"InPlaceRestart", 0) != 0;
+    g_cfg.fastGate = IniInt(L"Rematch", L"FastGate", 0) != 0;
+    g_inplaceOk = g_cfg.inPlace && GroupOk("INPLACE", &g_inplaceWhy) && InstallGroup("INPLACE", &g_inplaceWhy);
+    if (g_inplaceOk) { OnEntry("ScriptLaunchNamed", &ip::HkLaunch); OnEntry("ScriptCtxReplace", &ip::HkReplace);
+                       OnEntry("ScriptVmRelease", &ip::HkRelease); }
@@ Tick(): load edge (near :1131)
+    if (s.st[2] && !g_wasLoading) ++g_loadGen;          // any load invalidates every captured owner/manager/name
@@ TrackReplay landing (near :941, :958)
-            else if (t - g_loadDoneAt > 3000) {
+            else if (t - g_loadDoneAt > (g_cfg.fastGate && g_fight.load() ? 300 : 3000)) {
-                    if (g_gateWant.load()) g_gateDue = t + (g_fightWarp ? 2500 : 1000);
+                    if (g_gateWant.load()) g_gateDue = t + (g_cfg.fastGate ? 0 : (g_fightWarp ? 2500 : 1000));
     // GateTick still requests only after g_gateSeen (the native poll); FastGate removes only the add-on's fixed delays
@@ Tick() request dispatch (near :1192)
+            else if (kind == kReqAgain) ip::ExecAgain(row, s, t);   // pipe 'again <row>' and menu 'Fight again (no load)'
+++ b/mod/Nioh3BossRematch/src/inplace.cpp   (new, about 250 lines)
+namespace ip {
+struct Cap { uint64_t mgr, owner; const char* name; uint32_t ownerId, mission, loadGen; ULONGLONG at; bool ok; };
+Cap g_start;                       // the game's own launch of the boss start, captured in this load generation
+uint64_t g_clearSlot; std::atomic<int> g_clear{0};  // 0 none, 1 created, 2 released by the native task tick
+std::atomic<bool> g_replaying{false}, g_pendClear{false};
+void HkLaunch(uint64_t mgr, uint64_t name, uint64_t, uint64_t owner, const uint64_t*) {
+    if (!g_fight.load() || g_replaying) return;
+    char n[64]; if (!SafeStr(n, name)) return;                  // SEH read, max 63 chars
+    RLog("IP launch %s mgr %llX owner %llX", n, mgr, owner);    // phase-0 trace: every name in the fight mission
+    if (!strcmp(n, "sub_event_boss_battle_start")) {
+        uint64_t m8 = 0; uint32_t id = 0;
+        if (!Rd(owner + 8, m8) || m8 != mgr || !Rd(owner + 0x120, id)) return;
+        g_start = {mgr, owner, (const char*)name, id, CurMission(), g_loadGen, Ms(), true};  // name ptr: game-owned string
+    } else if (!strcmp(n, "sub_event_boss_clear_check")) { g_pendClear = true; g_clear = 1; }
+}
+void HkReplace(uint64_t slot, ...) { if (g_pendClear.exchange(false)) g_clearSlot = slot; }
+void HkRelease(uint64_t slot, uint64_t, uint64_t, uint64_t, const uint64_t* sp) {   // hot path: two compares
+    if (slot == g_clearSlot && InRange(sp[0] - g_img.base, kTickRelLo, kTickRelHi)) g_clear = 2;
+}
+void ExecAgain(int row, const Snap& s, ULONGLONG t) {
+    const Row& r = g_rows[row];
+    if (!g_inplaceOk) return Refuse(g_inplaceWhy);
+    if (!InList(r.id)) return FallbackFightNow(row, "row not in InPlaceRows");
+    if (!(g_fight && s.ms28 == g_fightMission && s.ms14 == g_fightStart)) return FallbackFightNow(row, "not in this row's mission");
+    if (s.online || s.guest || s.st[2] || s.st[3] || s.st[4] || !s.player) return Refuse("online / loading / paused");
+    if (!QueueEmpty()) return Refuse("a native request is pending (e.g. the game's own return)");
+    if (!g_start.ok || g_start.loadGen != g_loadGen || g_start.mission != s.ms28) return FallbackFightNow(row, "no live capture");
+    if (!OwnerLive(g_start)) return FallbackFightNow(row, "owner removed/ending");  // +8 == mgr, +134 == 0, +142 == 0, +120 == id
+    if (fight::g_active || !LastFightEndedByWin() || g_clear != 2 || t - LastRewardEvt() < SettleMs())
+        return Refuse("the last fight / its clear has not finished");
+    if (!StopBytesClear()) return Refuse("game stop flags set");                      // GameMain +588..+58D all 0
+    ReArmGate(r, t);   // g_gateSeen=false, g_gateReq=false, g_gateSlot=-1, g_gateWant=gateId, g_gateDue=t, g_fn=kFnWarped, deadline 180 s
+    g_clear = 0; g_clearSlot = 0; ++g_ipGen; g_ipAt = t;
+    bool al = false; g_replaying = true;
+    __try { al = ((bool (*)(uint64_t, const char*, uint64_t, uint64_t))F_LAUNCH)(g_start.mgr, g_start.name, 0, g_start.owner); }
+    __except (EXCEPTION_EXECUTE_HANDLER) { al = false; }
+    g_replaying = false;
+    Log("Rematch: AGAIN %s gen %d: 94B04(start) -> %d", r.id, g_ipGen, al);
+    if (!al) { Result("again FAIL %s: native launch refused", r.id); return; }       // no fallback after a call; user decides
+    Result("again started %s", r.id);
+}
+// Watchdogs on Tick while g_ipGen is active:
+// W1 door rows: no RightsCheck poll (g_gateSeen) within 15 s           -> FAIL "start did not wait for the gate"
+// W2 no Character::Create(r.placement) entry within 120 s of the rights -> FAIL
+// W3 no BossBattleMode within 180 s, or 58A/pause stuck > 120 s        -> FAIL + CallReturn() (native exit = the loader owns recovery)
+// W4 BossBattleModeEnd < 10 s after BossBattleMode (dead-boss reuse)   -> FAIL, latch InPlace off for the session, log reward events
+// On BossBattleMode: Log("Rematch: AGAIN %s request->fight %.1f s") and FnVerify PASS as for Fight now.
+}
```

Fallback (`FallbackFightNow`) = today's `ExecDirect(row, s, t, true, true)`, which is always available. No queue rewrite, flag
write, HP write, forced readiness, despawn or coroutine deletion is in the patch.

### 5.3 No-code lever to check first

The fight-now request carries tips byte head+47 = 1 [L+D], so the tip page is armed.
- With `AutoContinueLoad = 0` (pc2's setting), state 0C-0E can wait for a confirm, up to 68.7 s on the Ally
  [N loading-ally-phases s.3].
- The task 3886 loads ended without a press. Whether the page waited at all is unverified.
- Log the 58A clear time against the `TipPageSetLoadDone` time with `AutoContinueLoad = 0` and `= 1`. The difference is free time.

## 6. Live test plan (pc; one machine; ABBA; nothing else running)

**Setup.**
- Save backup and restore as in `tools/rematch/fightnow_test.ps1` / task 3886 `run.ps1`.
- `[Debug] RematchLog = 1`, `[Profiler] LoadStats = 3`, `LoadGates = 1`.
- Row R7B35 (door gate). Control row REA75 (box; fight starts at landing).
- **Timer:** the pipe `fight` / `again` reply (QPC) to the first frame where `fnow = pass`, field = 1, loading = 0, paused = 0,
  player = 1, plus 1 m of stick movement. This is the `measure.py` `ready_seconds` / `seconds` definition.

**Phase 0, trace only (InPlaceRestart = 1 with `ExecAgain` compiled out, so only the observers run).**
One Fight now, win, wait 60 s, then exit. Required outputs:
1. Every `IP launch` name. **Confirm `sub_event_boss_battle_start` actually goes through 94B04.** If it does not, stop: v1 has no
   capture point.
2. The clear-check slot release with a return address inside 28241A..282433.
3. Whether a native return request is enqueued after the win, and when relative to the clear release (the section 4 blocker).
4. The boss wrapper state after death: does Create on replay allocate? Only answerable in phase 1.

**Phase 1 (InPlaceRestart = 1).** Fight now -> win -> `again` -> win -> `again` -> lose (native retry) -> exit.

| gate | pass condition |
|---|---|
| G-speed | Median request-to-control for `again` is **<= 0.5x** Fight now OFF for the same row (n >= 3 each, ABBA order A=OFF, B=ON). Predicted pc2 R7B35: about 31-49 s vs 99-123 s. pc, if training is representative [R]: about 5 s |
| G-boss | Exactly one `Character::Create 375545` entry, then BossBattleMode. The boss visibly has full HP (screenshot). No W4 |
| G-phases | The fight reaches the boss's phase 2 or the kill. BossBattleModeEnd only after a kill |
| G-rewards | Per win, BOSSLOG reward/flag events equal the first native win's set. No grant without a kill |
| G-control | No W3; the player regains control after the intro; the camera behaves normally (screenshot after the intro) |
| G-retry | After a loss, the native retry still works (the gate re-arms) |
| G-exit | `exit` returns to the field; the spawn-override record is restored and verified (existing log line) |
| G-off | With `InPlaceRestart = 0`, the binary behaves byte-for-byte like the baseline: no INPLACE hooks installed (log), same Fight now timeline within noise |

**FastGate A/B.** FastGate 0 vs 1, R7B35 Fight now, n >= 3 each, ABBA. Gate: the first-frame-to-gate-request interval drops from
about 5.5 s to <= 1 s, and the gate intro/fight still PASSes in every run.

## 7. Risks and what is unverified

- **[R] The 4.6 s training measurement.** No artifact on pc2. The trigger state, script name and owner source are unknown here.
  If pc's training used a different name or owner path, port that path instead of section 5.2's capture.
- **[I] Dead-boss reuse.** If the dead boss's wrapper object persists, Create does not allocate (`669110`). BossBattleMode then
  starts on a dead boss and the clear could re-run. W4 limits this to one event, but a reward can already have been granted.
- **[L/N] Native auto-return after a win** (old session, mission 52). It may close the only trigger window (section 4).
- **[I] Thread and phase.** The pump (PerFrameTickCall 56529C) is assumed to be the game thread and outside the `282320` iteration
  of manager+40. That is not proven. A nested call could mutate the tree during the traversal.
- **[N] Unexported bodies.** `94550` (Script::Create) is not exported here, so the manager/owner chain is taken from E22D10 and the
  notes. Also unexported: 223B3F8, 223C53C, 223C4A8, 223BE08, 223C308, 5A29F0 and the 28241A call bytes. Their rows in section 3
  are [N].
- **Player state.** HP/elixirs are not reset in place, so restarts are not equivalent to a native retry. This is documented and
  there are no HP writes.
- **Rejected.** Skipping or forcing any dispatcher state or MissionLoadStep step; deleting queue records; calling 95244, 282320,
  E22D10, ResetReceivedDead or DeleteGroup directly.
