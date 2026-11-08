# Per-press height (#82594)

The snapshot already captures player Y fresh in PressSnapTake. FormFollowGroundBegin reset its state, but then overwrote the snapshot with the minimum of a previous exit anchor within 30 seconds, 100 horizontal units and 64 height units. That cross-press minimum was not harmless on changing terrain.

Removed FollowGroundAnchor and all cross-press reads/writes. Follow/hold still cap at this press snapshot Y; verified native-warp output still clamps any upward correction. Exit restores this press Y, including undershoot. Existing descent behavior is unchanged.

Production source a7d46a32, branch cx-pc/press-y-82594, atop smoke fix625659ca/checkpoint71c0a5c2. Release PASS. 615 relevant offline checks PASS: ground51, first-use52, primary/replay243, last-hit269. New sequential-height regression uses21792,21815,21772,21804 at unchanged X/Z with mocked +15 warp lift and restores each independently.

BR a79e8aa40ea8e3be1049e6e697d938b5c36487a79a878b6d2bc2dd97a416139e, bridge #6443. PC supplied installed BR629dc81f untouched. Next Claude live verifies two terrain heights. Queued generation/variable-count changes dropped by lead #82594 (LEFT stable2,5/5 live exits).
