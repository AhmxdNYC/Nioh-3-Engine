# Nioh 3 test hub (private maintainer checkout)

Run this from the repository root with Python 3.10 or newer:

```powershell
py -I -B tools/testhub/hub.py                      # catalog; runs nothing
py -I -B tools/testhub/hub.py plan all             # commands; runs nothing
py -I -B tools/testhub/hub.py run offline          # five small reviewed checks
py -I -B tools/testhub/hub.py run offline --report scratch/testhub-offline.json
py -I -B tools/testhub/hub.py list native --json
```

The offline suite checks the generated patch header, synthetic PresentMon statistics
and clock alignment, PDB scrubber fixtures, and the hub execution boundary. It uses
the standard library, temporary fixtures and source files. It does not build,
read a game dump, run a game, send input, contact the bridge, change installed files,
or take device locks. A missing script, timeout or nonzero exit fails the suite.
The JSON report names the limited scope and keeps a short output tail per check.
Reports remain private; never upload raw logs or reports without review.

Other lanes are **plans only**. `run all`, `run native` and `run device` are rejected.
The native catalog discovers `run_*.py` in both mod test folders, so newly added
native tests appear without becoming executable through the lightweight suite.
Some command lines are explicitly labeled templates and require local arguments.

| Lane | Existing coverage | Needs |
|---|---|---|
| `offline` | Header, stats/time, PDB fixtures, hub regression | Python only, this private checkout |
| `native` | All EngineFix and BossRematch `run_*.py`, including bench/audit/witness runners | Windows/MSVC; some need real D3D12, private dump, Capstone or private helpers |
| `dump` | EngineFix pattern scan; BossRematch site scan | User-owned decrypted game dump; private only |
| `wrappers` | PowerShell fixture tests; tour wrapper `-DryRun` | Reviewed dependencies, Windows and temporary/private state |
| `release` | Existing `release/check_release.py` quick/full/strict checks | Existing built ASI, profiles and private tree; full mode compiles native tests |
| `analysis` | `tools/bench` audits, proofs and CPU-ready control | Script-specific source/capture/dump inputs; some compile |
| `device` | Map tour, leak passes, pinned gameplay benchmark | Licensed game, actual hardware, device ownership, backup/restore and capture protocol |

The catalog organizes the main test families; it does not claim every research
script is a regression test. Analysis scripts have their own prerequisites and
interpretation. In particular the CBV recreation witness's default exit 0 means
it reproduced modeled counterexamples, not that production behavior is safe.

## The remembered map tour

`tools/tour/run_tour.ps1` runs the mod's Tour mode across mission maps. Mode 1
filters the route to unlocked/cleared missions. The wrapper backs up and restores
saves and installed ASI/INI files, verifies hashes, captures PresentMon/mod logs,
and handles bounded crashes/stalls. The mod blocks save writes while touring.
`-Passes 2` adds a leak comparison. `-Input ui` requires a manual Continue press;
`-Input vpad` uses the mod input pipe. Both are live game tests.

```powershell
# Print live commands; do this while another session is active.
py -I -B tools/testhub/hub.py plan device

# Only after the authorized lead has the device free and reviewed prerequisites:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/tour/run_tour.ps1 -Mode 1 -List main -Passes 1 -Input ui -OwnerPid $PID

# Read an existing private tour capture; this writes a report in that capture.
powershell -NoProfile -ExecutionPolicy Bypass -File tools/tour/report_tour.ps1 -Capture '<capture-directory>'
```

Mode 2 ignores unlock state and needs a separate review; it is not a default here.
The estimated route length is not a CI timeout or a guaranteed device runtime.
Tour coverage is stability/performance at sampled points. It cannot replace
manual combat, death/retry, menus/controller, loading and below-cap game-speed checks.

## GitHub scope

The private repository and public `AhmxdNYC/Nioh-3-Engine` are separate histories.
The public repository is a curated copy. This private hub must not be copied
wholesale: stats, wrapper, bridge and device tooling are private.

The public tree already contains native test sources, but its Python runners
still need dependency/path work before claiming a complete CI suite. The old
nested EngineFix build workflow is stale and the public repo has no active
workflow. A repository workflow belongs at the root `.github/workflows/`.

Start a public `portable` CI lane with the existing command:

```powershell
python -I -B engine-fix/tools/gen_header.py --check
```

The existing public `run_loadstats_harness.py` (synthetic call sites) and
`run_loadmem_harness.py` (synthetic heap) are reasonable first Windows candidates:
their wrappers already fall back to MSVC on PATH, and neither reads a game dump.
After activating MSVC x64, the intended commands are:

```powershell
python -I -B engine-fix/tests/run_loadstats_harness.py
python -I -B engine-fix/tests/run_loadmem_harness.py
```

These two builds were not executed during recording, so their hosted-runner
behavior remains to be validated. The GP harness uses a real D3D12 device and
belongs with hardware checks. Then extend a small no-dump native allowlist,
adapt selected runners to MSVC on PATH, remove home paths/private helper
requirements, and validate locally before enabling a Windows hosted job.
Do not label the remaining harnesses green
because a hosted runner has no game dump. Add further portable fixture tests only
with their reviewed dependencies. A build job should use the complete current
source list, not resurrect the stale three-file link command.

Hosted CI cannot reproduce map travel or device performance. Keep real runs in
the private repo and publish only scrubbed summaries with artifact/version hashes,
settings, protocol, results and limitations. Do not attach game dumps, shader
`.file` containers, extracted/decompiled game data, captures, saves, secrets,
device/bridge tooling, private history or handoffs. Scrub PDB paths from any ASI
before publishing; use the account's no-reply commit identity.

See the private `notes/handoff-claude-public-repo.md` before public staging.
