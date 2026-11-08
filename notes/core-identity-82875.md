# Spawn-owned core identity (#82875)

Source19a8f3db on cx-pc/core-identity-82875, base0da8fb1b.

Removed pressedId from the collision observer. Timing count lookup and count persistence now use the bound actor key>>4, matching FormKey/PressGhost descriptor identity. Binding requires a readable nonzero descriptor. Every active observer tick rejects a replaced bound actor or mismatched/unreadable descriptor and invalidates census completeness. Both native idle and native despawn learning independently require descriptor equality. Invisible tail stays diagnostic.

PickPoll ignores an opposite-direction edge while a form owns the player or within300ms of the accepted pick. It retains the original pick ID/direction/time and learned flag, and consumes the conflicting edge so it cannot be delayed until form end. A later fresh press after release is accepted. This covers the reported133ms D/L collision; it does not alter gamepad input itself.

Offline alternating same-address5B402/A65A1 sequence includes deliberately wrong latest pick IDs, first native learning and repeat exits: counts remain4/1. Mid-form address reuse changing descriptor blocks both idle and despawn learning. Picker tests reject133ms opposite edge and active-form opposite edge, then accept subsequent L press.

Release PASS. Targeted847 checks: hit334, picker115, primary/replay243, bind52, height51, smoke23, equipment29. Available sc22 ALL OK (unavailable captures skipped). Broad rematch recorded in final checkpoint.

BR99ef0ca678bbac1a9d30ede8e3ac10c747329a265e559c7b0e995a04186ab9fb, bridge #6477. Installed PC0da8fb1b untouched. Claude owns live switch/alternate tests and deleting corrupted5B402/A65A1 learned entries as agreed. No game or installed INI operation by companion.

Broad rematch357 PASS; total1204 offline checks PASS.
