# Eight-second cold wait (#82145, narrowed by #82155)

Base 06fe0eaf includes post-ghost learning and all combined fixes. The supplied pc2 evidence identifies a 5.49-second seal spawn; a later 17.2-second spawn exceeds the intended wait. DOWN was refused by the game while LEFT's actor still existed; no forced throw or game-state reset is added.

Known, transformations-ON, confirmed uses now have an absolute press+8.0 deadline, shared by hidden wait and late-spawn admission. Matching-ID direction learning extends to the same age. Unknown, OFF, unconfirmed and cancelled uses retain previous short behavior. Existing explicitly longer timing settings are still respected. Captures timestamped after an expired known wait cannot bypass timeout by arriving on the same tick. Captures inside the deadline retain normal observation delay behavior.

Safe cleanup clears pending begin metadata on throw-done-no-seal, press-no-seal or throw-cancelled. Existing restore still owns player/pad state and retry-on-fault semantics; no global forced reset, removal of live actors, or artificial accepted use.

Validation: 52 first-use/cleanup/admission,112 picker,114 hit/tail,23 smoke,29 equipment,357 rematch =687 unique checks PASS; sc22 available replay PASS. Fixtures accept +5.49 on capture/spawn-hit paths and at +8.0, reject +8.01 and +17.2, preserve refused/OFF/unknown behavior, exercise next-direction fresh deadline and pending cleanup. Release built. No game touched. Next pc/pc2 cold first press and timeout/repeat validation.
