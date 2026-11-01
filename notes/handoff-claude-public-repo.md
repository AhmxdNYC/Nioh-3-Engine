# Handoff: the public repo (Nioh-3-Engine) - 2026-10-11

Written by Claude Code for whoever continues (Codex, a later Claude session, or the user). Covers only the
open-source release. The engine work (FO, smoke tests, renderer) is tracked in STATUS.md / the Codex ledger as before.

## What exists

- **Public repo:** https://github.com/AhmxdNYC/Nioh-3-Engine (public, Discussions on).
  Local clone: `C:\Users\Ahmad Hamza\nioh3-public` (branch main, last commit `76d85ac`).
- **This private repo stays private and untouched.** Its git history contains secrets (webhook, bridge/device
  tooling), and Codex and the devices push to it. Never rename it, never make it public, never reuse its name.
- The public repo is a **curated copy with no history**, not a mirror. Nothing syncs automatically.

| public path | from (private) | notes |
|---|---|---|
| `engine-fix/` | `mod/Nioh3EngineFix` at 1.7.61 | src, tests, tools, ini, patches.json. CI workflow removed (it was stale). |
| `boss-rematch/` | tag `rematch-1.3.3` = main 1.3.3; HEAD = branch `rematch-instant` 1.4.0 (b2cfe09) | build.bat / test home paths stripped |
| `symbols/` | `re/symbols` (symbols.tsv + build_exports.py etc.) | plus `symbols/ghidra/N3ApplySymbols.java` with a GUI file picker (not compiled/run yet) |
| `research/` | 46 notes from `notes/` (all of `notes/renderer/` + the most-cited) | scrubbed copies; `SOURCE_MAP.tsv` maps `notes/x.md` -> `research/x.md` (90.8 % of symbol citations resolve) |
| `tools/progress/` + `data/` | chart/code-map scripts from `re/scripts_re`, `re/scripts_renderer` | ported to read `data/`; byte-identical output verified against the private scripts |
| `docs/` | `docs/` + new `BENCHMARKS.md` | every number evidence-checked (see below) |

**Releases** (tags): `enginefix-1.7.61` (Latest), `rematch-1.3.3` (tested, 33/33 in-game checks),
`rematch-1.4.0-beta` (pre-release, light test only), `symbols-v2.02`. Zips were built by hand in the session
scratchpad; see "Cutting a release".

**Issue #1** lists 9 suspect `symbols.tsv` rows (good first issue). The private DB has the same rows; whoever
confirms a fix should apply it in `re/symbols/symbols.tsv` first, then sync to the public copy.

## Rules that must not be broken

1. **Never publish:** game binaries or dumps (`Nioh3.dump.exe`, `data/local/`), extracted game text (`strings.tsv`),
   the GHDM/fog shader `.file`s (our HLSL spliced into Koei Tecmo containers), third-party mod zips (`mods/`),
   captures, saves, MODLOG/STATUS/handoffs, bridge/phone/Discord tooling.
2. **Strip the PDB path from every built `.asi`** before uploading: MSVC embeds
   `C:\Users\Ahmad Hamza\...\*.pdb`, which contains the user's name. Overwrite that string in place with the bare
   `*.pdb` file name padded with zero bytes (only the debug-directory string changes; code bytes stay identical).
3. **No personal info:** commit as `AhmxdNYC <104233015+AhmxdNYC@users.noreply.github.com>`; no name in titles;
   replace home paths with relative ones; replace Steam IDs; no bridge `#NNNN` ids.
4. **Accuracy:** every performance number must be backed by a capture and match `docs/BENCHMARKS.md`. Honest
   headline: on the RX 9070 PC the mod's same-picture fps gain is small at best (0 to ~+2.5 %, mostly noise).
   Its real wins are game speed below the cap (1.00x down to 48 fps at the 120 cap), Proton loading (Ally ~96 s ->
   ~38 s) and the opt-in presets (+3 % / +10 %, image changes). The old "+74 %", "+49 %", GHDM "+3/+11 %",
   W6 "+7-12 %", "B +5 %", "PP +2.9 %", ReBAR "+35 %" are retracted; do not reintroduce them.
5. Line endings: `.gitattributes` normalizes to LF; `.ini`, `.bat`, `.cmd` stay CRLF.
6. On Windows, the Bash tool mangles backslashes in heredocs: put Python edit scripts in files, use raw strings.

## Keeping it up to date

- **Symbols (most important):** copy `re/symbols/symbols.tsv` over `nioh3-public/symbols/symbols.tsv`, run
  `py -I symbols/build_exports.py` in the public repo, scan the diff for private text, commit. Bring merged
  community PRs back with `re/symbols/merge_incoming.py` (drop the public tsv into `re/symbols/incoming/`).
- **Charts and code map:** in the public repo, with `NIOH3_DUMP=<private>\re\Nioh3.dump.exe` (read-only use):
  `py -I tools/progress/regions.py` (if regions should refresh), then `progress_treemap.py`,
  `renderer_progress.py`, `progress_chart.py`. If the profile/GPU inputs changed, the maintainer-only
  `tools/progress/refresh_data.py` rebuilds `data/` from the private tree. Update the README numbers
  (functions named, renderer %) and `docs/code-map.md` table (`regions.py --md`) to match.
- **Research notes:** re-copy a changed note only after the same scrub (privacy, no long decompiled listings,
  numbers vs BENCHMARKS, names vs symbols.tsv) and update `research/SOURCE_MAP.tsv`.
- **"Currently working on"** in the public README is a 2026-10-11 snapshot; refresh it at checkpoints.

## Cutting a release

1. EngineFix: `py -I release/package.py --out <scratch>` in the private repo, then rebuild the zip without the
   optional shader files and the old `README-MOD.md`: `dinput8.dll`, the ASI (PDB path stripped), the ini,
   `README.md` = `nioh3-public/engine-fix/README.md`, `LICENSE.txt`, `LICENSE-UltimateASILoader.txt`
   (ThirteenAG, MIT), `SHA256SUMS.txt`.
2. Rematch: ASI + ini + `_bosses.tsv` from the matching build's `bin/`, README, LICENSE, SHA256SUMS. Check the
   version string inside the ASI matches the label.
3. Fix any ini comment that carries a retracted number (the 1.7.61 zip ini was cleaned for ReBAR, B, CBN/CBT, GF,
   MQ, PP and bridge ids).
4. Tag in the public repo, `gh release create <tag> <zip> --notes-file ...`; mark untested builds `--prerelease`.
5. **RULE (human #60918): every release carries a fresh-install zip.** Build `Nioh3-<ver>-full-install.zip` with
   `py -I release/full_install_zip.py build --asi-dir <released ASIs> --loader-zip <zip with dinput8.dll> --out ...`
   (run from the release branch worktree), attach it, put "New to the mod? download ...-full-install.zip" at the top
   of the notes, then `py -I release/full_install_zip.py check-tag <tag>` must print PASS before anyone announces
   the release. Update-only zips alone left new players without dinput8.dll in 1.7.87-1.7.90 (#60907).

## Open items

- **Private `README.md` still has the retracted numbers.** Not touched because Codex was writing in this repo.
  The corrected text is the public README + `docs/BENCHMARKS.md`.
- **Social preview image:** must be uploaded by the user in GitHub Settings -> Social preview
  (`Desktop\nioh3-preview-small.jpg`, 1280x640, built from the game's Steam art; not committed to the repo).
- `tools/progress/dump_nioh3.py` has never run against a live game (header patch verified offline only).
- `symbols/ghidra/N3ApplySymbols.java`: GUI file-picker path untested in Ghidra.
- `boss-rematch/tests/run_rematch_harness.py` needs private helpers (`tools/rematch_catalog.py`, portable MSVC);
  not runnable from the public repo yet.
- 41 lower-cited notes (550 symbol citations) are unpublished; `research/README.md` explains this.
- The optional GHDM shader could be shared as source (`gpu-shaders/GHDM_fast.hlsl` + build script) so users
  build it from their own install; not done.
- The user pasted an Anthropic API key into chat earlier; it should be rotated in console.anthropic.com.
- Renderer chart footer and README describe pc2 as a slow-link worst case; keep that framing.
