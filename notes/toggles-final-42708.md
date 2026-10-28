# Boss-toggle integration into final 1.7.86 (42708)

Base: origin/pc/final-build 27d18704. Source: pc/toggles-hosted-merge e30319fe.
Integration branch: cxl-pc2/toggles-final-42708; integration6ed11d3a, harness refreshb5de7cb9.

The final branch already contains the death-toggle ancestry. The remaining production delta is in BossRematch: mark a successful Nioh3Training_FightNow in-place swap, reset that marker at CoverBegin, and end an issued hosted cover as Ready when the marker is consumed. There is no native mission attempt to wait for in this path. The only merge conflict was the EngineFix version: retained1.7.86 and discarded source-branch1.7.74. Removed incoming whitespace-only EngineFix changes. EngineFix and Training production trees remain identical to final27d18704; no existing UI, updater, save gate, action records or hit-label work was downgraded.

Tests run offline on PC2:
- Native-attempt actual-source harness:4 standalone return checks; unreadable exit gap regression;15 map ownership checks;96 transaction checks, all pass.
- Reader/admission actual-source harness:86 checks,16 snapshot reads individually faulted;7 source integration assertions, all pass.
- Cover models:73 state checks,48 branch/death/reload checks,42 dojo regressions,200 passive reader checks, all pass.
- player_hp_test, boss_vitals_test, move_toggle_test pass; toggle suite covers5061 catalogue entries and4998 toggle/veto cases.
- Existing harness dependencies were stale on final-build: added missing external stubs, production rematch_core include and an explicit N3_TEST_ZIG=1 compiler option. Native Windows/MSVC default remains unchanged. External stubs are not native-runtime proof.
- Training built with Zig0.16 using its build.sh flags including asynchronous SEH. SHA256 a33201a5f85cfcff70eb3517600ceb178be959590677eab6cd62ab20d2b4065d. Source unchanged, so this is an offline rebuild, not a replacement required for this patch.

Pending artifact: BossRematch build. Zig0.16 and isolated Zig0.13 both exit5 compiling sites.cpp with -Xclang -fasync-exceptions (O0/O1/O2 attempts). Omitting that flag compiles the source but was rejected as a release build approach because the guarded memory reads need exception coverage. No candidate from that weakened invocation was linked or installed. gx-pc has offline MSVC build task42839 for exact sourceb5de7cb9; return ASI bridge file+SHA256. EngineFix requires no rebuild because its entire tree is unchanged.

No game, saves, installed ASIs, INIs or controller state were touched. Main and pc/final-build were not changed. This branch is an offline candidate, not a runtime acceptance result. Do not declare the delivery complete until the changed BossRematch ASI and its hash are received.
