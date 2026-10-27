# Side legends — task 41161
Base: release candidate 2cc759b4 (1.7.85), retaining ring size, pulse check and Wine fixes.
Ring legends are split 9 left / 9 right with the same names, palette and toggle predicates as Row. No bottom legend in Ring. The gap is 10 px at 1080p, scaled with viewport (minimum 8 px). Ring diameter is bounded by remaining panel width; panel height accommodates all entries.
Validation: all seven existing suites passed; expanded renderer passed 16,949,251 assertions, including 16:10 pixel checks and geometry at 84 viewport/size combinations (16:9 and 16:10, 600–2160 high). Offline production Small/Medium/Large preview sent to Discord. No game access or runtime claim.
Training SHA256: 316b8776f0efd9f670839cf5d5d1aa23b008d4a84f528437ec52c141e0a4959a.
Next: pc installs/tests the two artifacts; Row layout unchanged.
