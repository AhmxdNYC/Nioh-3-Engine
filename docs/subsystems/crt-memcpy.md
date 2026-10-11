# CRT memcpy / memset / libm (the "unclassified 0x8-0xC MB" bucket)
CPU share 7.9 % (Ally 12.9 %, PC 2.9 %); coverage 100 %. Threads: mostly [App]Game (token stream and upload copies); also workers.

## Role
Statically linked MSVC CRT block at B90000-BEFFFF. Not a game system, but it shows as one bucket in the treemap because it sits at 0x8-0xC MB.

## Functions
| RVA | name | % | note |
|---|---|---|---|
| B94230 | MemcpyErms (the `rep movsb` ERMS stub, reached from Memcpy B94250 for large copies) | 4.94 | large copies |
| B94250 | Memcpy | 2.13 | small/medium path; together about 7 % of profiled self time |
| B93EE0 | MemsetBytewise | 0.61 | byte-loop small/unaligned fill next to memset B93F00 |
| B94150 | memcmp | 0.09 | 128-byte keys (PSO cache B-tree find) |
| BE6740 | PowfAvx | 0.05 | libm powf, FMA path gated by flag 45B9D5C |
| BE0378 | SqrtfSlowPath | 0.02 | negative input, 3690 callers |

## What drives the cost
Callers, not the CRT: constants written into the KTGL token stream (`ShaderConstWriteStream` 1D7340, inline token-0x66 blobs), upload copies into
write-combined/upload heaps, parameter-block assignment. The copy cost is memory bandwidth and write-combined store behaviour. On the Ally the memcpy share is high because
the SoC is CPU-bandwidth-starved while the GPU takes the shared memory. Constant data is copied up to three times on the way to the GPU (candidate C1 in d3d12-submission.md).

## Deeper notes
notes/cpu-hotspots-ally.md (MemcpyErms line), d3d12-submission.md (C1 triple-copy), mainthread-drawprep.md (E/W1/W2: write-combined reads in the UI vertex path), ui-mainthread.md.
