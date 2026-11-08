# Burst-gap replay (#82280)

Base: follow-ground c4383d2a plus harness cleanup 3d72e837. Production code unchanged.

The current algorithm clusters NEW collider openings less than 100 ms apart. It does not cluster by empty-pool duration. Supplied pc2 log #6393 gives first/second openings 5.762/5.879 and 5.910/6.046: 117/136 ms apart, so the expected current count is THREE, not two. The final direct-seal burst occurs at 6.050/6.209. Missing the intervening empty sample does not suppress detection of new collider identities.

Added 78 checks to the extracted-production C++ harness (217 total PASS). Both source timing variants replay 28 helper colliders -> 4 new helper colliders -> 4 direct seal colliders, with/without the intervening empty sample. Each variant learns 3; switching sampling phase for subsequent uses still exits at the final opening (relative 1.288/1.299 seconds, configured offset zero, ghost request after existing 100 ms reveal). Existing A3=3 also exits correctly. Old A2=3 is ignored by the A3 reader, so the max-only learner cannot inherit that old-generation value. No source/binary change required for these replays.

Evidence limit: #6393 provides counts and timings, not collider addresses. Replay identities are synthetic; this proves new-object detection across a missed gap, not that the game always allocates new addresses for the second helper burst. Fully reused identities with no observed absence cannot be distinguished from a persistent burst by this observer. Do not present the offline result as a live pass. Next: lead-owned live test of current follow-ground build, checking NEW-object burst lines for both sampling variants if the issue persists. No game interaction by this companion.
