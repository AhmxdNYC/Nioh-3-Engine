// tilestats.h - TS: D3D12 tile-mapping / queue-sync census of Nioh3EngineFix ([Profiler] TileStats = 0 by default),
// plus an optional same-result merge of consecutive UpdateTileMappings calls ([GpuOpt] TileBatch = 0 by default).
// Background and the static census: notes\gpu-tiles.md. Integration: mod\Nioh3EngineFix\tilestats_INTEGRATION.md.
//
// What the engine does (v2.02): every UpdateTileMappings / CopyTileMappings comes from the KTGLGraph resource cache
// (texture streaming, cache object at [Nioh3+45BA588]+61A0h):
//   - job "KTGLGraph.Cache.UpdateTileMapping" (126630 -> 290900 -> 290F94): direct ID3D12CommandQueue::UpdateTileMappings
//     on the cache's own COPY queue, then Signal(cache fence);
//   - render-frame begin (RenderFrameBegin 4A529C -> 32FE938 -> CacheFlushOnGfx 2EF0938, twice per frame): whatever the
//     job left in the request ring is recorded as ECL token 4 (UpdateTileMappings) + token 7 (Signal) into the GFX
//     stream and replayed on the GFX (DIRECT) queue by the ECL thread (2F0103C);
//   - token 5 (CopyTileMappings, 2EFC060) on the GFX queue, rare.
// Each call carries ONE resource and ONE heap; every region is a single tile (1x1x1 box), every range one tile.
//
// TS hooks the runtime's ID3D12CommandQueue vtable (shared by all queues of the device; also vkd3d-proton's):
//   slot 8 UpdateTileMappings, 9 CopyTileMappings, 14 Signal, 15 Wait. ExecuteCommandLists (slot 10) is OWNED by the
//   OSD (osd.cpp, it needs the caller's return address): the OSD's hook calls OnEcl() first. When the OSD's hook is
//   not installed, InstallOnQueueVtable(qvt, false) patches slot 10 itself.
// Log (every 30 s, LogLine): "stats tiles: ..." calls / regions / ranges / tiles per frame, CPU us inside
//   UpdateTileMappings (mean, max), CopyTileMappings, and ECL / Signal / Wait per queue type (game callers only;
//   other modules' calls are summed as "other").
// TileStats = 2 also logs a one-shot trace of the first 256 UpdateTileMappings calls after 60 s (QPC, queue type,
//   regions, us; to line up with an RGP / GPUView capture) and an estimate of redundant regions (a tile re-mapped to the
//   page it already maps, as far as TS saw every change to that resource).
//
// TileBatch (1): a game UpdateTileMappings is held back and merged with the next one on the same queue when both are
//   for the same resource with the same Flags, use the same heap (or one side maps only NULL/SKIP ranges), every region
//   of both is a single tile, the ranges cover the regions exactly, and no tile appears in both. The held call is
//   issued (one call, regions and ranges concatenated in order) before ANY other operation on that queue (Signal,
//   Wait, ExecuteCommandLists, CopyTileMappings, an UpdateTileMappings that cannot merge) and at every Present, so the
//   final mapping of every tile and its position relative to every fence/list on that queue are unchanged.
//   TileBatch = 2: count what would merge, change nothing.
#pragma once
#include <cstdint>

struct ID3D12CommandQueue;

namespace tilestats {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Profiler] TileStats (0/1/2), [GpuOpt] TileBatch (0/1/2)
};

// Reads the ini. Returns true when TS needs its hooks (TileStats or TileBatch != 0).
bool Start(const StartInfo& info);
bool WantsHooks();

// Patches slots 8, 9, 14, 15 of the runtime's ID3D12CommandQueue vtable (and slot 10 when eclViaOsd is false). Call
// once, with the vtable of a dummy queue created on the game's device (osd.cpp InstallHooks has it), AFTER the OSD
// patched slot 10. Returns false (TS stays off, nothing patched) on any failure.
bool InstallOnQueueVtable(void** qvt, bool eclViaOsd);

// From the OSD's ExecuteCommandLists hook, BEFORE it calls the original: counts the call and issues a held
// UpdateTileMappings of this queue first (TileBatch). gameCaller = the OSD's CallerIsGame(_ReturnAddress()).
void OnEcl(ID3D12CommandQueue* q, unsigned numLists, bool gameCaller);

// From the OSD's Present hook, once per frame of the drawing swapchain: frame counter + issues every held call.
void OnFrame();

// Stats thread, every 30 s: one "stats tiles:" line (no-op when TileStats = 0).
void LogLine();

// Menu / tests: "TS: <calls>/f <tiles>/f ..." (empty when off).
const char* StatusLine(char* buf, unsigned n);

// Tests only: switch TileBatch at run time (0/1/2); held calls are issued first.
void SetBatchModeForTest(int mode);
struct Counters {
    uint64_t utmCalls, utmRegions, utmRanges, utmTiles, ctmCalls, signals, waits, ecls;
    uint64_t batchIn, batchOut, batchMerged, wouldMerge, redundantRegions;
};
Counters Totals();  // all queue types, game callers, since start

}  // namespace tilestats
