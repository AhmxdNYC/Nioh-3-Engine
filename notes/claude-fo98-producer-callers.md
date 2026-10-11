# FO98 producer callers (Claude-PC, 2026-10-11, static only)

Scope: who calls the opcode-98 emitter `2F06AB4(resource, context, tileDesc, mode)` (vtable+28 of the
resource class) and with which SRW mode. Decomp grep (D:/nioh3-re/decomp) + Capstone/.pdata on
re/Nioh3.dump.exe. No Ghidra, no game. Not a safety verdict; FO stays NO-GO.

2F06AB4 has no direct callers. Every 4-arg `vtable+0x28` site in the export was checked; the ones with the
(obj, context, desc, mode) shape:

| site | context arg | mode | notes |
|---|---|---|---|
| 2F11C9C (caller 2F11514) | param_2 | **1 shared** (const) | tile-batch unmap loop: CacheTileBatchUnmap, then emit, then 1282FC slot clear |
| 4906E8 (caller 3D1E4C) | param_1 (owner, has +6EA8 lock) | **1 shared** (const) | runs inside AcquireSRWLockExclusive(owner+6EA8) |
| 2F14FB0 <- 2F11C1C / 2F1245C | ctxpair[+0] | ctxpair[+0x10] | wrapper; mode comes from a {context, ?, mode} struct |
| 2F1245C <- **2EF0D20** <- 1A17815 | stack ctxpair = {rdi=recording context, rbx+590, **1**, 0} | **1 shared** | 2EF0D20 is NOT in the decomp export (no Ghidra function). It emits only if entry state==2 and entry value < threshold r8 = owner(rbx)+D10. 1A17815 is a cold chunk (.pdata 1A1779C-1A17869) of primary **7D9C24**, which also advances context+3C40 itself, i.e. the recording thread. Referenced in notes/renderer/frame-overlap.md. |

So far every resolved producer passes mode 1 (shared), so the resource+198 lock gives **no mutual exclusion
between producers**, only against a mode-2 holder. Not yet found: any mode-2 caller, and the remaining
2F11C1C callers (2F11EBC / 2F121D8 / 2F122C8) and their ctxpair mode. Next steps: (1) resolve those three
ctxpair builders; (2) check whether 7D9C24's deferred-retire emission (threshold owner+D10) can run after
endFrame 418C70 publication for the same context, which would be a late-producer path; (3) add 2EF0D20 to
the Ghidra function list.

Tools: scratchpad dis.py / refs.py / pdata.py (Capstone, rel32 call + absolute-pointer scan, .pdata chain walk).
