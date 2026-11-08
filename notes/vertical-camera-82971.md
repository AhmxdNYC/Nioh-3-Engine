# Vertical transform camera (#82971)

Source802b7ee1, cx-pc/vertical-camera-82971 atop19a8f3db/BR99ef0ca6. Paired BR and CamKiProbe required.

BR samples the active form seal/player after FormHide4Tick each game tick, with actor-presence and finite-coordinate checks. First valid bound sample per owner/seal/capture time establishes baseline. Publishes (sealY-sealStartY)-(playerY-playerStartY), clamped +/-400, via atomic read-only export Nioh3BossRematch_CameraVerticalTarget. Subtracting player displacement avoids double movement on slopes. Grounded unchanged relative height=zero. Disabled/missing/ended form=zero; export expires after250ms without a producer tick. [SoulCore] CameraFollowVertical=1 in Nioh3BossRematch.ini, default1.

CamKiProbe1.0.27 extends its existing validated 243ED0 output hook, gated to caller return2AFC59 (call2AFC54). No new game detour. It resolves BR export, critically damps offset with smoothTime0.15s, +/-400 clamp, and adds worldY equally to fresh output eye+14 and target+24 before view matrices. Existing camera right/up settings still apply independently. Menus targetzero. The game still controls camera orientation and player/seal movement. Native camera collision is not rerun after output translation; live clipping check needed. Baseline does not reconstruct pre-bind vertical motion.

1522 checks PASS: camera318 + existing1204 (hit334/picker115/replay243/bind52/height51/smoke23/equipment29/rematch357). Camera tests execute actual producer/export/output helper and shared spring: groundedzero, rising200, exitreturnzero, slopecompensation, diving, +/-400 clamps, invalid/despawn/toggle/stale source, output direction preservation and30vs60Hz. Both Release builds PASS; CamKi probe_test PASS; available sc22 ALL OK, unavailable captures skipped. BR export verified with dumpbin. No game or installed files touched.

BR42df32d04f58af206eda2ecb3643da6c18f7c648a69f4db10de1a58d89c052d5 bridge#6488.
CamKi fab688023331a0c067874062777ee7dab8c3737e3bcc6dc5dc69bb83a6ebf6fd bridge#6489.
Installed baseline supplied: BR99ef0ca6 + CamKi78302ba9. Next Claude installs both when appropriate, checks rising/diving/grounded cores and exit recovery, captures video. Opus design review requested via bridge#82979 because no Opus subagent model exposed; no response received before completion.
