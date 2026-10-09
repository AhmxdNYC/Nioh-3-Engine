# Boot dialogs: "autosave feature" notice + "reset all settings?" warning (pc2 block, 2026-10-09; ASI 1.7.48 SkipBootNotice)

Context: bridge #754/#756/#758/#790/#793/#797. pc2 hands-free runs stopped before the title from ~07:01. pc2 then found two
dialogs on every launch: (1) "This game has an autosave feature ... [Confirm]" and (2) "Current graphics settings may lead to
unstable gameplay. Are you sure you want to reset all settings to the defaults? Yes / [No]". pc2 and the Ally both saw them
come back after the harness killed the game during boot or at the menu. pc2 cleared them by hand (#797): a normal launch, then
Confirm, No, then a clean exit. Static work: Ghidra (`decomp_ro.cmd`), the dump, the mod logs in pc2's zip (#147), and the PC's
live tour log at 12:32 (another agent's run, observed read-only).

## 1. Where the dialogs come from

Boot sequencer **1C35188** is shared by the boot/title `Game::Mode`s. Its state is `[obj+14h]`. States 1-3 request modes;
state 3 is PreloadPSOMode. **State 4** (asm 1C35308..1C35540):

```
if (!SaveDisabled 283950)            // GameMain+3Eh || +3Fh: the same test SaveRequest 5B63A8 makes
  [demo build (GameMain+3Ch) only: two demo notices]
  SystemMessageAdd E6DA30(text 0x3C4ABF1, confirm=1)          // dialog 1   <- call site 1C35393 (BN1)
  if (![obj+55h] && GfxSettingsTooHeavy 1FE3D38())           //            <- call site 1C353A2 (BN2)
    queue Yes/No 0x3E7F0F1 + 0x1B80845 (Yes = reset every setting to the defaults)   // dialog 2
state = 5
```

- State 5 waits until the system-message queue is empty (`[45BBF80]+C0h`, 21AB54C) and no UI layer is open (F1B7C).
- State 7: `SaveRequest(saveMgr 45C8480, 5, 31h, 1)` loads the save data.
- States 8 and 9 wait on `[saveMgr]+10h`, then the title follows.
- The text id 0x3C4ABF1 occurs as an immediate only at 1C35373.

### Dialog 1: the autosave notice

- It is shown on **every** boot. No flag, marker file or registry key decides whether it appears, and nothing records that it
  was confirmed.
- The only gate is "saving disabled". So this dialog cannot cause a loop, and there is nothing to restore.
- The kill does not bring it back. It was always there, and the harness's A presses used to answer it.

### Dialog 2: the "reset all settings?" warning

`GfxSettingsTooHeavy 1FE3D38 = 1FE3C48() || 1FE3CD4()`. Both halves read in-memory values only. Neither reads a crash flag:

- **1FE3CD4, the VRAM check.**
  - 22CECFC asks the adapter for memory. On AMD/Intel it uses 1135F60 = `IDXGIAdapter3::QueryVideoMemoryInfo(0, LOCAL)`
    (vtbl +70h); NVIDIA has its own path.
  - It takes **Budget** (`+10h`) in MB, then compares it with the graphics tier, option #6 `[45BAF48]`:

    | Budget | warns when |
    |---|---|
    | < 10000 MB and <= 6000 | tier < 8 |
    | 6001 - 7000 | tier < 7 |
    | 7001 - 8500 | tier < 3 |
    | > 8500 | never |

  - An 8 GB card (pc2's RX 6650 XT, Budget ~7.2-7.6 GB) falls in the 7001-8500 row. On the PC (16 GB) it never fires.
- **1FE3C48, the options check.** Only when option #9 `[45BAF54]` != 0.
  - It sums how far options 19..33 exceed the reference table of the tier the hardware is rated for (1FE23B4(1FE018C)).
  - It warns at a sum >= 35, 20 or 10. The threshold depends on whether the tier is below, equal to or above the recommended
    tier 1FE0A48.
- The options array is `45BAF30` (60 ints, filled from config by 1FE6064). Index 6 is the tier, and index 2 is the adapter.

**Abnormal-exit link: not found in the code.**
- The check itself has no "unclean exit" input.
- Since a kill demonstrably brings dialog 2 back (pc2 + Ally), the kill must change one of its inputs: the option values the
  game loads at boot, or the VRAM budget at that moment. The likely route is the game rewriting `config.xml` /
  SYSTEMSAVEDATA00 on a clean exit. This is unverified.
- Answering "No" from a normal launch and then exiting cleanly is what clears it (#797).
- For the VRAM agent: on 8 GB AMD cards, the boot "unstable settings" verdict reads the DXGI local **Budget**, not
  CurrentUsage.

### Why A did nothing in the pc2 tour launch, and the black screen after answering (#790)

- **Not explained statically.** There is no input-device, focus or save check in the notice path. It is an ordinary system
  message.
- The PC's tour run (Tour 1, VirtualPad, unfocused, fg=0) got through dialog 1 hands-free at 12:32:
  1. notice open 12:32:29
  2. A at 34.6
  3. SB2/SB3 skipped two BACKUP.BIN writes at 35.3, and the guard denied two BACKUP.BIN DeleteFileW calls
  4. title
  5. load 58A at 55.9
- So tour's save block does not hold dialog 1 or the save load that follows it.
- On the PC, dialog 2 never fires (16 GB).

## 2. Fix: `[Debug] SkipBootNotice` (ASI 1.7.48, bootnotice.cpp, patches.json BN1/BN2 check-only)

| value | effect |
|---|---|
| -1 (default) | 1 when `[Debug] Tour != 0` or `VirtualPad != 0`, else 0. Normal play stays stock. |
| 0 | nothing installed |
| 1 | skip both dialogs |
| 2 | state log only |

What it does:
- **BN1 1C35393:** the call to SystemMessageAdd goes to a thunk that calls StringReset 310C18 on the text instead. E6DA30
  frees its argument the same way at its end, so nothing leaks. Nothing is queued.
- **BN2 1C353A2:** the thunk calls 1FE3D38 and answers 0. This is the same as the player choosing **No**: no setting changes.
- The sequencer then goes 4 -> 5 -> 7 at once, the same path the PC takes on every boot.
- The answers are not "pressed" through the UI, so the "No" callback, the system-save write it might need, focus and input do
  not matter.
- **Logs:** `BootNotice: installed ...`, one `skipped ...` line per dialog, `boot sequencer state a -> b`,
  `save manager state [+10h] = n` (states 7-9), and the line `past the save-data load` ends the state log. If a boot still
  stalls, the last state line says where.
- **Tests:** `tests/test_patterns.py` ALL 239 SITES OK; `tests/run_bootnotice_harness.py` ALL PASSED. The harness runs a fake
  image with every ini mode and checks the thunk behaviour, the refusal on wrong bytes, and the dump facts.
- **Not run live:** the PC's game lock was held by another agent's capture.

## 3. Prevention

- **The harness kills are the trigger.** pc_gpu_session / pc2_run_tour / abba already do CloseMainWindow, wait 50-60 s, and
  only then kill. A game stuck at a boot dialog ignores WM_CLOSE, so it gets killed, and the next launch shows dialog 2 again.
- **With 1.7.48, a launch after a kill is safe without a clearing pass:**
  - Any Tour/VirtualPad launch skips both dialogs.
  - Nothing is reset, so config stays exactly the bench config.
- **Before a launch the user plays normally after a killed run:** answer Confirm, No once, then quit from the title menu.
- **Keep killing as the last resort.** Prefer CloseMainWindow, then the in-game Quit via vpad, then the kill.
