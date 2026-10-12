# Yamagata shrine67064 to arena66599: retained-run navigation evidence

2026-10-11. Read-only review of `D:/nioh3-re/rematch-pilots/training-R1CBC-20261011-200033-18996`, extracted native placement data and existing script decoder. No game/input/warp/gate call. Next-preset note checkpoint:ce3a5fd.

## Why this attempt missed the trigger

The retained run did not reach the arena box. It began at339660,10195,206265. Walking-1 reached339145,10124,206834, then Walking-2 reversed the useful X progress to340182,10140,206746. Walking-3 reached339044,10158,206507; Walking-4 and arena-gate-view were338327,10144,206173. The latter screenshot shows the character against the wooden side palisade/shields, with the curtain opening ahead-left. The final gate-aligned screenshot is338686,10115,206599: X increased359units since the earlier wall position, moving away from the entrance plane rather than through it.

These image/position associations use nearest status to each PNG modification time; offsets are within0.08s except shrine-camera-confirmed0.24s. They are correlations, not frame-synchronous camera telemetry. Minimum recorded horizontal distance to the boss-box center was11.09m at20:07:58.440, position338391,10135,206364. No claim that the character entered and the script failed is supported.

## Exact native entrance/trigger geometry

Read the actual mission0x1CBC record's +5C table through the extracted ID table, not a guessed world coordinate. File: `data/extract/lfm/archive_03/36/4626ACC88F8CF75DEB4BE53030A110EC40ABCE9C5D0D89076920DF8B9931F3`. PlacementBox records are0xA0 bytes, center/yaw at+10/+20, dimension triple+28.

| Record | Center X,Y,Z | Yaw | Dimension triple |
|---|---|---|---|
|65340, HITOKOTO_GUESTBARRIER_BOSS|337898.96875,10527.0,206747.484375|-4deg|212.2400,917.5600,2616.6799|
|66599, HITOKOTO_BOSSAREABOX|337366.34375,10273.599609375,206787.78125|-4deg|1063.6400,569.3700,1535.3400|

65340 supplies an entrance-plane reference, not proof that every point on that plane is traversable. Its centerY is volume center above the walking surface; do **not** target10527 as ground height. The observed nearby ground is roughly10115–10158. The parser treats dimensions as full box dimensions when selecting a point; this review does not newly prove collision/navmesh extent semantics.

## Concrete next walk direction, with limits

From the retained wall position338327,206173, the entrance center is4.28m toward **decreasingX** and5.74m toward **increasingZ**. A short manual correction towardZ˜206750 on the shrine side, then through the visible curtain opening while decreasingX past˜337899 toward˜337690,Z˜206750, is the smallest geometry-supported route to try. Follow actual ground height; these are horizontal navigation targets, not teleport coordinates or a proven collision-free polyline.

From the final gate-aligned position338686,206599, target direction is primarily decreasingX (7.87m) with a small increasingZ correction(1.48m) to the entrance reference. Do not continue the last recorded movement direction, which increasedX. For camera alignment, face the visible opening rather than the wooden side shields; the required world-forward direction is approximately-X with a small+Z component. Retained status contains no camera yaw, so an exact right-stick duration or camera angle would be invented. Verify any subsequent short forward walk by whether telemetry actually decreasesX; camera-relative controls and player facing are not interchangeable.

The simple route is supported by entrance/trigger geometry and the screenshot obstacle, but a fully certified navigable path cannot be extracted from these screenshots/placement boxes alone. There is no navmesh/collision-path proof here. If ordinary walking toward the visible opening stalls, stop and inspect the obstruction instead of introducing a gate-native call or assuming an interaction button.

## No door-button interaction in the native trigger path

Decoded the retained mission script `data/extract/lfm/archive_09/C6/8008F3E2B14D*` using existing `scratch/dec_a.py` without writing output files. `sub_event_check_boss_area` at script offset0x1139E loops on `Player::IsEnterPlacementBox(66599)`, then calls `Function::RequestAndWaitWarpRights` with constants66599/60/300, waits20, and calls `Function::StartWarp(66599)`. `sub_event_boss_battle_start` at0x11410 waits for that warp-start, disables player control for the intro, requests RTM31648 and sets mission flag456 before native boss flow. This path contains no Gadget interaction or StartWarpByGadgetRights button gate. The script itself acquires its normal rights after physical box entry; the runner should not inject them.

A previous native walk receipt described in `notes/boss-rematch-build.md:150` records box entry followed by flag456/Create65092/BossBattleMode. Current retained run has none of those boss events, consistent with never crossing the entrance/trigger. This is an explanation supported by coordinates/script, not a new live successful route or proof that no other physical obstacle exists.
