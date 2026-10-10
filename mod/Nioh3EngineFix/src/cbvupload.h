// cbvupload.h - CBU: cheaper per-draw constant uploads on the replay thread (notes\renderer\cbv-upload.md), all default off.
//
// Why: SynUploadCbvTable 118E28 (inside every 6A / 69 draw flush) memcpys (B94250 at 118F4D) the FULL declared cbuffer of
// every dirty CB slot from the CPU shadow (SynCtx+BB0h) into the UPLOAD ring (SynCtx+B28h), creates a CBV on it and then
// re-copies the stage's whole CBV table into the shader-visible ring. With ReBAR the ring is BAR VRAM: regular (tail-first,
// overlapping) AVX stores across PCIe. pc2 (5600X, PCIe 4.0 x8): 6A costs 5.7 us/draw of CPU vs 0.43 us on the PC.
//
// Keys (each 0 = not installed, 1 = installed + on, 2 = installed + off; live 'Tuning.<key> = 0/1'):
//   [Patches] CBS (F3) streaming copy: the memcpy call sites 118F4D (graphics) and 495AF2 (compute CBV table 4959EC) call
//       CbuStreamCopy: forward, 16-byte non-temporal stores (movntdq), whole 64-byte lines in order. Same bytes [0, size)
//       as memcpy; nothing outside [dst, dst + size) is written. One sfence per table (CbuTableCall, so CBS installs the
//       wrapper too) / per compute copy; an sfence per copy cost ~+400 ns per upload on the RX 9070 (tests/cbs_bench.cpp).
//       Falls back to memcpy if dst is not 16-byte aligned (never seen: ring blocks are 256-aligned).
//   [Patches] CBN (F1) skip no-op uploads: the SynFlushCbvTables -> SynUploadCbvTable call (118E19) goes through
//       CbuTableCall. A dirty slot whose shadow bytes equal the bytes of that slot's last upload, made in the SAME open
//       command list (SynCtx+6D0h fence tag and SynCtx+580h list unchanged) with the same declared size and staging heap,
//       gets its dirty bit cleared before the original runs: no ring block, no memcpy, no CBV; the stage's staging CBV
//       still points at the previous block, which holds identical bytes and is fenced by the same list. The table copy
//       and SetGraphicsRootDescriptorTable still run. Graphics stages only (the compute 4959EC / async paths are not
//       touched by CBN / CBT).
//   [Patches] CBT (F4) CBV table reuse: when, after CBN, no slot of the stage needs an upload and the stage's last table
//       copy was made in the same list with the same descriptor count / staging heap / root index, the original is not
//       called: SetGraphicsRootDescriptorTable(root + stage, previous table) and SynCtx+B90h[stage] = n. The previous
//       table holds the same descriptors (only 118E28 writes the graphics CBV staging slots 0..97, and every call is seen).
//       Needs CBN's tracking (CBT alone counts but cannot hit except on bit-31-only masks).
//   [Profiler] CbvStats = 0 / 1: counts uploads, bytes, no-op uploads (bytes equal to the slot's last upload: any list /
//       same list = what CBN can skip), table-reuse candidates; log line 'CBU:' every CbvStatsSec (10) seconds.
//   [Profiler] CbvRecord = N: records the inputs of the first N SynUploadCbvTable calls after CbvRecordDelaySec (60) s to
//       Nioh3EngineFix_cbvtrace.bin next to the ini (tests\run_cbvupload_harness.py replays it: stock vs CBU, byte-exact).
// CBN / CBT are refused when [GpuOpt] CbvMirror != 0 (the mirror's own lifetime rules; CM is the no-ReBAR path anyway).
// v2.02 only: the three call sites and the whole of 118DA8..118E27 / 118E28..1190BE are byte-checked (FNV-1a) first
// (patches.json group CBU, tests\test_patterns.py).
#pragma once
#include <cstddef>
#include <cstdint>

namespace cbvupload {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};
void Start(const StartInfo& info);
// LiveCtl 'Tuning.CBS / CBN / CBT / CBSTATS = 0/1': true = key handled (reply set)
bool SetTuning(const char* key, const char* val, char* reply, size_t n);
void StatusLine(char* buf, unsigned n);

#ifdef CBU_HARNESS  // tests\cbvupload_harness.cpp: the real wrapper / copy against relocated v2.02 code on a mock SynCtx
using UploadFn = void (*)(uint8_t* c, int* rootIdx, int stage, uint8_t* layout, uint64_t* fenceCache);
void TestSetup(UploadFn orig, int cbs, int cbn, int cbt, int stats);
void TestReset();
struct TestCounts {
    uint64_t calls, uploads, bytes, noopAny, noopSame, skipped, skippedBytes, tblCand, tblHit, copies, copyBytes;
};
TestCounts TestGet();
#endif

}  // namespace cbvupload

extern "C" {
void* CbuStreamCopy(void* dst, const void* src, size_t n);    // graphics site: CbuTableCall fences after the table
void* CbuStreamCopyCs(void* dst, const void* src, size_t n);  // compute site: sfence after the copy
void CbuTableCall(uint8_t* c, int* rootIdx, int stage, uint8_t* layout, uint64_t* fenceCache);
}
