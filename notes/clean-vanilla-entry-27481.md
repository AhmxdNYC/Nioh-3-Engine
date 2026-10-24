Offline findings for scope27474 (no game touched). Verdict: no proven native Yoritomo entry exists locally; best viable route = keyboard/mouse runner (tools/pc2_enter_game.ps1 + ui.fsx, mod-free) -> Continue -> shrine menu Battle Scroll/Mission Replay "Frozen City, Blazing Tree" (mission 0x7B35). UNPROVEN: needs 1 live probe of whether pc2 save has it unlocked/cleared.

NATIVE ENTRY
- Every earlier pc2 Yoritomo run (R7B35 audit/AB, 5 arms) used mod "Fight now" (XWORLD warp 0xE0->0xB1, start 374664) = not clean. Continue lands in open world 0xE0 (153478,21792,201895), not at the boss.
- Native paths (notes/battle-scrolls.md, boss-rematch-paths.md): shrine item 3 Battle Scroll -> replay (needs mission flag 0xAB; list shows cleared main missions) -> Heian "Frozen City, Blazing Tree" = Yoritomo; boss arena start 374664 / door 62177372. Whole mission runs (stage+script), boss at end, so expect a long native load (earlier 30-120 s) + walk to boss box 371660; no god mode in clean (plan: take hits, count deaths, or accept early-fight window).
- Old pc2 trial 3080 (walk/menu nav to Yoritomo) timed out at menu navigation: no proven key sequence. Root cause of cleanup bug fixed in 3094 prep (/c/nioh3-re/pc2-yori-3094-preparation).
- Input without mod: tools/pc2_enter_game.ps1 (steam_autoplay + ui.fsx click/key, needs foreground) reaches Continue->field. Menu steps after that must be done by screenshot-guided ui.fsx keys. VPad/Rematch/Training pipes unusable in clean.

CLEAN REMOVAL (game dir C:\Program Files (x86)\Steam\steamapps\common\Nioh3)
- Move out (stash+hash restore): Nioh3EngineFix.asi (07b9f06e) + .ini/.log, Nioh3BossRematch.asi (c5474808)+ini/tsv, Nioh3Training.asi (d3cf3cca)+tsv/log(37 MB), dirs Nioh3EngineFix_pj/_tour, *.ack, _tourlist.txt.
- dinput8.dll (6203c5a0, 1.1 MB) is the ASI loader; remove/rename too for true vanilla (it has no OptiScaler string). Leave sl.*.dll, nvngx_*, libxess*, amd_ags (stock files, dated Oct 8). Check shader_dx12 / %LOCALAPPDATA% shader cache are untouched. tools/playsession/pc2_session_lib.ps1 has a stash/restore with manifest hashes.
- Without mod: no OSD speed/HP/fps logs. Need PresentMon (tools/PresentMon.exe) only; game speed cannot be logged (ratio from earlier arms: stock ~0.70x vs fix ~1.02x).

PINNED ARMS (from pc2 5-arm run 170705, captures/audit-screen/ in worktree nioh3-pc2fg-26768, branch cx-pc2/pc2-fg3arm-26768, commit 916c4fd3; perf ASI = nioh3-cbn-mce-26068/mce-build.asi sha 149e3fde)
- A (mod loaded, all off) ini 171cac99fa3d. FIX-only = A + [Latency] VRR=1, VRROnline=-1 only (ini e1ed31071d10; lines 121/125). 
- SAFE (pure perf, no look change) = B + MCE=1, W1/W2/W3, R1, L1, WK, W3WineSleep, FG(patch)=1, W6, B, PP, FPC, AABB, M2, J1, S1, IK, ED, GF, WL, WG, DQ, P1E7, LK, MA, BPO, W1B, RPA, CP, SPF, MQ, PT1, RWb, PCM, CBN/CBT, PJ_MaxMB 64, PJ_QuietMs 3000, PJ_KeepMax 4096, PL_MaxMB 1024, GL1, LW, EP, SR -1, RCMB/RCKB/RCMaxReadKB 64, AQThreads 4, PF* 4/768/4/50, GIMergeQualify 60, GpuOpt Enabled 1 + MergeAcrossBinds, DropDuplicateUav, MaxBatch 64, TraceDelay 30, InstallDelay 20, CbvMirror 128/1/1, SceneBufMirrorQueue 1, SkipDebugString 1 (ini fbb39b84136d).
- EVERYTHING = SAFE + E1, ESM1, XCLD, XFOG, XGI, XSHD, XSHF, XLOD, XTER, XPRT, XAO, XPOST = 1 (ini 718777b69845). Per-run INIs: run-pc2-20261023-160355/f3fg-pc2-0008.ini (SAFE), -0009.ini (EVERYTHING).
- FG-on variant of fix/SAFE/EVERYTHING = same INIs + config.xml FRAME_GENERATION=FSR (E arm). Careful: SAFE/EVERYTHING INIs contain mod/overlay but need Nioh3BossRematch/Training absent -> boss entry then must be native.

DISPLAY SYNC (preserve): %LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml now VSYNC=OFF, REN_FPS=120, REN_RAT=65, HDR=OFF, FRAME_GENERATION=OFF. Back it up, keep identical for all arms except FG (arm-set), restore after. Clean vanilla gets no VRR present fix (that is the point of FIX arm).

NEXT (needs game, pc2 Claude): one clean probe: remove mods, run enter_game, open shrine menu, see if Battle Scroll/replay lists Frozen City; record key sequence. If absent -> fall back to walking from Continue spot to Yoritomo gate (needs a route; none stored).
