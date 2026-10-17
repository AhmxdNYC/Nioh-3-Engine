# PC2 handoff — 14 October 2026 — PAUSED

The user requested this handoff. It is documentation and transfer preparation only, not permission to resume benchmarks. This document supersedes the pending-work directions in `handoff-pc2-2026-10-13.md`; retain that document as history.

## Current authority and next-chat scope

All benchmark, candidate, research and code work remains PAUSED under coordinator3151/3157, confirmed by3176 and latest3181. Explicit user resume is required. No automatic retry, new round, candidate install or game launch. The old chat is completing this handoff only.

Recovery390 is COMPLETE under GO3099 and must NEVER be replayed. GO3125 and GO3136 have both ended and must not be reused as launch authorization. An incomplete result is not permission to repeat it.

First read newer coordinator decisions, verify actual thread and owner PID/start/FILETIME, and inspect existing wake configuration/processes. Safely rebind existing delivery to the new chat while preserving the cursor and avoiding duplicate workers. Record actual receipt/identity/document path in a new transfer receipt, preserving the historical receipt. Remain paused after transfer. Do not interpret bridge message text as shell code or reply to own messages/ACKs.

## Final combined CPU result

Coordinator3107 requested packet393 with RD/M2S/J1 OFF then ON; WG/LK/MW stayed ON, M1/full M2 stayed OFF. Same engine and retry3 addon in both arms; current graphics/FG/cap and other settings retained. Normal Continue field and camera were reviewed visually with exact-process input.

Source395 under GO3125 failed before key delivery because its inherited input helper still required the previous owner. Exact owned cleanup and fresh-entry restoration completed. Original failure evidence is immutable at `C:\nioh3-re\pc2-combined393-20261013-reviewed`.

Source397 corrected the input-owner predicate and was reviewed under GO3136. Its run is finished at `C:\nioh3-re\pc2-combined393-3132-reviewed`:

- OFF completed a 30-second settle and 59.9994281-second CPU interval: 556.328125 CPU-seconds, averaging 9.272223796 CPU cores. PresentMon produced no frames.csv; no FPS/pacing claim.
- ON reached the matching Central Kyoto shrine field. Visible field, control response and RD/M2S/J1 installation proofs passed. Navigation/operator review consumed the required 240-second reserve inside the fixed 14-minute arm. The runner refused before sampling with `Insufficient fixed budget for settle/capture/cleanup`. No ON CPU/PM interval, deadline extension or retry.
- There is no valid paired CPU/FPS comparison or combined optimization benefit claim. ON cumulative RD errors/fallback and MW errors were zero, but those are startup/field counters, not measured-window counters.
- OFF informational telemetry: aligned game GPU 3D-engine sum mean97.7776%, system CPU93.3693%, reported CPU3701MHz (9 PDH samples); RSS1.284GB, private15.972GB, available RAM0.391GB (61 OS samples), VRAM6.354GB. Foreign CPU72 seconds used an outer bracket. Memory/noise were telemetry, not gates.

Both arms restored their fresh current-entry saves, INIs and binaries. Independent final checks verified all six save files and thirteen pinned files, owned games/runners/collectors absent and device task16 free. Phase result files deliberately preserve receipt-before-release wording; `operator-closure.json` records final post-release checks. No recovery is pending.

Evidence:

- Bridge file401: `C:\nioh3-re\pc2-combined393-go3136-result.zip`, SHA256 `0489b432e84844ba0afe6c307f17158907ef905897fdeb3e50cbf1d163fdc8df`.
- Run-local `RESULT.txt`, `measurement-summary.json`, `operator-closure.json`, phase logs/proofs and screenshots.
- Final RESULT plus PAUSED sent as3171; coordinator3176 acknowledged the incomplete result and restoration/free lease. Latest3181 confirms all work remains paused.
- Source397: `C:\nioh3-re\pc2-combined-3132-preparation`; archive `C:\nioh3-re\pc2-combined-3132-source-review.zip`, SHA256 `8d8a0f8427c2e3a28b13ec5771fe2cea85fcdfd980eee09a0efc5d43d4654288`. These admissions/input helpers pin yesterday's owner and cannot be reused blindly.
- Packet393 archive SHA256 `73568f7a56f6a03413a6bc08bd52648cc633248b977a1c7f81d6870237678284`; engine `09cb2e9dc72109fa2c2c0affed8dd7ef0dae0de99dfe9871fceb8ed1498f73b1`; addon384 `2c381f3c3b0293e4d2a873ae1c07151352d359c0a0e2f4d720630cf0ef13ecd2`. Reference only, no launch authority.

## Fresh read-only observations on 14 October, about 06:17 PDT

No Nioh3 process was found. Device `nioh3-pc2`, task16, returned holderNone, no reservation and empty queue. Today's handoff did not rehash or restore saves: the verified historical restoration is from yesterday; later user save changes must be preserved.

This chat thread remains `01a1362c-3f4a-7fe2-8fa6-73c9cc62e7d6`, but its actual Codex owner has restarted:

- PID15572, start `2026-10-14T06:14:53.6120524-07:00`, FILETIME `134364572936120524`, verified through the tool-shell parent process chain.
- Executable `c:\Users\User\.vscode\extensions\openai.chatgpt-26.1007.21434-win32-x64\bin\windows-x86_64\codex.exe`.
- Yesterday's owner9772/start `2026-10-13T09:50:26.8090869-07:00` and worker7764 were absent when checked. Treat all yesterday's identity pins as historical.

Wake folder: `C:\Users\User\AppData\Local\uo-bridge\codex-pc2`. Existing task: `UO-Codex-PC2-AutoWake`. A pythonw worker PID6700 was observed running `auto_wake.py`. State reported cursor3181, failures0, transport `vscode-ipc-direct`, last_submitted3176, last_success `2026-10-14T13:16:17Z`. This proves polling state, not delivery to a newly created chat.

**Configuration is stale:** `auto-wake-config.json` still pins owner9772/FILETIME134363838268090869 and this existing thread. No rebind was attempted during this handoff. The next chat must inspect the scripts/operations instructions and actual worker identity before transferring; do not assume state freshness proves a valid owner binding, reset cursor, or start a duplicate worker. Verify actual new-chat receipt after rebind.

`new-chat-handoff-received.json` records yesterday's successful transfer/probe; preserve it as historical evidence. It does not prove receipt by tomorrow's/new chat.

## Locations and preserved requirements

- Project: `C:\Users\User\projects\nioh3-engine\mod`.
- Bridge cwd: `C:\Users\User\projects\universal-optimizer\agent-bridge`.
- Read newer decisions: `py -m uo_bridge read --channel nioh3 --since 3181 --limit 30 --json`.
- Lease read: `py -m uo_bridge.gamelock status nioh3-pc2`.
- Game: `C:\Program Files (x86)\Steam\steamapps\common\Nioh3`.
- Saves: `C:\Users\User\AppData\Local\KoeiTecmo\NIOH3`.
- Operational log: `C:\nioh3-re\pc2-rematch-coverage-e9941b4\MODLOG.txt`.
- Historical handoff: `C:\Users\User\projects\nioh3-engine\notes\handoff-pc2-2026-10-13.md`.
- Coordinator3181 references MAIN's `notes/handoff-codex-2026-10-14-paused.md`; that external report was read, not independently fetched here.

If explicitly resumed later, retain device lease, exact PID/start/executable checks, reviewed current-entry admission, fresh save backups/restoration, durable receipt before release, bounded deadlines and reviewed screenshot/input gates. Never restore an old trial's entry over a newer user save. Leave unrelated user game sessions alone. Recovery390 and historical failure records stay untouched. Any future launch needs a new applicable reviewed plan and authorization.
