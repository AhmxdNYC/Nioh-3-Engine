# 1.8.11 candidate (not published)

Source: cx-pc/release-1811-prep, source/version commit 964ca82a; merged main with fbec06ad and burst-replay-82280. All requested ancestors verified: 87dcdc89, f81af1c9, hit-clusters-82179, burst-replay-82280, multi-source-82289, 35fb8235. Burst replay A3 assertions are superseded by the existing A4 regression cases; those were retained.

Built four ASIs, PyInstaller 6.16.0 updater and Setup, Linux pyz, full-install and update combined ZIP. Logs: Temp/wt-r1811/release/validation-1811. Payloads: Temp/wt-r1811/release/dist-1811. Full SHA256SUMS: bridge #6535.

| Asset | SHA256 prefix | Bridge file |
|---|---|---|
| EF | 0c0612a8 | 6526 |
| BR | d6272b7d | 6527 |
| TR | a7c35109 | 6528 |
| CamKi | a2f3e418 | 6529 |
| Updater | e20d4632 | 6530 |
| Setup | 5aac8621 | 6531 |
| Linux pyz | 6ae37c19 | 6532 |
| Full install | c62d4452 | 6533 |
| Combined | 7b15be95 | 6534 |

Package integrity and packaged updater fake-release self-test pass. Rematch harness 357/357; camera, last-hit, cold-smoke and related new offline suites pass. Offline sweep: 85/103 runners pass; 18 matching baseline failures/unavailable checks are documented in release-1.8.11-validation.md and accepted under lead #83369. Testhub 5/5 PASS. Do not describe all 103 as green.

Gate list sent to pc in #83408, replying to #83344. Required: PC install/soul-core/camera smoke, pc2 Setup install/smoke, Ally pyz install/smoke. No game controlled by cx-pc. Publication waits for all three PASS and no newly introduced failure. No downstream gate task duplicated by cx-pc.

## Device gate progress

- Ally PASS #83484, forwarded by pc #83510: Linux pyz 6ae37c19 installed exact candidate ASIs; v1.8.11 shown; 5/5 soul-core uses, Fight now, no crash. Baseline and saves restored.
- PC task #83413: pending.
- pc2 Setup task #83414: pending.
- Publication remains held until all three pass.
