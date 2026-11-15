# Pacing census: 1% low causes, all 3 devices (#103464, performance-pc/0, 2026-11-14)

Offline, from frame data already on disk; no game launched. Tool: `tools/bench/pacing_census.py`
(PresentMon per frame, EF HITCH lines, or Ally MangoHud). Raw JSON: `D:/nioh3-re/pacing103464/out.jsonl`.

Buckets for the slowest 1% of frames. PresentMon frame: gpu = GPUBusy >= 80 % of the frame, present = in Present()
>= 50 %, cpu = CPUBusy >= 80 %, wait = neither busy (CPU/GPU serialization, sync). A frame at/over HitchLogMs is matched
to its HITCH line (time offset fitted on the big frames) and gets the HITCH bucket: loadscreen (pause/menu), gpu
(GF fence wait >= 30 %), present (W6 >= 30 %), stream (rd >= 8 MB, pf >= 5000 or loads in flight), cpu-busy
(game thread on CPU >= 60 %), cpu-starved (game thread not running). Lost = frame time minus the median frame.
"play" = load-screen frames removed.

## Main PC (RX 9070, 12 threads; harness ini VRR tier 60 cap 60, FG off; public-era EF)

| run | data | avg fps | 1% low | 0.1% low | play 1% | play 0.1% |
|---|---|---|---|---|---|---|
| pjh-96049 N (Boss Match R7B35 load + fight, PJH=0) | PM + HITCH 100 ms, 13/16 aligned | 58.8 | 50.2 | 13.4 | 50.6 | 31.1 |
| pjh-96049 P (same, PJH=1) | PM + HITCH, 14/17 | 58.6 | 48.8 | 10.4 | 49.5 | 25.5 |
| fastab l1m6W (walk tour, 1.8.12) | PM + HITCH, 8/11 | 59.0 | 53.3 | 18.6 | 53.5 | 36.0 |
| wf102935 a12 / fk12, lc101979 r1-off | HITCH 50/100 ms only | 56-60 | >20 | >20 | >20 | >20 |

Slowest 1% of gameplay frames (PM runs, lost ms share): **CPU-bound frames 20-100 ms 91-93 %**, present wait 5-8 %,
GPU 2 %. Load screen adds 18-31 % on top when it is in the window. 12-thread HITCH logs: zero gameplay frames >= 50 ms
(all 23 / 23 / 8 hitches are load screen), so Main PC 1% lows are many 20-40 ms CPU frames, not big stalls; the
100 ms HITCH threshold cannot split them into starved vs busy (needs HitchLogMs 25 next time).

## Main PC 4-core emulation (affinity 0x55; Ally-CPU proxy; 16 runs wf102935, wf103237, c4_102556, dd102727)

| arm group | avg fps | 1% low | 0.1% low | play 1% | play 0.1% | load screen s |
|---|---|---|---|---|---|---|
| public a1/a2 (wf102935, wf103237) | 28-42 | 4.3-4.8 | 1.1-2.4 | 6.0-7.0 | 2.3-3.2 | 123-151 |
| FileLoadPrio+KidstsCap fk1/fk2 | 46-51 | 14.6-14.8 | 5.0-5.9 | 15.2-15.6 | 8.2-9.0 | 17-25 |
| F only f1/f2 | 37-44 | 9.9-10.2 | 2.4-2.8 | 10.6-11.1 | 4.6-4.8 | 42-59 |
| K only k1/k2 | 28-31 | 3.3-4.6 | 1.1-1.7 | 3.6-5.6 | 1.2-2.3 | 86-101 |
| c4 (CamKi pub a / new b), dd (draw dist A / L) | 27-43 | 3.2-5.3 | 0.6-1.8 | 7.6->10 | 1.8-5.0 | 67-166 |

All gameplay hitches >= 50 ms summed over the 16 runs (1011 s lost): **cpu-starved 61 %**, stream 16 %, GPU 14 %,
present 9 %. FileLoadPrio+KidstsCap (#102935) triples the 4-core 1% low (4.8 -> 14.7 fps) and the play 0.1% low
(2.4 -> 8.3 fps).

## PC2 (RX 6650 XT on the slow chipset slot, 5600X; user ini VRR tier 120 cap 120, FG off)

| run | data | avg fps | 1% low | 0.1% low | play 1% | play 0.1% |
|---|---|---|---|---|---|---|
| ret99633 (3 home Returns + play, 11-14) | PM + HITCH 100 ms, 29/29 aligned | 53.2 | 25.1 | 8.4 | 26.4 | 15.8 |
| rc1818 smoke, ls102384 L1-L3 | HITCH 100 ms only | 47-67 | >10 | 6.6->10 | >10 | >10 |

Slowest 1% (load screen 45 % of lost time). Gameplay part: **GPU-bound 46 %**, wait (neither busy) 22 %,
CPU-bound 21 %, present 11 %. Of the slowest 1%, 236 of 270 frames are 40-100 ms; 116 of those are GPU-bound,
49 CPU-bound; all 29 frames > 100 ms are CPU-side load frames. HITCH logs: no gameplay frame >= 100 ms.

## Ally (Z1 Extreme, Linux; MangoHud 100 ms samples, Oct 6-9 builds only: no newer Ally frame data on this PC)

| run | avg fps | 1% low* | 0.1% low* | slowest 1% |
|---|---|---|---|---|
| cvl_1749 (10-09) | 35.0 | 10.5 | 4.0 | GPU >= 95 % load 94 % |
| v1 mod L1 FG wine (10-06) | 30.4 | 13.3 | 9.2 | GPU 54 %, CPU/other 46 % |
| v2 mod (10-07) | 36.6 | 10.3 | 7.9 | GPU 100 % |
| v4 fgoff lightest50 (10-07) | 20.0 | 3.7 | 3.3 | CPU/other 100 % (GPU 89-93 %, load-like 300 ms windows) |
| v5 W6 lightest50 (10-07) | 22.4 | 5.6 | 3.9 | CPU/other 100 % |

*Rows are 100 ms averages, so lows are understated and causes coarse. The live Ally CPU picture is the 4-core proxy above.

## Top causes by frame time lost, and the open.md PERF item that hits each

Main PC: 1) CPU-bound 20-100 ms frames (92 %) -> CPU bundle isolation (open.md l.12) and FileLoadPrio+KidstsCap
(#102935 branch performance-pc/walkfix-102935); 2) load screen -> loading item (l.18) + PJH prewarm (l.11);
3) present wait (5-8 %) -> VrrPresent latency test (l.27).

PC2: 1) GPU-bound (46 % of play) -> culling reuse (l.23), VRS v3 default ON (l.20), shadows every other frame (l.22);
2) load screen (45 % of all) -> loading (l.18); 3) wait/neither busy (22 %, CPU-GPU serialization on the slow slot)
-> CbvMirror v2 no-ReBAR (l.26) and PC2 dispatcher critical path (l.17).

Ally (4-core proxy): 1) cpu-starved main thread (61 %) -> FileLoadPrio+KidstsCap ship (#102935) and weak-CPU gains
(l.28) / TG governor (l.13); 2) streaming (16 %) -> loading (l.18) + FileLoadPoll wait patch (not built);
3) GPU (14 %; old Ally MangoHud says GPU-bound outside lightest preset) -> culling (l.23), FSR4-class upscaler (l.24).

## Limits
- No per-frame data for Ally since 10-09; 4-core Main PC is a CPU proxy, not the Ally GPU.
- Main PC PM runs are at the harness 60 cap: 1% low headroom above 60 is invisible.
- HITCH 100 ms runs cannot split CPU frames under 100 ms into starved vs busy; set HitchLogMs = 25 for the next census.
