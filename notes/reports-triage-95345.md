# Report triage #95345 (reports-pc, 2026-11-13)

30 open GitHub reports (#30-#61). Installs: e92e1226 = the owner's own machines (Main PC + Ally, test builds),
844f9b32 = Main PC (older id), 88518576 / b5f5802d = modified exe (image 86773760, known=false), 1f63fe33 / b877a3c3 = outside players.

| Group | Issues | Verdict | Next |
|---|---|---|---|
| A Nioh3.exe+0x1312e0 read 0 | #42 #46 #48 #54 (#56 = same crash as #54, double upload) | Game code. Worker thread frees memory through the 16 MB sub-allocator (vtable 408fcc8) after its destructor 294eee0 cleared the parent at +0x18 (`and [rsi+18h],0`). Same signature on pc2 14 s into a game close (#79309). Logs show 0 game ticks / no render before it, so most likely game exit. No mod frame. | Not fixed. Needs proof that it is exit-only (a window/exit flag in the crash record needs a site schema change, crash.py TOP_KEYS). |
| B +0x31074a / +0x480b12 | #30 #41 #47 #57 | Game player-action chain walker 480a74 (ctrl+0xE8 current action record) copies a {base,u16 start,u16 count} slice that runs past its buffer (b47/b57: entry 157 of 180). Follows a soul-core transform / paired action. No mod write reaches the action controller or records (FormHide writes chr+B440/B444, root+3A8/+248, shell46+C only). Unproven. | Live capture: on each player action change log [ctrl+E8], id, base/start/count at +50/+58/+5A, VirtualQuery(base+(start+count)*8). |
| C stack overflow ntdll | #38 | MOD BUG: Present hook loop. framepacer.dll hooks the DXGI vtable before and again after EngineFix; each calls the other until the stack overflows. | FIXED on reports/38: depth-8 loop breaker calls the real DXGI Present/Present1 read from dxgi.dll on disk (vtdisk.h). tests/vtdisk_test.cpp passes. |
| D +0xfa65d read 0x20 | #44 | 60 ms after menu "Despawn all bosses" deleted a boss that was already dead (HP 0 for 11 s) on the modified exe, where GAUGE force hide also threw C0000005. Game reads actor+0xE8 = null. Suspect. | Repro on stock 1.8.13: kill a Training boss, open menu, Despawn all at once. |
| E +0x1a7581e | #32 #50 | Owner machines, dev/test builds, job worker, no mod frame. | Watch. |
| F startup +0x43xxxx | #51 #55 | Owner test builds, uptime 2-8 s right after InvLog item adds. | Watch; recheck on next public build. |
| old builds | #31 #33 #34 #36 #37 #39 #49 #52 #53 | 1.8.5-1.8.11 or owner test builds; see notes/reports-triage-79081.md for #31 #33 #34; #36 answered (#93559). | Close as outdated when no repeat on 1.8.13. |
| player | #40 favorites | 1.8.12 Ally (owner) - handled #84704. | Check fixed in 1.8.13. |
| player | #43 arena wall, #59 crab, #60 Death Veil | Owner, 1.8.12 / Death Veil candidate BR 9b2dede2 TR f759e0fa. | Development (Death Veil RB24D known bug). |
| player | #45 Extreme preset invisible | notes/report45-extreme-85345.txt | Performance owns (paused). |
| wish | #61 skip waves without 25 % HP | Feature request. | NOTE to pc. |
