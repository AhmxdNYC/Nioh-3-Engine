// mlprobe.h - MLP ([Profiler] MeshletProbe = 0 by default): the CPU half of the meshlet front-end probe (notes\meshlet-frontend.md).
// MEASUREMENT ONLY. Entry thunks (fnprobe_core.h) on the three meshlet recorders of v2.02:
//   531948 MeshletResolveLOD (LOD), 5301FC MeshletPreculling (PRE), 53FD3C MeshletInstanceCulling (MIC)
// Per call: caller RVA, thread, the MeshletSys pointer (param_1), and the recorder inputs the GPU cost depends on:
//   LOD: level count (param_4+24h), root count (+2Ch), cull-code count (+30h), the 32-byte camera block (param_5), param_11;
//   PRE: cnt (= MPC chain draw count; param_5+20h, +1Ch in mode 4), mode (param_1+169D48h), the #23 chain gate (param_1+14h and
//        +169D2Dh: MPC #23/#28-#31/#26 are recorded only when both are set), param_6..9, the commit function (param_3 vtbl+C8h);
//   both: the ID3D12Resource of the first UAV barrier the recorder emits (*(*(param_1+16A170h)+18h): tok 0x8A carries the
//   ID3D12Resource pointer itself, SynTokUavBarrier 113A0C), so the GPU half (gpuprof.cpp) can tell which recorder call a
//   GPU region belongs to.
// Per 30 s window ("MLPC:" lines): calls/frame per (recorder, caller), duplicate-resolve test (two LOD calls in one frame on the
// same MeshletSys with the same camera block and counts = a skippable second resolve candidate), cnt histogram of the MPC chains
// (0 / 1..1024 / > 1024 = how often the MPCS tail skip applies), the most frequent per-frame call sequences.
#pragma once
#include <cstddef>
#include <cstdint>

namespace mlprobe {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only (the RVAs and offsets above)
};

enum Fn : uint8_t { kLod = 0, kPre = 1, kMic = 2, kNumFn = 3 };

// Reads [Profiler] MeshletProbe. When >= 1 and the build is known: installs the three entry thunks. Returns true when installed.
bool Start(const StartInfo& info);
bool Active();

// GPU half: label of the k-th (0-based, GPU order in the frame) region of recorder fn whose first UAV barrier names `res`.
// Returns the caller RVA of the matching CPU call of the last complete CPU frame (k-th call of fn), 0 = unknown. *exact = the CPU
// call's first-barrier resource equals res.
uint32_t CallerOf(Fn fn, int k, const void* res, bool* exact);

// Present thread, once per frame (gp::OnFrame): frame boundary for the per-frame call sequences.
void OnFrame();
// stats thread, every 30 s
void LogLine();

// MPCS guard: true when <exe dir>\shader_dx12\0x29ee0902.file is the proven `mpc` build (sha256 3947f3d7...). `why` gets the reason.
bool MpcFileOk(char* why, size_t n);
// SHA-256 of a file (lower-case hex into out[65]); false if unreadable. Exposed for the harness test.
bool Sha256File(const wchar_t* path, char out[65]);
void Sha256(const void* data, size_t n, uint8_t out[32]);

}  // namespace mlprobe
