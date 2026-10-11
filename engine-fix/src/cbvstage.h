// cbvstage.h - CBF (F8): per-draw constants written to a SYSTEM-MEMORY staging ring and copied in one batch per submission
// into a VRAM ring that the CBVs point at (notes\renderer\cbv-upload.md section 7). Part of the CBU package (cbvupload.cpp).
//
// Why: pc2 RPF2 (ReBAR ON, CPU-bound): with the CB ring in BAR VRAM the replay thread pays 4.1-4.6 us of CPU per draw for
// the CPU stores across PCIe; with the ring in system memory (VSForceSysUpload) 1.07 us/draw and CPUBusy 13 ms, but the
// GPU then reads every constant over PCIe (GPUTime 18.8 -> 26.8 ms). CBF keeps both halves: cheap CPU writes, VRAM reads.
// How: the graphics SynCtx's CB ring (SynCtx+B28h: [0] resource, [1] size, [2] CPU base, [3] GPU base; only
// UploadRingAllocate 1190C0 reads [2]/[3], callers 118E28 + 4959EC only) gets [2] = a CUSTOM WRITE_COMBINE / L0 buffer
// (system memory) and [3] = a DEFAULT buffer (VRAM) of the same size: every allocation keeps its ring offset, CPU writes land
// in system memory, CBVs are created on the VRAM address. The two memcpy relays (118F4D / 495AF2) note each written range;
// at the next ExecuteCommandLists of the game queue that submits this SynCtx's lists, all noted ranges (coalesced, a few
// runs per submission because the ring is a bump allocator) are copied system -> VRAM:
//   [Tuning] CBF_Queue = 0: a prologue command list in front of the game's lists in the same ExecuteCommandLists (copy +
//       COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER barrier; the buffer decays to COMMON at the end of the submission);
//   [Tuning] CBF_Queue = 1: our own COPY queue (DMA engine): copy, Signal, and the game queue Waits on that value.
// Lifetimes: a VRAM range is overwritten only when the ring hands its offset out again, i.e. after the list that used it
// completed (the engine's fence-tag rule), and every copy runs on (or is waited for by) the queue that reads it, before the
// lists of that submission. Gaps between coalesced runs are re-copied with the bytes the staging ring already holds there
// (same bytes for live blocks, dead bytes for free ones): harmless.
// Live: switching CBF off restores the ring's original bases (new allocations go to the UPLOAD ring again); ranges already
// noted are still copied. Refused with [GpuOpt] CbvMirror != 0 or [Profiler] MemStats >= 2 (VSForceSysUpload).
#pragma once
#include <cstddef>
#include <cstdint>

struct ID3D12CommandQueue;
struct ID3D12CommandList;

namespace cbvstage {

using EclFn = void(__stdcall*)(ID3D12CommandQueue*, unsigned, ID3D12CommandList* const*);

void Configure(void (*log)(const char*, ...), int mode, int copyQueue);   // mode: [Patches] CBF (0 / 1 / 2)
bool Installed();
bool WantsEclHook();                 // osd installs its ExecuteCommandLists hook for us
volatile uint32_t* Switch();         // live on / off
volatile uint32_t* QueueSwitch();    // live 0 = DIRECT prologue, 1 = COPY queue
void NoteWrite(const void* dst, size_t n);  // relays, after each ring write (any SynCtx; ignored outside the staging ring)
void OnTable(uint8_t* c);            // CbuTableCall entry (replay thread): learns the list, creates, swaps / restores the ring bases
// osd HkExecuteCommandLists (game callers): true = it submitted the game's lists itself (prologue mode)
bool OnEcl(ID3D12CommandQueue* q, unsigned n, ID3D12CommandList* const* lists, EclFn orig);
void StatsLine(char* buf, size_t n, double sec);

#ifdef CBU_HARNESS
void TestSetup(uint8_t* sys, uint8_t* vram, uint64_t size);   // buffers instead of D3D12 resources; the queue counts as known
void TestFlush(void (*copy)(uint64_t off, uint64_t len));     // what the next game submission would copy
void TestReset();
#endif

}  // namespace cbvstage
