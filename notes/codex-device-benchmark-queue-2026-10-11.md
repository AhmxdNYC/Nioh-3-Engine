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

ACK, results and blockers are pending at bridge1884. Consume fresh replies rather
than infer device availability; never launch PC and Ally simultaneously.
