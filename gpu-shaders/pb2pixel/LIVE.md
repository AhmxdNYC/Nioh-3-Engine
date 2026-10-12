# Private combined PB2 live test

Windows only. This packet changes exactly `shader_dx12/0xb4751b0f.file`, record 1;
the other record and auxiliary tail are preserved. Original file SHA is
`c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a`;
candidate is `d8511aa0bea0d778de9e8d29342470cec644fe907559415ec0bafdfc9f0844d5`.
Combined candidate passed its own eight recorded root creations and sixteen
original/candidate PSO creations on RX 9070. This establishes no image or FPS result.

Operator prerequisites: hold the existing device game lease, take the normal
current-entry save/INI backups, and preserve the current settings and ASIs.
The switch owns only the shader file; it does not back up/restore saves or launch
games. Do not combine this timing run with training/rematch testing. Do not enable
controllers, wake systems or new overlays for this packet. Keep any prior shader
experiment out of this test: unknown current hashes are a hard refusal.

Set paths to the actual device installation and unpacked private delivery:

```powershell
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Nioh3'
$packet = 'D:\nioh3-re\deliveries\pb2-combined-live-20261012'
$run = Join-Path 'D:\nioh3-re\live-tests' ('pb2-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run | Out-Null
python -B "$packet\live_switch.py" status --game "$game"
Push-Location "$packet\native"
.\pixel_admit.exe
if ($LASTEXITCODE -ne 0) { throw 'Local exact recorded PSO admission failed' }
Pop-Location
```

For A1, launch normally through Steam (`steam://rungameid/3681010`), reach a
repeatable material-rich field scene, and leave gameplay unpaused with a fixed
camera. Allow the same ten-second settling interval in every arm. Use the same
scene, camera, resolution, scaling, FG, graphics settings and weather/time where
controllable. Run the capture through the existing remote execution session so
the game remains foreground; the script never steals focus or sends input.
Each capture is bounded to 25 seconds plus ten seconds collector shutdown.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "$packet\capture_window.ps1" -GameDir "$game" -Output "$run\A1" -Arm A -Scene 'operator: exact field/camera description' -Seconds 25
```

Exit the game normally and verify its process has ended. Activate B once:

```powershell
python -B "$packet\live_switch.py" activate --game "$game" --package "$packet" --run "$run\switch"
```

Launch normally, return to the same scene/camera, settle ten seconds, then:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "$packet\capture_window.ps1" -GameDir "$game" -Output "$run\B" -Arm B -Scene 'same exact field/camera description' -Seconds 25
```

Exit normally; while the game is absent, restore the current-entry shader:

```powershell
python -B "$packet\live_switch.py" restore --receipt "$run\switch\transaction.json"
python -B "$packet\live_switch.py" status --game "$game"
```

Launch once more for A2, reproduce the scene, settle and capture:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "$packet\capture_window.ps1" -GameDir "$game" -Output "$run\A2" -Arm A -Scene 'same exact field/camera description' -Seconds 25
```

Close normally and finish the existing device save/INI restoration protocol.
Retain all receipts, original shader backup and captures. A switch failure retains
its lock; never delete the lock or overwrite an externally changed target to
force recovery. The receipt path is recorded inside `.pb2pixel-test.lock`.

Admission limits: installed file hash alone does not prove this shader executed.
This shader also exists in other packs, which this packet deliberately does not
change. Retain the existing native shader/PSO evidence showing the candidate
digest from `delivery.json` in the tested scene. If existing instrumentation
cannot establish that, label the result EXERCISE_UNCONFIRMED, not a win or loss.
Compare before/after screenshots manually for material, lighting and geometry
regressions; animated pixels are not a bit-exact oracle. Compare matching
PresentMon swapchains and frame types (including FG), frame-time/GPU-busy
distributions across A1/B/A2, excluding loading/menu frames. Twenty-five seconds
is a screening window, not a stable FPS claim. An improvement needs repetition
beyond baseline variation. No exact resource replay, native image equivalence,
real-game benefit or Proton admission has passed yet.

## Confirm the installed record participates in drawing

Use the existing profiler in a separate short diagnostic boot before timing.
The operator's existing INI transaction must retain and restore the exact current
INI. No new overlay, ASI or system change is needed. The reviewed local INI has
GPUProf=0, so its current logs alone cannot supply this evidence. Temporarily use:

```ini
[Profiler]
GPUProf=2
GPUProf2_SampleEvery=8
GPUProf2_MinRun=1
GPUProf2_Top=100
GPUProf2_AB=0
```

Require ShaderSwap, ShaderHot and SkipFx disabled: no substitutions, twins or
skipped draws. Keep their existing disabled values; do not create an override
directory. Keep the file-based candidate installed through the owned switch.
Reach the chosen field camera and remain there for two complete GP2 log windows
(about 60 seconds), then close normally. Retain the current boot's Engine log,
INI and ASI hashes. Extract only the second scene window (to exclude the
preceding loading interval), keeping its timestamp and `GP2: frame` header.
Run the read-only checker on that slice:

```powershell
python -B "$packet\exercise_log.py" "$run\B-scene-only.log"
```

The required row is `ps=e9e2d0b05b89a2d262f0e59bf39f1841` with positive
draw-call and interval columns. MinRun=1 avoids attributing a merged mixed-PSO
run to its longest member. Capture the G-buffer marker as well as the digest.
The output deliberately says CANDIDATE_DRAW_INTERVAL_OBSERVED: a successful
candidate-associated PSO was bound during submitted draw work with resolved GPU
timestamps. It does not prove nonzero pixel shader invocations, visible pixel
coverage or image equivalence. Absence is inconclusive (top-100 truncation,
sampling, unvisited materials or another pack can all explain it). Do not expand
the replacement to unreviewed packs to force a hit. If both complete windows
lack the row, stop this scene's performance comparison as EXERCISE_UNCONFIRMED.

Restore the original INI before the A1/B/A2 timing boots. GP2's queries and hooks
have overhead; these diagnostic windows are not the performance comparison.
If a second instrumented comparison is useful, instrument every arm identically
and label it separately. No default-picture setting changes are required.

Source provenance (reviewed at isolated commit 7e6977d): `psoswap.cpp:603`
Consider records the input container key; `:559` TrkCommit accepts only successful
PSO creation; `:914` HkGfx brackets the call. `gpuprof.cpp:477` Gp2Event resolves
bound PSO and counts draw/dispatch calls, `:343` IvClose records intervals,
`:1223` consumes GPU timestamp results, and `:1469` prints shader-set rows.
This is stronger than PJ creation-only evidence. ShaderSwap cannot simplify the
proof: tracking records the original input key before replacement; ShaderHot
also deliberately preserves stock attribution (`gpuopt.cpp:893`). Therefore a
stock digest plus a successful swap count is not direct candidate-digest draw
evidence. Exact nonzero pixel coverage still requires an actual graphics capture
or a suitable existing pixel-invocation measurement, absent from this packet.
