# PlayMode admission evidence

2026-10-11, bounded read-only native/decompile review by Codex. No native invocation, game run or source changes. Addresses below are RVAs in the private `re/Nioh3.dump.exe`; PE section headers were parsed to resolve file offsets before Capstone inspection. Existing decomp exports were read without regeneration.

## Recommendation

For the first native-arena training route, admit only **readable raw mode 0**, with a separately readable disconnected NetMgr state and the other request gates. Refuse raw 1/2 as online and other readable values as unsupported/unknown, rather than interpreting every value except 1/2 as offline. Missing pointer/read failure is unknown, not raw0. Keep the active-attempt quarantine on session-read loss; this inspection does not justify resuming after a transient gap.

## Native facts

| Evidence | Observed instructions / interpretation |
|---|---|
| Constructor `1C2C780` | `1C2C78A xor eax,eax`; `1C2C7A6 mov qword ptr [rcx+30h],rax` initializes both +30 and +34 to zero. Object allocation helper `1C2E574` requests 38h bytes; `1C2BC58` calls this constructor after allocation. |
| Publication `1C2C050` | Releases the previous object through `1C2BD48` when owner pointer `[4709B58]` exists; constructs through `1C2BB64`; `1C2C079` stores returned pointer to `[4709B50]`, `1C2C084` stores allocator/owner pointer to `[4709B58]`. The setter's existing export is missing, so this is direct-byte evidence. |
| Online predicate `85834` | At `85846`, compares object+34h with 1; otherwise calls `85948`, which compares +34h with 2 at `85954`. Null returns false in these native predicates. Native false does not prove an initialized, usable offline session. |
| Explicit raw0 | `1C2EB4C` reads `[4709B50]`; if nonnull, `1C2EB58 and dword ptr [rax+34h],0`. Constructor independently establishes initial0. |
| Explicit raw1 | `1C31D0C mov dword ptr [rax+34h],1`; additional direct stores `1D7860A`, `1D9FFE1` use register value1. |
| Explicit raw2 | `1C31DC4 mov dword ptr [rax+34h],2`; additional immediate stores `1D7890F`, `1DA004A`. |
| Destruction/null | `1C31ACF` releases the current object, then `1C31AD7 movdqu [4709B50],xmm0` zeroes pointer/owner together. Existing export `D:/nioh3-re/decomp/0x1c31ab8.c` agrees. |
| Lifetime route | `0x1c2fa5c.c` clears the GameModeDrawThread object then calls/tail-jumps ClearCurrent. Its recorded caller is global teardown function26F41C4. `0x1c304c8.c` calls SetCurrent from initialization function2703124. |

The direct-reference scan considered RIP-relative 64-bit mov accesses to4709B50 in `.text`, then inspected native windows. It is not a complete alias/indirect-write inventory. The small leaf `1C31D28 mov [rcx+34h],edx` exists nearby, but its object provenance/callers were not established; it must not be advertised as a proven public PlayMode setter or enum closure.

`notes/frame-timing.md:606` records a previous live solo observation: raw +34h mode0, guest flags0/0. That historical observation supports the zero allowlist; this task made no fresh runtime observation. Raw1 versus raw2 native predicates are definite, but human-facing submode names remain inherited symbols, not a newly reconstructed full enum.

## Title/load limitation

The current pointer can legitimately be null during object lifetime teardown. The reviewed call graph does **not** prove that ordinary mission loading, intro loading, death/retry or each title transition destroys/recreates it, nor quantify any unreadable interval. Therefore do not weaken request gates because such a transient is merely plausible. A future controlled native run may measure this through existing valid/unknown snapshots; until then an actual gap during an accepted attempt remains indeterminate and quarantined.
