| shader | chip | vgprs | waves_per_simd | sgprs | instructions | valu | vmem_loads | samples | branches | mem_waits_full | mem_waits_partial | scratch |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| stock | gfx1102 | 108 | 4 | 102 | 9096 | 5067 | 83 | 81 | 88 | 40 | 41 | 0 |
| stock | gfx1103 | 108 | 4 | 102 | 9096 | 5067 | 83 | 81 | 88 | 40 | 41 | 0 |
| stock | gfx1201 | 92 | 8 | 102 | 9251 | 5130 | 83 | 81 | 85 | 55 | 26 | 0 |
| u | gfx1102 | 124 | 4 | 102 | 8955 | 5074 | 83 | 81 | 63 | 22 | 40 | 0 |
| u | gfx1103 | 124 | 4 | 102 | 8955 | 5074 | 83 | 81 | 63 | 22 | 40 | 0 |
| u | gfx1201 | 140 | 5 | 102 | 8773 | 5178 | 83 | 81 | 59 | 24 | 45 | 0 |
| p | gfx1102 | 128 | 4 | 102 | 10121 | 5945 | 83 | 81 | 63 | 22 | 43 | 0 |
| p | gfx1103 | 128 | 4 | 102 | 10121 | 5945 | 83 | 81 | 63 | 22 | 43 | 0 |
| p | gfx1201 | 132 | 5 | 102 | 9717 | 6034 | 83 | 81 | 59 | 22 | 47 | 0 |
| pfu | gfx1102 | 120 | 4 | 102 | 9257 | 5216 | 87 | 85 | 63 | 24 | 51 | 0 |
| pfu | gfx1103 | 120 | 4 | 102 | 9257 | 5216 | 87 | 85 | 63 | 24 | 51 | 0 |
| pfu | gfx1201 | 108 | 6 | 102 | 9483 | 5314 | 87 | 85 | 59 | 24 | 47 | 0 |
| pfp | gfx1102 | 128 | 4 | 102 | 10593 | 6153 | 87 | 85 | 63 | 24 | 49 | 0 |
| pfp | gfx1103 | 128 | 4 | 102 | 10593 | 6153 | 87 | 85 | 63 | 24 | 49 | 0 |
| pfp | gfx1201 | 136 | 5 | 102 | 10165 | 6316 | 87 | 85 | 59 | 24 | 48 | 0 |
stock.gfx1102.isa: sbufload 661 sload 186 lgkm0 514 samples 81 vmwaits 81 valu 5067 vgpr vgpr_count(108)
u.gfx1102.isa: sbufload 634 sload 170 lgkm0 447 samples 81 vmwaits 62 valu 5074 vgpr vgpr_count(124)
p.gfx1102.isa: sbufload 633 sload 170 lgkm0 447 samples 81 vmwaits 65 valu 5945 vgpr vgpr_count(128)
pfu.gfx1102.isa: sbufload 676 sload 178 lgkm0 472 samples 85 vmwaits 75 valu 5216 vgpr vgpr_count(120)
pfp.gfx1102.isa: sbufload 678 sload 178 lgkm0 502 samples 85 vmwaits 73 valu 6153 vgpr vgpr_count(128)
