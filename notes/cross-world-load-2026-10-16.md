# Cross-world Fight now load time - status 2026-10-16 (pc lead)

Cause (static RE, notes/seq-state6-gate-RE.md): ~3,000 arena-area prop loads (~31 s) + loader-idle check (~9 s).

Measured (black screen = luminance sampler, not the log's "arena settled" number):
- pc RX 9070, Yoritomo build 09aa569b, 1 run/row: black OFF 21.9 s Shingen / 17.8 s Otoroshi; NearArenaOnly ON 22.3 / 23.9 s. No gain.
- pc, CoverReleaseMs 0 (old pc2 #676 build, run ON2 15:25): log said "arena settled 6.8 s" and control 0.52 s, but the luminance csv shows black 15:25:25.9 -> 15:25:59.9 = 34 s. The "7 s" was a log number only, NOT a visible 7 s.
- pc, merged build (pc/cover-release-merge 590e9bb, asi EC72C2C4, CoverReleaseMs 0): control 0.52 s, settled 8.9 / 6.8 s per log, but black 54:49.7 -> 55:30 (~40 s Shingen) and 57:21 -> 57:48 (~27 s Otoroshi). Worse than OFF.
- ally (Linux/Proton 1080p, 09aa569b): NearArenaOnly OFF 31.1 s (n=1) vs ON 30.1/30.2 s (n=2) to control; black until 47-65 s.
- pc2: not measured (was paused).

Verdict: the 7 s is NOT real. Clearing GAMESTATE[2] early gives player control sooner (0.5 s) but the screen stays black ~30-40 s; something else keeps/redraws the cover. Next lead if resumed: find what keeps the screen black after GAMESTATE[2] is cleared (overlay export fade? render-side cover), and whether the fight is visible under it. Branch pc/cover-release-merge holds the port (default off).
