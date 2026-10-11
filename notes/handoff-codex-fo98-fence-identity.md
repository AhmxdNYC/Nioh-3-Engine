# FO98 primary graphics fence identity - 2026-10-11

OpenAI Codex bounded static audit. Documentation branch
`codex-fo98-fence-identity`, parent `d61b88a`; main/source untouched.
Input: inert `re/Nioh3.dump.exe` and selected exports from
`D:/nioh3-re/decomp`. Continues `handoff-codex-fo98-allocator.md` (15c9e2d).

## Verified conclusion

For the primary graphics context, allocator owner O is the device stored in
global RVA 45BA588 and context C is embedded at O+10h. Therefore
**O+6D8h == C+6C8h: one pointer slot, one fence, one sequence domain.**
The previous distinction between these offsets does not establish a second
fence. This closes that primary-context identity question. FO remains NO-GO;
identity is not a producer, last-use, completion or retirement safety proof.

## Native pointer provenance

- 229BFD6 loads RDI from `[RIP+231E5ABh]`, global45BA588.
  229C011 stores RDI at caller RSP+38h; 229C016 calls228CD84.
- Wrapper228CD84 saves entry RSP in R11 and subtracts98h in total.
  228CDEE loads the incoming device from current RSP+D8h (=entry RSP+40h).
  228CDF6 forwards it at R11-60h (=new caller RSP+38h), then228CE0E
  calls32EE510. This argument is omitted from the decompiled factory call.
- Constructor32EE510 preserves the new state pointer in RSI. After its38h
  stack adjustment,32EE54F loads RAX from RSP+78h (=entry RSP+40h), and
  **32EE561 stores RAX at state+70h**. Thus state+70h receives global45BA588.
- DisplayResetRebuildSwapchain226D170 loads RSI from state+70h;226D30B/30E
  passes it as owner to RecreateSwapchainDevice2EEA690.
- 2EEA6D3 preserves owner in R15;2EEB739/743 passes it to2EED444.
  2EED45E/462 adds10h and calls context initializer2EFEA14. Independently,
  owner constructor2EE015B/15F calls context constructor2EFA840 at owner+10h.
- Context initializer2EFEA66 captures the same global45BA588;2EFF735 stores
  it in C+590h (embedded allocator+8). 2EFF743 stores C in C+598h
  (allocator+10h). The allocator's owner and the initialization owner agree.

## Fence creation, assignment and sequence use

- 2EE9AA0/2EE9AB5 creates a fence into O+4310h through2EE93D4. The helper
  calls device vtable+120h with initial value0, flags0 and the output pointer
  at stack+20h. This source is not the separate queue aggregate+43B8h.
- Recreate2EEB1B7 loads `[O+4310h]`;2EEB1C7 places it at descriptor+28h.
  2EFEAE0 forms C+6C8h;2EFEB18 loads descriptor+28h;2EFEB20 calls2EFB6E4.
  Its2EFB6F9 stores the pointer after releasing the previous reference,
  then calls AddRef. The resulting slot is also O+6D8h by the proven base.
- ECL0 target4F90DC:4F9111 calls ExecuteCommandLists;4F911A loads the
  fence from C+6C8h;4F9124 calls Signal on the same queue. The decoded
  sequence is preserved in RSI and copied to R8 at4F9117 without conversion.
- Reuse492278 first polls C+6C8h. Cold19B0992 loads allocator+8 into RCX,
  19B0999 preserves the old-slot sequence in RDX, and19B099C calls7D9E1C.
  Its O+6D8h poll therefore tests the same fence/value domain.

## Reset lifetime and remaining proof boundary

The inspected normal swapchain-reset path preserves O rather than allocating
a replacement device:2EEA7A8/7AB calls teardown with O;2EEB739/743 later
reinitializes C at the same O+10h and copies the then-current O+4310h fence
back into the shared slot. The slot geometry remains identical across this
path. Teardown2EEC4C8 calls2EEDDEC, which waits DispatcherRingBusy false,
then2EEDE16/24 passes O+10h to2F00A74. Its exact entry tests C+580h and
dispatches to2F01108 or ring cleanup2F14328. It has no containing pdata
record; a preceding function's pdata must not be mistaken for its body.

This audit does **not** prove GPU completion before reset, sequence/cached
completion restart rules, stale-list exclusion, or identity across full
device replacement/shutdown. The address identity is verified for the
constructed primary context and the inspected normal reset call chain;
safe cross-generation retirement remains open. It does not extend to
independent async/copy contexts.

Next exact proof: trace2F01108/2F14328 and list-ring reinitialization for
old uses, fence completion, sequence and C+2510h cache handling before any
new generation reuses storage. Coordinate this with the existing retirement
owner; do not duplicate producer closure or resource vtable+28 callers.

## Byte provenance

SHA-256 of dump RVA windows reviewed directly (RVA / byte length):

| Window | SHA-256 |
|---|---|
| 229BFD6 / 71 | f3eca43915eee96223718e3912c6220371c1c8278fac5979c2d9c08c6b355f77 |
| 228CDEE / 37 | 9515fc0828751fb057da02e45d708bbf7df5b9e455410500ec2236ce6efafc13 |
| 32EE510 / 89 | 4c6ddae76cbe13c5fb81ccaef6e19a7e5051df13374749b0c5cc6b118a8a020e |
| 2EE0144 / 103 | cd4302a7c3f6679f6d2b7bea07088512fd1465f48d339e4bd5d75a80418e56bc |
| 2EED444 / 35 | c1f6066ebae288a5d6da821f7292b46a4af22afe0fe4f8718d7d5f5974e105f7 |
| 2EFEAE0 / 69 | 8f65483ec655b2db3277354574b6e267adbf023dc22ef6c258e96db25cf0b737 |
| 2EFB6E4 / 44 | 801d648d355f6318a9127778286a596f6bb592731c6ea54faac8ee3f35e8b64d |
| 2EEB1B7 / 26 | 3a80e11a6aba6a21ea719476d211e0c7c1309eb9add10a0fbac3dbfc434cc640 |

Existing4F90DC/19B0992/7D9E1C reviewed window hashes match the previous
allocator provenance. These hashes pin bytes, not behavior. No harness,
build, game/attach/Ghidra, bridge/API, source edit or public publication.
