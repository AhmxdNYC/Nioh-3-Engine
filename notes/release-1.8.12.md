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
