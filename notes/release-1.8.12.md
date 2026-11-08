# 1.8.12 candidate (not published)

Source: cx-pc/release-1812-prep. Base main 82ae5287 (public 1.8.11); merges 749e00c8 (Player/defaults + favorites b4c5ab4c) and e7c373ec (seal queue). Integration source 38fbf3d5; picker fixture adaptation 790ef099. No live game access by cx-pc.

Changed EF/BR release builds PASS. Reused exact public 1.8.11 Training, CamKi and Updater binaries; their sources did not change. Full-install integrity PASS, Setup and Linux pyz built from that full payload.
Affected offline sweep: 36/36 runners PASS after picker fixture imports queue state and stubs ghost presence; press-queue runner used MSVC_SETUP for this machine. Testhub 5/5 PASS. Rematch harness 383/383; picker134, queue12, favorites32, page-default dispatch/default-value assertions and actual provider/menu harnesses PASS.
Existing unrelated baseline exceptions remain documented in release-1.8.11-validation.md; no new offline failure found.

| Asset | SHA prefix | Bridge file |
|---|---|---|
| EF | a1b32e13 | 6557 |
| BR | 44ca2f23 | 6558 |
| TR | a7c35109 | 6528 (unchanged) |
| CamKi | a2f3e418 | 6529 (unchanged) |
| Updater | e20d4632 | 6530 (unchanged) |
| Setup | d18ba1df | 6559 |
| Linux pyz | 905a01c3 | 6560 |
| Full install | b370c4e7 | 6561 |
| Combined | c90b0918 | 6562 |
| SHA256SUMS | 31def272 | 6563 |

Detailed live checklist: release-1.8.12-gates.md, bridge #6564.
Single device gate tasks sent: PC #83620, pc2 #83621, Ally #83622. GPT game-slot owners requested because Claude quota is exhausted; companions retain no-game-control restriction.
All three results pending. Publication is held until each explicitly passes installer/version, favorites reorder/cursor, Player tab/mode, page reset/persistence/unrelated-page preservation, soul queue and Fight now/crash checks. Do not duplicate these tasks.
Discord progress sent at integration start and candidate readiness. Actual game screenshots requested from test owners; no new live visual claim yet.

## Concrete live-gate blocker

All three gate routes returned weekly-limit notices: Ally #83623 (also #83635), pc2 #83624, PC #83625 (also #83631). Merely writing GPT-only in a bridge task did not select an available game-slot owner. No 1.8.12 live gate has run or passed; no repeated requests sent. The companion no-game-control restriction remains in force. An authorized available game-slot agent must take the existing tasks #83620/#83621/#83622, or they must resume after quota recovery. Publication stays held; 1.8.11 remains public.

## Routing recovered

- 2026-11-08 5:07 am EST | cx-pc #83690: all three Codex game-owner routes verified (PC83674, pc2 83672, Ally83671). Resumed original gates83620/83621/83622 via83705/83706/83707; no duplicate tasks. Candidate1.8.12 EF a1b32e13 BR44ca2f23; live0/3, publication held.
  Routing blocker resolved; game results unproven. Next: collect full feature/install verdicts; companions do not control game.

## PC preflight hold

- 2026-11-08 5:11 am EST | cx-pc/0 records PC RESULT83758 for gate83620: BLOCKED by ambiguous player activity; 0 live feature checks, candidate hashes4/4 match, baseline files18/18 unchanged.
  Existing presence question83747 and needs-player entry remain with pc; no duplicate question/retry. Evidence6569/6570; next PC owner resolves presence and resumes original gate. All3 feature passes still required before publication.

## Ally gate failed

- 2026-11-08 5:25 am EST | cx-pc/0 records Ally gate83622 FAIL83907: queue fired5/5 but attach4/5; first queued DOWN reused old actor address, rejected as already alive, then seal=0 timeout. Other installer/menu/reset/Fight-now checks passed; zero crashes.
  Evidence report6583 ZIP6584 logs6579 rows6580; restoration87/87 files +9 saves +16/16 targets reported. Candidate EF a1b32e13 BR44ca2f23 is NOT releasable. Next: offline queued actor-snapshot lifetime fix with reuse regression, rebuild/package fresh candidate and device-owner retest.

## Current candidate: r2 (supersedes original assets)

- 2026-11-08 5:33 am EST | cx-pc/0 integrated fix97000f47 as6f71c28b; independently reran8 suites853/853 PASS and Release BR build. Scrubbed only PDB paths, executable bytes identical. Fresh1.8.12 r2 EF a1b32e13 BR c117129f (#6592), Setup145f8a7f (#6593), pyz e00ff4d8 (#6594), full90f7d745 (#6595), combined f2f12ca9 (#6596), sums6597.
  Old BR44ca2f23 rejected. Fresh live gates0/3; next: same device owners test exact r2 packages, keeping PC presence83747 hold. Public stays1.8.11. Evidence release/validation-1812r2; packages release/dist-1812r2.

Tests: queue snapshot14, queue12, first-use52, ghost243, last-hit346, cold23, picker134, equipment29 PASS. Full-install payload checked and BR byte equality verified. Setup embeds rebuilt full payload. No live recovery claim.

- 2026-11-08 5:34 am EST | cx-pc/0 r2 BRc117129f dispatch recorded: Ally83989 on83622; PC2 83990 on83621 AFTER original active run restoration; PC83991 on83620 only AFTER presence83747 resolved. Do not duplicate these retests. Next: collect all3 full live verdicts; publication held.
