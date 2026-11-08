# Full vertical yokai camera, task 83104

Replaces spawn-relative displacement with current seal Y minus player Y, including displacement already present on first binding. Uses existing CamKi output hook (eye and target world Y), no native target-pointer swap or actor writes. Y only; existing FormFollow handles horizontal travel.

20 ms exponential follow lag with unity gain. Separate smoothstep exit reaches zero in 0.2 s. Sanity bound 100000 units replaces 400 cap. CameraFollowVertical toggle unchanged. Each active session exports an ID; BossRematch logs peak absolute seal-player Y on exit, CamKi logs peak requested and successfully applied Y per session on exit. Zero displacement while still active is not treated as exit.

Validation: vertical camera 321 checks PASS; ground/restore 51 PASS; both Release builds PASS; CamKi probe_test PASS. Tall 2000-unit steady target within 5%, first bind at height, grounded zero, slope compensation, dive, stale producer, disabled producer, world-Y output and 0.2 s exit covered. Existing logs only print first three offsets (-0.03/-0.16/-0.41), not full-session peaks, so no prior peak inference.

BR 26f37616e013db056c88a239077dbcc2d81d315cc388fd4136fc85ec8e0cd45f
CamKi 5ccab1c6949e2eaf920b444dda036864eae8a186d1553b136b2f5793a4b5a8f8
Both must be installed by Claude pc; game untouched; live visuals and post-collision translation remain unverified.
