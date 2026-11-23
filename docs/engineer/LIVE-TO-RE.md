# From something you see live to the code

How to go from a thing you can see in the game, a Player log line, or the overlay, to a named address, and then to a mod that finds that address by byte pattern.

Read [SYMBOLS.md](SYMBOLS.md) before you edit `symbols/symbols.tsv`. The mods never read that file. They find addresses at startup from `rvas.json`, `sites.json`, and `patches.json` ([SYMBOLS.md](SYMBOLS.md) section 4).

## 1. Write down the live fact

One sentence, with the number you can already see. Do not start in Ghidra.

| You saw it in | What to copy down |
|---|---|
| The game | What you pressed, what the character did, and about how long it took |
| Player log (`Nioh3PlayerLog.txt`, [FEATURES-BOSS-TRAINING.md](FEATURES-BOSS-TRAINING.md) section 7) | The `ACT` line: action id, previous action, animation speed, fps, wall time. `SPD` lines add the move's own animation time |
| Overlay | The fps / frame-time / speed row that was on screen when it happened |

Player log is off unless its row is on. `ACT` lines already print the player's action id, so you often do not need a memory search just to learn the id.

## 2. Find that value in memory

Prefer a field you already know over a process-wide scan.

- Action id and the move clock are on the player's action controller. Animation time is the float at `[ctrl+0x60]`, in 1/60-second units (`player_speed.h`). The step for one frame is the float at `[ctrl+0x40]`, and it is `60 / fps` of the **fps setting**, not of the frame rate you measured.
- The fps setting the game uses for that division is `GameMain::TargetFps` (offset `0x614` on the game-main object; [SYMBOLS.md](SYMBOLS.md)).
- If you do not know the field yet: reproduce the moment twice, watch the dword you suspect, and keep the address only if it changes to the logged action id at the same wall time as the `ACT` line. Stop when you have one address. A scan of every writable page from a menu or render thread will hitch the game.

## 3. See what writes it (x64dbg)

A hardware **write** breakpoint stops on the instruction that stores the value. x64dbg has four hardware breakpoints.

1. Attach to `Nioh3.exe`. For a first look, do this with the mods not loaded, so their hooks are not the writer. Load them only if you need the Player log in the same session.
2. Breakpoint the dword, access **write**, length **4**. If the field is stored every frame, condition the break on the new value (the action id from the log), or log and continue.
3. When it hits, note `RIP` and the module base of `Nioh3.exe` from the memory map. `RVA = RIP - base`.
4. Also note the value in the source register. That is often a return value from the function just called.

Packed `Nioh3.exe` on disk is not the code you are looking at. Dump the decrypted image at the title screen with mods off (`py -I tools/progress/dump_nioh3.py`) before you trust any file offset.

## 4. Open that RVA in Ghidra

1. Import **your** dump. Do not commit the dump or a decompiler export ([PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md)).
2. Run `symbols/ghidra/N3ApplySymbols.java` on `symbols/symbols.tsv` so existing names are already there.
3. Go to the RVA. If it is not a function start (check `.pdata` or Ghidra's function bounds), it is a `label` inside a function, not a new `function` row.
4. Decompile only the containing function. Quote a few instructions in your note. Do not paste the whole function into the docs or into `symbols.tsv`.

## 5. Add a `symbols.tsv` row

Edit `symbols/symbols.tsv` directly in this repository and open a pull request. Column rules and the confidence levels are in [SYMBOLS.md](SYMBOLS.md). One line, no tab inside a field:

```text
0x35C13F	label	SetChargedMove_WriteActionId	u32	Writes the charged-attack action id 0x629d. The id is the return value of the function at 0x35EF04. Live: Player log ACT line shows 0x629d when the charged follow-up starts.	high	x64dbg write breakpoint; decompiled
```

That row is an illustration of the format. Check whether the address already has a name before you add one (`grep` the RVA in `symbols/symbols.tsv`). A site in the middle of a function is a `label`. The function that contains it gets its own `function` row. Run `py symbols/build_exports.py` and fix every error. Do not rename the function only inside your Ghidra project.

## 6. Reach it from a mod by pattern

No hard-coded `base + 0xRVA` in a mod. Pick the table that matches what you need ([SYMBOLS.md](SYMBOLS.md) section 4):

| You need to | Table | Generator |
|---|---|---|
| Read or call the address | that mod's `rvas.json` | `mod/<mod>/tools/gen_rvas.py` |
| Hook or retarget a Boss Rematch site | `mod/Nioh3BossRematch/sites.json` | `mod/Nioh3BossRematch/tools/derive_sites.py` |
| Patch EngineFix code bytes | `mod/Nioh3EngineFix/patches.json` | `mod/Nioh3EngineFix/tools/gen_header.py` |

Take the pattern from your dump. It must match **once** in the executable sections. Wildcard rip-relative displacements and branch targets. On the known build the resolved RVA must equal the one you measured, and the bytes must equal the recorded originals. On any other build, zero matches or several matches skip the feature. The code then does nothing instead of writing to a wrong address.

`Grva(rva)` (`mod/Nioh3EngineFix/src/gamerva.h`, and the same pair in the other mods) returns the RVA unchanged on the known build and runs the pattern lookup otherwise. EngineFix patches go through `Resolve()` in `mod/Nioh3EngineFix/src/dllmain.cpp`. Groups that would break the game if only half of them installed (the VRR group is the example in SYMBOLS.md) are all-or-nothing and have their own ini switch, default off until an A/B says otherwise.

## Worked example: charged attack id `0x629d`

What you see: a held attack sometimes starts its charged follow-up at about half a second, and sometimes at about a quarter of a second. Nothing in between. Player log `ACT` lines show the windup action `0x624d`, then the charged action `0x629d`. On a late one, `0x629d` shows up near animation time 28 (the `[ctrl+0x60]` clock), not near time 12.

Where the id is stored: the instruction at `Nioh3.exe` RVA `0x35C13F` writes `0x629d`. A write breakpoint on the action-id dword stops there. The value it stores is the return value of the function at RVA `0x35EF04`. The set-move wrapper around RVA `0x4F54C4` commits move id `0x638C` from the return at `0x4F5526` (the call into `0x35BC68`). That branch always resolves to action `0x629d`. The chooser at that point is not reading the button. So the late `ACT` line is not "the writer picked a different move". The writer ran late because an earlier check said no.

What actually rejects the on-time branch: the move's key scan is the function at RVA `0x20DFC0`. It fires a key when animation time `t` satisfies `t <= keyTime < t + step`, with `step = [ctrl+0x40]` (`60 / fps setting`). The on-time key sits near `t = 12`. Its input check is the controller vtable slot `+0x120`, function RVA `0x20CF68`. That check asks "has this button been held for N frames?" through the getter at RVA `0x901BA4`:

```text
frames_held * 60 / GameMain::TargetFps
```

`TargetFps` is the **setting** (30, 60, or 120), read from offset `0x614`. It is not the fps in the overlay. At a 120 setting and about 90 real fps, a hold that has lasted long enough in wall-clock time still reads as 11 frames when the key wanted 12. The scan returns no, and the next key that can start `0x629d` is near animation time 28. That is the half-second swing. At a 60 setting the same getter matches real time, and the late swing does not show up.

The same `60 / [+0x614]` conversion is used by many other "held for N frames" checks, including the one at RVA `0x43619C`. A fix for this getter does not by itself fix those.

What a mod change would touch: not the writer at `0x35C13F`. That instruction is doing the right store. The pattern you would add is on the held-frames getter (or on the key scan, if you only need to observe it), in the table from section 6, default off, and only after the kind of A/B in [performance-measurement.md](performance-measurement.md). Name `0x35C13F` as a `label` if it is not a function start, and name `0x901BA4` as the function the check calls, with the description saying it divides by the fps setting.
