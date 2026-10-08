# Auto-continue after loading (ASI 1.7.22, `[General] AutoContinueLoad = 1`)

Static work only (decomp_ro + capstone on `re\Nioh3.dump.exe`); the game was not run.

## The tip page and its press
- The "[DECIDE] Continue / Retry" page with loading tips is UI menu slot **0x2B** = `[g_UiManager 45C84A8]+158h`. Its vtable is
  4015EC8 (ctor 1CD1DA8, +38h close 1CEF9D0, +70h tick 1D244F8). The text ids are in `text_en.tsv`: 0x3CF40F1 "Continue",
  0x3D13F7F "Retry", 0x4DED1B1 `^00~DECIDE~`, 0x3DFD731 "View Another Tip".
- Page bytes: +71Bh load done, +717h prompt shown, +718h/+719h skip/force-prompt flags (719 = 1 for the death "Retry"
  variant 1D1B758), +71Ch skip parameter, +724h tip text id.
- **TipPageLoadDoneTick 1CECB24**, run every frame from the +70h tick:
  - It returns at once while +71Bh = 0.
  - Then the game's own auto-close: if !719 and (718 || 71C || online mode 2), or online mode 1, the page closes.
  - Otherwise it calls **TipPageConfirmCheck 1CECAE4** = `ConfirmPressed 283950 (GameMain+3Eh||+3Fh) && !GameMain+3Fh &&
    !SystemMessageQueued 27FBC44 ([UiMgr+700h] queue)`. When that passes, the page closes (22C5248, vfunc +38h(0)).
  - If the check fails, it shows the prompt (sound D33E9BC6, layout 139h), sets +717h, and keeps calling the check every
    frame.
- **+71Bh is the load-complete signal.** Only TipPageSetLoadDone 1D010E4 writes it (and an unreferenced twin 1D0112C).
  The mission/load sequencer (dispatcher 5A29F0, state in `[obj+4]`) calls it in state 0Bh 223BE08 when the request has the
  tips byte 45BC467. That happens after state 8 has passed LoadIsLoadEnd 3B3424 and 219F3A0 is idle. The same step then
  calls UiLoadingScreenEnd.
- State 0Ch-0Eh 223BEA8 then waits for the page (slot 2Bh +2Ch == 3, or slot 2Ah 0x150 if that one is active). State 0Fh
  223C308 clears 58A after 30 (or 100) dt60 units. So in the Ally logs the 58A clear follows the press, and the "G9 tail"
  is this wait.

## The fix
- A 5-byte call-site retarget at **1CECAED** (patches.json AC1, `check` kind + deps). The call goes to a near thunk:
  `cmp byte [switch],0; je orig; mov al,1; lock inc [count]; ret; orig: jmp 283950`.
- The thunk touches only al and the flags. The original leaf touches rax and the flags. rcx, rdx, r8-r11 are untouched.
- Effect: on the first frame where the page would accept a press, the confirm check reads "pressed". The page then
  closes through the game's own path.
- The game's own holds still apply: GameMain+3Fh and a queued system message.
- Only 1CECB24 calls 1CECAE4, so no other prompt, dialog, death choice or menu is affected.
- The menu row "Skip 'press button' after loading" writes the switch byte live, and writes `[General] AutoContinueLoad` at
  once on a background thread. "Save to ini" also writes the key.
- Log: `AutoContinue: installed ...`, then one `AutoContinue #n: load done - answered ...` line per load.

## Not verifiable offline
- Whether every route the user sees sets +71Bh through 223BE08:
  - title -> load and mission start: yes, statically.
  - death respawn: the Retry page is opened by the game-over sequence 223A15C via 1C20414 -> slot 2Ah 1D06610; whether
    its load sets 71Bh is not shown.
  - shrine fast travel: FastTravelSeq 3B2A04 never opens slot 2Bh, so it has no tip-page press of its own.
- The meaning of GameMain+3Eh/+3Fh.
- Whether slot 2Ah closes by itself after the page closes.
- How it looks in game.

Ally test: Title -> Load, a fast travel, and a death. Grep `AutoContinue` and check the 58A clear in LoadStats.
