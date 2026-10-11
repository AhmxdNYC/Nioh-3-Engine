# Small optimizations and handheld playtest request

User steering, 2026-10-11, approximately19:20-19:22.

- Keep small optimizations in the mod when correctness/gameplay checks pass;
  a tiny or unmeasurable FPS gain alone is not a reason to discard the feature.
  Retain honest evidence labels and same-picture/default protections.
- Existing distinct `lodu` option remains in tracked apply_cull4.ps1 and
  ally_shaders.sh. Exact private container SHA256
  5f7f3b120b909c74c0238d7af3f4f3cd43299840b21a5a49e195545ea54150f1 is retained.
  Ally2033 reports exact execution and static/moving smoke without visible
  defects, original0.087ms/candidate0.084ms across a small GP2 sample. No
  profiler-off FPS pair was run; this is not a demonstrated FPS gain. Archive
  241 is retained/hash-verified for audit. Keep it separate from `mshlod`,
  which replaces the same file with a different record rewrite. No silent mix.
- Root archive audit: file241 SHA256
  `90394a60477bc4e8a13a5adf6345c28b3d2c31173f8d05ee5a357cd236710f88`,
  ZIP CRC PASS. All four valid/invalid/aborted arm save manifests exactly match
  the archived current entry. Valid gA field_time19:02:19 precedes its two
  original63c07c21 rows0.087ms; gB field_time19:07:49 precedes its two
  candidate d5a1a069 rows0.084ms. Exact candidate container is present in gB's
  hash receipt and original in gA. This establishes field execution within
  the archived run, not that a test is currently running or perceptual equality.
- Fog remains retained; the fresh controlled sampled result is FPS-neutral.
  Future candidates still require correctness and native gates; this steering
  removes a minimum-size gain requirement, not those gates.

## Required Ally delivery once ready

The user explicitly wants boss rematches and playable training on the handheld,
followed by a Discord/webhook notice inviting them to try both. This is future
deployment authorization once the features pass their gates; do not ask again
for the same permission. Do not call observer-only training a playable mode.

Complete local native arena/control/return and playable practice checks, review
the exact candidate builds, then coordinate an idle Ally slot and back up its
CURRENT saves, INIs and binaries. Preserve the Ally's own graphics/settings and
controller preferences. Deploy the hash-pinned private feature build, complete
short handheld control/navigation/combat/return and restoration checks, and
leave the accepted feature build installed only with a complete rollback path.
Do not touch an active user game. Record exact installed hashes and accessible
menu actions before sending the requested **ready to try** webhook.

Current state: private Engine1.7.67/addon1.4.9 is built/offline checked. Local
C72E native start334140 reached expected mission/boss333550 with zero warps;
the harness discarded an early landing proxy and stopped during the intro,
so root control/return acceptance is still open. Native Yamagata practice
tooling is under review; arbitrary enemy spawning/stage switching is not yet
implemented. No handheld feature deployment or ready notice has occurred.
