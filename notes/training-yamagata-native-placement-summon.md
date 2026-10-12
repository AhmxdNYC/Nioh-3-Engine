# Yamagata same-arena native-placement summon boundary

2026-10-11. Static retained-dump continuation after user prioritized summoning and arena selection ahead of selectable moves. No ASI edits, native calls or live testing. This is a concrete alternative to the still-unclosed fresh-child owner transaction in `training-owner-lease-and-reset.md`.

## Actual shipped activation path

Mission0x1CBC script8008F3E2B14D calls `boss_id()` (65092) then `Character::Create` at bytecode1171A, inside `sub_event_boss_battle_start`. It then enters BossBattleMode11732, disables AI1175E for the intro, and enables AI117DE after control restoration. This establishes an exact native placement activation, not a generic summoned enemy.

Lower function1180740 resolves the placement-owning record collection through2C0308 (temporary strong pair, released before later lookup), requires placement record+8 bit0, and finds the pre-registered actor wrapper through135B78. It sets wrapper+DA word=1 and wrapper+EE byte=0. If wrapper+50==0 and context+2679 is nonzero, it additionally invokes20A05C and2869A0. Its boolean true means this activation path was admitted; it does not mean actor construction, AI readiness or a boss battle has completed. It neither allocates a new generated child ID nor installs another actor wrapper.

`2869A0` retains native appearance eligibility: +DA, +F0, +F8, mission/group checks2871EC/30A390/30A1D4/90EABC/620EEC and current-group comparison. On eligible entry, if wrapper+E0 is zero it calls **3F9E58(wrapper+28 character,2,wrapper+20 placement)** at286AE3 then sets+E0=1. It sets wrapper+50=1 and calls669110. Its inactive path calls symmetric **3F9900(character,2,placement)** at286B2B, then clears+E0/+F3, only when +DA=0, +E0!=0 and the additional global-context+58 value is zero (286B10..1D). Otherwise release is deferred and native resource ownership remains held. This resource acquisition belongs to the existing native placement; the mod should not also increment script-owner+3C8 or create an independent resource obligation.

`669110` checks readiness669388 and other native eligibility. At66919E a nonnull wrapper[0] skips the allocation branch. Thus repeated Create against a live Yamagata cannot honestly be advertised as a second summon or reset. When unmaterialized, native branches choose the existing placement representation (+58/+70/+78/+80), create through native factories and register using669A10, followed by2BE920/669990. Keep this complete path rather than calling a leaf factory. The exact selected branch for Yamagata still needs valid native wrapper evidence; no guessed wrapper layout is fabricated.

## Proposed first implementation, with a precise blocker

A bounded **Activate existing Yamagata** command can use the shipped1180740 path once, only inside the correct owned mission with exact registered65092 record and no live actor, loading/online/teardown excluded. It must refuse an already materialized boss and treat the return only as activation admitted. Native placement code owns construction/resource release; after issuing, retain pending state until actor identity/materialization readiness or an explicit native failure is established. Do not automatically re-request or raw-clear appearance fields on timeout.

Before wiring that command, settle **mission script ownership**: the current script itself executes the same Create during the boss intro. Manually creating before the gate can leave a live boss while its native script is still waiting for entry; creating after death can race `sub_event_boss_clear_check` and reward/clear flow. A safe first trial must select a phase where the script's one-shot Create has not yet run and coordinate its later activation, or use the native replay entry to reset the complete encounter. No arbitrary cross-arena spawn, instant respawn, reward suppression or paused idle behavior follows from this path.

This narrows implementation choices: existing placement activation avoids fresh-child reservation/lease problems; fresh-child summon still requires safe native weak-pair acquisition under keyed-container writer exclusion. Do not implement both at once or claim the easier activation is repeatable training. Root owns selecting the initial live scenario and ASI writer assignment.

## Byte provenance

Exact retained-dump windows (RVA/length hex), sufficient to reproduce the bounded observations:
- `1180740/F3` SHA256 `2a0b3cb79cf2b3042f572909a499c41318ab0f597d5bec6115bd15bf40f49f6d`.
- `2869A0/1B3` SHA256 `b8bf2197c749776b27da8a7f9569fb2cc1382f9296e17da6134d24a000ca152f`.
- `669110/268` SHA256 `f62127c4629927022ca16ff3b62c616dc3b9308f1fda7492330839cce60887c5`.

## Fresh-child lease implementation refinement

A concrete native upgrade helper exists: **1361A8(const WeakPair*,StrongPair*out)** clears output, reads weak control+8, uses nonzero strong-count `lock cmpxchg`, and publishes the pair only on success; returns output. **1361E4(dstStrong,srcWeak)** performs the same upgrade with a boolean result. This removes the need to reimplement atomic upgrade in a future adapter. It still requires an already-owned valid weak pair; it does not serialize135E7C traversal or protect logical owner identity. No safe-keyed-container acquisition proof is claimed.

Bounded independent review (rendering_opportunities): reproduced1180740/F3 and2869A0/1B3 hashes, activation/resource calls, and66919E live-pointer allocation bypass. Deferred-release qualification above incorporated. No repeatable-summon admission.
