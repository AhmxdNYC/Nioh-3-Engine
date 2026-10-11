# FO98: primary reset boundary

2026-10-11, finite inert C/native read by OpenAI Codex. Follow-on to
`handoff-codex-fo98-fence-identity.md`; C=owner+10, L=C+5A0, W=L+70.
FO remains NO-GO. No source, game, bridge, Ghidra or public operations.

## Verified normal owner-reset ordering

- 2EEC4C8 calls2EEDDEC before2EEDEF8/context teardown. 2EEDDEC
  drains owner+3BE0's dispatcher, then tailcalls2F00A74(C).
- 2F00A74 selects2F01108 when C+580 is nonnull; otherwise2F14328(L,C+588).
  Both branches contain a GPU completion wait, beyond a CPU ring drain.
- 2F01108 calls496078 ->1A7C4FC ->490F64. 490D5C increments L+30,
  records the closed slot's sequence at slot+8, invokes allocator+28 with
  that sequence, and returns it;490F64 recycles then returns that same value.
  2F01123 checks4920A0(C,seq). On incomplete it submits via1A84C58
  ->945664, then945610. Cold1A444CA repeatedly tests1180A4(C+6C8,seq)
  with Sleep(0), then9C9EF0 refreshes the completion cache.
- Null-current branch2F14328 calls2F13E74 when L+28!=-1. That routine
  walks the ascending closed-slot prefix, pushes its last token via86F760,
  clears L+24/L+28, and writes its last sequence into L+38.
  2F14347 drains W using252730;2F14352 passes L+38 to allocator+10
  (492278), whose same-fence GPU wait was proved in the previous note.
- 252730 compares W+2C producer count with W+30 consumer count. Worker
  1A759AF calls4F9054 replay before1A759B7 calls2526E4;2526F2 increments
  the consumer count. This proves CPU replay-return ordering, not GPU
  completion; the subsequent allocator wait supplies the latter.
- 2EEDEF8 ->2F00BC4 repeats2F01108 if C+580 is nonnull, then2F14364
  calls2F14448 to wake/join the ECL worker before releasing slot lists
  (2F143C0). This excludes the existing worker from executing during release.
  It does not prove that every external producer has been disabled.

## Verified sequence and cache treatment

- Ring constructor2EFA770 zeroes L+30 and L+38 once. Reviewed normal ring
  teardown2F14364 and reinitializers2F13F08/2F13FBC do not clear them.
- 2F13FBC zeros each new slot's list/old-sequence/token fields before list
  creation. FIFO init2F140FC zeroes worker counters. 2EFF7A3/7A7/7AA reset
  L+20=0,L+24=L+28=-1;2EFF7AD calls4920E0. 2EFF7B2 reads retained L+30,
  and2EFF7C1 stores L+30+1 into C+6D0. No counter rewind appears here.
- Reuse4920E0 calls allocator+10 on the old slot sequence at4921E0,
  clears that slot sequence at4921ED, then calls allocator+18 Reset.
- C+2510 is zeroed by the original context constructor, not explicitly
  cleared in reviewed2F00BC4/2EFEA14 reset bodies. 9C9EF0 performs one CAS
  from the cached snapshot to GetCompletedValue; it is not a max() update.
  Retaining this cache is consistent with the same primary fence and retained
  sequence counter. This is not proof for a replaced fence or other queues.

## Concrete conditional gap and next proof

The direct context destructor2EFB020 calls2F00BC4 at2EFB046 without the
owner's2EEDDEC predrain. If C+580==0,2F00BC4 skips2F01108 and proceeds to
worker join/slot release; it does not substitute2F14328's GPU wait. CPU join
alone is insufficient if L+38 has uncompleted GPU work. That combination is
not proved reachable; this is a conditional unsafe path, not a demonstrated
game defect. Normal owner reset has the prior null-current GPU wait.

Next exact proof: establish that C+580==0 implies no uncompleted submitted
sequence at the direct destructor entry, or establish an earlier GPU wait for
that lifecycle. Separately, the producer owner must prove no enqueue/record
can race after the reset predrain. These reads do not establish FO resource
retirement completion, unsubmitted-resource exclusion, or producer closure.

## Native provenance

Source: `re/Nioh3.dump.exe`, flat RVA offsets; inert exports
`D:/nioh3-re/decomp/0x<rva>.c`. SHA256 of RVA/hex-byte-count windows:

```text
2EEDDEC/3D  7d13bdd2fbd3d366138d295125da7a84c5b25cf809f993fb9cae9c1828cd297b
2F00A74/21  02332335632360e2a0d006b73b0c53554df41c03bc6306afb6acff3d980801d8
2F01108/45  5a301d7cee475f82eb7378ae287e0da5f2e6fd58eff7ed2f1cbe958466bc3b7f
1A444CA/29  51f8b703f6682d2aaecef8cbacdc4c14d659d1a5d760652bb687fa54a2f90961
2F14328/3C  19adf529482897ebf7b4958b1c025833a8dddda5098c2a7a57508bd497961068
2F13E74/90  c43f8b69e87bb761c39f394ec5043d4e70df2e666506a0e0d937b455d1fb6550
1A75970/59  14b8a879eb5e942230bf7f336555d92cfb08447c9abbf87c02e707a65113deee
2F00BC4/40B 3d903e9a613ca1dee94bfebba3dff6e757dd79ed938e3efc7e65575471812317
2F14364/A6  6ffb125a1dbb620d300711b1535b48a86f839b85b14c638a0bb95da4f13259d9
2F14448/69  ae5f956f0940e5ae4c7d035f41b6399734724258a309ed8a2352f0b1dc8c5db6
2F13FBC/13F 957b4ff41e52a8aaa63e156f0a98a4a99e09b432f30d7f7a505b0f328fe63b6d
2EFF796/32  0b78ca4442f9344298ba5b94569ddfa168b5cda9e8e9ac0f1282f36e20dd7040
2EFA770/AE  d46fc1df74653614c6e45b0b5c633075eb4feaff05951752a533f0444d0c1842
9C9EF0/3B   2b3bef37d041069bfe68350dfe162c10e9cb5b187d640879fc75b777b6c204c5
```
