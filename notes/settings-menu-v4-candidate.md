# Private settings menu candidate — handoff

Stopped at the user's clean-handoff request. Branch `codex-settings-menu-v4`, worktree `D:\nioh3-wt\codex-settings-menu-v4`. Addon/native v4/training base is `0b25b2b` (1.4.6). Alignment commit `e1ce9b6` restores the tracked EngineFix subtree from main `8fcb81b` (1.7.61, ExtraPerf and CB upload) without replacing addon code. UI-only donor is `88af4b9b`; source candidate versions are EngineFix **1.7.67** / addon **1.4.7**. The final source commit is this branch's HEAD, separate from the alignment commit. Neither is merged or deployed. Existing installed addon 1.4.0 was not touched.

EngineFix root → Settings retains display, frame generation, gameplay, performance, Extra performance, experimental children, diagnostics and next-launch controls. “Original controls (flat order)” preserves kOrder and Layout=0 automation. “Configuration by ini section” adds section submenus with every shipped key and 18 documented optional keys. Addon root → bosses → boss actions retains existing request IDs, validation and confirmation; root → Settings works even with Rematch off. A/Enter selects, D-pad/arrows or left stick navigates, Left/Right edits, B/Esc/Backspace returns one level, and LB/RB/PgUp/PgDn skips ten rows. Controller ownership/release masking is from the donor. Native acceptance of those gestures remains open.

The supplemental catalog editor saves **only explicitly edited supported rows**. Startup preferences do not change current game state. Overlay brightness/popup and menu scale apply through existing overlay state; 13 tuning rows use the existing LiveTuning setter, report its reply and display **requested**, so a missing installation/unknown build is not presented as a live application. Mode 2 for CBS/CBN/CBT requests OFF now and installs OFF on restart. An explicit original-menu/Remote edit supersedes a staged preference for the same key. No option is applied merely by loading/opening a catalog page. Defaults, original enums/kOrder/ExpId/INI keys and Remote operations are preserved; Training Observe remains 0, and Training Enable remains a non-editable reserved key. The native Prepare/pump/action/routes/pipe command bodies and training.cpp match `0b25b2b` in the light source check. Gate-only boss routes now say “Boss gate via shrine (not direct)” rather than implying verified arena entry; native ReqFight/CanArena/calls are unchanged.

| Catalog | Shipped keys | Optional keys | Numeric / choices editable | Ini-only |
|---|---:|---:|---:|---:|
| EngineFix | 289 | 18 | 114 / 175 | 18 |
| Boss Rematch | 24 | 0 | 1 / 18 | 5 |

Coverage authority: `mod/settings/catalog.json` contains each key, default, group, label, type, domain/step, apply mode, legacy mapping and limitation. Generated `src/settings_catalog.h` files come from `mod/settings/generate_catalog.py`. Numeric UI steps/caps are reviewed subsets where the native domain is broader; arbitrary custom values still need an ini editor. Some typed bounds have conservative UI caps rather than the full native integer domain. Existing optional instrumentation not documented in the shipped ini is outside this catalog. This candidate does **not** satisfy literal controller editing of every possible free-form/private option.

Exact remaining controller-edit gaps:

- Engine text/list inputs: `Menu.PauseHintFlags`, `GpuOpt.DeferThreads`, `Profiler.JT_Thread`, `ShaderSwap.Folder`, `ShaderHot.Folder`, `Debug.FnProbeList`, `Debug.TourList`, `Debug.SkipFxFile`, `Debug.SkipPSO`, `Debug.SkipMarker`, `Debug.SkipClear`, `Debug.LiveCtlFile`.
- Engine workflow/test-only controls: `Patches.CBF`, `Tuning.CBF_Queue`, `Debug.FnProbe`, `Debug.Tour` (session saving disabled), `Debug.VirtualPad` (can hide physical controllers), `Debug.SkipFx` (changes rendering). They are visible, cannot be toggled here and point to manual setup. Their appearance is not a safety endorsement.
- Addon: reserved `Training.Enable` (mutation unavailable), executable `Rematch.SelfTest`, and free-form `Rematch.Catalog`, `Rematch.Verified`, `Rematch.Arenas`.

Verification completed: `mod/settings/test_catalog.py` PASS on the final metadata (coverage/defaults/generated headers/protected native source); `tools/testmatrix/menu_map.py --selftest` PASS for PC/Wine and both layouts; `git diff --check` PASS. Actual-source C++ EngineFix menu and addon UI-model harnesses compiled /W4 with zero warnings and PASS, including catalog reachability/bounds/changed-only save/live-vs-startup behavior/reserved training no-op/back priority/controller release/left-stick directions. Those C++ runs preceded the final expanded metadata, version/title/log text edits: **rerun them on final HEAD**. The initial addon fixture link needed an empty `g_img` fixture, fixed before the passing rerun. Runner now uses an explicit worktree-local batch to avoid nested cmd quoting. No game calls/hooks are installed by those harnesses.

Outstanding gates before sharing any ASI: final actual-source menu harness rerun; both DLL release builds (including ui.cpp/main.cpp compilation); addon lifecycle/training/rematch harnesses; site resolution and EngineFix patterns (`ALL N SITES OK`); strict `check_release.py --full` on this hybrid worktree with current ignored release inputs; root source review. No candidate ASI was built or shared. Build/harness outputs stay ignored in this worktree. Use main's read-only `tools/msvc/setup_x64.bat` in a child shell; do not copy its relative setup without the toolchain. No new game/native/release/build work was started after the handoff request.

Runtime gates remain: PC/Ally panel fit/readability, controller slots 1–3 and disconnect/reconnect, held buttons/triggers/sticks after close, mixed Back/confirm input, coexistence with EngineFix hooks in both load orders, keyboard fallback/foreground behavior, actual save failure/restart reload, installed-versus-uninstalled tuning replies. The addon still uses asynchronous keyboard polling. EngineFix fallback polling can expose input if its hook is unavailable. There is no playable-training, direct arena, same-look/performance, input-latency or fps claim. Root owns the separate native arena pilot and any device/bridge/public/deployment work.

## Successor validation - 2026-10-11

The successor reran both actual-source C++ menu harnesses on 0870530: PASS,
zero warnings. Catalog/default/protected-native-source checks PASS (331 rows,
308 typed and 23 explicit manual-only); menu-map selftest PASS in both layouts
and both PC/Wine modes. Engine patterns255/255 and addon sites94/94 PASS.
Addon rematch60, training snapshot53 and lifecycle143 checks PASS.

The lifecycle test still expected UI startup to depend on Rematch being live.
Updated that stale wiring expectation to the reviewed settings-while-disabled
behavior; pump enable/install/failure checks remain intact. Restored main's
current tools/testmatrix/live_keys.py so diagnostics and CB upload controls
match the already aligned1.7.61 core. Its selftest passes. No native route,
training mutation, setting default or runtime source was changed in this unit.

Both release ASIs built locally (not installed/shared):
- Engine1.7.67 SHA2566890c0605e3346d915f7f2263337716ea1ddad6c0b4ea36efebb2c4d4aa8e32d.
- Addon1.4.7 SHA256532712864032b70337d0b58d77ac0baa33da001799317e7e61199b4981c38d1e.
Engine build reports inherited C4456 shadowing in dllmain.cpp:1467 (unchanged
from1.7.61); addon build has no warnings. These remain private candidate builds.

Initial full-release attempt lacked compiler environment for older runners,
used an old live-control key list, and started before the binary existed.
Rerun uses main's setup_x64.bat in a child shell and corrected live_keys.
Full-suite final receipt follows. Existing quick check has0 errors/3 warnings:
undocumented/defaulted keys, menu-read keys and provisional ALL-profile flags.
Strict warning-free release acceptance is not claimed. Runtime/controller,
source integration and public packaging gates remain open.

Final full-suite receipt: check_release.py --full PASS,0 errors/3 warnings,
36 actual harness runners plus patterns255/255, on the candidate with aligned
live_keys and both built ASIs. Log remains private/ignored at
mod/Nioh3EngineFix/build/successor-release-full.log. No runtime acceptance or
strict warning-free result implied. No remaining validation process from this run.
