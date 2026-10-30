# FormHide=3 direct-byte experiment (bridge #55469)

Base c5b38289. Adds only mode 3; the existing native FormHideTick body is unchanged. Use [Spirit] FormKey=1, FormSlot0=9 (capture only), FormHide=3 for the extra pc2 arm.

The new helper verifies all 29 bytes at A550E8, including the B440 store; saves the current byte; writes 1 at capture+0.5s; and restores the saved byte after 20s. It never calls native hide/unhide or pins the seal position. Capture replacement/clearing and mode disable also request restoration. Writes have readback checks. Failed restore writes retain ownership for retry. A changed current player pointer abandons the old address without writing it. An externally changed hide byte is left alone.

Offline verification: tools/rematch/test_formhide_direct.py compiles the actual helper with g++ C++17 -Wall -Wextra -Werror and passes 13 scenarios (guard last-byte rejection, modes 0/1/2 inert, delay, failed write/read, timer, saved one, player replacement, external change, disable, cleared/replaced capture, restore retry). Existing native FormHideTick compared byte-for-byte with base c5b38289. git diff --check passes.

Not verified: MSVC build, absence of tint, hide/body rendering, attacks. No game install or launch by ally/1. pc builds; pc2 owns the one extra live arm. Direct-byte mode intentionally skips SetHideModel's other native side effects; the visual verdict remains open. Pointer comparison is not an actor-generation proof; this experiment does not provide a general cross-mission lifetime facility.

Checkpoint: implementation and offline tests complete; next action is MSVC build then pc2 records tint gone yes/no, yokai still shown yes/no, APPLY/RESTORE log and screenshot. Shared handoff edits are deferred while ally/0's test-active marker exists.
