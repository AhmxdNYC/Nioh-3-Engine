# Ally fog native archive audit, 2026-10-11

Archive236 SHA256 d0b541c858490064d78c01d5c5cbe38221542a4a6fcf2cb0850f4f752ce706c0
and ZIP CRC pass. Six launch shader/loader/ASI hash receipts match their
declared arms; six restored save hash lists match entry, all exit receipts
record no game PID and restore_ok=1. These are archived device receipts,
not independent access to the Ally filesystem.

Rounded two-window summaries reproduce paired percent mean +0.08228% with
launch-pair SE0.31010% (three pairs), consistent with NEUTRAL. Full sampled
MangoHud logs are present, but twelve actual w1/w2.csv measurement slices
are absent. Requested retained slices or exact elapsed/index boundaries
in bridge1993; do not infer exact bounds from nearby rounded summaries.
Sampled-window p99 is not a pooled per-frame tail measurement.

Manually inspected fg_menu.jpg: Frame Generation Off, FSR3.1, rendering50%,
dynamic resolution Off, sharpness1.0. This is evidence for this queue, not
retroactive proof for the earlier default-stack queue232. Reported image
noise comparison and native G1 execution are separate from FPS acceptance;
no new universal same-picture or performance claim follows from receipts.

Remote cache sha/remotetime fields match entry; localtime/time change, so
the complete remotecache file is not byte-identical. No cloud-server upload
proof. G1 profiles candidate only; original cost must not be borrowed from
an older scene as a matched saving. Original-cost follow-up queued1990,
Ally acknowledged1992. No additive gain claimed.

pc2 archive235 SHA/CRC and internal current-state hash comparisons pass:
four entry files, four save files, three loader/shader controls agree with
their archived/reference values at2026-10-11T21:55:57Z. Old PID absent in
receipt. This closes the reported current-state mismatch question, not
historical recovery timing or independent old clock calibration. Original
failed L5 record remains intact. Fresh AABB timing must use new anchors.

Reproduce bounded audit: py tools/bench/audit_fog_native_236.py.

## Exact slices received and independently bound, 19:07

Bridge237 SHA256 ee4a116f1d47fee75334dafa9003884f3c0458901ee972a80c5fe62eebf6c315
contains all24 retained windows for default232 and fog236. Each200-row window
matches one exact contiguous source interval in the corresponding full CSV;
all timestamps are monotonic and sample intervals checked. CRC and all archive
SHA pins pass. Three window pairs in EACH queue share one boundary row. Treat
these as repeated samples; do not claim24 independent observations.

Exact unrounded launch-pair reconstruction: default232 +2.956726% with
SE0.432609%; fog236 +0.083129% with SE0.312534%. Deduplicating the shared rows
gives +2.957191%/SE0.426613% and +0.083132%/SE0.307401%, respectively. Fog remains
neutral. These are three launch pairs per queue, based on100ms sampled FPS;
no pooled per-frame tails, causal isolation or universal gain is established.
The earlier default232 missing actual FG-menu observation is not resolved by
receiving CSV slices. Fog236 has its own inspected FG-Off screenshot.

Reproduce: `py -I tools/bench/audit_ally_slices_237.py`.
Detailed exact source-line/timestamp windows are emitted to ignored
`scratch/ally-slices-237-audit.json`. Original-cost archive238 also downloaded
and SHA-verified; its different profile span is not a matched FPS comparison.
