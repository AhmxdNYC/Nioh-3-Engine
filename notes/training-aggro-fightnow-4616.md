# Aggro probe + Fight-now menu patch (bridge 4616)

2026-10-14, Ally (Proton, game v2.02, 1920x1080 fullscreen, focus asserted per input/shot; saves/wrapper/mod files restored, result.json restored=true). Evidence /tmp/boss-4616.

## Aggro (Yoritomo R7B35, Rajomon save, world 0xB1)
- New `dump rva len` command (training.c) + objdump gave the bodies. SetInitAccumulatedHate (VM 2395B44) = FindById(a0), FindById(a1), amount; calls 1FF23C8(*(actorA+0x3B0), *(actorB+0x3B0), amount) (hate list at [[ai+0x50]+0x10]+0x518..0x520, 16-byte entries, float at +0xC; 1FF23C8 -> FC52EC adds, FC534C = SetTotalFixedHate only edits existing). Works on a dynamic boss without a wrapper: plug/hate.c (trplug) call -> hate list 0 -> 1.
- Result: hate alone does NOT make a passive Yoritomo approach (boss placed 16.5 m away, hate list 1, paced 14-16 m for 40 s, no attack). Notice is distance/state driven, not hate-driven.
- Default Fight now (BossDistance 900 = 9 m): Yoritomo attacked and killed the player within ~25 s (one run, 21:54:55 -> 21:55:22). So the 9 m default is the working aggro fix; do not raise BossDistance for Yoritomo.

## Fight-now menu path
rematch-fightnow-4616.patch (mod/Nioh3BossRematch/src/rematch.cpp, +17 lines): ExecDirect "Fight now (in the arena)" calls Nioh3Training_FightNow after the clear check (returns 1 = swapped, no queue slot), ExecOpen calls Nioh3Training_Preload. Syntax-checked only (zig c++ -fsyntax-only, no errors); not built, not run (no MSVC on Ally). pc/pc2 apply onto the real 1.4.15 source.

## Not done
- Arena script (fog gate, intro camera, clear flags) and arena BGM: not found without per-boss script RE; setting clear flags would also mark progression/rewards on the save, so not attempted. BBM only switches HUD/boss mode.
