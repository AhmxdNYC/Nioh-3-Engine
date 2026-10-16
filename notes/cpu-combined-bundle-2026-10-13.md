# Combined CPU trial bundle ? 2026-10-13

User explicitly requested MAIN and PC2 try the available CPU candidates together. This isolated branch starts from hosted592a888 and combines RD a52f82a with M2S a1b5fef, retaining existing J1, MW, WG, LK and hosted menus. The engine keeps its1.7.73 version string; identify this candidate by source commit and artifact hash. Root may pair the separate latest retry3 addon; no addon is bundled or modified here.

Apply only these keys to the CURRENT device INI; do not copy repository defaults:

```ini
[Patches]
RD=1
M2S=1
J1=1
WG=1
LK=1
MW=1
M1=0
M2=0
```

All other settings and native save progress stay unchanged. M1/fullM2 remain OFF because their source safety gaps are unresolved; this bundle does not enable arbitrary experimental flags. RD/M2S are default-off in the shipped generic source and require these explicit per-run deltas. Startup restart required. The paired wait installer and engine menus match the reviewed hosted sources.

Merge resolution: retained RD reader/writer rows and added M2S0; retained both groups' exact-byte dependencies; combined Config members; regenerated patches_gen.h from merged JSON. M2S reuses the existing M2e row and installs only M2e+M2S0 with fullM2=0. RD helper/caves remain text-identical to a52f82a, MW helper remains text-identical to592a888. No live behavior was added during combination.

Checks performed on this combined tree: selected RD/M2S/J1/MW/LK plus WG's WS patch intervals have no overlap; ALL260SITESOK (patterns, bytes, dependency pins, targets and generated header). Existing focused native tests passed: RD predicate/enrollment/error/publication races and100completion/reset cycles; exact production RD cave RtlVirtualUnwind at four PCs; MW50concurrent completion/enrollment cycles; M2S40,002 exact-machine-code equivalence/contention cases. Release build uses the standard build.bat; only existing unrelated nt-shadow warning. The package contains build result and hashes.

No game launched, installed files modified or bridge used by this task. Root handles device ownership, current saves and recording. This bundle has NO proven combined gameplay/CPU/FPS result yet. One matched OFF/ON comparison can evaluate the bundle; it cannot attribute a gain to any one component. Capture actual game CPU and matched frames plus available GPU/RAM telemetry; confirm startup logs for RD, M2S and the retained switches. See component notes for preserved predicates, CAS barriers and limitations.
