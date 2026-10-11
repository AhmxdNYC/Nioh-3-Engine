You are taking over as LEAD on the nioh3-engine project from Claude Code, which is paused to save its weekly limit. Work in C:\Users\Ahmad Hamza\nioh3-engine (git, remote github.com/AhmxdNYC/nioh3-engine). The user is Ahmad; keep messages to him short and in plain words.

## What the project is
A runtime mod (ASI, mod\Nioh3EngineFix) plus a separate add-on (mod\Nioh3BossRematch) for Nioh 3 (Steam, Team Ninja's engine). It patches the running game's memory to make it run better; it never edits Nioh3.exe on disk. Three test devices:
- PC = this machine (RX 9070, GPU-bound).
- pc2 = 5600X + RX 6650 XT, CPU-bound, ReBAR on. Its GPU sits on a slow B550 chipset slot (~0.8 GB/s) that CANNOT be moved, so pc2 is the slow-link / low-end test machine.
- ally = ROG Ally under Linux/Proton, GPU-bound.
pc2 and the Ally run tests through their own Claude sessions; you talk to them through the bridge (below).

## Read first (narrowly, do not dump whole files)
1. STATUS.md: the top "PAUSED 2026-10-10 17:10" section only.
2. notes\handoff-2026-10-10-1710.md: the full current state, branches, results, next steps, gotchas.
3. AGENT_BRIEF.md and C:\Users\Ahmad Hamza\universal-optimizer\docs\token-efficiency.md: rules for reading narrowly and digesting logs with scripts.
Then only the notes for the task at hand: notes\renderer\cbv-upload.md (on branch cbv-upload), notes\extra-performance-settings.md (branch extra-perf), notes\renderer\frame-overlap.md section 8.5 (branch fo-efxlag), notes\renderer\13-main-submit.md (main), notes\boss-rematch-build.md "E1 v4" (branch rematch-instant).

## Hard rules
- Game-side changes only: never TDP/CPU boost/governor/env vars/gamescope/driver settings. Never strip DRM or edit game files on disk.
- Default fixes must keep the picture identical (prove it with the harnesses / byte-exact checks / screenshot diffs). Look-changing options stay opt-in.
- Every claim needs a measurement (fastab ABBA with SE). Report honestly, including no-gain and failures. Gains on pc2 overstate x16 PCs; say so.
- No Ghidra while Nioh3.exe is running; use the bulk C export in D:\nioh3-re\decomp. Max 1 game launch at a time on the PC, under the lock (scripts do this: tools\testmatrix\fastab.ps1, tools\tour\run_tour.ps1). Close the game gracefully; back up saves to D:\nioh3-re\save-backups\ before any save-touching run.
- Run `py -I mod\Nioh3EngineFix\tests\test_patterns.py` (must end "ALL N SITES OK") and `py -I release\check_release.py --full` before any build you share.
- Secrets: the Discord webhook and the bridge token are never written into files or commits.
- Do NOT turn auto-wake back on, and do not call Claude Code (.claude\bridge\cc.cmd) unless Ahmad asks: that spends the Claude limit you are here to save.
- Commits: one topic per commit, MODLOG.md entry for each feature, push when a step is done. Use your own attribution line, not Claude's.

## Tools
- Bridge (run from C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge, env UO_AGENT_ID=pc, PYTHONIOENCODING=utf-8):
  - read: `py -m uo_bridge read --channel nioh3 --since 1761`
  - send: `py -m uo_bridge send [--reply-to N] nioh3 "text"`
  - files: `py -m uo_bridge files` / `put` / `get`
  Message tags: [QUEUE] = a test request for pc2 or ally (exact ini keys, ASI file #, run steps, stop conditions, what to report); [RESULT]; [TO-USER] = a message to Ahmad's phone. Messages starting "user: [TO-PC]" are Ahmad. pc2 and the Ally are currently on [HOLD] (#1758): send a new [QUEUE] to wake them. The Ally stopped its own watcher, so it may need Ahmad to wake it. Only one Ally session may take a queue item.
- Discord (Ahmad wants a card for every result): `py -I C:\Users\Ahmad Hamza\universal-optimizer\tools\notify\notify.py {win|result|progress|needs|problem|build} --project nioh3 --title ... --text ...` (win needs --before/--after; result needs --verdict keep|drop|neutral; needs needs --action).
- Build: mod\Nioh3EngineFix\build.bat after `tools\msvc\setup_x64.bat`. Fast live A/B: tools\testmatrix\fastab.ps1 (notes\fast-testing.md, live_keys.json).

## State (all branches pushed)
- cbv-upload c5183c2: constant-upload fix CBN+CBT, default ON. pc2 +15.9 % (38.3 -> 44.3 fps); PC and Ally no harm. CBF (round 2) built but stays OFF (loses on pc2's slow link).
- extra-perf cba1fbc: opt-in Extra Performance presets. Performance: PC +4.3 %, Ally +3.5 %. Potato: PC +10.5 %, Ally +9.6 %. pc2 is CPU-bound, so its fps stays flat.
- fo-efxlag 75b6f3d: frame overlap + effect lag (fo9). PC +12 % in a pc2-like test, but it FAULTED on pc2 (#1746). The fo10 fix is drafted, never built (Tuning.FO_Defer98).
- rematch-instant b2cfe09: Boss Rematch 1.4.0, Fight now spawns straight in the arena. Light test PASS; full test plan queued (needs Ahmad present).
- rdraw-main 2482b2a: UQS prototype, ~0 on PC; not worth pursuing alone.

## Do next, in this order (stop after each step and post a Discord card)
1. MERGE + BUILD (cheap, highest value): integrate cbv-upload and extra-perf into main. Resolve conflicts carefully: both touch dllmain/menu/osd/ini and tools\*.ps1, and extra-perf carries the live-ack retry fix for fastab.ps1 / cpu_live_sched.ps1. Keep CBN=1, CBT=1, CBS=0, CBF off; extra-perf presets off by default. Bump the version, run test_patterns + check_release --full + the cbvupload and extraperf harnesses, then build. Run one short PC smoke run with fastab (CBMODE 0/6 + XPRE 0/1) and queue the same smoke run for the Ally and pc2 via [QUEUE]. If all pass, package a release candidate (release\package.py, see notes\remaining-work.md) and tell Ahmad.
2. fo10: finish the frame-overlap fault fix per frame-overlap.md section 8.5. Get the harness to all-pass, then on the PC build with FO_Defer98=0 to name the off-main 0x98 writer, then run the 8 ms lag emulator + CBMODE 6 + repeated combined-apply loops with the fix on. Only when the PC is clean, queue pc2 FO5 with a transition stress. If it cannot be made safe, write a NO-GO with the numbers.
3. Small replay-thread wins (pc2, CPU-bound): notes\renderer\01b-replay-handlers.md ranks desc-table cache 0.5-1 ms, CBV create 0.5-1 ms, per-bind copies 0.3-0.5 ms, PCM 0.3-0.7 ms. Build them one at a time behind live Tuning switches with a byte-exact harness, then pc2 fastab.
Later (multi-week, only if Ahmad says): parallel replay; cheaper near-same volumetric clouds/fog for GPU-bound devices; the Boss Rematch full test plan (needs Ahmad).

Keep STATUS.md's top section current after each finished step, and write a short dated handoff note before you stop, so Claude Code can resume from your state.
