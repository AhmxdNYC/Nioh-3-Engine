// zinflate.h - Inflate: libdeflate in place of the game's zlib 1.1.4 for its 16 KiB archive chunks ([Patches] Inflate = 0
// by default; #108787, notes\stream-workers-decision-108385.md step 3, notes\loading.md 3.1). Same bytes out.
//
// Proven wrapper contract (v2.02 disassembly, re\Nioh3.dump.exe; notes\loading.md 3.1):
//   bool W(void* dst, u32 cap, u64* outLen, const void* src, u32 csize)   (only al is meaningful)
//   ZlibInflateChunk16K 420654 (sites 258A5A 4201F5 420605):
//     z_stream{next_in = src, avail_in = csize, next_out = 16 KiB stack buffer, avail_out = cap}; inflateInit_ B34C60
//     ("1.1.4": zlib header, 15-bit window, adler32 checked) with a 46 KiB stack bump arena; inflate(Z_FINISH) B34670;
//     result != Z_STREAM_END (1) -> inflateEnd, return 0 (dst / *outLen untouched). Else *outLen = total_out (u32,
//     zero-extended), inflateEnd, memcpy(dst, stack buffer, total_out), return 1.
//   ZlibInflateChunk16K_C A09A40 (sites A099DD 324C372 324C4A1): src = {u64 sum; payload[csize - 8]}; return 0 unless
//     A347D8(payload, csize - 8) == sum, then exactly as 420654 on (payload, csize - 8).
//   NOT replaced: ZlibInflateChunk16K_B 324D064 (sites 324BD61 324C409). It NOTs the caller's source bytes in place and
//     gives zlib only a 16 KiB arena, which cannot hold zlib 1.1.4's 32 KiB window (the arena allocator 46F65C returns
//     NULL, then zalloc 46F4D8 memsets it): its live behaviour is unproven, so it stays the game's.
// Replacement: libdeflate_zlib_decompress into dst (via a 16 KiB scratch when dst and src overlap), output limit
// min(cap, 16 KiB) (the game's stack buffer). Success -> *outLen = size, return 1. Any libdeflate error -> the game's own
// function with the same arguments (its result is returned). One decompressor per thread, never freed.
// Same bytes: on every stream zlib 1.1.4 accepts, libdeflate gives the same output (both are exact RFC 1951/1950 decoders,
// both check the adler32; libdeflate refuses distances beyond the output start where 1.1.4 would read its zeroed window
// -> fallback). libdeflate is more lenient than 1.1.4 only on streams no encoder writes (an incomplete one-codeword code,
// litlen symbols 286/287 decoded as length 258): the game refuses those, libdeflate returns data when the adler32 still
// matches. Offline: 2 of 1985 corrupted chunks (tests/zinflate_test.cpp), both gave the original bytes.
// Inflate = 2 (verify): the first 2000 chunks run both (libdeflate into scratch, then the game's into dst = the result
// used) and compare result, length and bytes; one summary line, then libdeflate only.
// Call sites are patched by loadstats.cpp (one patch per site): with LoadStats on its timing hook calls Fn(w).
#pragma once
#include <cstdint>

namespace zinfl {

using InflFn = uint64_t (*)(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize);

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Patches] Inflate (0 off / 1 on / 2 verify first 2000 chunks, then on)
    uintptr_t base = 0;
    bool knownBuild = false;  // v2.02 only
};

// Reads the ini and checks the wrapper bytes. Installs nothing (loadstats::Start patches the call sites). Call before
// loadstats::Start.
bool Start(const StartInfo& info);
int Mode();
// Replacement for wrapper w (0 = 420654, 1 = 324D064, 2 = A09A40), nullptr when that wrapper stays the game's.
InflFn Replacement(int w);
// loadstats: one line after the sites are patched ("Inflate: ON (libdeflate) N sites").
void NoteSites(int patched, int wanted, const char* bad);

// ---- core, no game code (tests/zinflate_test.cpp) ----
// libdeflate zlib decode with the 420654 contract. 1 = ok (dst and *outLen written), 0 = libdeflate refused (the caller
// falls back; dst may be partly written unless it overlaps src, which goes through scratch).
int Decode(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize);

}  // namespace zinfl
