# Native 'This will end the mission. Proceed?' box: the real source (static RE, 2026-10-19)

Offline/static only: re/Nioh3.dump.exe (flat image, base 0x7FF74F410000, all addresses are RVAs), data/extract, Ghidra RO pool.
The game was not launched. Follows notes/gate-native-suppress.md (on pc/gate-native-suppress), which found that our 4 game-side
hooks never fired.

## Answer in one paragraph
The box is not opened by game C++ UI code. The clear gadget's own **gadget script** (LFM archive_16, symbol G_CLEAR_GADGET) opens it.
On interact it calls the gadget-script native **`UI::OpenCommonWindowTwoButton` = 0x23F8CC0**, with text 0x3E0C230. That native
calls **CommonWindowOpen 0x2806F80**, which calls **DialogShow 0x348B90** on the dialog manager [[0x45C84A8]+0x6C8].
Our DialogOpen hook 348274 is only a *sibling* caller of 348B90. 12A1F80, 1DAE4E0 and 1DB58B0 are the pause-menu / ChoiceBox
(2806FD4) path. The script path goes through none of them, so they never fired. Hook 0x23F8CC0. When the current gadget is our gate
and arg0 == 0x3E0C230: do not open the box, set the result global 0x44BAE44 = -1 ('no button'), open our page, and return. The
script then takes its not-Yes path and re-arms its interaction. No input masking is needed.

## Evidence
1. Text: 65061424 = **0x3E0C230** (group 15) 'This will end the mission. Proceed?' (D:\nioh3-re\text_en.tsv). As an imm32 in the EXE it
   appears only in 12A213E / 1DAE6C4 / 1DB5A9D, inside the 3 hooked functions. In game data (all of data/extract except the text
   archive) it appears only in two archive_16 gadget scripts:
   - `lfm/archive_16/2A/319B314E61D7...` (G_CLEAR_GADGET, CLEAR_GADGET_WAIT_DECIDE/FINISH) at 0x2F5C
   - `lfm/archive_16/3E/E97932260A44...` (variant: is_from_training_and_play_end, OpenCommonWindow3/4Button) at 0x3114, plus
     65591390 'Do you wish to end the mission?'.
2. Script decode (`py -I re/scripts_re/lfm_disasm.py <path> --stmts`; the fn-name column is off by one function, read the bodies):
   - `get_select_window_text` (2F0E): `Object::GetSubID`: 12 -> 65585054, 13 -> 65930289, otherwise **65061424**. Our gate is clear
     gadget id 10 (Training log 'GATE ... clear gadget id 10 spawned', record type 0x353), so it gets the end-mission text.
   - `is_select_window` (2DCC): SubID 10/11/12 -> true (show the Yes/No window).
   - Main interact loop (3312..3834): `Interaction::IsInteraction` / `Interaction::IsPlayerInteraction` -> if
     `!UI::IsOpenCommonWindow` and is_select_window -> **`UI::OpenCommonWindowTwoButton(get_select_window_text(), [lit])` at 348C**
     -> wait while `UI::IsOpenCommonWindow` -> `UI::GetButtonNumLastCommonWindow` == 0 (Yes) -> `Online::SetClearGadgetWait`,
     fade, `Status::Set(2)` = mission end. Not Yes -> back to the interaction wait. A second TwoButton call at 3686 is for the
     host in parallel-stream missions (same native).
3. Native binding tables (re/out/strings.tsv descriptor column; entries are 16 B {char* name, fn*}):
   - Gadget-script table (base 0x39794F0, registered by 0x94F4C / 0x34DB38):

     | native | descriptor | fn |
     |---|---|---|
     | UI::OpenCommonWindowTwoButton | 0x3979640 | **0x23F8CC0** |
     | UI::OpenCommonWindowNonButton | 0x3979620 | 0x23F8814 |
     | UI::OpenCommonWindowOneButton | 0x3979630 | 0x23F88FC |
     | UI::OpenCommonWindowThreeButton | 0x3979650 | 0x23F8A0C |
     | UI::IsOpenCommonWindow | 0x3979700 | 0x23F60FC (-> 0xF1B7C) |
     | UI::CloseCommonWindow | 0x3979710 | 0x23F5E10 (-> 0xA54E50) |
     | UI::GetButtonNumLastCommonWindow | 0x3979720 | 0x23F5F78 (returns int at **0x44BAE44**) |
     | Interaction::IsInteraction | 0x397C990 | 0x2E6B7C |
     | Interaction::IsPlayerInteraction | 0x397CA60 | 0x23DCA50 |
     | Object::GetSubID | 0x3979540 | 0x9A8C88 |
     | Status::Set | 0x3979500 | 0x9DE288 |

   - The mission-script table (base 0x397E770) has its own copies, for example UI::OpenCommonWindowTwoButton 0x3981EA0 -> 0x23B6A14
     (result global 0x44BAE40). The clear gadget does not use them.
   - There are no code xrefs to the descriptor slots. They are copied by the registrar, so swapping the table pointer is not reliable.
     Use an inline hook.
4. 0x23F8CC0 body: `reader 0x1399B0(rd, ctx)`; `*(int*)0x44BAE44 = -1`; args via `0x139904(rd, &idx)`: arg0 = message text id,
   arg1/arg2 = button texts, arg3 = default cursor. It builds the window struct (callback 0x118FB60 through 0x1E46F00), calls
   **0x2806F80** (`mgr = [[0x45C84A8]+0x6C8]; if mgr: 0x348B90(mgr, copy)`) and returns `*(u16*)(rd+0x10C)` (the number of
   return values, 0 here). The reader is index-based and does not consume anything, so a hook can peek arg0 and still call the
   original.
5. Current gadget: gadget natives read the running gadget from global **[0x45BA228]**. GetSubID = `0x3624D4(cur)` (gadget type
   check) then `+0x110`. IsPlayerInteraction compares `[[player+0x3B0]+8]+0x940` with it.
6. 348B90 callers: 348274 (our DialogOpen hook), 28073D8, 28046F4, 2806F80. 2806F80 has 32 callers (every gadget/mission
   CommonWindow native, plus 2834370, which is also a caller of 12A1F80). This explains every 'never fired' line.
7. Prologues (dump): 0x23F8CC0 `48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20` (15 B, no RIP-relative bytes, the same bytes as the
   existing 12A1F80 check `ecp`). 0x2806F80 `48 89 5C 24 10 57 48 81 EC C0 00 00 00` (13 B, then a RIP-relative mov).

## Proposed hook (Nioh3Training, replaces the A-button mask as the primary fix)
```c
typedef uint16_t (*Cw2Fn)(void* ctx); static Cw2Fn o_cw2;
static uint16_t h_cw2(void* ctx) {
    if (g_cfg_gate && g_gate.up) {
        __declspec(align(16)) uint8_t rd[0x120];
        ((void* (*)(void*, void*))(B + 0x1399B0))(rd, ctx);
        uint32_t i0 = 0, text = ((uint32_t (*)(void*, uint32_t*))(B + 0x139904))(rd, &i0);
        uint64_t g = ((uint64_t (*)(uint64_t))(B + 0x3624D4))(*(uint64_t*)(B + 0x45BA228));
        uint32_t sub = g ? *(uint32_t*)(g + 0x110) : 0;
        if (text == 0x3E0C230 && sub == g_gate.id) {          // our Return gadget only
            *(int32_t*)(B + 0x44BAE44) = -1;                   // 'no button' -> script: not Yes, re-arms the interaction
            /* open OUR gate page here (same entry the tap-A opener uses), log once */
            return *(uint16_t*)(rd + 0x10C);                   // = 0 return values, same as the original
        }
    }
    return o_cw2(ctx);
}
// install: memcmp(B+0x23F8CC0, ecp, 15) == 0 -> o_cw2 = (Cw2Fn)Hook(0x23F8CC0, 15, h_cw2);
```
- Wrap the reads in __try like GateRecType. If the type check returns 0, pass through to the original.
- Effect: the game's own A interaction on the gadget runs the script, the hook swallows the box and opens our page. ReturnGateZone
  masking and the raw-XInput GatePadA opener can then be turned off (keep them behind the ini as a fallback). A real clear gadget
  with another SubID, or any other CommonWindow, is untouched.
- Fallback if SubID does not match at runtime (the first run should log `sub`, `text` and `cur` once per call while the gate is up):
  match on text 0x3E0C230 plus player distance to the gate < 600 u, as the 348274 hook did.
- Broader chokepoints, not needed: 0x2806F80 (every CommonWindow; the text is only inside the built struct) and 0x348B90 (every
  dialog, shared with 348274).
- The old hooks on 12A1F80 / 1DAE4E0 / 1DB58B0 / 348274 can stay. They cover the pause-menu End Mission entries, not the gadget.

## Needs a live check (not done, offline)
One Practice session: walk to the Return object, press A. Expect the log line from h_cw2 (text 0x3E0C230, sub 10), our page only,
and no native box. The gadget prompt should still work after closing our page (the script re-armed).

## Implemented (checkpoint 3, built, NOT installed)
- Re-verified offline with Capstone on re/Nioh3.dump.exe: 23F8CC0 prologue, `mov [rip] -> 0x44BAE44 = -1`, args 0..3 via 139904
  (index-based peek, reader 1399B0 only fills rd from ctx), return `movzx eax, word [rd+0x10C]`; GetSubID 9A8C88 reads [0x45BA228],
  3624D4, +0x110.
- Variant script (archive_16/3E): SubID 10 also goes through TwoButton (text get_select_choice_window_text = 65591390 0x3E8D85E or
  65061424). ThreeButton/4Button with 65591390 are only for SubIDs 14..19 (in-game/training play end), not our gate; not hooked.
- mod/Nioh3Training/src/training.c: `h_cw2` / `Cw2Peek` hook on 0x23F8CC0 (15 B, same `ecp` prologue as 12A1F80), installed in the
  ReturnGateBlockNativePrompt block. Matches text 0x3E0C230 or 0x3E8D85E while g_gate.up && g_tr.active && !g_rh.active, and
  (SubID == g_gate.id or player < 600 u from the gate). Then: result global = -1, GateUse() (Rematch GateChoice export) unless the
  boss is alive (with ReturnGateAlways 0), our page is open, or it was requested < 1.5 s ago; returns the native's own return count.
  Otherwise passes through (and logs a diag + stamps the native-modal safety net). ini: [Return] ReturnGateHookScriptBox = 1.
- ReturnGateZone (A mask near the gate) is unchanged and still default 1, so with it on the game's interact may never reach the
  script; the hook is the game-side guarantee. After a live check confirms the 'script end-mission box ... BLOCKED' log line, the
  mask can be turned off.
