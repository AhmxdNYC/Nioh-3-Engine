# Virtual input for test harnesses (ASI 1.7.37: VirtualPad + KeepRunningUnfocused)

Problem: the harnesses (abba2_pc.ps1, enter_game.ps1, run_pc.ps1, tour's title step) drive Nioh 3 with global SendInput
(tools\ui.fsx). That needs the game in the foreground, so the user cannot use the PC while a test runs, and touching the mouse
breaks or aborts the test. Goal: tests run while the user keeps working normally.

Static work on `re\Nioh3.dump.exe` (v2.02) + decompile `D:\nioh3-re\decomp`, scripts in the session scratchpad (capstone xref /
caller scans). Live check: section 6.

## 1. How the game reads input (all on `[App]InputUpdateWorker`, once per logic frame; notes\input-thread.md)

| device | path | RVA |
|---|---|---|
| Xbox pads | `XInputGetState` (XINPUT1_4 #2, IAT 0x38E2C50): PadPollDevices 434E18 (connected slots + 4-slot probe every 2 s), active-pad scan 435B8C | 434E18, 435B8C |
| keyboard | `IDirectInputDevice8::GetDeviceState(256)` | DiKeyboardGetState 435D58 |
| mouse | DirectInput mouse GetDeviceState(DIMOUSESTATE2), `GetAsyncKeyState` VK 1/2/4/5/6, GetCursorPos / ScreenToClient / SetCursorPos (recentre), wheel from WndProc | 4371DC, 435E08, 435864, 1FE66C8 |
| Sony pads | own HID reader threads (HID.DLL) | AE4xxx-AF7xxx |

No RawInput, GameInput, Windows.Gaming.Input or `GetKeyState`. The WndProc (878054) ignores WM_KEYDOWN/WM_CHAR except IME and
Win+Shift+arrows; game input never comes from window messages.

## 2. What the game does on focus loss

- WndProc handles only **WM_ACTIVATEAPP** (0x1C): `GameMain(+0x1AB) = wParam != 0` (G = [0x4759910], the same object as the
  window's GWLP_USERDATA). No WM_ACTIVATE / WM_SETFOCUS / WM_KILLFOCUS handling, nothing about rendering, audio or frame rate.
  The game keeps simulating and rendering while inactive.
- The flag has exactly one reader, **IsAppActive 436CF8** (`G ? G->+1AB : 0`, leaf, writes rax + flags only), with 12 callers,
  all input:

| caller | inactive behaviour |
|---|---|
| PadMgrUpdate 43671C (call 436758) | 4 pad slots reset: pads ignored, unless options+3Eh (`[0x45B9E70]+0x3E`, no writer through the global) |
| pad slot read 436A1C (call 436A20) | no state for the slot (same options+3Eh exception) |
| 89B230 from FrameDtSetup (call 89B2FD) | **focus-loss pause**: when options+3Bh is set (a game option: setter case 12A4588, getter 60837C), PadMgr+69B08 = 1 -> the pause menu opens (same path as a pad disconnect, 69B09) |
| DiKeyboardGetState 435D68 / 435D94 | keys cleared before the read; no re-Acquire after a failed read |
| MouseDeltaLatch 4357F4, CursorUpdate 4359EF, MouseButtonsPoll 435E69/4360AC/4360B9, 1FE63C5 | mouse deltas / buttons / cursor work skipped |
| rumble 438770 (call 4387FB) | rumble zeroed |

- Windows adds its own: a background process may get EcoQoS (lower clocks), and Windows 11 ignores timer-resolution requests of
  processes whose windows are occluded / minimised (the L1 limiter and DS use high-resolution waits) - unfocused timing can differ.

## 3. Options

| option | verdict |
|---|---|
| **(a) in-mod virtual controller** - the ASI's XInputGetState IAT hook answers from a named pipe; menu items set directly | **chosen.** No driver, no OS input, nothing outside the game process. The pad path is the one the game polls anyway; works unfocused once the IsAppActive pad gates answer "active" (KeepRunningUnfocused). |
| (b) ViGEmBus virtual Xbox pad | works (a real XInput device), but installs a kernel driver (needs the user's OK) and still needs (d), because the game ignores every pad while inactive. Option only. |
| (c) PostMessage / SendMessage to the window | useless: the game never reads input from window messages (section 1); DirectInput state is not fed by posted messages. |
| (d) run unfocused | needed for (a)/(b). Forcing `G+1AB = 1` (e.g. swallowing WM_ACTIVATEAPP) is **not safe**: MouseButtonsPoll uses GetAsyncKeyState (the user's clicks anywhere would attack in game) and CursorUpdate recentres with SetCursorPos (steals the cursor). The safe form patches only the pad + pause callers. |
| mod menu without keys | a pipe command `menu set KEY VALUE` that runs the menu's own `Change()` on the Present thread: same code and same `MENU: <label> -> X` log line as a key press, no navigation, no F10 / focus problems (run2 lost F10 after refocus). |

## 4. What was built (1.7.37, all default off)

- `[Debug] VirtualPad = 1` (exclusive: slot 0 = virtual, slots 1-3 "not connected", the real XInput is never called - the user's
  pad cannot disturb a test) or `= 2` (merge). IAT hook installed in the dllmain worker **before** padlat / osd, so the mod menu
  and PadLatency chain on top and see the virtual pad like a real one. Pipe `\\.\pipe\nioh3vpad` (PIPE_REJECT_REMOTE_CLIENTS,
  default DACL, 4 instances, one thread per client). Commands (one line each, reply `ok ...` / `err ...`):
  `press <btn[+btn]> [ms=100]`, `hold`, `release <btn|all>`, `stick L|R x y [ms]`, `trigger L|R v [ms]`, `key <name|0xDIK> [ms]`,
  `menu get|set <KEY> [v]`, `state`, `ping`, `arm` / `disarm`, `wait ms`, `log text`, `help`. A press is visible to at least two
  game frames (calls < 2 ms apart = one frame) and the reply comes after it was released (timeout -> cancelled + `err`).
  `state`: frame counter, appActive, loading (58A), paused (58B/58C), player present + position, `field=1` (player, not loading /
  paused, no game menu), menu pause reason, foreground, pad read age, held state, KRU count.
- Virtual keys ride the mod menu's DirectInput GetDeviceState hook (`menu::SetKeyFilter`); while Nioh3 is not foreground the real
  256-byte keyboard state is zeroed, so typing in another window never reaches the game.
- `[Debug] KeepRunningUnfocused = 1` (v2.02 bytes verified, patches.json group VP0-VP3 check-only): call sites 436758 / 436A20 /
  89B2FD retargeted (E8 rel32) to a thunk `call [IsAppActive]; test al; jne ret; cmp [switch]; je ret; mov al,1; lock inc [count];
  ret` - writes al + flags only; `hook_liveness.py clob 436CF8` = rax + flags, the callers keep r9/r10/r11 live across the call
  (89B2FD) and those survive. Mouse / keyboard / cursor / rumble callers keep the stock answer. Plus
  `SetProcessInformation(ProcessPowerThrottling, EXECUTION_SPEED | IGNORE_TIMER_RESOLUTION, state 0)` so unfocused = focused timing.
- `[Debug] VirtualPadIdleReleaseSec = 600`: held inputs released when no command came for that long (dead harness).
- Harness: `tools\testmatrix\vpad.py` (client: one command, `run` script, `wait-pipe`, `in-game`, `title-continue` = A presses
  while not in the field and not loading until `field=1` for 3 s; `--owner-pid` exits 3 when the owner dies; `selftest`),
  `tools\testmatrix\vpad.ps1` (VP / VP-TitleContinue / VP-MenuSet / Force-Vpad / VP-GiveBackFocus). `abba2_pc.ps1 -Input vpad`
  and `tools\tour\run_tour.ps1 -Input vpad`: no clicks, no focus / idle waits, no refocus, toggles via `menu set`, windows kept with
  `user_active` recorded; the old SendInput path is unchanged and still the default. harness_guard still checks owner pid,
  MaxMinutes, commit headroom, crash limit.
- Launch caveat: the new game window takes the foreground once when it is created; the wrapper hands it back to the window the
  user had (`VP-GiveBackFocus`, ui.fsx focus - no input sent).

Offline tests: `py -I mod\Nioh3EngineFix\tests\run_vpad_harness.py` (parser, composer rounds rule, hook on fake IAT slots
exclusive / disarm / merge, fake 60 Hz game thread + the real pipe server + 2 pipe clients, real keys hidden while unfocused,
thunk run natively + disassembly check, dump facts: IsAppActive clobbers, its 12 callers, WM_ACTIVATEAPP store; wiring) ALL
PASSED; `tests\test_patterns.py` ALL 236 SITES OK; `py -I tools\testmatrix\vpad.py selftest` ALL PASSED; `abba2_pc.ps1 -DryRun
-Input vpad` and `run_tour.ps1 -DryRun -Input vpad` OK.

## 5. Limits

- `menu get|set` needs the overlay to present ([Menu] Enabled = 1, game not minimised); `key` needs the menu's DirectInput hook.
- Exclusive mode does not intercept XInputSetState: rumble goes to a real pad on slot 0 while the game is focused (stock code
  zeroes rumble while inactive).
- `title-continue` presses A blindly (no screen reading); it relies on Continue being the title menu's default item and on
  AutoContinueLoad for the tip page. Checked live (section 6).
- Never play normally with VirtualPad = 1 (the real pads are hidden for the whole session).

## 6. Live check

(pending: PC flag test holds the game lock)
