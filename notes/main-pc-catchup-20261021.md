# Main PC catch-up brief (written 2026-10-21 ~11:00 EDT, read-only research, game untouched)
Sources: .handoff/{pack,goal,open,owners,next,checkpoints,done,needs-player,untried-ideas,user-directives,gxl-pc-pause-22241,yoritomo-*}.md,
docs/code-map.md, mod READMEs/headers, installed files in Steam\Nioh3. "UNVERIFIED" = could not confirm from files on disk.
Goal (goal.md): make Nioh 3 run better on PC (RX 9070), PC2 (RX 6650 XT), ROG Ally via game-side mods only; CPU -10..20%, fps, loads, boss rematch/training. Honest numbers only.
User directive #22272/#22277 (today): DO NOT touch the game; read all docs and catch up.

## 1) What each mod does
- **Nioh3EngineFix.asi** (v1.7.73, built Oct 19; loaded via ASI loader; patches memory only). Performance mod: CPU waits/spin fixes (W6, W3, L1, RD, MW, WK, WG...), GPU fence/barrier/shader tweaks (GF, R1, R2a, PJ shader-warmup journal), frame-gen/VRR, FPS overlay, loads, plus a mod menu.
  Menu: F10, or Esc x3, or controller triple-tap Start; saves to Nioh3EngineFix.ini. Most keys are in [Patches]; most new ones default 0, "v2.02 only" (exe TimeDateStamp 6AA7C106 = known build).
  Key INI keys: MW, VRR, FG, PP (page-pool skip, +2.9% PC, only solid CPU win), M1/M2, J1, WG, TG (30 Hz logic tick; changes sim step/input, tap lasts ~33 ms; measured only ~3% fps, not the remembered 26%), LK, PJ (shader journal), RD, M2S, LM (load-memory sweeper).
- **Nioh3BossRematch.asi** (add-on): the game has no rematch; this adds "Boss Matches" (menu text, catalogue Nioh3BossRematch_bosses.tsv, 41 cleared missions scanned), Fight now (FightNow=1, FightNowWarp=1, FightNowSpawn=1), Return/Fight-again gate object, boss/move menus, repeat-move rows. Menu: F7 or pad gesture. Calls Training via API (Nioh3Training_FightNow/_Preload). Source: mod/Nioh3BossRematch.
- **Nioh3Training.asi** (RE-probe turned feature host): natives for spawning/despawning bosses (CreateDynamic), in-place arena swap + cross-world Fight now, ReturnGate, idle stance (Practice = boss idle; Boss Matches should fight), per-boss move lists/sequences/"Repeat move" drill (gap 500 ms), Aggro me, infinite health (PlayerCantDie=1), Ki-break gold bar / no-finisher (KiBreakNoFinisher=1, HP0 stagger), BossLoot/DeathBlowRequired/CutsceneOnKill toggles (0), BodyFade, F9 freeze + BUG HERE marker. Pipe/command-file interface (Nioh3Training.cmd). Practice menu rows: Train here, Training Mods, Summon a boss where I am, Boss Matches.
  NOTE: Training source is NOT in this checkout (mod/ has only Nioh3BossRematch and Nioh3EngineFix); it lives in worktrees, e.g. ../nioh3-native-edges-15620/mod/Nioh3Training (branch cx-pc/native-edges-15620) -- UNVERIFIED that no other copy exists.
- docs/code-map.md = chart of what regions of Nioh3.exe are for (RTRig codegen 22%, menus 12%, etc.); background only.

## 2) What is installed now (C:\Program Files (x86)\Steam\steamapps\common\Nioh3)
| file | sha256[:8] | mtime | matches |
|---|---|---|---|
| Nioh3EngineFix.asi | ac69d486 | Oct 19 01:35 | engine 1.7.73 "built Oct 19"; source commit UNVERIFIED (no note maps ac69d486). Older notes/pack cite engine 09cb2e9d (Oct 13 combined build) = STALE |
| Nioh3BossRematch.asi | a13ef0ed | Oct 20 01:07 | seqfix build from pc/seqfix 325f968a (checkpoint: "Rematch A13EF0ED"); installed by #15602 |
| Nioh3Training.asi | 7144514e | Oct 20 02:48 | cx-pc/native-edges-15620 @ 11cd27a1 (SHA 7144514e650c...), installed 3:08 am EDT by #15602, game relaunched |
Game running: Nioh3.exe PID 26532 since 2026-10-20 9:20 PM (user session; logs written up to 10:56 today). Training log shows a boss (chr 16102 Yoritomo) idle at HP 38222/42525 for ~13.5 h.
INI toggles (Nioh3EngineFix.ini): TG=1, WG=1, RD=1, M2S=1, J1=1, LK=1, MW=1, VRR=1, FG=1, PP=1, PJ=1, M1=0, M2=0, LM=0 (matches pack: RD/M2S/J1/WG/LK/MW=1, M1/M2=0; TG also on). RD and M2S are on in the ini but not in this build (per gxl pause note; the log does print "RD totals" so RD actually runs -- contradiction, see 6).
Rematch ini: FightNow/FightNowWarp/FightNowSpawn=1, Overlay F7, Pipe=1, DirectStart=0. Training ini: CrossWorld=1 (+EarlySwap, Prewarm, Preload), Return LoadScreen=1 ReturnGate=1, KiBreakNoFinisher=1, DeathBlowRequired=0, CutsceneOnKill=0, BossLoot=0, PlayerCantDie=1, AutoRetryOnDeath=0, GoldKiBar=1. Nioh3Training_learned.ini holds [boss 3EE6] good=0 bad=0xFA6.
**seqfix (325f968a)**: 325f968a itself is NOT an ancestor of 11cd27a1, but 11cd27a1 contains the seqfix content commits (32de9608 repeated sequence plays game's own pieces, 111fcc61 same-family follow-ups, 91cfe289 merge) and the Rematch hash a13ef0ed equals the seqfix Rematch. So seqfix content is IN by ancestry; the 325f968a-vs-11cd27a1 diff is only 17 lines (idle_hold.h, move_toggle.h, training.c). Live-proven? NO (checkpoints: "2 live checks outstanding, native AI not proven").
**#14877 Boss-Matches-spawn-idle (b52c451d)**: b52c451d IS an ancestor of 11cd27a1, so the installed Training 7144514e should contain it. BUT pc's last note (next.md ~1851, 2026-10-21) says "idle-gate #14877 NOT installed; waiter install_idle_14877b.ps1 (Training only, 84F5D68C) running". No install_* waiter process exists now, and installed hash is 7144514e, not 84F5D68C. Treat as: probably in the binary, NOT confirmed. Training.log has 16 "IDLEHOLD adopted" lines incl. 21:26:21 (chr 16102, "Training Idle stance") -- cannot tell from logs whether that was Practice (expected) or a Boss Match. Needs a Boss-Match fight check (boss must attack).

## 3) Work in flight / ownership
- Fleet state: .handoff/PAUSED is DELETED in the working tree (uncommitted; HEAD still has the Oct 20 7:18 pm Yoritomo-pause text). Shared pause intentionally not created by #22241, so other fleets (pc, cx-pc, pc2, ally) are NOT globally paused.
- This Grok lead (gxl-pc / gx-pc workers): PAUSED itself at 10:49 am EST by #22241 (write-up gxl-pc-pause-22241.md, HOLD #22247). No new optimization work until human says resume. No injector. No game launch/close. Yoritomo training search STOPPED (#21839; write-ups yoritomo-search-paused-21591.md, yoritomo-training-stop-21839.md).
- Findings to date: Yoritomo 173 moves/88 attacks/6 sequences (4E53>4E54, 4E74>4EB7, 4E7A>4E8D, 4E7C>18D1, 4E93>4E5B, 4EB8>4EB9); 0 frame data, 0 grab/unblockable labels. Optimization: 73 picture-safe hooks, 20 on, 13 of 64 measured, only CBN (pc2 +15.6%, flat on PC/Ally) >2%; 10 graphics-API hooks vs 54 Nioh-address hooks; next-game injector "unlikely without looking at the game".
- Owners per older notes: main-PC install/relaunch owner = pc (Claude, uo-agentd) -- but #15180/#15198 say Claude has no credits; routing was Codex-only, then #21323 Grok (gx-pc) for RE, Codex out of credits until 2:32 am Oct 27. Current effective actor on this PC: gxl-pc/gx-pc (paused). Game lease: none held ("No game lease" in all latest owners lines). Do not install/relaunch/close the game (user is playing).
- Branch now: pc/fightnow-swap-wire (many uncommitted/untracked captures; .handoff has uncommitted edits).

## 4) Open user requests (user-directives / open.md), not done
- #14877 confirm Boss Matches fight normally (see 2). #16296 include multi-hit 4EA5 in Sequences; #16278/#16376/#16332 fix repeat Aggro me, multi-hit vs sequence classification, dead boss hanging after repeat -- code on cx-pc/training-16389 (a238066c), runtime unproven, not known to be installed.
- #14655 remove death/self-kill + twitch moves from selection (built in b52c451d lineage; live check pending).
- #20999/#21433 one automatic algorithm for move/sequence classification for all bosses (sorter built on gxl-pc/move-sort-20999, not wired into menu).
- #14153 SF6 frame-meter overlay (blocked: no frame data found). #14742/#14751 native Yoritomo AI logic (partly superseded by the stop #21839).
- Old open.md items still unchecked: first rematch load 37 s (#4114), Return time ~51-61 s (#9608), cross-world black screen (#9140), summon-anywhere (#4747/#5335), soul cores (#6092), live arena switching (#6466), menu clarity passes (#13293/#13296/#13537), object menu mode-aware (#13262), pause-in-menu (#13274), Return object always ours (#9664/#13799), HP bar timing (#9685).
- #14922 TG OFF vs ON benchmarks on Ally and PC2 -- status UNVERIFIED (Ally on battery hold until #16601 lifted it for CPU test only).
- #21834: user will do the Yoritomo practice block test himself. #22241: wait for human "resume".

## 5) Unproven / needs-player (needs-player.md, all main PC)
Combined hosted-menu build play-check; no-finisher/gold-Ki toggle (#7465); lsfade cross-world Fight now; b950cb2 (HP0 stagger, Return loading screen); Return gate shows only our menu (no native "End mission?"); Ki-break death pose (5675ecc8); multi-boss build (2-3 summons, per-boss pages); Aggro me during drill; filtered Yoritomo moves (Attacks 24 / Unnamed 71 / Animations 76; Sequences 12); Boss Matches Fight now attacks at once while Practice starts idle. Also: PJ on-vs-off pair never run (hitch to beat 167 ms; journal 64 MB, Oct 11); TG gameplay feel is user's call; seqfix + native sequence playback; 30 Hz real-throttle proof ("85 ticks/s is not proof").

## 6) Contradictions / stale
- pack.md and old Owners line cite engine 09cb2e9d + addon 2c381f3c and "game PID 28240"; installed is ac69d486 / a13ef0ed / 7144514e and PID 26532. Stale.
- Owners says "pc = LEAD, Claude manager" vs directives #15180 (Claude out of credits) and #21323 (use Grok, not Codex/Claude). pc's last note (next.md 1851) exists but pc status UNVERIFIED.
- #14877: pc note "not installed, waiter for 84F5D68C" vs installed 7144514e whose source ancestry includes b52c451d; waiter not running.
- gxl pause note says RD/M2S "not in this build", yet EngineFix.log prints "RD totals ... wakes 174618801". RD appears active; note is likely wrong or refers to a different key set.
- Checkpoints "seqfix 325f968a ... Training 4E2A62A2" was superseded by Training 7144514e (cx-pc build); 325f968a is not in the installed build's history even though its content is.
- .handoff/PAUSED deleted locally vs "STATE: ACTIVE ... pause = .handoff/PAUSED file"; the file's last committed text is the Yoritomo pause. Pack says "Do not create the shared PAUSED file".
- open.md has checked items (rematch death arena, PC2 recorder) next to still-open duplicates (first-load, Return time); open.md/needs-player.md are uncommitted edits. needs-player lists "build b950cb2 not installed yet" -- ancient (Oct 17), now superseded.
- Boss in Training log sits at 38222/42525 HP idle for 13+ hours (session left running); TG/EngineFix log "game ticks 95.7/s vs rendered 83.9/s" with logic tick cap 110 -- TG throttling to 30 Hz not evidenced there.
- Nioh3Training.ini has stale Arenas path in Rematch ini (D:\nioh3-re\rematch-pilots\...); Nioh3BossRematch_verified.tsv is referenced but not present in the folder (UNVERIFIED if needed).
