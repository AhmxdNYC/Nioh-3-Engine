# Complete-run count correction and native exit (#82379)

Base ec977c22 / early-ground-82344 includes primary/group exit and the newer prebind appearance + press-time Y fix. Public release untouched.

Complete natural despawn and closed-pool idle completion now replace the learned count with the latest fully observed count, including downward corrections. Any failed pool read in that form prevents completion learning. Capped/cancelled/capture-change/ordinary early exits cannot lower counts. Partial post-exit tail observations retain raise-only learning. Current A4 section retained, so a stale high count heals after one complete readable run.

Both live and tail observers cluster NEW openings within 150 ms. Supplied helper openings 117/136 ms apart now merge, giving two clusters in both seen/missed-gap replays, with final direct-seal opening unchanged.

Natural seal-despawn now queues exit smoke, hides all recorded press actors with the existing group policy, holds player restore until the existing reveal delay, then applies player fade-in. Completion reason remains seal-despawn for learning. One transition per capture; last-hit processing pauses during that short pending reveal to avoid duplicate exits. This does not predict or trim the first run with an over-high learned count; it repairs the next run.

Validation: 254 hit/transition checks, 24 primary/group/queue, 52 first-use, 29 ground, 112 picker, 23 cold-smoke, 29 equipment, 357 rematch = 880 PASS; available sc22 replay PASS; Release PASS. Exact 11->10 natural and idle fixtures then exit at tenth opening next use. Unreadable/cut observations preserve 11. Native smoke queues once before reveal. No game touched, live visuals remain for pc.

BR sha256 d4d14a8f2436d2c5567323e55bce7a0af52e9644ca057c94345e958d76aade33.
