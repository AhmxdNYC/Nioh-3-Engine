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
