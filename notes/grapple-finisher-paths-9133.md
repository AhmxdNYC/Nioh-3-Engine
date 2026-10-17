# Bug 1 (grapple finisher still possible under gold Ki bar, user #9135) - static RE findings
Log evidence (captures/pc/userplay_9133.log, 20:41:01.343): boss action carried finisher flag (state 0x35) + death-blow bit 101; the grapple-target block counted 362 frames but CanGrapple, grab box and down-target counters stayed 0, so the finisher came through a slot we do not block.
Open paths (none tested in game):
1. Grab boxes that don't require Ki-broken: hit filter checks the Ki-broken target only if box flag +0x4C has bit 7; with bit 0 only it jumps past our hook at 117A9EE and only needs attacker ai+0x668 == target. Fix: hook 117A9AC (`test al,al; jns 117AA2A`), reject (117C6ED) any grab box whose target is a gold-window boss (g_nf_goldact).
2. HP-floor state test: StateTest 0x27 = 2A3AC0(ctrl) || ctrl+0xA8 bit 45. Make 0x27 return false for a gold-window boss (detour 2E9948 like the existing nofintrace one).
3. Bit 101 is set at 946D68 from the boss action data +0x110 bit 21 - a symptom; clearing it per frame doesn't help.
Builder on pc/cover-fixes-9133 (worktree nioh3-coverrel) already has gold-revert (KiBreakGoldFadeMs), HP0 stagger (KiBreakHp0Stagger), boss grace after cover; bug 1 not yet covered by it.
