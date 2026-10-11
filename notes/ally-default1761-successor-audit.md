# Ally default1761 archive audit - 2026-10-11 successor

Bridge file232 SHA2562f3564f038c18fb99e6a336950333b0998405575a0cae28fbfce1edfc14696a7
was downloaded privately to scratch/ally-default1761-file232.zip. SHA and ZIP
CRC verified. Six full MangoHUD CSVs, window summaries/times, restore receipts,
save lists and remotecache snapshots are present. Each of six save-after lists
is byte-identical to saves_before.txt. These are archived local receipts, not
independent remote cloud verification.

Two rounded200-sample window means per launch yield1A39.445,2B40.655,
3B40.895,4A39.460,5A39.665,6B40.520 FPS. Adjacent pairs(1A/2B,4A/3B,5A/6B)
reproduce3.06756%,3.63659%,2.15555%: mean2.95324%, sample SE0.43134%.
This reproduces the reported rounded-summary arithmetic, not exact raw windows.

arm.sh uses tail-n200 snapshots w1.csv/w2.csv after each20s interval; those12
slices are missing from the ZIP. Clock strings alone do not uniquely identify
the rows in the full elapsed-time CSV. Requested slices or exact elapsed-index
bounds in bridge1964. The reported aggregate p99 averages window quantiles;
it is not a pooled per-frame percentile. Data are100ms samples, not frame traces.
Actual game FG-menu observation was missed in the historical run, confirmed1959.
A new fog test cannot retroactively close that old gate.

Ally ACK1962 holds both locks for isolated fogpar on USER Engine1.7.44, fixed
GHDM and user settings. Root1963 requires fresh current-entry backup before any
change, exact full hashes/execution digest, actual FG-menu observation, native
image/noise gate, then profiler-off ABBAAB only if all prerequisites pass.
No new FPS gain is established. Root local game remains absent.
