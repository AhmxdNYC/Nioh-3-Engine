# No-load boss spawn: what an other-world boss needs (pc2, task #4943, 2026-10-15)

Device pc2 (RX 6650 XT, 1080p). Nioh3 v2.02, Nioh3Training.asi as installed on pc2 (sha256 0c8598f1...), Boss Rematch 1.3.3.
I did a static RE pass on a runtime memory dump. pc2 has no Ghidra, so I dumped the SteamStub-decrypted image and
disassembled it with capstone: `re/scripts_boss/spawn4943/n3dis.py`, `memdump.py`. Then I ran an in-game probe with
`tools/rematch/spawn_probe.ps1`. The probe held the game lock, backed up the saves and restored them byte for byte.

## Verdict
The boss assets are **not** the problem. `Load::Character` already loads everything the character needs, from any
world, without a map load. It takes **0.16-0.70 s** per boss on pc2. What is missing is fight behaviour: aggro/owner,
BossBattleMode, BGM and the gate. Asset preloading is the cheap part.

## Measured (spawn_probe, captures/rematch/spawnprobe-20261015-054920/)
The player stood in world **0xE0** (Nuribotoke's world), stage 4019512, at a shrine. For each boss: `spawn <chr> 700`,
watch 60 s, then `despawn all`.

| chr | boss (home world) | resources ready | CreateDynamic | actor | moved in 60 s |
|---|---|---|---|---|---|
| 16102 | Yoritomo (0xB1) | 703 ms | 0x303D3DB7 | team 1, HP bar shown | ~150 units |
| 307259 | Yamagata (0x4B) | 516 ms | ok | team 1 | no |
| 42450 | Takeda Shingen (0x96) | 641 ms | ok | team 1 | ~10 |
| 392871 | Nuribotoke (0xE0, same world) | 172 ms | ok | team 1 | ~10 |
| 656503 | Kusanagi (0xD8) | 297 ms | ok | team 1 | ~150 |
| 478320 | Yoshitsune (0x8E) | 156 ms | ok | team 1 | - |

- All 6 bosses rendered with their models: screenshots `*_a/_b/_c.png` (`grid_c.png` has all of them). There was no
  crash, and every one despawned cleanly through 2BF7E8.
- **The player kept full HP (2658/2658) in every run.** The bosses were on the hostile team (owner 4013495, a live
  registered actor) but never attacked within 60 s at 7 m. This is the passive-boss issue (#4852/#4855), not an asset
  issue. One caveat: the player stood at a shrine.
- Only the first spawn showed the boss HP bar (fake-id gauge). The spawn also lands behind the camera: the forward
  convention is flipped (`fwdmode`).

## The load path (static, exact RVAs)
- **Request:** `3F9E58(chrId, mode=2, x=0)`. The script `Load::Character` A42A7C is `3F9E58(id, r8+2, ..)` plus a
  count in the mission loader's map at loader+0x3C8.
  1. `3FB62C` walks a **dependency closure** in the `g_DataMgr+0x50` table (lookup D1C5C0), so linked characters
     (arms, summons, phase models) come along automatically.
  2. For each id it builds the key `134314(0,id) = 0x3_00000000 | id`. When `g_DataMgr+0xB0A` is set, the id first goes
     through a remap (1344B4).
  3. `3F9C10(g_ObjectManager[45B9E60], key)` finds or creates the entry: `3F8DA4` type 3 makes a 0x708-byte object
     (ctor 3F7E7C, 11 sub-load slots at +0x2F0). It then increments the refcount at **+0x2C0**.
  4. In mode 2 the id is also registered with the ActorManager: `3F9CD8(g_pActorManager[45B9E40], key, {0, id, 0})`.
  5. Then **type-4** companions: `g_DataMgr+0x48` → `3FB02C` lists extra ids for the character. Each gets the key
     `0x4_00000000 | id2` (0x2F0-byte object, ctor 3F82A8). Its load step reads two hashes (+0x6C, +0x70) from table
     `g_DataMgr+0xD8` and requests them through global `[475A178]`, probably effect/sound sets. These are registered
     with kind 4.
- **Ready:** the entry byte **+0x2C8**, looked up with `133440(rm, key)`. That is what Training's ResEntry checks, but
  only for the main id. **Better:** walk the same closure and type-4 keys and require +0x2C8 on all of them. Otherwise
  a summon or effect can arrive late. The mission load-end check uses LoadCharacterReady 3B39AC (model +0x804 == 6).
- **Release (the exact inverse):** `3F9900(chrId, 2, x)` walks the same closure and type-4 list, then releases with
  `3F9A8C` on the ObjectManager and `3F9A28` on the ActorManager. The wrapper is **`1C1DF80(chrId, 0)`**, which is what
  mission cleanup 1C08D1C calls for each loaded id. `Load::UnloadCharacter` is `1C217A8(loader, id)`: a map lookup,
  then 1C1DF80, then a decrement.
  - **Training calls 3F9E58 directly (not through the loader), so nothing ever releases those refs.** A boss pool needs
    `1C1DF80(chr,0)` once per `3F9E58` when a boss is evicted. Otherwise every boss tried stays resident until the
    process exits.
- **Spawn / despawn:** `1C0C5E0` CreateDynamic(chr, &pos16, yaw, 0, owner, 0) and `2BF7E8` delete (Training already
  does both).

## Recommended design for ally (hook points)
1. **Preload = assets only, no map:** `3F9E58(chr,2,0)` when the training menu opens or a boss is picked. Wait on
   +0x2C8 over the whole closure. That costs about 0.2-0.7 s off the critical path. There is no stage or mission
   request, so no loading screen.
2. **Pool + release:** keep at most N (2-3) preloaded bosses. Evict with `1C1DF80(chr,0)`, and also when leaving the
   world or mission, because the mission teardown does not know about refs taken outside the loader.
3. **Spawn** with CreateDynamic in front of the player. The current map supplies the navmesh, so spawn on open, flat
   ground. Arena props, fog gate and boss-specific gadgets belong to the boss's home stage and do not come along.
4. **Make it fight:** this is the real gap, so the owner/aggro fix (#4852) comes first. Then BossBattleMode 23A2DCC,
   ShowBossGauge 1D115D4 and PlayBossBGM 29057EC / StopBossBGM 59FFDC, as in notes/boss-swap-polish-pc2-4641.md
   option A. Whether a boss BGM from another world is resident when called is **unverified**: the probe could not hear
   audio.

## Open (not proven)
- Memory cost per resident boss: not measured. Read process private bytes before and after `spawn` to get it.
- Whether type-4 (`[475A178]`) is effects or sound: inferred, not named.
- AI/aggro with a non-mission owner. Movement shows the AI ticks, but it never engaged.
