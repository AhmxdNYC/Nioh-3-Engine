# Final bench

Stock, pure-perf, and full. One mean per device and frame-generation setting.
Only rows with valid=1 and a filled metric are included. Empty cells were not measured.

Valid groups: 9. Skipped rows: 0.

Frame generation on is pending. No fg=on row is in these files. pc2 session 3689466d was still running that set.

| device | fg | arm | n | avg_fps | low1_fps | hitches | cpu_temp | gpu_temp | cpu_power | gpu_power | cpu_util | gpu_util |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ally | off | full | 2 | 39.02 | 24.39 | 3.00 | 68.60 | 66.40 | 9.75 | 20.25 | 59.85 | 97.70 |
| ally | off | pure-perf | 2 | 32.58 | 26.44 | 1.50 | 68.65 | 66.65 | 9.35 | 20.05 | 57.95 | 98.45 |
| ally | off | stock | 2 | 34.63 | 29.79 | 0.00 | 69.00 | 67.00 | 9.55 | 20.20 | 64.90 | 98.50 |
| pc | off | full | 2 | 98.78 | 73.32 |  |  |  |  |  |  |  |
| pc | off | pure-perf | 2 | 98.46 | 74.62 |  |  |  |  |  |  |  |
| pc | off | stock | 2 | 75.84 | 60.52 |  |  |  |  |  |  |  |
| pc2 | off | full | 2 | 47.32 | 33.63 | 0.00 | 67.29 | 67.82 | 76.02 | 123.59 | 96.44 | 97.77 |
| pc2 | off | pure-perf | 2 | 46.87 | 33.44 | 0.00 | 67.56 | 67.91 | 76.19 | 126.54 | 96.31 | 98.00 |
| pc2 | off | stock | 2 | 41.72 | 29.18 | 0.00 | 66.44 | 67.57 | 75.94 | 122.54 | 98.96 | 97.91 |

## Ally chunks

ROG Ally, Linux/Proton, frame generation off. One row is one fight chunk.
a1 is stock, a2 is pure-perf, a3 is full. The means above include every valid row.

| chunk | arm | avg_fps | low1_fps | hitches | cpu_temp | gpu_temp | cpu_power | gpu_power | cpu_util | gpu_util |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| n135-a1-r1 | stock | 35.15 | 28.71 | 0.00 | 70.00 | 68.00 | 9.20 | 20.10 | 64.00 | 98.80 |
| n135-a2-r1 | pure-perf | 34.96 | 32.52 | 0.00 | 69.00 | 67.50 | 8.50 | 20.00 | 53.80 | 99.00 |
| n135-a3-r1 | full | 33.06 | 25.36 | 0.00 | 68.90 | 66.70 | 9.30 | 20.20 | 57.20 | 98.60 |
| n135-a1-r2 | stock | 34.11 | 30.87 | 0.00 | 68.00 | 66.00 | 9.90 | 20.30 | 65.80 | 98.20 |
| n135-a2-r2 | pure-perf | 30.20 | 20.35 | 3.00 | 68.30 | 65.80 | 10.20 | 20.10 | 62.10 | 97.90 |
| n135-a3-r2 | full | 44.97 | 23.42 | 6.00 | 68.30 | 66.10 | 10.20 | 20.30 | 62.50 | 96.80 |

Sources:
- `captures/final-full/ally/results.csv`
- `captures/final-full/pc/results.csv`
- `captures/final-full/pc2/results.csv`
