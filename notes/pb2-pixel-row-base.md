# PB2 pixel integer row-base factorization

2026-10-12. Separate candidate from the previously admitted UV hoist; this
slice does not combine them or repeat its Windows PSO gate. No game/install,
draw, ASI change or FPS claim.

## Mechanism and exact binding

Original PS SHA256
`0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268`,
normalized module `f01abd800dbbe594ffe931f3f373db599abb749695e031b0064a9825ceeeeabd`.
The twelve typed min-mip reads form three four-corner groups. Each original
address is `(width*y+x)+(base+3)`, computed for two x and two y values.
The probe computes `row0=width*y0+(base+3)` and `row1=width*y1+(base+3)`
once per group, then adds each x. All arithmetic wraps modulo2^32; no
nsw/nuw flags, float reassociation, wider reads or cross-lane sharing.

Original gfx1103 already CSEs the four multiplies to two per group, but emits
four separate `+3` address additions. Thus merely deleting duplicate DXIL
multiplications is not the result being claimed.

`gpu-shaders/pb2pixel/address_probe.py` requires the exact original module,
checks three complete square-coordinate patterns and sole users of deleted
MADs, signs the candidate and binds its fresh body and interface chunks.
An independent whole-body certificate normalizes ONLY integer add/UMad as
polynomials over Z/(2^32). Every block's ordered resource/float calls,
branches and outputs must then match; all other operations remain opaque
exact expressions. PASS for the actual signed candidate. An actual-body
base-offset mutation rejects, plus4096 arbitrary32-bit overflow cases pass.
This structural domain does not model driver behavior or replace native
visual verification; it adds no resource-validity assumption.

## Emitted result, with tradeoff

Same synthetic gfx1103 RGA diagnostic as original, not a captured draw:

| Metric | Original | Row base |
|---|---:|---:|
| Instructions | 658 | 655 |
| VALU | 355 | 346 |
| VGPR / SGPR | 32 / 38 | 32 / 38 |
| Buffer reads / texture samples | 17 / 3 | 17 / 3 |
| Branches / scratch instructions | 12 / 0 | 12 / 0 |
| Full / partial VMEM waits | 7 / 9 | 4 / 12 |

Exact opcode deltas: v_add_nc_u32 18→12, v_mov_b32 7→6, v_swap_b32 2→0;
s_delay_alu 86→91 and total s_waitcnt24→25. Six actual address additions
disappear, but scheduling overhead partly offsets that reduction. Fewer full
waits does not establish lower elapsed GPU time. This is a small retained
offline candidate, not a measured speedup or automatic native promotion.

Candidate SHA256
`6f90ab04072ae94d2061f199764882e7e8cccc6c74692e8ba4c5a21c3e72bfdf`;
fresh body `7b17abb825748c0a886fc1075c7b9c0fc17e5464db99de4f63e042c1d127bc3b`;
ISA `c35bd115221ee5cb1b00a1848d46db1fe22dcfe406e00cfd3e31be33ff7cf080`.
Private artifacts: `D:/nioh3-re/pb2pixel-20261011/row-base.*`.

Reproduce with `python -B gpu-shaders/pb2pixel/address_probe.py --repo
'C:/Users/Ahmad Hamza/nioh3-engine' --out D:/nioh3-re/pb2pixel-20261011`.
Next useful unit is independent certificate review and a bounded UV+row-base
composition/codegen comparison: gains cannot be added arithmetically. This
candidate has no one-record pack, captured-PSO admission, Proton or draw PASS.
