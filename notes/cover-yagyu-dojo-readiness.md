# Yagyu dojo cover fix

Source parent4c33164; private Engine1.7.73/addon1.4.21, ABI2 unchanged.

Actual user trace: R1A5E begins04:10:06.407, BossBattleMode04:10:25.878,
no Character::Create witness, cover ends only after player death04:10:37.719.
The previous RA4B4 had already exited04:09:26.008; this failure is not a retained
previous mission. No runtime acceptance is transferred to this candidate.

Exact row is mission1A5E/start5021112/placement5027806. Its script is
`data/extract/lfm/archive_09/F4/BFC230194A06A497782995EE915EFD7B59F7DC3743771F17D5E3FDC68B2796`.
The shipped TeacherZakoRushCommon flow obtains Duel::GetBossPlacementID atF1DA,
tests Character::IsKneelDownFlag atF1F0, operates on that local teacher, requests
action140 atF392 and passes the same local to BossBattleMode atF3AA. It does
not require Character::Create in that route. Old generated CSV function labels
can be shifted; these offsets are from fresh decoding of the retained script.

The fix does not guess an alternative constructor or reinterpret a VM context as
an actor ID. For this exact tuple only, the same matched attempt and fresh
BossBattle witness can use the existing passive exact-placement actor reader
(materialized, algorithm active, native not-dead) instead of a Create event.
Both mission snapshots must remain known and identical; the current player must
be alive, offline, outside loading/pause, with native control restrictions clear.
The existing500ms continuous settle still applies. The code emits `existing_actor`
once, with token/attempt/mission/start/actor; `observedActor=5027806` thereafter.
Create-based paths and R1DEA's strict branch-specific ordering are unchanged.

CoverBegin and the post-presentation pre-enqueue gate reject starting this route
from the same mission or an unknown mission. This prevents treating a retained
teacher in the current dojo as a fresh destination epoch. No request/recovery
record is cleared by this refusal or by cover cancellation/death/staleness.

The regression first failed against the old actual model: all current actor and
control predicates true plus owned BossBattle still remained Loading forever.
The new test covers exact tuple, no Create, missing/wrong battle/attempt/read/
mission/load/player/pause/fight/control/actor evidence, continuity loss, cancel
and known death. Actual UI tests exercise both initial and delayed same-mission
refusal before native Post. Existing reader, clock, lease and input fixtures run
alongside it.

The Engine loading page now draws an animated indeterminate segment with phase
labels Preparing / Starting rematch / Entering boss arena. Width is bounded by
the viewport; actual DrawImpl fixtures cover640x360,1280x720,2560x1440 and reject
diagnostic text or percent signs. No time-based percent, asset-progress estimate,
deadline extension, controller setting or native pause change is introduced.

Live gate remains root's fresh exact-pair Yagyu entry, visible control and owned
return/restoration. Builds and offline tests alone do not satisfy it.
