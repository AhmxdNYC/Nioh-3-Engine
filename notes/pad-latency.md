# PadLatency: controller input -> Present / GPU-done, measured inside the game (ASI 1.7.35)

Why: the user plays with a controller. PresentMon's click-to-photon / all-input-to-photon only see mouse and keyboard
input, so a play session with a pad leaves them empty. `[Debug] PadLatency = 1` (default 0) measures the pad path inside
the mod. Measurement only: the game reads the same pad states at the same time. Not run in game yet.

## What the game uses for pads (dump imports + name strings, checked by `tests\run_padlat_harness.py`)

| API | used for | PadLatency |
|---|---|---|
| `XINPUT1_4` #2 XInputGetState (IAT 0x38E2C50), #3 SetState | Xbox-style pads, also Steam Input's virtual pad (PC and Ally) | **hooked** (IAT chain, like the mod menu) |
| `DINPUT8` DirectInput8Create -> GetDeviceState | keyboard (256 B) and mouse (notes\input-thread.md 2) | no (PresentMon covers kb/mouse) |
| `HID.DLL` + ReadFile on HidReaderThread0..2 | DualShock / DualSense when connected natively | no (not covered; Steam Input turns them into XInput) |
| RawInput, GameInput, Windows.Gaming.Input | not imported, no name string | - |

All XInputGetState calls go through the IAT (`FF 15`); the X1 cave also calls through the slot (`x1_iat`), so it is covered.
The game reads the pads on `[App]InputUpdateWorker` once per logic frame: PadPollDevices (connected slots), the active-pad
scan (slots 0..3) and the active slot again = ~6 calls per frame.

## Method

1. **Read stamp.** `HkXInputGetState` takes QPC before and after the original call. Per slot it keeps the last state; a change
   counts when buttons differ, a trigger crosses XINPUT_GAMEPAD_TRIGGER_THRESHOLD (30), or a stick leaves / enters its XInput
   deadzone (7849 / 8689) or changes octant. Raw stick jitter inside the deadzone is ignored. The change is stamped at the QPC
   after the call (the moment the game has the state). The first read of a slot is the baseline (no event).
2. **Poll interval.** Calls more than 2 ms apart start a new poll round. Round start -> next round start = the game's own pad
   sampling period (= the logic frame at a held cap; with IK the same period, only the phase moves).
3. **Frame hand-off.** The TS probe ring (caves.asm `lat_ring`, IK/TS sites, notes\input-latency.md 6.3), the same chain as the
   TS `K->P sample->present` median: change at t -> **C** = next pad-consumed stamp (5ACAAA, the logic frame that uses this read)
   -> **L** next limiter exit -> **W** next render begin -> **P** next present hand-off (3D6552 SetEvent) -> the next hooked DXGI
   Present at or after P. PadLatency switches the TS probes on by itself (`c.ts = 1`, logged "(for [Debug] PadLatency)"); with
   IK = 0 the K/C caves behave stock. Without the osd Present hook the P stamp stands in (logged as `present=TS P stamp`).
   A Present more than 250 ms after P (hook gap, swapchain switch) is not taken: that change counts as unmatched.
4. **GPU done.** osd's Present hook (drawing swapchain, before the original Present) calls `padlat::OnPresent`: one
   `Signal(ownFence, ++v)` on the game's present queue, so `v` completes after all of that frame's work on that queue. Thread
   "GPU waiter" uses SetEventOnCompletion and stamps QPC when the value is reached (a batch of completed values gets the time it
   saw them: later, never earlier; Windows wake ~10-50 us, more under Wine). PadLatency keeps the osd Present hook installed
   even with FpsCounter / StartupPopup / menu all off. DXGI frame statistics are not used (no module tracks them, and vkd3d
   does not report them); present -> display comes from PresentMon on the PC (below).
5. **Report** every `PadLatencyReportSec` (default `[Profiler] ReportSec` = 30):
   - `PadLatency 30s: changes N (buttons, triggers, sticks), matched, unmatched | input->present p50 p95 p99 ms (buttons p50) |
     input->GPU-done p50 p95 p99 ms (n, missing) | poll interval p50 p95 p99 ms (rounds, reads/round)` (window, exact)
   - `PadLatency total: ...` (whole session, 0.05 ms histogram bins) and the sources (`present=DXGI Present hook, gpu=own fence,
     frames=TS ring`).

Reading it: input->present is from **the game having the state** to the Present of the frame that used it. The press itself
happened up to one poll interval earlier (uniform, mean = interval / 2) plus the pad/USB/Steam Input time (not visible to
software). So button -> photon ~= poll interval / 2 + input->present + present->display (PresentMon) + scanout to the middle
of the screen. `tools\playsession\summarize.py` prints that estimate next to PresentMon's display latency.

## Cost and safety

- Two QPC reads + a compare per XInputGetState call (harness: ~40 ns per call, ~6 calls per frame); one ring push per change.
- One fence Signal per presented frame on the game's queue; one extra thread wake per frame (GPU waiter); a report thread
  waking every 100 ms. TS probes: one ring store per stamp (5-6 per frame).
- Register safety: an IAT (import call) hook, plain Win64 ABI; the result and the XINPUT_STATE are returned untouched
  (harness: identical for 8000 calls incl. empty slots). Exceptions in the hook path stop tracking only.
- Co-op / online: reads nothing the network sees, changes nothing.

## Offline tests

`py -I mod\Nioh3EngineFix\tests\run_padlat_harness.py` -> `PADLAT: ALL PASSED`:
change classes; a synthetic 60 Hz timeline with the game's 6 reads per frame, stock order (poll at the end-of-frame kick, used
by the next frame: 32.257 ms exact) and IK order (23.540 ms exact), GPU done +9 ms exact, poll interval 16.667 ms, noise not
counted; no-Present-hook fallback; lost GPU completions; expiry when no logic frame consumes; the 250 ms guard; ring with 4
producers and a concurrent consumer (no torn records); the real hook on a fake IAT slot (pass-through, counts, cost); import
check of the dump; source wiring. `test_patterns.py` ALL 228 SITES OK (no new sites: the TS/IK sites are reused).

## Open

- Live check: the log line `PadLatency: hooked the game's XInputGetState ...`, then `PadLatency 30s:` lines with matched ~=
  changes, poll interval ~= 16.7 ms at a 60 cap, input->present ~30-40 ms stock (expected from notes\input-latency.md 4).
- Sony pads on the native HID path: would need a hook on the HidReaderThread report parser (not done).
- Ally: same code path (Wine xinput, vkd3d fence); present->display has no PresentMon there - use TS `input->display`.
