# Native completion learner (#82808)

Base BR bdd651af; source8f738b29 on cx-pc/native-learn-82808.

Removed the action-boundary publisher and short-count recoveryFire rule entirely. C80->C81->C82 is never classified as attack completion by a boundary. Added provenance to terminal signals: only native idle act1 sets g_sieNativeEnd. Loop/hold predicted terminal signals may retain prior exit behavior but cannot teach a count. Natural despawn learning still requires unfired form and uninterrupted readable census. Our own hit/cap/fallback exits never teach; invisible tail remains diagnostic.

New actual-motion replay: core16127 learned10, ten burst openings .9 through4.5, switches after burst4 and7, no premature exit, INI remains10, final exit at4.5 (+100ms reveal). A2ACB stale2, open1.383/close1.433, C56->C57 produces no exit/learn; native despawn2.68 learns1; next use exits1.383. A synthetic non-native terminal fallback exits without lowering3.

Release and833 targeted checks PASS: hit/motion323, primary/replay243, bind52, height51, smoke23, picker112, equipment29. Available sc22 ALL OK, unavailable captures skipped. Broad rematch result follows in checkpoint.

BR0da8fb1b6aa9151411c4009f9cf3481997e49b3c65f1ffc8f2d22fbf33f0b929, bridge #6468. PC installed bdd651af untouched. Claude owns live test and resetting bug-corrupted16127=10 as agreed; no automatic data rewrite or game operation by companion.

Broad rematch357 PASS; total1190 offline checks PASS.
