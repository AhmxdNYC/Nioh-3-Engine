# Crash tests for the v1.8.19 live runs (reports-pc, task #106758, human #106703)

One block per fixed crash group. Log lines were checked against the RC2 source (f6180c85); rc-1819 is built on it.
Logs sit beside the .asi files: Nioh3EngineFix.log, Nioh3BossRematch.log. A crash report is Nioh3EngineFix-crash-latest.txt.
Every run, all devices: 0 new crash reports, 0 "Fatal error" popups. Report a crash with its signature line, not just "fail".
Restore the shipped ASIs and inis after the run, as usual.

## #78: Fight again unloads the home area too early (BossRematch)
- Devices: Main PC + Ally (the Ally showed the double settle-wait line).
- Steps: Boss Match, start one boss fight. Pause menu > Fight again, 3 times in a row on that boss, then once on a second boss.
  Let each trip home finish loading; do not press anything during the return.
- Proof (Nioh3BossRematch.log), before each boss request:
  `Rematch: native gate retry: home is up, waiting 5000 ms for it to settle before the boss request`
  (the same line twice per retry is known and does no harm; rc-1819 also logs why the wait restarted).
- Pass: 4/4 bosses load with a health bar and full control, 0 crashes. A crash 0-2 s into the boss load = #78 not fixed.

## #79 / #83: empty link in the pad list at boot (EngineFix PadListGuard)
- Devices: all three (PC, PC2, Ally).
- Steps: 3 cold boots with a controller plugged in from the start; 2 cold boots with the controller unplugged at launch,
  plugged in 3-6 s after the window appears (the crash window was ~6 s), then unplugged and re-plugged once on the title screen.
  Ally: the built-in pad counts as plugged; for the unplug arm connect and disconnect an external pad instead.
- Proof (Nioh3EngineFix.log), every boot, before the first title screen:
  `PADGUARD: installed at N of N pad list walks (v2.02) -...; unwind registered - [General] PadListGuard = 1 (...)`
  Fail if it says `not installed`, `walks NOT guarded`, a lower first N, or `unwind NOT registered`.
  `PADGUARD: null list link at +XXXX (hit n)` = the guard caught the race (a pass, and worth noting the count).
- Pass: 5/5 boots reach the title and Continue into the field, pad works, 0 crashes.
- Not covered: the separate startup throw #51/#87/#88 (0xE06D7363 in KERNELBASE, unordered_map in the pad code) is NOT
  fixed in this build. If it shows up, log it as "#51 repeat", not a #79 fail.

## #85 / #86: cold-boot races (EngineFix FLP + PTS)
- Devices: all three. #86 was seen on the Ally (Proton) and #85 on a 4-CPU PC2 setup, so 5 cold boots each on Ally and PC2;
  Main PC 3 boots.
- Steps: cold boot (game fully closed for 5 s before), title, Continue, walk 30 s in the field. Repeat.
- Proof #86 (Nioh3EngineFix.log), every boot:
  `PTS: present thread waits for its wake event before the first Present (game start race, crash 1A822CC)`
  then, once the overlay stats start: `PTS: present thread start waits N round(s) of 1 ms for its event` (N may be 0).
  Fail if it says `PTS: ... - skipped`.
- Proof #85 (only when FileLoadPrio is on, as on the 4-CPU PC2 run): before the first level load,
  `FLP: level load none yet (boot / title, loaders unchanged)`, and no `FLP: lower ...` line until after
  `FLP: level load done (field)`.
- Pass: every boot reaches the field, 0 "Fatal error 0xC0000005 ... 141A822CC" popups, 0 crashes in the first 30 s.

## #84: GPU device lost: the report names the reason (EngineFix crash report)
- Devices: one PC (Main PC). The device loss itself cannot be forced; this checks the new report section.
- Steps: set `[Debug] CrashTest = 1`, `CrashTestDelaySec = 30` in Nioh3EngineFix.ini. Boot to the title or field and wait
  for the forced crash. Point the report upload at a dead local URL or mark the run as a test, so no public issue is filed.
  Then set CrashTest back to 0.
- Proof (Nioh3EngineFix-crash-latest.txt): a `-- gpu device --` section whose next line is
  `GetDeviceRemovedReason = 00000000 S_OK (device not removed)` (live device). Fail: section missing, or
  `no game device captured` once the field was reached.
- Optional (FG toggle path that crashed #84): in the field switch Frame Generation FSR -> OFF -> FSR twice. Pass = no crash;
  if the device is lost, the report must show a non-zero GetDeviceRemovedReason line (send it as a #84 repeat).
