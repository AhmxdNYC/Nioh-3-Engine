# [App]InputUpdateWorker: what it does, and what can be saved

Nioh 3 Steam v2.02, static analysis of the unpacked exe dump (raw offset == RVA, base 0x7FF74F410000) plus the Windows
cpuprof captures. Script: `input_live.fsx` (private dev repo). The symbols named here are in symbols/symbols.tsv.

## TL;DR

* **It is not a polling loop.** The thread sleeps on a condition-variable event and runs once per game frame. It is
  kicked by the frame task (`UpdateDispatchB` -> `InputWorkerKick` 5AB7B0) and joined later in the same frame. Nothing
  in it uses Sleep(0), Sleep(1), a timer or a spin. Pacing it to 1 kHz would **add** wake-ups (it wakes at the frame
  rate now, which is 60-120/s). No pacing patch is proposed.
* So ~8,500 wakes/s on the ROG Ally cannot come from the loop itself. They have to come from blocking calls **inside**
  the per-frame body, or from another thread. That works out to about 70-140 blocking syscalls per frame at 60-120 fps.
  The static call inventory is below (section 2). The Wine-side cost of those calls depends on the Wine/Proton version
  and cannot be pinned down from the binary. Section 6 gives the commands that settle it on the Ally.
* The one safe saving inside the body is **X1**. Every frame, the "which pad is active" scan calls `XInputGetState`
  on all 4 slots, connected or not. On Windows an empty slot makes XInput 1.4 re-enumerate devices
  (cfgmgr32 + DeviceIoControl). That is most of the thread's CPU: XInputGetState is 2.7 % of the thread's samples in
  the "ingame" cpuprof capture and 0.5 % in "r7-A1", against 4.2-4.4 % CPU for the whole thread.
  X1 skips the slots the game itself already knows are unplugged.
  Expected saving: about 0.5-2.5 % of a core on Windows, most of it kernel time. Input latency does not change.
  On Proton the saving is negligible, because Wine's XInputGetState returns at once for an empty slot.

## 1. Thread, loop and frame sync

| what | RVA | detail |
|---|---|---|
| name string | 0x3BEEFE0 (copy at worker+0x160 = 0x45C3970) | `"[App]InputUpdateWorker"` |
| creator | `InputUpdateWorkerCreate` 0x2973E3C -> `InputUpdateWorkerStart` 0x2973AD0 | `AppWorkerThreadStart(&g_InputUpdateWorker 0x45C3810, InputUpdateWorkerMain, ..., stack 0x40000, name)` |
| generic creator | `AppWorkerThreadStart` 0x2936554 (35 callers) | worker+0x158 = entry, name to +0x160, thread via 0x5B7434, trampoline 0x2937114 `jmp [rdx+158h]`. The CRT start is BDB740 -> 5B6670 (every thread in the profiles shows that start address) |
| loop | `InputUpdateWorkerMain` 0x1A7B8B4 | `KtglEventWait(w+8); while (!xchg(quit w+0x150)) { vf[2](w); KtglEventSet(w+0xA8); KtglEventWait(w+8); }` |
| wait primitive | `KtglEventWait` 0x3D7CC8 | MSVC `_Mtx` + `std::condition_variable` = SRWLOCK + `SleepConditionVariableSRW`, INFINITE. The profiles agree: 95-98 % of the thread's samples are in `NtWaitForAlertByThreadId`. Under Wine this is one futex wait per frame |
| body | `vf[2]` = `InputUpdateWorkerRun` 0x435418 (vtbl 0x3B88660) | `PadUpdateIfMgr()` -> `KtglEventSet(w+0x1A0)` ("pad stage done") -> `if (g_MouseKbEnabled 0x45BA540) MouseKbUpdate()` |
| kick | `InputWorkerKick` 0x5AB7B0, from `UpdateDispatchB` 0x5AB644 <- `FrameDtSetup` <- `FrameTask_Update` | joins the previous run, resets the pad-stage event, `KtglEventSet(w+8)`, pending=1. **Once per game frame** |
| join | `InputWorkerJoin` 0x5AB7E0 (from `GameModeUpdateMain` 0x39E650 and the kick) | `KtglEventWait(w+0xA8)` |
| early wait | `InputWorkerWaitPadStage` 0x86C018 in `FrameEarlyUpdate` | `KtglEventWaitFor(w+0x1A0, 1 ms)`: **the main thread waits up to 1 ms for the pad stage** |

So the rate is the game's logic frame rate. The pad stage is also on the main thread's path, with a 1 ms cap, so time
saved inside `PadUpdateIfMgr` can shorten the main-thread frame when the frame is short.

## 2. What it reads every frame (static inventory)

`PadUpdateIfMgr` 0x437258 -> `PadMgrUpdate` 0x43671C:

| call chain | API per frame | notes |
|---|---|---|
| `PadPollDevices` 0x434E18 (always first) | `XInputGetState` x (connected slots). Plus x4 every 2.0 dt-units, accumulated at PadMgr+0x698A0 | per-slot "connected" byte PadMgr+0x69850+i*0x14. Set by the 2 s probe, cleared as soon as a poll fails |
|  | `IDirectInputDevice8::GetDeviceState(256)` keyboard (`DiKeyboardGetState` 0x435D58) | |
|  | Sony HID pads (libScePad-like code at 0xAE4xxx-0xAF7xxx): 3 calls per open handle, each under a CRITICAL_SECTION | only when a DualShock/DualSense is present. On connect: SetupDi lookup `PadDeviceInfoLookup` 0x43506C |
| `PadUpdateActive` 0x434D24 -> `PadActiveScanWrap` 0x435B1C -> `XInputFindActivePad` 0x435B8C | **`XInputGetState` x5**: slots 0..3 always, then the active slot | the X1 target (section 3) |
| 0x43837C -> 0x1FDBB40 | `XInputSetState` (rumble) when needed | |
| alternative branch (`[0x4759910]+0x1AB` / `[0x45B9E70]+0x3E`) | 0x296D3E8: resets the 4 slots, no API | |

`MouseKbUpdate` 0x435698 (g_MouseKbEnabled = 1 in the dump):

| function | API per frame |
|---|---|
| `DiMouseGetState` 0x4371DC | `GetDeviceState(DIMOUSESTATE2)`, plus Acquire and a retry if that fails |
| `CursorUpdate` 0x435864 | `GetCursorPos`, `ScreenToClient`, `GetClientRect`. When the cursor is recentred: `ClientToScreen` + `SetCursorPos` |
| `MouseButtonsPoll` 0x435E08 | `GetClientRect`, `GetAsyncKeyState` x5-9 (VK 1/2 via 0x99F5C0/0x99F5E8, then 4, 5, 6) |
| `MouseDeltaLatch` 0x435748 | `WaitForSingleObject(mutex [g_pMouseKbState]+0x418)` + `ReleaseMutex` (deltas accumulated by the WndProc), `GetClientRect` |
| tail of 0x435698 | `GetClientRect` |

Total: about 25-30 user32/xinput/dinput/kernel calls per frame, and 1 condvar wait. On Windows this is about
40-80 µs per frame. It is cheap apart from the empty-slot XInput enumeration.

The thread does **not** use RawInput, Steam Input (`SteamInput006` has no code reference), `GetKeyState`, HID polling
or a timer. The Steam overlay (gameoverlayrenderer64) hooks XInputGetState and the cursor/key APIs, so it appears inside
these calls in the profiles.

## 3. X1: skip known-empty slots in the active-pad scan

The 435B8C loop, unchanged by the patch apart from the call at 435BB7:

rsi = padMgr (`[45BA1E0]`, loaded at 435B48), rdi = the XINPUT_STATE buffer, ebx = slot 0..3 (loop head 435BB2, the
slot goes to ecx before the call):

```
435BB7 call [XInputGetState]       ; FF 15 93 D0 4A 03  (IAT 38E2C50 = XINPUT1_4 ord 2)   <-- X1
435BBD test eax,eax / je 19A9B2C   ; connected: activity test -> PadMgr+69B20 = slot
435BC5 inc ebx / cmp ebx,4 / jl 435BB2
435BCC mov ebx,[rsi+69B20h]        ; then XInputGetState(active) at 435BD7 (left stock)
```

Patch: `435BB7: 90 E8 <rel32 cave>`. `call_at = 1`, so the return address stays 435BBD. The cave:

```
cmp dword [en],0 / je pass
cmp ecx,3 / ja pass
mov rax,[&g_pPadMgr] / mov rax,[rax] / test rax,rax / je pass
mov r11d,ecx / lea r11,[r11+r11*4]
cmp byte [rax+r11*4+69850h],0 / jne pass   ; PadMgr::Connected[slot]
inc dword [skipped] / mov eax,48Fh / ret     ; ERROR_DEVICE_NOT_CONNECTED, API not called
pass: inc dword [passed] / mov r11,[&IAT] / jmp qword [r11]   ; tail call through the current IAT (hooks kept)
```

Offline proof (`input_live.fsx offline`, all passed):
- the site bytes match, the call goes through the XInputGetState IAT, and the loop context matches.
- `PadPollDevices(g_pPadMgr)` runs before `PadUpdateActive` in `PadMgrUpdate` (43671C). The probe code at 43502C..
  writes the connected bytes.
- The pattern is unique in the executable sections (site = hit + 13). The padMgr-load pattern is unique at 435B48.
- No overlap with any other patch set (cpu_caves, fg, vrr, mainthread, W1/W2, meshlet, pool, workers, patches.json).
- No direct branch or call target lies strictly inside 435BB7..435BBD (full sweep of .text).
- r11 and the flags are dead at 435BBD and in the cold block 19A9B2C. The cave clobbers only rax (the result), r11 and
  the flags. r11 is volatile and is never an argument.
- Native test: 20,000 random cases (switch off, null padMgr, out-of-range slots, random connected bytes). There were
  0 mismatches against the model "skip ⇔ switch on ∧ slot<4 ∧ padMgr≠0 ∧ connected[slot]=0; otherwise forward the
  args and return value unchanged".

Behaviour change: a newly plugged-in pad can become the **active** pad only after the next 2 s probe marks it
connected. Before X1, the scan saw it on its first frame. Unplugging takes effect at once, as before: the per-frame poll
clears the byte. Connected pads go through exactly the same calls as before, so **input latency does not change**.
The active-slot call (435BD7) is left stock, so a pad also works during the first 2 s after boot.

Expected saving:
- **Windows desktop:** with 1 pad, 3 of the 5 calls per frame no longer enumerate. With no pad (keyboard and mouse), 4
  of 5. Measured cost of those calls: 0.5-2.7 % of the thread's wall time, mostly kernel DeviceIoControl into the
  XUSB/HID stack. So expect about 0.5-2.5 % of a core less, plus a shorter pad stage, which the main thread waits on
  for up to 1 ms. On a desktop this frees a little of the SMT sibling. It is not large.
- **Proton / Ally:** Wine's xinput returns ERROR_DEVICE_NOT_CONNECTED for an empty slot after a critical-section
  check. X1 saves 3 trips per frame through the overlay hook and Wine, which is negligible.
  **X1 is not the fix for the 8.5 kHz wake rate.**

## 4. ASI mod entry (patches.json format)

```json
{ "id": "X1", "group": "X1", "rva": "0x435BB7", "len": 6, "orig": "FF 15 93 D0 4A 03", "kind": "call", "target": "hookXInputScan", "call_at": 1, "reads": "iat_disp32@2",
  "pattern": "48 8B FA 48 8B F1 33 DB 48 8B D7 8B CB FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? FF C3 83 FB 04 7C E6", "offset": 13 },
{ "id": "X1m", "group": "X1", "rva": "0x435B48", "len": 7, "orig": "48 8B 0D 91 46 18 04", "kind": "read", "reads": "ripglobal_disp32@3",
  "pattern": "48 8B 0D ?? ?? ?? ?? 48 8D 54 24 30 0F 57 C0 0F 11 44 24 30 E8 ?? ?? ?? ?? 84 C0", "offset": 0 }
```

`X1m` is read-only. It gives the mod `g_pPadMgr` (0x45BA1E0), and the mod writes nothing at that site. It needs a new
`kind: "read"` / `reads: "ripglobal_disp32@3"` in `engine-fix/tools/gen_header.py`. The W3 shape (`call`, `call_at` 1,
`iat_disp32@2`) needs no change. Both patterns are verified unique by `input_live.fsx offline`. Add them to
`engine-fix/tests/test_patterns.py` when wiring this in. The `orig` of X1m is the dump bytes: disp 0x04184691 = 0x45BA1E0 - 0x435B4F.

C++ hook, with the same semantics as the cave:

```cpp
extern "C" DWORD WINAPI hookXInputScan(DWORD idx, void* state) {
    if (g_x1On && idx < 4) {
        auto mgr = *reinterpret_cast<const uint8_t* const*>(g_padMgrSlot);     // from X1m
        if (mgr && mgr[0x69850 + idx * 0x14] == 0) return ERROR_DEVICE_NOT_CONNECTED;
    }
    return reinterpret_cast<DWORD (WINAPI*)(DWORD, void*)>(*g_x1Iat)(idx, state); // read the IAT on every call
}
```

## 5. Wine / Proton behaviour (what each per-frame call becomes)

Hedged: this depends on the Wine version. Proton 9 is Wine 9.0. Proton 10 / Experimental are Wine 10.x.
- `KtglEventWait`: one futex wait per frame. That is the thread's only wake from its own loop.
- `WaitForSingleObject`/`ReleaseMutex` on the mouse mutex: with fsync these are userspace operations plus a futex only
  under contention (the WndProc locks the same mutex on mouse messages). With `PROTON_NO_FSYNC` and esync off, each
  becomes a wineserver round-trip.
- `GetAsyncKeyState` x5-9, `GetCursorPos`: newer Wine reads these from the desktop shared memory. Older Wine makes a
  wineserver round-trip per call. winex11 `GetCursorPos` can add an X round-trip (`XQueryPointer`, through Xwayland
  under gamescope).
- `SetCursorPos` (recenter): a wineserver request plus `XWarpPointer`.
- `GetClientRect`/`ScreenToClient`/`ClientToScreen`: local for a same-process window, but they take win32u's
  process-wide user lock. The WinMain pump thread spins on PeekMessage (W3) and takes the same lock constantly, so
  these calls can contend, and each contention is a futex wait/wake pair. **Applying W3 (the existing pump cave /
  ASI W3) is the most likely Proton-side reduction for this thread's futex traffic.**
- `XInputGetState`: Wine's xinput takes a per-controller critical section. The `wine_xinput_hid_update` thread holds
  that lock while it parses each HID report (Steam's virtual pad on the Ally reports at up to ~1 kHz). Contention costs
  futex waits on the connected slot. Empty slots return at once.
- DirectInput `GetDeviceState` (mouse + keyboard): Wine dinput takes a critical section that its LL-hook thread also
  takes for every input event. It can contend in the same way.
- Sony pads only: `DeviceReadTimed` 0xAF55E0 does `CreateEventW` + `ReadFile` + `GetOverlappedResultEx` +
  `CloseHandle` **for every report**. Under Wine that is several wineserver requests per report, 250-1000 per second.
  This runs on the separate HidReaderThread0/1/2 (TIME_CRITICAL, `timeBeginPeriod` around Sleep(2) retries), not on
  InputUpdateWorker.
- The game detects Wine (`IsRunningUnderWine` 0x1230, `wine_get_version`), but uses the result only in the
  nvapi/Streamline loader (0x1300). The input code does not branch on it.

## 6. How to pin the 8.5 kHz on the Ally (read-only, no game changes)

```
PID=$(pgrep -f 'Nioh3.exe' | head -1)
for t in /proc/$PID/task/*; do echo "$(cat $t/comm) ${t##*/}"; done | grep -E 'InputUpdat|xinput|hid'
TID=<tid of "[App]InputUpdat">
grep ctxt /proc/$PID/task/$TID/status; sleep 1; grep ctxt /proc/$PID/task/$TID/status   # wakes/s; divide by fps (MangoHud)
sudo strace -c -p $TID -e trace=futex,read,write,recvmsg,sendmsg,poll,ppoll,ioctl  # 10 s, Ctrl+C; single thread only
sudo perf trace -s -t $TID -- sleep 10                                              # same, with latency
sudo perf record -g -t $TID -e syscalls:sys_enter_futex -- sleep 5; sudo perf report # who issues the futexes
```

How to read the result:
- About 1-2 wakes per frame: the loop is fine and the figure was really a per-frame rate at high fps (or another thread).
- Dozens of wakes per frame, mostly `futex`: lock contention in win32u, xinput or dinput. Try W3, then A/B with the
  Steam overlay off and with Steam Input off for the game.
- Mostly `read`/`recvmsg` on the wineserver socket: older-Wine server round-trips (GetAsyncKeyState/GetCursorPos/
  SetCursorPos). A newer Proton fixes these. A further game-side cut would be to cache the 5-9 GetAsyncKeyState
  calls; that is not done here.

## 7. Windows test commands

```
dotnet fsi input_live.fsx offline                       :: static + native checks, no game access
dotnet fsi input_live.fsx dry                           :: game running: site state, padMgr, connected bytes
set FT_ALLOW_WRITE=1 & dotnet fsi input_live.fsx apply  :: in a mission, not the title screen
dotnet fsi input_live.fsx status                        :: skipped/s should be about 3 x fps with one pad (4 x fps with none)
dotnet fsi input_live.fsx off   /  on                   :: A/B without touching code
dotnet fsi input_live.fsx revert
```

Then compare the `[App]InputUpdateWorker` section of two cpuprof captures, on and off. Check XInputGetState / cfgmgr32 /
NtDeviceIoControlFile inclusive %. Also check hot-plug: unplug and replug the pad, and the pad should respond within
about 2 s.
