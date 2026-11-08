# FormHideUntilEnd (bridge #55492)

Implemented on top of 7cfd7df0, offline. No game installation or launch by ally/1.

## Signals found

Living Artifact controller = player extension +178, extension = [player+3A0]. Begin 843010 sets controller+10 bit 1 at 843044/843046. End 1AB4C5C clears that bit at 1AB4C6F, and calls native unhide at 1AB4D55. Thus the observed active->inactive edge is byte [[player+3A0]+188] bit 1. This is Living Artifact state, not proof of a yokai transformation.

Seal actor disposal goes through 2BF7E8 -> 2BF818 -> 2BFC90. The wrapper loads the manager from RIP-resolved global (in this dump RVA 4745348); bucket table is [manager+BA0], 16 {head,count} buckets, linked node = actor+E0. 2BFC90 unlinks that node and marks it self-linked. Training's existing Actors()/Despawn() independently uses this registry. The new observer reads membership; it never calls deletion or reads a freed seal pointer to decide liveness.

Evidence: notes/evidence/formhide-end-55492/disassembly.txt. Source dump SHA256 f8a4fb31b691aaf9bb829fca4a32b49a4ad4c1c780f6c11cc516108b75b5d968. Runtime reads require exact 23-byte End, 9-byte Begin-bit, and 31-byte remove-wrapper guards; the registry global is decoded from the verified RIP displacement. No unknown-code hook or call is installed.

## Behavior

[Spirit] FormHideUntilEnd=0 (default): current 20-second restoration behavior.

FormHideUntilEnd=1: applies to FormHide 1, 2 and 3. Poll every 100ms from hide application. Once LA-active has been observed, its falling edge ends the hide, even if the seal already disappeared. Until LA has been observed, a seal that was confirmed present and subsequently disappears from a complete registry walk ends the hide. Merely finding LA inactive or seal absent initially is not an end event. If neither source is positively observed, restore at the original 20-second deadline and log unobserved-end-fallback-20s.

Unknown reads after a source was observed trigger a safety restore with LA-state-unreadable or seal-table-unreadable, explicitly distinct from LA-ended/seal-despawn. Registry traversal bounds total nodes at 4096 and rejects short lists, count mismatches and cycles rather than reporting absence. No timer-based success inference is used.

While a valid observed end source is holding the hide, the pre-existing FormSlot0 60-second capture cleanup is suppressed so it cannot silently impose a second duration cap. Explicit capture/mode changes and manual restoration remain available. Mode 3 restores its saved byte; native modes use native unhide. End-driven native mode retries failed unhide and avoids unhiding a replacement player. Mode 2 checks current registry membership before position pinning in this mode because LA can outlive its seal.

## Verification and next step

25 offline scenarios PASS in tools/rematch/test_formhide_direct.py. The extracted production observer/direct/native control flow compiles with g++ C++17 -Wall -Wextra -Werror against fake memory. Native calls and Windows SEH are explicit test seams. Covers the original 13 direct tests plus >20/>60 hold, LA edge, LA precedence, seal disappearance, no-source fallback, byte-guard failure, unknown/cyclic/truncated registry, default native timer, native end and failed-native-restore retry. All 3 new literal guards were independently compared against the dump. MSVC and actual gameplay are still untested.

Suggested pc2 arm: FormKey=1, FormSlot0=9, FormHide=3, FormHideUntilEnd=1. Report observed source (LA/seal), exact restore reason/duration, and whether body returns at the visual end. Capture the usual screenshots/control/fullscreen/focus evidence. A source-state log alone does not prove the visual test.

Checkpoint: code, tests and evidence committed on ally/formhide-end-55492; next pc builds then pc2 runs one arm. Shared handoffs/TECH_DEBT entry are deferred while ally/0 has a live test marker. Known limitation to record there: 100ms polling can miss a shorter entire active interval; without an observed positive state the code falls back to 20s, so it cannot claim to detect every end event.
