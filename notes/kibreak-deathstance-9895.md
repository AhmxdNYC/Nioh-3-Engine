# Ki-break death-stance, live catch #9895 (build 1e3679e, 02:53:59-02:54:02 am)
Boss ...9DF0, first Ki break of the round, user froze (F9) while it showed the death-like pose.
- Break start 02:53:59.310: action 0x4E4C, bits1060 4400200200001000 (same bits as the good breaks at 01:32:41 / 01:36:52).
- +157/+407 ms: still 0x4E4C. First player hit +797 ms (bits 4480..2).
- +1000 ms: action 0x4E3E, bits 0400200200004000 (bit 0x1000 cleared, 0x4000 set) although 23=1 (Ki-broken) still true and Ki 0.
- Freeze at ~+3.5 s: action 0x4E40, bits 0400200200000000, last motion request 32.5 s ago (no forced motion).
So state stays Ki-broken but the boss action moved 0x4E4C -> 0x4E3E -> 0x4E40 mid-break; BareFix did not fire (start bits had the stagger bits, not bare).
Unknown: action ids of a GOOD break in this build (only this one break was logged). Next: log a good break's ids, compare; likely 0x4E3E/0x4E40 = the crouch/'death' idle.

## Update 03:13 (user F9 #9937, log Nioh3Training.log)
- Break 03:06:24 (HP 48 %, 23=1): 0x4E4C at start/+156/+406, then 0x4E3E at +1000 - same 4E4C->4E3E swap as the "bad" 02:53 break, so that swap alone is NOT the bug signature (KiBreakActFix 00c7098 may re-request on a normal break: unverified, risk).
- Breaks 03:07:46, 03:07:49, 03:09:42 are BARE (action 0xFA6 / 0x4E3E, no stagger bits, 23 drops to 0 within 400 ms in two of them).
- F9 03:12:05: action 0x4E38, 23=0, Ki -1000 (gold bar in shot, boss upright attacking, HP 27297): not a Ki-broken moment per flags.
