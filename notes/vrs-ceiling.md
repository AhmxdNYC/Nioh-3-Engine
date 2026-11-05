# VRS ceiling experiment (#69050 / #69059)

Base: pc/crash-23-fix 1ef6ada0, source of installed EngineFix 85ebffdc. No selective VRS or other performance branch merged.

Restart between arms using EngineFix.ini:
```ini
[Perf]
VrsCeiling=0
```
Use 1 for 2x2, 2 for 4x4 when OPTIONS6 reports Tier 2 AND AdditionalShadingRatesSupported; otherwise 2x2. Unsupported VRS or failed OPTIONS6 query leaves shading untouched. Default 0.

Integration: existing GPUOPT command-list vtable hooks, same List5+ interface layout validation. A Reset wrapper invokes the normal hook chain and original Reset, preserves its HRESULT, and applies only after success. Before DrawInstanced, DrawIndexedInstanced, ExecuteIndirect, ExecuteBundle and DispatchMesh, reapply via the original slot 77 (no recursive hook). No UI/pass exclusions; compute shading is unchanged. Actual recording-list GetDevice supplies OPTIONS6, not the dummy installation device. Per-thread scalar cache refreshes on Reset or list-cache miss; no retained raw COM interface.

Logs every 2 seconds: list_resets_tagged counts successful recording resets, direct_draw_calls_tagged counts direct graphics draw API calls (including DispatchMesh), indirect_or_bundle_calls_tagged counts API calls, NOT expanded draw count. Unsupported calls are separately counted. First eight capability checks log tier, extra-rate support and effective rate. Zero counters after gameplay invalidate a performance conclusion. Existing GPUOPT installation delay defaults to 20 seconds.

Old cxl-pc/vrs-27602 tagged zero because vrs_scope.h explicitly remained an offline integration prototype; its Scoped2x2 was never called from shipped GPUOPT hooks. Hardware support was not the demonstrated failure.

Offline validation: mock actual-device OPTIONS6, unsupported/failure fallback, mode 0, Tier 1 and Tier 2 rate selection with/without additional rates, missing List5 interface, successful/failed Reset HRESULT and tag counters. Run vrs_ceiling_test.cpp under MSVC. build.bat includes normal HUD pattern checks. No game launch, install, or benchmark by this companion seat; the Claude lead owns pc/pc2 OFF/1/2 tests and visual evidence. GPU counter evidence is still required; successful API calls do not prove all pixels were coarsely shaded.

Reference: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options6 (4x4 requires AdditionalShadingRatesSupported).
