# Untried optimization ideas (2026-10-15, from notes read-through)
Queued as board tasks 2026-10-15: 1 AABB, 2 TG (all 3 devices). Rest in order:
3 PJH/SPR PSO prewarm: boss-fight hitches; test via Fight-now route, 3 launches cold/warm/off, worst frame + frames>33ms (notes/stutter-pso.md)
4 PP page pool (+PC-only CPU keys MA,P1E7,FPC) on Ally; PP=1 only; then texture time-to-sharp check
5 KL_Div=4 vs 0 (pc2 +1.2%; Ally inconclusive; pc none)
6 RD, M2S, J1 individually vs all-off (Ally, pc)
7 LM=1 load-memory sweeper on Ally (task #46 got 0/4 runs)
8 LC=16 / TSY=6 first-load via fightnow_test.ps1 Yoritomo R7B35, native path
9 MC small-copy memcpy probe (size histogram B94230/B94250); UI-node memo
10 AS0 / R2a / R1 barriers on Ally (check final-readiness B5 first)
11 JT critical-path probe then JG/BPO
12 PB2 / MPC20 shader scalarization: Ally USER capture first
Dropped already: VCLOUD pfu, lodu, mpc/mshlod, FO, fog, compute-cache, ripple decal, DQ/P1E7.
Rules: gameplay gate, same fight both arms, both arms measured, all 3 devices for verdict.
