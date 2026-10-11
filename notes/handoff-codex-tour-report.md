# Codex tooling follow-up - 2026-10-11 00:40 New York

User asked to find other useful work while remote tests remain pending. Fixed the repeated completed-tour reporting failure, without a game launch or ASI/default changes.

Cause: run_tour has ErrorActionPreference Stop, and PowerShell5 turns redirected native stderr into terminating NativeCommandError. summarize_tour.py's normal [pm_time] clock-offset message therefore interrupted its execution/capture. New report_tour.ps1 resolves py first, captures diagnostic/output with Continue only around the native call, restores caller policy in finally, and throws on nonzero exit. run_tour calls it after save/file restoration and lock release. Actual report errors remain visible.

Verified against captures/pc/fastab/20261011-0019-fo11fix/tour/20261011-001954-tour: old path reproduces +4h diagnostic failure; new path generates tour_report.md/tour_points.csv. Missing-log negative returns explicit exit1 failure; caller Stop preserved; run_tour parses. captures/tour-report-check.ps1 and .log retain the manual checks; captures/tour-report-harness.log ALL PASSED (44 checks, dump+wiring). No new build shared, no performance claim. Use fastab's mod-clock report for FPS; the recovered general tour report is supplementary.

Main1.7.61/FOoff, experimental codex-fo10 ddabf46 and kit225 unchanged. Pending: pc2 smoke1767, Ally1768, conditional pc2 FO5 queue1781. No remote results through1781. No auto-wake or Claude calls. Next authorized performance work is small replay-thread wins from the original brief. Graphics PCM already exists and previously measured about+0.5% on pc2; do not treat it as a new fix. Candidate SRV cache needs descriptor mutation/lifetime proof; compute PSO memo is still unbuilt. No new performance queue issued here.
