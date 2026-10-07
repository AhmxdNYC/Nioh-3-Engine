# GPU profiling (RGP) — headless pipeline for Nioh 3

Status 2026-10-06: **working end to end.** Daemon armed before launch -> game launched -> RGP_TRIGGER
-> `captures/rgp/Nioh3_2026-10-06_14-16-02.rgp` (191 MB, 12 s after the trigger). The stdin `c` trigger
works once the game is connected. First analysis: `notes/gpu-profile.md`.

Files:
- `patcher/gpu/rgp_daemon.ps1` — capture daemon (wraps `RadeonDeveloperPanelCLI.exe`, file-triggered)
- `patcher/gpu/rgp_report.py` — `.rgp` -> per-segment / per-event / per-pass cost tables (+ shader ELF dump)
- `patcher/gpu/rgp_markers.py`, `sqtt_rdna4.py`, `rdf.py` — RDF container reader, RDNA3/4 SQTT token
  decoder (stdlib port of tinygrad's decoder, which comes from AMD's open-sourced rocprof-trace-decoder
  definitions), RGP marker extraction
- `patcher/gpu/rgp_frame.py` — frame-level view: CB timeline per queue, GPU idle gaps, async overlap, wave occupancy per time bin
- `patcher/gpu/isa_stats.py` — disassembles every dumped PSO with rga (handles the no-spaces staging) -> `isa/<pso>.txt` + `isa_stats.csv`
- `patcher/gpu/rgp_uia.ps1` — reads RGP GUI values through UI Automation (Frame summary only; see limits)
- Captures + logs: `captures/rgp/`

## What runs headlessly

| Step | Headless? | Notes |
|---|---|---|
| Arm profiling (dev-mode router) | yes | `RadeonDeveloperPanelCLI -m profiling -p Nioh3` hosts its own router. **Do not also run `RadeonDeveloperServiceCLI`** — the Panel CLI then fails with `Failed to create router`. |
| Trigger capture | yes (file trigger -> `c` on the CLI's stdin) | verified with Nioh3 2026-10-06 (the CLI only reads commands after `Process connected`). Output is named `Nioh3_<date>_<time>.rgp`. Fallback: `-AutoFrame N`. |
| Per-pass/per-event table | yes, own parser | RGP has **no CLI export** (`RadeonGPUProfiler.exe` only has GUI "Save Event timing CSV"). |
| RGP's own numbers | partly | `rgp_uia.ps1` reads the Frame summary pane (frame duration, GPU idle, event counts) with the window minimized. Switching panes through UIA does not work (Qt ignores Invoke/Select on the overview list) — that would need a real click. |
| Shader ISA of hot PSOs | yes | `--dump-shaders` writes the pipeline's ELF code object; `rga.exe -s bin --isa out.txt x.elf` disassembles it (rga cannot read paths containing spaces — copy to a space-free path first). |

## Restart requirement

**Yes — the game must be (re)started after the daemon is running.** The D3D12 driver only enables
SQTT/RGP instrumentation for a process that connects to the developer-mode router when it creates its
device; an already-running Nioh3 cannot be attached. (Verified: with no router, nothing listens; the
CLI waits at "Waiting for GPU application to connect...".)

## Exact capture procedure

State at hand-off: daemon already started at 2026-10-06 ~14:00, `captures/rgp/daemon.state` = `WAITING`.
Processes: one hidden `powershell.exe` (daemon) + `cmd.exe` + `RadeonDeveloperPanelCLI.exe` (~23 MB).

1. (Only if not already armed) start the daemon BEFORE launching the game:
   ```powershell
   Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','"C:\Users\Ahmad Hamza\nioh3-engine\patcher\gpu\rgp_daemon.ps1"'
   Get-Content 'C:\Users\Ahmad Hamza\nioh3-engine\captures\rgp\daemon.state'   # -> WAITING
   ```
   Options: `-SqttBuffer high` (default; raise to `maximum` if the trace is truncated, lower if the
   game shows corruption), `-AutoFrame N`, `-InstructionTracing` (big files, not needed).
2. Launch Nioh 3 normally (Steam). The CLI connects to `Nioh3` only (`-p Nioh3` filter).
3. In actual gameplay (same scene each time for comparable numbers), create the trigger:
   ```powershell
   New-Item 'C:\Users\Ahmad Hamza\nioh3-engine\captures\rgp\RGP_TRIGGER' -ItemType File
   ```
   The daemon sends `c`; the driver captures the next frame (clocks forced to **peak** for the capture
   frame only) and the CLI writes `profiling_<timestamp>.rgp` into `captures/rgp/`.
   Watch `captures/rgp/daemon.log` for `NEW TRACE ... (xx MB)` (state goes CAPTURING -> IDLE).
   If nothing appears in 180 s it re-sends `c` (3 tries). One trigger at a time.
   Expect a hitch of a second or so on the capture frame; the trace is ~50–300 MB.
4. Analyze (game can keep running or be closed; uses mmap + compact arrays; Nioh3 trace: ~1 min, <1 GB RAM):
   ```powershell
   py "C:\Users\Ahmad Hamza\nioh3-engine\patcher\gpu\rgp_report.py" "C:\Users\Ahmad Hamza\nioh3-engine\captures\rgp\profiling_<ts>.rgp" --dump-shaders
   ```
   then `py rgp_frame.py <trace>.rgp > <trace>.frame.txt` and `py isa_stats.py <trace>_shaders`.
   Faster first look: `--no-waves` (skips wave decoding; splits a segment's time equally among its
   events) or `--ses 0` (decode one shader engine only).
5. Stop + cleanup (any time; best after the last capture):
   ```powershell
   New-Item 'C:\Users\Ahmad Hamza\nioh3-engine\captures\rgp\RGP_STOP' -ItemType File   # daemon sends q, kills CLI, exits
   # hard stop if needed:
   Get-Process RadeonDeveloperPanelCLI -ErrorAction SilentlyContinue | Stop-Process -Force
   ```

If step 3 never produces a file (stdin command path not accepted), check `cli.log` (CLI output is
block-buffered and may only appear when it exits). Fallback: stop the daemon, quit the game, restart the
daemon with `-AutoFrame N` (N = frames since launch at which you will be in gameplay, e.g. 120 fps × 180 s
= 21600), relaunch the game and be in the scene before that frame.

## Side effects / what to revert

- No persistent system change. Developer mode is per-connection: it ends when the Panel CLI exits.
  Driver experiments are not used (they revert when the panel closes anyway).
- Clocks: RGP forces **peak** clocks only during the captured frame. We do not use
  `-m clocks --clock-mode stable` (note: if ever used, GPU index **1** is the RX 9070; index 0 is the
  iGPU — `--gpu-index 1`; reverts to normal when the CLI exits).
- While connected, the driver adds SQTT marker writes to every command buffer (small CPU/GPU overhead).
  **Don't take FPS/benchmark measurements in a session launched under the daemon.**
- The trigger file must not be created while Nioh3 isn't connected (it would just be consumed and retried).

## Outputs of rgp_report.py

- `<trace>.summary.txt` — frame span, queues, clock ratio, **top segments**, **pass rollup**.
- `<trace>.segments.csv` — barrier-delimited segments: queue, start, duration, #events, dominant pass, PSOs.
- `<trace>.events.csv` — every draw/dispatch: time, segment, DX12 API (DrawInstanced / Dispatch / …),
  threadgroup dims, pass path, PSO hash, approx waves / wave-time, estimated µs.
- `<trace>.passes.csv` — per pass: `est_us` (incl. wave-weighted share of mixed segments) and
  `exact_us` (only segments that contain nothing but this pass).
- `<trace>_shaders/` (with `--dump-shaders`) — `pso_map.csv` + ELF code objects of the hottest PSOs.

### How timing is derived (and its limits)
- The driver writes RGP instrumentation markers (CB start/end, per-draw/dispatch EVENT, BARRIER
  start/end, PIX/AGS USER_EVENT push/pop, BIND_PIPELINE) through `SQ_THREAD_TRACE_USERDATA_2/3`
  (REG tokens 0xC342/0xC343; REG bits [9:7] = writing pipe: 0 = gfx, 5 = async compute in the sample).
- SQTT deltas are shader-clock cycles; TS_DELTA_OR_MARK tokens with low bits `0x081` carry the absolute
  100 MHz GPU timestamp — used to map cycles -> ns (interpolated between anchors; robust to the
  occasional mis-framed delta). Markers are decoded on every SE and the median time is used
  (SE-to-SE agreement is typically < 0.1 µs).
- **Segment = [last drain before its first event, last drain before the next event].** Back-to-back
  barrier chains with no work in between are folded into the preceding segment (`drain_tail_us` column),
  since the preceding work isn't really finished until those flushes/EOP waits complete.
- (Original definition: interval between consecutive BARRIER_END markers on a queue.) A barrier end is written
  after the wait for the preceding work, so segment duration = real GPU time of the events in it.
  This is the reliable number. Passes in a DX12 engine are nearly always separated by barriers.
- Inside a segment with several events, the split is approximate (each wave is credited to the most
  recent EVENT marker whose segment is still open). Async-compute overlap makes that split wrong
  for the overlapped part; the segment totals stay right. Per-event numbers are a guide only.
- Pass names exist only if the game emits PIX/AGS markers RGP can see (PIX needs the app to call
  `SetUserDefinedAnnotationMode(DRIVER_RETAIL)`; a retail Nioh 3 likely doesn't). Without names, passes
  are keyed by PSO hash -> use `--dump-shaders` + rga ISA (or Ghidra) to identify what each hot
  pipeline does (GI, volumetrics, shadows, SSR, AO, post...).
- DX12 api_type codes 0/2/9/34 are verified against RGP's own event statistics on the sample
  (DrawInstanced / Dispatch / ClearRenderTargetView / DevDriverOverlay); the rest are guesses (marked `?`).

### Validation on the RDTS sample (D3D12nBodyGravity, RX 9060 XT)
RGP GUI: frame duration 2325 µs, 10 DevDriverOverlay dispatches, 2 Dispatch, 2 DrawInstanced,
1 ClearRTV. Parser: marker span 2249 µs, same event counts/types, markers parse cleanly (2 junk dwords
rejected), user markers "Draw particles", "SimulationStart/SimulateStep 0/1" recovered; the compute
step is a clean barrier-isolated segment of 1258 µs; the two particle draws form one 2126 µs gfx segment.

## Reference
- RDF container: `AMD_RDF ` magic, index at end, 64-byte entries; chunks seen: AsicInfo, ApiInfo,
  ClockCalibration, CodeObject, COLoadEvent, PsoCorrelation (96-byte records: api_pso_hash u64 at +8,
  pipeline hash 16 B at +16), SqttData (one per SE; header u32 SE index at +4), SpmSession/SpmCounterData,
  QueueInfo, QueueEvent, TraceConfig (JSON), SystemInfo, DriverOverrides, TraceProcessInfo.
- SQTT tokens: tinygrad `tinygrad/renderer/amd/sqtt.py`; marker layouts: Mesa `src/amd/common/ac_sqtt.h`.
