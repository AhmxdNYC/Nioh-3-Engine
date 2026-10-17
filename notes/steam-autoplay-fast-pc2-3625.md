# Fast Steam autoplay, pc2 task 3625

PASS for the requested dialog latency: final installed helper clicked Play anyway 0.40-0.55 seconds after its first visible appearance, below the 3-second target. Three successive development versions all launched the actual game automatically; conservative latency upper bounds were 1.34, 1.12, and 0.55 seconds.

The canonical helper now warms OCR before signaling readiness, checks one Steam window per poll, searches the centered dialog at 2x scale before rotating through fallback tiles, and schedules polls every 250 ms including processing. It no longer waits 1.5 seconds after clicks. A per-window cooldown prevents repeat clicks without blocking other windows. Open Steam is searched directly in Launching windows. Clicks verify foreground ownership and log the actual mouse-up UTC time to millisecond precision. The existing production wrapper still waits for readiness before Steam launch; its new optional TracePath argument records polling timings.

Final run evidence (UTC on 2026-10-14):
- Watcher ready 17:42:52.913; launch request 17:42:53.0519067.
- Screenshot 47 (no dialog) capture interval 17:42:53.6039672-53.6550014.
- Screenshot 48 (dialog visible) capture interval 17:42:53.7049590-53.7560282.
- Actual mouse-up 17:42:54.1536593; resulting appearance-to-click bracket 0.3976311-0.5496921 seconds, conservatively using the prior capture start and first-present capture end.
- Nioh3 process start 17:42:54.5717561, PID 18428. Acceptance is independently visible in screenshot 53.
- Measured pre-click poll intervals: 501.8 and 251.4 ms; the acceptance scan took 500.4 ms. Requested timer cadence is 250 ms. Scheduling limitations are recorded in universal-optimizer/TECH_DEBT.md.

The independent Python screenshot observer captured timestamped frames nominally every 100 ms; visual review identified first appearance rather than using the clicker's own OCR detection timestamp. Contact sheets and original boundary frames are included. Earlier run1 used frames18/19 and mouse-up17:40:19.0549748; run2 used frames22/23 and mouse-up17:41:20.9810969. The final run used the installed production prelaunch wrapper and canonical helper SHA256 6380833e71563a35757e45bef130d6d97446269cef85f8b2cec4b27d53cbccc1.

All three tests acquired nioh3-pc2, backed up six fresh save/config files, registered their exact processes with the harness guard, and restored/hash-verified all six files afterward. No mod installation or graphics change was made. Task3625 owns no running test/helper/game or lease. A subsequent independent task3639 worker began its own game test after restoration and retains its processes and lock; do not stop those.

Validation: three actual game starts; PowerShell parsing and git diff --check; universal-optimizer scripts/check.ps1 -SkipFSharp PASS (tools, agent-bridge, nexus, header sync, ABBA, harness guard). No F# changes. Full captures and private save backups remain at C:/nioh3-re/task3625-fast-autoplay. Delivery excludes saves. Included canonical helper, prelaunch wrapper, complete universal diff from current HEAD and incremental helper diff from task3620 for integration. Pc owns fleet integration; Linux is outside this pc2 task.
