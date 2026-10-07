# Nioh 3 (v2.02): does it leak memory, and does that cost performance over a session?

Date: 2026-10-07. Inputs:
- 4 Ally MangoHud CSVs (5-9 min each, 10 Hz).
- 6 Ally mod logs, including v7.1: 70 min of 30 s fps and thread-count lines.
- Community research.
- Static RE of the allocator, the swapchain rebuild and the frame-begin path.
- A D3D12/DXGI test harness on this PC (RX 9070, Windows 11 26200).

Nothing was run in the game.

## 1. Verdict

| Resource | Leak? | Evidence | Rate | Hurts fps? |
|---|---|---|---|---|
| **OS handles (Windows / PC)** | **Yes, located in code** | 4A5338 calls `GetFrameLatencyWaitableObject` every frame and never closes the handle. The harness shows DXGI returns a new duplicated handle on each call (+1000 handles for 1000 calls). | One handle per rendered frame: **216k/h at 60 fps, 432k/h at 120 fps**. That is ~3.5-7 MB/h of kernel handle table. The process ceiling of 16.7M handles is reached after ~39 h at 120 fps. | No measurable fps cost per frame. It only grows for as long as the game renders. See §6 for the indirect risks. |
| OS handles (Ally / Proton) | Probably not | The Ally swapchain reports flags 2, so it has no waitable object and the getter returns NULL. Confirm with MemStats. | – | – |
| System RAM (process) | No unbounded growth possible from engine allocations | All engine CPU allocations come from **one 6 GiB `MEM_COMMIT` arena** created at boot. It is split into 52 fixed-size category pools, and their sizes add up exactly to the arena (§5.1). VirtualAlloc and VirtualFree each have a single caller. | Ally steady state: system `ram_used` +1 to +2 MB/min after the load settles (v5), which is noise. | No. |
| VRAM / GTT | No growth in steady play | Ally `gpu_vram_used` is flat at 6812±3 MB over 5.5 min (v5) and ±150 MB streaming steps in v1. The texture page pool is fixed capacity with LRU stamps (252510 / 2522C4). | 0.0 MB/min (v5, after 240 s). | No. |
| D3D12 objects on swapchain rebuild (FG / resolution / display change) | No | Every object that `RecreateSwapchainDevice` 2EEA690 creates is released by its teardown 2EEC4C8 (§5.2). | – | – |
| Threads | No | SP thread census on every 30 s line for 70 min: KIDSTaskScheduler 8, KIDSTS 77, dispatcher 1, [App] 42→44 after the first load, then constant. | 0 | – |
| Repeated loads (shrine travel, death) | **Not measured** | Every Ally capture has exactly one title→level load. Static analysis can't rule out per-load D3D12 or driver allocations outside the pools. | ? | Test plan in §8. |

**Short answer for the Ally:**
- There is no memory leak in the data. Over 5-9 min of play after a load, the GPU memory, system RAM and swap curves are flat.
- Frame time does not get worse: p99 trends slightly *better* as streaming settles.
- Over a 70-min session the fps stays at 16.35 ± 0.15, a drift of +0.1 fps/h, i.e. noise.
- **What does cost performance on the Ally is capacity, not leakage.** Loading a level takes the APU's GPU memory from ~4.8 to ~6.8 GB in about 90 s, on a 15.3 GiB system that already has 9-10 GB in use. At that moment the kernel pushes 0.75-0.85 GB to swap (v2, v5), and frame times spike to 260-300 ms during the load.
- The only leak found is a Windows-only handle leak. It does not apply under Proton.

## 2. Ally MangoHud CSVs (`captures\ally\*.csv`)

- The columns are in GB, so I converted to MB.
- `process_rss` is 0 in every file: MangoHud can't see the Wine process. That is why MemStats exists (§7).
- The APU carve-out is only ~0.7 GB, because the OS sees 15.3 GiB of the 16. So `gpu_vram_used` (up to 7.5 GB) must be mostly GTT, which is system RAM. MangoHud's APU figure includes GTT.
- The arithmetic supports this: `ram_used` (10.7 GB) + 6.8 GB of GPU memory would be more than physical RAM if they were separate. So **on the Ally, "VRAM" and "RAM" are the same pool.**

| Capture | Length | Load window (mod log stop flag 58A) | GPU mem growth during load | Swap jump | Plateau slope after load: GPU / RAM / swap (MB/min) | Frame time trend on plateau |
|---|---|---|---|---|---|---|
| v1 L1FGwine | 363 s | ~t 90-200 s (no log) | 4.6 → 7.2 GB | – | +37 / +40 / -5. Streaming steps of ±60-210 MB while moving (t 222-361 s) | -0.42 ms/min |
| v2 mod | 312 s | ~t 90-200 s (no stop flag in this build) | 4.4 → 7.0 GB | **+740 MB at t≈180 s** | -11 / +6 / -0.2 | -0.93 ms/min |
| v4 fgoff | 307 s | t 106-207 s (log 04:24:39-04:26:20) | 3.8 → 6.8 GB | 0 | +39 / +21 / 0 (only ~100 s of plateau, still settling) | -0.67 ms/min |
| v5 W6 ABA | 557 s | t 122-207 s (log 04:34:34-04:35:59) | 4.6 → 6.8 GB | **+840 MB at t≈120 s** | **0.0 / +2.6 / -1.8** (t ≥ 300 s), so RAM + swap ≈ +1 MB/min | -0.42 ms/min |

How to read the table:
- The whole-session fits (+300 to +730 MB/min of GPU memory) are just the one-time level load.
- After the load, the longest clean plateau (v5, 5.5 min) is flat to within 1-3 MB/min, and that is system-wide `ram_used`, not the game alone. Even if all of it were the game, it would be ≤ 0.15 GB/h.
- Frames during the load: p99 reaches 261-299 ms and the 30 s window falls to 7.9-9.6 fps at the end of the GPU-memory ramp (v4, v5). Gameplay p99 afterwards is 52-60 ms (60 Hz tier, ~45 ms average) and stays level.
- The swap jumps happen *during* the load, at the peak. Swap then drains slowly (-1 to -5 MB/min) as pages are touched again. Nothing grows afterwards.

## 3. Mod logs (`captures\ally\*.log`)

- **v7.1 (05:33-06:44, then 13:08-13:09):**
  - 146 OSD stats lines over 70 min at MAX settings (FG off).
  - Rendered fps for minutes 5-15 / 15-30 / 30-45 / 45-60 / 60-72: 16.17 / 16.42 / 16.39 / 16.36 / 16.34.
  - Fit: +0.1 fps/h. **No decay in 70 minutes.**
  - The SP thread census is unchanged across all 146 reports, so no thread leak.
  - After a 6.4 h gap (handheld suspend) the game rendered 14.7-14.8 fps for 1 min, about 10 % lower. There is no memory data for that minute, and thermal or power state after resume is just as likely. MemStats over a suspend would settle it (§8 test D).
- **Swapchain:** every Ally log shows `flags 2`, i.e. ALLOW_MODE_SWITCH only, no FRAME_LATENCY_WAITABLE_OBJECT. No log contains an FG toggle or swapchain rebuild, so a rebuild was never measured on the Ally.
- **Loads:** v4 and v5 contain one level load each (stop flag 58A). v6 has one level load plus a short 58D window (7 s, menu or travel).

## 4. PC captures and community reports

- **PC captures:** the PresentMon CSVs, `results.csv` and the round CSVs have no memory columns. There is no PC mod log in the game folder. **No PC memory data exists yet.**
- **Community (searched 2026-10-07):**
  - **No credible, measured leak report.**
  - The strongest claim is an unverified post on the Nioh3Fix Nexus page. It says VRAM accumulates with frequent fast travel and long sessions, spills into system RAM, and stutter then persists. It gives no numbers or hardware.
  - Two quoted Steam reviews in an AI-written review roundup mention a "VRAM leak" and "lag past the two-hour mark".
  - RPG Site ran the ROG Ally with 6 GB of VRAM for 8 h at a locked 30 fps and saw no decline.
  - TechPowerUp's handheld review describes slowing when the frame buffer fills and recovery after a flush. That is capacity, consistent with §2.
  - Patch notes 1.02.01 to 2.02.00 never mention memory, leaks or VRAM.
  - On the same engine family, Rise of the Ronin's patch *reduced* VRAM use; it was not described as a leak fix.
  - Reddit, ProtonDB and the Steam discussion search could not be reached, so the community picture is incomplete.
- **What fits the claimed fast-travel pattern:** the static picture rules out growth in steady play and on swapchain rebuilds, but **not per-load growth**. Test B in §8 is the exact repro.

## 5. Static analysis

### 5.1 CPU memory: one fixed 6 GiB arena (`EngineArenaInit` 294C2E0)

- `EngineArenaReserve` 294D214 makes **the only VirtualAlloc call in the game**:
  - `VirtualAlloc(NULL, 0x1800FFFFF, MEM_COMMIT, PAGE_READWRITE)`, i.e. 6.0 GiB.
  - Optionally +60 MB, and +230 MB with a second flag.
- `EngineArenaInit` then carves the arena into **52 fixed-size category sub-allocators**:
  - Sizes range from 2 MB to 1450 MB; the largest are 1450, 577, 500, 463, 416, 360, 310 and 254 MB.
  - **The sizes sum to exactly 6145 MiB, the arena size.**
  - These are the `ktgl::IMemoryAllocator`s that RTTI names `get_system_allocator`, `get_stage_allocator`, `get_battle_allocator` and `get_temp_allocator`.
- `VirtualFree` has a single caller (1FE6948, from 294FA68), which runs only at shutdown.
- `HeapAlloc` is used only by the CRT (BF04DC, BF201C). Third-party code (Streamline, FFX, XeSS, audio, Steam) uses the CRT heap.

Consequences:
- **The engine cannot grow its commit.** A leak inside a pool can only fill that pool, ending in `Allocator allocate fail`, not unbounded RAM growth.
- Process "private bytes" sits at ~6.2 GB from boot on both Windows and Wine. Untouched pages cost no RAM: on Linux with overcommit, and on Windows they count against commit only.
- **Freed pool memory is never decommitted.** The working set ratchets up to each pool's high-water mark and stays there.
  - In a RAM chart this looks like slow "growth" as new content is visited, but it is bounded by the 6 GiB arena.
  - On the Ally those pages stay dirty-resident, so under pressure the kernel must swap them out instead of dropping them.
  - A future optimisation, not a leak fix: `MEM_RESET` the stage pool's free range on stage unload. This is unverified under Wine and needs the stage allocator's reset function, which has not been traced.

### 5.2 Swapchain rebuild: balanced (`RecreateSwapchainDevice` 2EEA690 + `GfxDeviceTeardownForReset` 2EEC4C8)

The path is FG toggle, resolution or display change → `DisplaySettingsWatcher` 1350210 → `DisplayResetRebuildSwapchain` 226D140 → 2EEA690. 2EEA690 calls the teardown 2EEC4C8 **first**, which:
- waits for Present;
- shuts down or resets the page pool (1896AC8 / 2EF0938 on g+61A0);
- releases the back buffers (2EEDEF8);
- releases the timestamp query heap g+4768 and the readback buffer g+4770;
- releases 16 command-allocator slots g+4290.. and their companions g+4698..;
- releases the render targets g+4718/4720/4728;
- frees the frame block g+4288 (allocator g+3D90 vtbl+58h).

2EEA690 then recreates exactly those objects:
- the swapchain is either Released and recreated through `DxgiCreateSwapChain` 2EE5A38 (which releases its factory and the temporary IDXGISwapChain), or kept with `ResizeBuffers`;
- `CreateCommandAllocator` × BufferCount via device g+4268 vtbl+48h;
- `CreateQueryHeap` via vtbl+138h;
- page-pool and texture-block allocator init.

The temporary `IDXGIOutput` from `GetContainingOutput` is released, and the extra reference on g+4720 is matched. **No leak per rebuild.** The page pool (g+61A0) is a fixed set of buckets × ways with `lastUsedFrame` stamps (`PagePoolCountUsed` 2523FC). It is capacity-bounded LRU, not a growing heap.

### 5.3 The leak: the waitable handle at render-frame begin (4A5338)

```
4A5318  cmp  byte [rsp+2564h], r15b      ; per-view "use waitable" byte (filled by 247708) - set on PC (32 % of main-thread samples in this poll)
4A5322  mov  rcx, [g_RenderGlobals 45BA588]
4A532E  mov  rcx, [rcx+4278h]            ; IDXGISwapChain3*
4A5335  mov  rax, [rcx]
4A5338  call qword [rax+108h]            ; GetFrameLatencyWaitableObject()  <-- new handle every call
4A533E  mov  rbp, rax
4A5341  test rax, rax / jz 4A5359
4A5348  pause / xor edx,edx / mov rcx,rbp / call [WaitForSingleObject] / test eax,eax / jnz 4A5348   (R1 site)
4A5359  ...                              ; rbp's handle is never passed to CloseHandle
```

- Microsoft's documentation says the caller must `CloseHandle` the returned handle.
- The scratch harness built the game's swapchain on this PC: legacy `IDXGIFactory::CreateSwapChain`, 3 buffers, flags 0x842, FLIP_DISCARD. **1000 calls added +1000 to the process handle count, and each call returned a different value.** So on Windows every rendered frame leaks one event handle.
- PC config 0x842 includes the waitable flag, and the PC profiles show the poll active, so the leak is live on the PC.
- On the Ally (flags 2) the getter returns NULL and nothing leaks.

**Fix WL (ASI, memory-only, game code):**
- Site 4A5338: `FF 90 08 01 00 00` → `E8 <rel32 to near-page thunk> 90`.
- deps: `48 8B 01` at 4A5335, `48 8B E8` at 4A533E.
- No overlap with R1 (4A5346..4A534B, 4A5357).

```
caveWL:                          ; rcx = swapchain, rax = its vtable (both loaded by the game)
    sub   rsp, 38h
    call  qword ptr [rax+108h]   ; h = GetFrameLatencyWaitableObject()
    mov   [rsp+28h], rax
    mov   rcx, rax
    xchg  rcx, [g_wlPrev]        ; rcx = previous frame's handle, store h
    test  rcx, rcx
    jz    done
    cmp   rcx, [rsp+28h]
    je    done                   ; same value: runtime does not duplicate - keep it
    cmp   byte ptr [g_wlEnable], 0
    je    done                   ; switch off = stock behaviour
    call  qword ptr [__imp_CloseHandle]
    lock inc dword ptr [g_wlClosed]
done:
    mov   rax, [rsp+28h]
    add   rsp, 38h
    ret
```

Why this design:
- It closes the **previous** frame's handle, whose wait loop has already finished. So it never waits on a closed handle.
- A rebuilt swapchain is handled for free: the next frame's handle comes from the new swapchain, and the old duplicate is closed.
- It clobbers only volatile registers, which the original COM call clobbers too.

Cost: one extra `CloseHandle` per frame, well under 1 µs. Benefit: the handle count stays flat. Add a 30 s `stats/s WL: closed N/s` line. ini `[Fixes] WL = 1`, safe as a default on Windows; under Proton it never triggers.

## 6. What the leaks and capacity cost

- **The handle leak (PC):**
  - **No frame-time cost in normal sessions.** Handle creation is O(1), and every handle points to the same kernel event, so no new objects are created.
  - Kernel paged pool grows ~3.5-7 MB/h.
  - Indirect risks:
    - Tools that enumerate system handles (Process Explorer, some overlays and anti-cheat-style monitors) slow down with millions of handles.
    - Process exit takes longer.
    - After ~39 h (120 fps) to ~78 h (60 fps) of *rendered* time, including pause-menu idling but not suspend, the 16.7M per-process limit is reached. Then `DuplicateHandle` fails and every file, event or thread creation in the game fails too.
  - Worth fixing because it is trivial and certain, but **it is not a cause of the reported 2-hour stutter.**
- **Ally capacity:**
  - Steady state is 10.7 GB of 15.3 GiB in use, of which ~6.8 GB is GPU memory (GTT), plus 1.3-2.5 GB of swap.
  - The cost shows up **at loads**: a 0.75-0.85 GB swap-out burst and 260-300 ms frames at the end of the GPU-memory ramp. Afterwards the plateau runs normally.
  - Repeated loads could make this worse if anything per-load is retained. That is test B.

## 7. MemStats: new module `mod\Nioh3EngineFix\src\memstats.cpp/.h`

The module writes one line per stats period:

```
stats mem: private 6420 MB, working set 3810 MB (peak 3990), commit 6420 MB, handles 2311, threads 214,
  VRAM local used/budget 6050/7800 MB, non-local 410/6100 MB | sys RAM avail 4380 MB, commit 15200/19400 MB
  | since start (31 min): private +2, ws +35, handles +0, threads +0, VRAM local +12, non-local +0
```

Where each value comes from:
- Process memory: `GetProcessMemoryInfo`.
- Handles: `GetProcessHandleCount`.
- Threads: a Toolhelp snapshot, counting this process only.
- VRAM: `IDXGIAdapter3::QueryVideoMemoryInfo` for LOCAL and NON_LOCAL.
  - The adapter is found by the game device's LUID. The device is read at `[g_RenderGlobals 45BA588]+4268h` (v2.02 only, SEH-guarded), or comes from an optional `getD3D12Device` callback that the OSD can supply because it already captures the device.
  - The factory comes from `CreateDXGIFactory1` in the **system** dxgi.dll, loaded by full path at run time. This bypasses OptiScaler's dxgi.dll and sl.interposer, so no wrapper sees the factory, and it adds no import.
  - After 10 lines without a device it falls back to the first hardware adapter.
- System: `GlobalMemoryStatusEx` for available RAM and system commit.
- Deltas are measured from the first line. VRAM deltas start from the line where the adapter was first found.

Proton notes:
- system32\dxgi.dll is DXVK's dxgi, which implements `QueryVideoMemoryInfo` through VK_EXT_memory_budget.
- The budget is right. **"used" may count only DXVK's own Vulkan device**, because the game's allocations go through vkd3d-proton's separate instance, so cross-check with MangoHud `gpu_vram_used`.
- `private` under Wine is Wine's commit bookkeeping (~6.2 GB, static). **On the Ally the metrics that matter are `working set` (RSS), `handles` and the MangoHud GPU column.**

Verified with a scratch harness on this PC (d3d12 device + the game's swapchain flags):
- It compiles with the mod's exact `cl` flags (/W4 /permissive- /EHsc /MT /O2) **with zero warnings**.
- The linked image depends only on KERNEL32 and USER32.
- VRAM is resolved by LUID (RX 9070, budget 15417 MB).
- A 256 MB commit plus a 64 MB touch showed up as private +257 / ws +64.
- One LogLine takes about 1 ms.

**Integration** (the main session owns `dllmain.cpp`, `build.bat` and the ini):
1. `build.bat`: `cl ... /Fobuild\memstats.obj src\memstats.cpp`, then add `build\memstats.obj` to the link line. No new libs (psapi resolves to K32* in kernel32). CMakeLists: add `src/memstats.cpp`.
2. `dllmain.cpp`, next to `sp::Start`:
   ```cpp
   memstats::StartInfo mi; mi.log = &Log; mi.iniPath = iniPath.c_str();
   mi.base = reinterpret_cast<uintptr_t>(g_base); mi.knownBuild = known;  // optionally mi.getD3D12Device = <osd getter>
   memstats::Start(mi);
   ```
3. In the stats thread, inside the `sec % 30 == 0` block: `memstats::LogLine();`. It is a no-op when off.
4. ini, under `[Profiler]`:
   ```
   ; MemStats: one "stats mem:" line every 30 s - process private/working set/commit, handles, threads, VRAM local and
   ; non-local used/budget (game's adapter), system RAM; "since start" deltas. Measurement only. 0 = off.
   MemStats = 0
   ```
   Ally zip: `MemStats = 1`.

## 8. Ally test plan (MemStats = 1, MangoHud logging on)

Leak criteria: a monotonic rise in working set or GPU memory of more than 20 MB per load over 10 loads, or more than 50 MB/h in steady play; or handles rising more than 50 per load or by any steady per-second rate.

- **A. Steady (30 min, no loads).** Stay at one shrine region: 10 min idle, 10 min of fighting the same enemies, 10 min in the pause menu.
  - Expect ws, VRAM and handles flat within ±50 MB and ±20 handles.
  - On a Windows PC run, handles should rise at the fps rate until WL is on.
- **B. Repeated loads (the community's fast-travel claim).** Teleport between the same two shrines 10 times (or die and respawn 10 times).
  - After each load, stand still at the same spot for 60 s and note the MemStats line plus MangoHud `gpu_vram_used`, `ram_used` and `swap_used`.
  - Fit each against the load number. Also compare each load's frame-time spike (p99 of the 10 s after the load ends), to see whether swap bursts get bigger.
- **C. Swapchain rebuilds.** Toggle FG off→on→off ×5 from the mod menu (FrameGenToggle = 2), or change resolution ×5. Static analysis says flat; this confirms it.
- **D. Suspend.** Note the line before a handheld suspend and 2 min after resume, and compare fps and ws. This explains the v7.1 resume result of 14.7 vs 16.3 fps.
- **E. Long session.** 2 h of normal play with MangoHud on. Fit p99 frame time against elapsed time per 10 min, excluding load windows (MangoHud frametime > 100 ms ± 10 s).

**If B shows growth:** the next step is a per-load diff of D3D12 object counts. Hook `CreateCommittedResource`, `CreatePlacedResource`, `CreateHeap` and `CreateDescriptorHeap` on the device vtable, plus `Release` on the returned objects (gpuopt has the vtable plumbing). Log live counts per type after each load. That pins the leaking call site, and the fix goes in that site's unload path.
