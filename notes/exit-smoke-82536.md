# Exit smoke stacking fix (#82536)

Base 3a9eb26d / BR22345f1c. Exit replay previously remapped every recorded bone effect to the real player, then appended the same recording for each press-spawned actor; the repeat option could duplicate those queues again.

Exit recording and dispatch now admit only world smoke (62557/62DF5 plus configured smoke-center IDs), never bone effects. Recording deduplicates by ID. Exit dispatch deduplicates by ID across all actor callbacks for the entire press, even after playback drains; repeated exit callbacks do not erase pending smoke. The repeat option no longer duplicates exit replay. World smoke has owner0 and keeps existing exit-position/travel relocation.

A press snapshot resets recordings, exit-ID history and old queued exit effects; form Begin does the same while preserving queued start-cover effects. Stored slot/core smoke templates are retained. The learned aura is never reattached to the player, even if the legacy Bone setting is enabled.

Release build PASS. 1137 offline checks PASS: primary/replay243, hit269, first-use52, ground45, smoke23, picker112, equipment36, rematch357; available sc22 PASS. Actual recorder/reset/queue functions are exercised with 24 repeated bone effects, duplicate smoke, two smoke IDs, append callbacks, enabled repeat/Bone settings, 100 presses, queued start-cover preservation and travel relocation.

Next Claude pc installs/relaunches when the human requests restart, checks repeated exits for smoke at endpoint and no accumulating aura, and supplies visual evidence. No game/install by this worker; existing attached effects require the planned relaunch for a clean comparison.
