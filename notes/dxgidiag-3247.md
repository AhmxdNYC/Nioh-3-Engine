# Opt-in DXGI diagnostic capture / bridge 3247

Implemented on `ally/dlss-fg-dxgi-3235`, following the evidence design in
`dlss-fg-dxgi-3235.md`. This is a diagnostic implementation, not a fix for the
unreproduced Nvidia crash. No game launch or installed binary/configuration change.

## Behavior

- `[Debug] DxgiDiag=0` default. Only the configuration read and fast disabled checks
  run: no diagnostic COM queries, worker, output or additional hooks. Existing
  Present arguments, positive statuses, retry condition and return values are retained.
- Mode 1 samples the first 16 Present calls and one whole call per second, plus all
  failures. Mode 2 also traces every call for 30 seconds from first Present.
  Latest entry, return, failure and 64 outstanding call slots update even when trace
  events are suppressed. Retry results are separate records; retry success never
  erases the last failed HRESULT. Nested Present1 -> Present calls have distinct IDs.
- Restart-only `DxgiDiagPresentPassthrough=1`, effective only with successful capture
  initialization, skips Hooked (ApplyLatency/drawing) and PresentArgs. Existing
  installation/ECL hooks and unrelated patches still run. No new retries are added.
- Entry before observation/overlay work; overlay/filter/original/retry/return/finish
  stages; CPU overlay faults; dummy factory/device/queue/chain entry/return; hook-slot
  predecessors; SetColorSpace1 entry/return; queue provenance when BuildCtx selects
  a queue. Diagnostic installation validates IDXGISwapChain3 before reading its
  color-space slot. No queue pointer is dereferenced by diagnostics.
- Actual first observed swapchain GetDesc and GetDevice HRESULTs, adapter LUID,
  EnumAdapterByLuid identity and CheckInterfaceSupport driver result. Failures are
  explicit. No GetBuffer and no retained diagnostic COM reference. On the first
  DEVICE_REMOVED/RESET, query removed reason through the live device; failed GetDevice
  is distinguished from a returned reason. No exception handler, debug layer or DRED
  is installed. Present1 retains its original parameter pointer unchanged.
- Module basenames/versions (worker only), mod version/build timestamp, OS/Wine,
  patch group counts/skipped names and relevant startup INI values. FG activation
  remains unknown. Settings are labeled INI rather than claimed as live overrides.

## Storage and recovery

`Nioh3EngineFix-dxgi.bin` is 40,464 bytes beside the INI. It contains a fixed POD
schema, 256 rolling **event** slots, 64 pending slots and separate latest records.
Records have sequence/commit markers, explicit publication fences and FNV-1a
checksums. A crash can leave invalid records; recovery ignores them. Header/counters
are best effort, not checksummed. Pending slot collisions, ring overwrites and
suppression are counted; try-lock capture drops are counted in the live report.
A dropped finish can leave a stale pending slot: pending is evidence of incomplete
capture, not proof a thread is hung. Concurrent capture never waits for the worker;
metadata publication uses a short lock with no external COM call under it.

The process-lifetime worker wakes every 250 ms or on failure notification. It
flushes the mapping/file, writes a 15-line report and <=64 KiB detail snapshot to
temporary files, flushes them, then atomically replaces the previous text files.
Hook recording does not allocate or synchronously write a log per frame. First-chain
metadata invokes COM once while the object is live. Worker I/O can take longer than
250 ms; recent events can be lost on termination/OS failure. There is no shutdown
handler and no claim that a live snapshot is a complete session.

Copy files before restarting; capture is replaced per session. Recover binary
records using `mod/Nioh3EngineFix/tools/dxgidiag_report.py`. It validates exact schema,
length and individual records, sorts committed events and prints a 15-line report.
Optional JSON contains only the bounded committed ring. Offline address-to-module
resolution requires the flushed detail/report. The implementation intentionally
labels unobserved creation/generation/lifecycle coverage rather than inferring it.

## Validation and build

Executed on Linux:

```
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread mod/Nioh3EngineFix/tests/dxgidiag_harness.cpp -o /tmp/dxgidiag-harness
/tmp/dxgidiag-harness /tmp/dxgidiag-fixture.bin
python3 mod/Nioh3EngineFix/tests/test_dxgidiag_report.py
```

PASS: production forwarding template preserves arguments/opaque parameter identity,
positive status, failure and exact pre-existing retry; nested pending IDs; failed
result survives successful retry; checksum/commit rejection; 80,000 concurrent
serialized writes; bounded ring overflow and pending collision accounting. Three
Python checks pass for cross-language fixture decoding, 15-line output, torn-record
rejection and malformed input. These do not validate real COM or Win32 behavior.

Both CMake and `build.bat` include `dxgidiag.cpp`. No cl/clang-cl, ml64, Windows SDK,
or Windows cross-compiler is present on ally, so no ASI was built. PC/pc2 must run
`mod\Nioh3EngineFix\build.bat` and review build warnings before distributing. Runtime
FG off/on/toggle/passthrough validation still needs an authorized Windows session.
