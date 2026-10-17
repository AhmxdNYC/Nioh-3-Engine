# Cross-world no-load arena: pc2 static status (task 5243, 2026-10-15)
Not a finished analysis; no unload-side decompile exists on pc2 (no Ghidra here; Nioh3.exe on disk is packed - bytes at 0x1FCECE4/0x1FCEE1C/0x1FC4208 are garbage on disk, valid only in a live dump).
## Facts
- 0x1FCEE1C(mgr, a, stageId, b) = thin wrapper: calls 0x1FCE21C(mgr+0x15E8, a, ...) then tail-jumps 0x1FCE6F4(mgr+0x500, a, stageId, b) (from rdump_1FCEE1C.bin, captures/rematch/xworldprobe-20261015-062727/dumps). mgr+0x500 is the same stage manager object that FB0530 takes (pc #5030: FB0530(stageMgr+0x500, stageId, world, 0)); so 1FCEE1C is a stage REQUEST path, not unload.
- Another pc2 worker (slot 1) is running the live cross-world probe at the same time (tools/rematch/xworld_probe.ps1, scenarios C:/Users/User/projects/xw5116/s1..s5: native Yoritomo stage timeline, `oworld` keep-loaded, `stgreq` arena-stage-only from world 0xE0). Its game runs hold the pc2 lock, so no second game test was started for 5243.
- Measured on pc2 (this session): same-world swap/summon = 0.8-0.97 s; other-world = native load 18.9 s (30 s command-to-fight).
## Feasibility / plan (unverified)
1. Preload while in world A: request the target arena stage only (stgreq, FB0530 path, cell 67253 for Yoritomo) and poll stage state ([45B9E58]+760h) until 5; check A's own stage/actors survive (player not teleported, world byte 45BC428 unchanged). This is what xw5116 s5 tests.
2. If the stage stays resident, FightNow = warp to arena point + CreateDynamic of the boss (same as the same-world summon, with Distance 300).
3. Unload side (to free memory when leaving): needs decompiles of 0x1FCE21C/0x1FCE6F4 and the stage-release caller - ask pc for them; not derivable on pc2.
Risks: memory (stage cells of two worlds), the old world's streaming evicting the preloaded cells, mission script gate/door objects of the arena not created (placement filter 0x7D38FC from notes/arena-swap-static-pc2-4947.md).
