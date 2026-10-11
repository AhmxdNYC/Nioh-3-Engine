# CBV retained-cache recreation witness

2026-10-11. Test-only follow-up to `handoff-codex-cbv-reuse-identity.md`. No mod source/default changes, game launch, bridge, Ghidra, public output or performance claim. Existing normal synthetic/30000-call replay was not rerun.

**Result:** two deliberately modeled repeated-key resource-generation counterexamples reproduced against the actual production wrapper; four controls bind the current generation correctly. This establishes which identity/lifetime assumptions the retained cache needs. It does **not** establish a reachable native-game defect.

## Fixture and exact results

New files are `mod/Nioh3EngineFix/tests/cbvupload_recreation_witness.cpp` and `run_cbvupload_recreation_witness.py`. The witness reuses the existing harness's relocation/mocks, never runs its original main, and calls production `CbuTableCall` versus relocated stock v2.02 `118E28`/`1192B0`. Native function fingerprints remain BAE85B33/2542B674. Only the necessary image prefix is read. All six patched contexts and their stock counterparts remain allocated, preventing raw-address recycling or the eight-entry cache limit from confounding the cases. `TestSetup` changes switches/original function only; `TestReset` is never invoked.

Each arm first binds a 64-byte slot and seeds its cache. Recreation then retains the same raw context, tag, list, CPU staging heap, native uploaded size, table count, root, and bytes. The CB case poisons and retires the old upload backing, gives the current generation fresh backing and allocation stamps, and leaves the prior allocation alive for safe inspection. The heap case retires the old descriptor heap and replaces its CPU/GPU bases and descriptor stamps/cursor. The observation checks the bound handle belongs to the current heap, its CBV belongs to the current backing with a live allocation serial, and its bytes equal stock. It never dereferences a retired GPU handle.

| Case | Stock current | Patched current | Skipped slots | Table hits |
| --- | ---: | ---: | ---: | ---: |
| Same lifetime reuse control | 1 | 1 | 1 | 1 |
| CB backing replacement; repeated key; CBN only | 1 | 0 | 1 | 0 |
| CB backing replacement; advanced tag; CBN only | 1 | 1 | 0 | 0 |
| Shader-visible heap replacement; repeated key; CBN+CBT | 1 | 0 | 1 | 1 |
| Shader-visible heap replacement; advanced tag; CBN+CBT | 1 | 1 | 0 | 0 |
| Both replacements; fresh native stage sizes/counts; CBN+CBT | 1 | 1 | 0 | 0 |

The final control clears F30's112 slot sizes and B70/B90's eight stage fields, keeps the raw context/tag/list/staging identity, and writes a positive-size dirty slot. This models the initializer's proven miss barrier, not a complete constructor or its native call route. CBS/CBF are off throughout; the two principal cases isolate CBN and CBT respectively.

## Native reachability boundary

The independent native audit is `handoff-codex-cbv-native-reset-identity.md`, commit bfe312aa5dce3c923697f41feceec50944047708. It proves ordinary owner reset advances the open tag (`490E50`, retained list sequence increment), while full initialization clears F30/B70/B90 (`2EFF491..2EFF4CB`). A fresh positive-size layout entry is marked dirty by `114B00` and forces stock upload. Those native barriers prevent the ordinary paths from reproducing the deliberately preserved-key cases. The new fresh-size control verifies that barrier for this fixture.

Still conditional: native repeated-address reconstruction with all relevant identities restored, a reachable shader-visible heap replacement during the same open identity, and a valid fresh sentinel-only/bit31 layout. The native audit does not establish these states; the witness does not simulate constructor/reset reachability or call `114B00`/`118DA8`. No cache-generation fix is justified by this test alone. Positive-size initialization and normal tag advancement are closed conditional invariants; valid-layout and ownership constraints remain the useful next proof.

## Reproduction and gates

From `D:/nioh3-wt/codex-default-bench`:

```powershell
py -I -B mod\Nioh3EngineFix\tests\run_cbvupload_recreation_witness.py --source-root 'C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix'
```

Default exit0 means the two expected modeled counterexamples reproduced **and** all controls passed. `--require-safe` exits3 when these modeled counterexamples exist; an unexpected fixture result exits1. It is intentionally not a general safety PASS. Source selection is explicit; headers must match the fixture worktree. The runner refuses the PC benchmark marker before compilation and again before execution. It uses the existing local MSVC setup/native dump and writes only its isolated ignored build directory.

Validated: default exit0 with the table above; strict-safe exit3; Python AST parse; redirected LOCALAPPDATA marker fixture refuses before compilation, leaves its marker intact and the executable timestamp unchanged. The real benchmark marker was not touched. Source SHA256s printed by the successful run:

```text
cbvupload.cpp d628c5b5f8d8f1746dcc2fc06cfef17b1a298a57b3fd6c5d38add765535a3304
cbvstage.cpp 2bd91c4f992fab721851a9d701c02d55c38024c11a881209b7358f6704b7d59a
existing relocation helper 51a1310d499ad4701deb8419f76980e004cc89368a43d048ac41b984284374c4
```

Production evidence: `cbvupload.cpp:90-113` raw-context cache; `:292-318` reuse identity/hit checks; `:360-373` table admission; `:541-554` harness reset/setup distinction. Selected decompilation exports were only `D:/nioh3-re/decomp/0x118e28.c` and `0x1192b0.c`; no new export census.
