# Boss filters: Dojos and Not recently confirmed

Task #84676, human #84670. Base public 1.8.12 source 448b7a7c. No publication or main-PC/Ally game action.

Boss Matches gains two distinct list stops after the eras. Training retains its original list cycle: boss-path confirmation does not certify Training. Favorites sorting, row IDs, and cursor anchoring are unchanged.

Dojos uses existing catalog mission names `The Way of the Warrior:`, `The Way of the Ninja:`, and `The Way of Onmyo:`. This selects all nine Novice/Adept/Veteran missions. The Rift region also contains Edo Castle, while the lesson flag covers only four of these nine; neither alone defines dojo membership. Direct master duels elsewhere are not classified as dojo missions without supporting catalog metadata.

Not recently confirmed means no audited named-row working-entry evidence applicable to the current boss path and matching launch descriptor. It does not mean unbeaten, and does not certify every death/retry/exit scenario. There is no wall-clock age cutoff. Per human #84740 the existing results were reused rather than repeating a boss sweep.

Final registry: 56 confirmed / 65 unconfirmed across 121 catalog rows. Records #75395 and #75409 supply 50 individually named clean boss/wave-entry passes, not their aggregate pass totals. #76607 supplies R1DEA after the alternate-entrance fix; #76524 supplies completed RD759 lesson; #79221 supplies four actual scroll-wave entries on the straight-entry path (E00/E02/E04/E07), plus a newer RA0CB smoke. #80240 refreshes R3513/REA75; #84163/#84214/#84169 refresh R1CBC. Dates remain the actual November 6/7/8 report dates. The JSON preserves each row's evidence and limited scope.

Compatibility audit: compared 551d4663 against public 448b7a7c. All imported R-row catalog launch fields match; RBAB8 changed to refusal and is excluded. ExecDirect's change is resume bookkeeping, while native branch/no-boss-mode fixes target specific cases. The later scroll entry path is materially different, so only named post-change scroll evidence is included. Wrong-identity, manual-warp-only, timeout/retry-only, incomplete lessons, and undocumented rows remain unconfirmed; the exclusion reasons are recorded. These are reviewed earlier results, not a claim that all 56 were rerun on 1.8.12.

`tools/recent_boss_evidence.json` records the actual evidence date, source revision, covered source hashes and launch-descriptor key. The normal build regenerates `recent_boss_evidence_gen.h`: a covered boss-path source change invalidates all confirmations instead of carrying stale PASS forward. UI/filter-only changes do not invalidate the path fingerprint. Runtime launch/arena descriptor changes also invalidate the row. Adding confirmations or refreshing the recorded source hashes requires new named-row evidence review; never regenerate expected hashes merely to make a check pass.

Offline: actual catalog membership and descriptor/source invalidation tested by boss_filters_offline.py; production list/favorite/cursor tests pass 32 checks; core catalog/route harness passes 383 checks. The only legacy assertion update restricts the era-length check to era labels, since the requested filter name is longer. Release build and PC2 menu/screenshot gate recorded in handoff when available.

Re-audit #84941 supersedes the56/65 count above:63 confirmed/58 unconfirmed. See boss-filter-evidence-audit-84941.md and JSON for all65 original entries, sources, qualified historical failures and aliases. Seven additional confirmations, no new live runs.
