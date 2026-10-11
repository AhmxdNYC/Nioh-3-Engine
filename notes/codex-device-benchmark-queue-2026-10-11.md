# Device benchmark delegation - 2026-10-11

User explicitly authorized asking the existing device Claude chats to benchmark.
Requests went through the existing bridge; no Claude CLI/API invocation or wake
toggle. Root retains source/build integration. This is preliminary performance
testing, separate from training research and guard installation-only smoke.

- **pc2 queue1882 / default1761-pc2:** six launches ABBAAB, current USER setup,
  60s gameplay settle and two fixed-camera20s windows per launch. A=EngineFix
  absent; B=exact stable1.7.61/default INI. Same external PresentMon process,
  swapchain and frame metric in both arms. Per-launch means are the units; three
  adjacent paired differences/SE and A-to-A drift. Observed field gameplay required.
- **PC preflight1883:** read-only current headroom/operator/account readiness;
  no launch until clearance. Proposed USER4launch ABBA through pinned_final_pc,
  FilePrompts, SkipCombat. Do not repeat an unchanged insufficient-memory launch.
- **Ally preflight1884:** read-only compatible external-capture/schema/unit/origin
  and restoration recipe. Sampled MangoHud data cannot automatically establish
  per-frame p99. No launch until PC/Ally shared-account clearance.

Pinned ASI SHA256:
4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00.
Pinned INI SHA256:
93CA1930165FB49073E0E4CB291C48164936B2B38041B14EE82486F2766B95B6.
CBN1/CBT1/CBS0/CBF0/FO0/XPRE0; ShaderSwap/ShaderHot0. Inputs outside game
directory; loader, other ASIs, OptiScaler, disk shaders, graphics and cap identical
and hashed. Existing FG-off configuration required; no device KEEP substitution.

Each owner uses its existing lock/launch guard, backs up CURRENT entry saves,
config, ASI/INI/log externally, restores and hash-verifies after every launch,
then releases. Graceful exit only; stop/recover on failure or invalid windows.
No Ghidra, OS/power/driver/env changes or source builds. No mod-clock-only stock
arm because EngineFix is absent. Archive raw captures, exact windows, schemas,
observations and hash restoration evidence. Standing results do not establish
combat/load/latency/visual equality/full-protocol performance.

## Lead checkpoint - 2026-10-11 14:46

- PC preflight1886: NOT READY at5.4GB commit headroom below6GB guard; no
  launch. Holds1891/1920 reserve the shared PC/Ally Steam account for Ally.
  No unchanged launch retry or closing unrelated apps to rescue the test.
- Ally1894/1917: compatible manual route and EngineFix-only removal established;
  entry GHDM/fogpar shaders stay identical in both arms. Exact kit223 default INI
  in B; no Tour/TourList/AutoContinue/LiveCtl substitutions. FG must be observed
  in the actual menu; displayed/rendered equality was only an inference.
- Ally MangoHud is a100ms sampler in both arms. Mean FPS and sampled tail
  statistics are allowed; these are not per-frame p99 measurements. Filename
  local EDT plus elapsed-ns origin and observed window offsets must be archived.
- Queue1919 permits normal automatic save/sync writes within authorized testing,
  with no Cloud/network/OS changes. Owner must first confirm CURRENT-entry
  local file/save restoration after every arm and normal Steam sync completion
  consistent with restored files before another launch/final account release.
  Keep backups and entry Cloud/cache metadata externally. Local hashes and
  cache/sync attestations are different evidence; no remote-server hash claim.
  Conflict prompts, unavailable restoration/sync postconditions or failure mean
  BLOCKED/no further launches, with ownership retained for recovery. READY and
  actual launch/result remain pending; conditional clearance is not a run.
- pc2 queue1882 status requested again by1915; no fresh ACK/result observed
  through1918. Do not infer availability or call it running.
- Device questions must lead with[Q], blockers with[BLOCKED], results with
  [RESULT]. ACK-only metadata does not wake the Codex continuation worker.

User wants substantial measured pc2/Ally performance gains sent to Discord and
the phone through the existing notifier/bridge. Include baseline, scene/config,
paired units/SE, visual/default scope and restoration limits. Static audits,
title-menu FPS and estimated shader savings are not new gameplay gains. Latest
high-level Discord card and phone1916 report no new large gain.

Consume fresh replies after1918 and the lead queues1919/1920; never launch PC
and Ally simultaneously. Stable pinned builds remain unchanged.
