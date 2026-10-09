# One mod for every OS: release profiles and `-1 = auto by OS`

Status 2026-10-08. The mod is not tied to a device. Windows behaviour applies to every Windows PC, Linux/Proton behaviour to every
Linux device (Steam Deck, Linux desktops, other handhelds). The old PC / ALLY zips are gone; there is ONE zip,
`Nioh3EngineFix-<ver>[-rc].zip`, built from `release\profiles.json` profile `ALL` (nothing else referenced the PC/ALLY zip names;
`tools\playsession` and `universal-modder` hold no references).

## Defaults (all in the one ini)
| key | default | Proton/Wine | native Windows | measured on |
|---|---|---|---|---|
| `[Patches] W6` | -1 | 1 | 0 | Proton: ROG Ally (main-thread spin gone, fps flat, watts to GPU); Windows: RX 9070 PC ~0 |
| `[Patches] GF` | -1 | 1 | 0 | Ally ~0.6 core of fence spin; PC abba2 run6 +0.38 % (< +1 % keep bar) |
| `[Patches] MQ` | -1 | 1 | 0 | bit-exact, CPU power only; PC abba2 run5 no gain; handheld run still open (provisional) |
| `[Tuning] W3_TimeoutMs` | -1 | 3 | 1 | cpu-hotspots-ally.md row 4 (pump 890 loops/s at 1 ms); not A/B tested (provisional) |
| `[Latency] MaxFrameLatency` | -1 | 1 | 0 | shipped in Ally zips since 1.1.0; on Windows the game's own 1 is already lowest |
| `[Patches] EP`, `SR` | -1 | on | off | already auto since 1.7.23 / 1.7.28 |
Off everywhere: B, PP, AS0, WorkerCap, LoadOnlyCpuFixes, Barriers, AutoContinueLoad (reasons in profiles.json). Shaders GHDM
(0x0104840c) and fogpar (0xa9910db8) ship in `optional\shader_dx12\` of the one zip: both are DXIL, OS/vendor neutral; GHDM measured on
Ally and PC, fogpar (bit-exact) on the Ally only.

Handheld/power-limited opt-ins (documented, not defaults): `WorkerCap = 10` (job-system.md WC: 16-thread APU pools 127 -> 78 threads; costs
width on a 6c/12t desktop, and the Deck at 8 threads is already <= cap, so it is a no-op there); `PP = 1` (CPU-bound desktops, visual gate open).

## Implementation (branch `osauto`, not merged)
`A()` in `LoadConfig` (dllmain.cpp) and the Latency read in osd.cpp resolve `-1` with `wine_get_version` (same test as SR/EP). Startup log
prints `OS: ...` plus one `auto <key> = <v> (<OS>: <why>)` line per resolved key. The mod menu shows `ON (auto)` and Save writes `-1` back
while the value is unchanged. `package.py` refuses an ASI without that log string (a `-1` would read as ON on an old ASI).

## Proposal for the src owner: more keys that could become `-1`
| key | proposed auto rule | evidence |
|---|---|---|
| `L1OnWine`, `FGOnWine`, `W3WineSleep` | already Wine-only by construction; leave | - |
| `PP` | Wine 0, Windows 1 once the texture-sharpness gate passes | PC +5.3 % one window; weak at 16-30 fps (pc-keep-candidates s3/s5) |
| `WorkerCap` | `-1` = on (cap 10) only when Wine AND logical cores >= 16 AND the machine reports a mobile APU/battery (GetSystemPowerStatus) ; else off | job-system.md WC: -64 threads on Ally; "never on the 7600X"; Deck is already <= cap |
| `W3_TimeoutMs` | keep Wine 3 until an Ally A/B; if flat, 1 and drop the auto | cpu-hotspots-ally.md row 4 estimate ~0.07 core |
| `GpuOpt Barriers` | stays 0 on Windows (-7 %); Wine 2 (installed, off) for traces only | gpu-barriers.md, results_pc_1716.md |
| `B` | no auto: fps-neutral both sides, not reproducible | abba 2026-10-08 |
