# Final bench

Default (stock), optimized (pure-perf), and optimized-visuals (full).
One mean per device and frame-generation setting.
Only rows with valid=1 and a filled metric are included. Empty cells were not measured.

Valid groups: 18. Skipped rows: 0.

| device | fg | arm | n | avg_fps | low1_fps | hitches | cpu_temp | gpu_temp | cpu_power | gpu_power | cpu_util | gpu_util |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ally | off | full | 2 | 39.02 | 24.39 | 3.00 | 68.60 | 66.40 | 9.75 | 20.25 | 59.85 | 97.70 |
| ally | off | pure-perf | 2 | 32.58 | 26.44 | 1.50 | 68.65 | 66.65 | 9.35 | 20.05 | 57.95 | 98.45 |
| ally | off | stock | 2 | 34.63 | 29.79 | 0.00 | 69.00 | 67.00 | 9.55 | 20.20 | 64.90 | 98.50 |
| ally | on | full | 2 | 52.38 | 38.99 | 0.50 | 68.05 | 65.45 | 10.70 | 20.25 | 48.00 | 87.85 |
| ally | on | pure-perf | 2 | 44.65 | 32.96 | 1.50 | 68.85 | 65.65 | 11.70 | 20.25 | 54.05 | 87.75 |
| ally | on | stock | 2 | 42.06 | 25.98 | 3.00 | 72.35 | 66.75 | 15.45 | 21.50 | 64.40 | 88.30 |
| pc | off | full | 2 | 98.78 | 73.32 |  |  |  |  |  |  |  |
| pc | off | pure-perf | 2 | 98.46 | 74.62 |  |  |  |  |  |  |  |
| pc | off | stock | 2 | 75.84 | 60.52 |  |  |  |  |  |  |  |
| pc | on | full | 2 | 151.58 | 86.47 |  |  |  |  |  |  |  |
| pc | on | pure-perf | 2 | 144.41 | 78.80 |  |  |  |  |  |  |  |
| pc | on | stock | 2 | 141.42 | 73.32 |  |  |  |  |  |  |  |
| pc2 | off | full | 2 | 47.32 | 33.63 | 0.00 | 67.29 | 67.82 | 76.02 | 123.59 | 96.44 | 97.77 |
| pc2 | off | pure-perf | 2 | 46.87 | 33.44 | 0.00 | 67.56 | 67.91 | 76.19 | 126.54 | 96.31 | 98.00 |
| pc2 | off | stock | 2 | 41.72 | 29.18 | 0.00 | 66.44 | 67.57 | 75.94 | 122.54 | 98.96 | 97.91 |
| pc2 | on | full | 2 | 74.30 | 38.70 | 10.00 | 65.45 | 65.97 | 73.33 | 115.50 | 87.84 | 91.03 |
| pc2 | on | pure-perf | 2 | 73.59 | 37.98 | 9.50 | 64.61 | 65.55 | 73.08 | 118.43 | 89.90 | 91.69 |
| pc2 | on | stock | 2 | 77.07 | 39.94 | 11.50 | 65.64 | 66.73 | 74.88 | 121.94 | 97.69 | 91.21 |

## Ally chunks

ROG Ally, Linux/Proton. One row is one fight chunk. fg is off or on.
a1 is default (stock), a2 is optimized (pure-perf), a3 is optimized-visuals (full).
The means above include every valid row.

| chunk | fg | arm | avg_fps | low1_fps | hitches | cpu_temp | gpu_temp | cpu_power | gpu_power | cpu_util | gpu_util |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| n135-a1-r1 | off | stock | 35.15 | 28.71 | 0.00 | 70.00 | 68.00 | 9.20 | 20.10 | 64.00 | 98.80 |
| n135-a2-r1 | off | pure-perf | 34.96 | 32.52 | 0.00 | 69.00 | 67.50 | 8.50 | 20.00 | 53.80 | 99.00 |
| n135-a3-r1 | off | full | 33.06 | 25.36 | 0.00 | 68.90 | 66.70 | 9.30 | 20.20 | 57.20 | 98.60 |
| n135-a1-r2 | off | stock | 34.11 | 30.87 | 0.00 | 68.00 | 66.00 | 9.90 | 20.30 | 65.80 | 98.20 |
| n135-a2-r2 | off | pure-perf | 30.20 | 20.35 | 3.00 | 68.30 | 65.80 | 10.20 | 20.10 | 62.10 | 97.90 |
| n135-a3-r2 | off | full | 44.97 | 23.42 | 6.00 | 68.30 | 66.10 | 10.20 | 20.30 | 62.50 | 96.80 |
| n139-a1-r1 | on | stock | 43.96 | 28.37 | 1.00 | 74.70 | 68.90 | 15.80 | 22.70 | 62.50 | 88.80 |
| n139-a2-r1 | on | pure-perf | 42.27 | 27.06 | 3.00 | 70.00 | 65.80 | 13.30 | 20.20 | 58.60 | 86.40 |
| n139-a3-r1 | on | full | 54.13 | 43.37 | 0.00 | 67.90 | 65.40 | 10.50 | 20.30 | 47.60 | 88.50 |
| n139-a1-r2 | on | stock | 40.17 | 23.60 | 5.00 | 70.00 | 64.60 | 15.10 | 20.30 | 66.30 | 87.80 |
| n139-a2-r2 | on | pure-perf | 47.02 | 38.86 | 0.00 | 67.70 | 65.50 | 10.10 | 20.30 | 49.50 | 89.10 |
| n139-a3-r2 | on | full | 50.63 | 34.61 | 1.00 | 68.20 | 65.50 | 10.90 | 20.20 | 48.40 | 87.20 |

Sources:
- `captures/final-full/ally/results.csv`
- `captures/final-full/pc/results.csv`
- `captures/final-full/pc/results-fgon-on.csv`
- `captures/final-full/pc2/results.csv`
- `captures/final-full/pc2-fgon/results.csv`
