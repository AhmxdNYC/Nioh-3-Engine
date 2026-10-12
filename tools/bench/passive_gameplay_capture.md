# Passive gameplay capture (Linux / SteamOS)

Stdlib Python; no installation, uploads, input, game launch, logging toggles, configuration writes or original-log deletion. The game and explicitly named Engine/MangoHud logs must already exist. `/proc` supplies identity; this collector is for Linux/SteamOS, although pure fixtures run on Windows too.

Run during an existing boss fight, with operator-verified values:

```sh
python3 passive_gameplay_capture.py --pid 12345 --start-ticks 987654 \
  --exe /exact/proc/exe/link/target \
  --engine-log /game/Nioh3EngineFix.log \
  --mangohud /existing/MangoHud.csv \
  --hash-file /game/Nioh3EngineFix.asi --hash-file /game/Nioh3EngineFix.ini \
  --boss "Yamagata" --mission "67064" --phase combat \
  --seconds 25 --output /existing/benchmark-receipts
```

`--start-ticks` is `/proc/PID/stat` field22. Parse after the final `)` of the process name; spaces in that name make a naive whitespace-column parser wrong. `--exe` is the exact `/proc/PID/exe` symlink target, possibly Wine's executable rather than Nioh3.exe under Proton. The operator must identify the actual game PID; identity checking is not game-image discovery. PID/start/exe/boot ID are checked throughout. Process exit/replacement aborts with a retained failure manifest.

At least one log is required; omit `--mangohud` when no CSV is already growing. Default25 seconds, maximum60. The collector records initial byte offsets and keeps only new complete lines, dropping the continuation of any preexisting partial line. Original MangoHud schema and two prelude lines are retained separately; Engine header/schema lines are retained if found in the initial256KiB prefix. Missing Engine schema is explicitly reported. Each input is capped8MiB, lines256KiB. Replacement/truncation and changes to the initial trailing fingerprint fail closed. Same-inode rewrites elsewhere outside that fingerprint are not exhaustively detected; no filesystem lock is acquired. Reads are bounded ordinary local filesystem operations, not a hard real-time deadline if storage blocks.

Output is a unique directory, ZIP and SHA256 sidecar, with UTC interval, process identity, raw appended data, hash-only ASI/config metadata and sampled-frametime summary. Explicit hash files are restricted to regular ASI/INI/CFG/CONF/XML files <=32MiB each; contents are never copied. Their hashes are compared at start/end. Operator-supplied logs themselves may contain sensitive paths and should be reviewed before sending. The collector never sends them.

Percentiles use only newly appended positive finite MangoHud `frametime` rows with strictly increasing `elapsed` nanoseconds. These are **logger-sample percentiles**, not necessarily per-presented-frame percentiles. No historical rows, synthesized FPS, utilization-derived GPU/CPU milliseconds, or inferred GP2 FPS. GP2 buckets remain raw diagnostics. A complete capture with zero frametime rows has no timing percentile; it is not a performance result. No proof is made that the supplied PID wrote the supplied log or that the window stayed in combat; lead review supplies that context.

The optional boss/mission/phase labels are supplied by the operator, not detected by the collector. Mark interrupted combat as `mixed`, or capture loading/death separately. `timing_samples_eligible` requires a complete capture with positive row count and no invalid rows; failed or empty captures retain diagnostic data without becoming accepted timing results. Reports include sample p50/p95/p99, subject to the sampling limits above.

Fixtures: `python tools/bench/test_passive_gameplay_capture.py`. They use temporary files and a fake clock/process provider, never an actual game capture.
