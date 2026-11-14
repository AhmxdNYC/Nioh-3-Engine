/* known_build.h - one shared "is this the v2.02 game exe?" check for EngineFix, BossRematch, Training and CamKiProbe
 * (issue #70/#77, lead #98993). Header-only C (Training is C), no imports beyond kernel32 types.
 *
 * Two exes run the v2.02 code at the same RVAs:
 *   N3KB_STEAM     the Steam exe: stamp 6AA7C106, SizeOfImage 0x52FB000 (DRM wrapper in a last '.bind' section).
 *   N3KB_STRIPPED  the same exe with the Steam wrapper removed (Steamless / another store): stamp 6AA7C106,
 *                  SizeOfImage 0x52C1000 (= the VA of .bind), no .bind section, the 7 other sections at the Steam RVAs,
 *                  and the IN-MEMORY first 64 KB of .text hashing to the v2.02 code. The on-disk .text of the Steam exe
 *                  is encrypted, so only the mapped bytes count. A stripped exe has no decrypting stub, so its .text is
 *                  plain from DllMain on; that window carries no base relocations (checked offline), so the hash does
 *                  not depend on the load address.
 * Everything else is N3KB_UNKNOWN. Callers keep every per-patch byte check; a mismatch still skips that patch.
 */
#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>

enum { N3KB_UNKNOWN = 0, N3KB_STEAM = 1, N3KB_STRIPPED = 2 };

#define N3KB_STAMP 0x6AA7C106u
#define N3KB_SIZE_STEAM 0x52FB000u
#define N3KB_SIZE_STRIPPED 0x52C1000u
#define N3KB_HASH_RVA 0x1000u
#define N3KB_HASH_LEN 0x10000u

/* sha256 of v2.02 .text [0x1000, 0x11000) as mapped (decrypted), from the runtime dump re/Nioh3.dump.exe */
static const uint8_t n3kb_text_sha[32] = {
    0xc4, 0x13, 0x58, 0x88, 0xcd, 0x0c, 0x3c, 0x0d, 0x24, 0x88, 0xb2, 0x43, 0xca, 0xe9, 0x4c, 0x15,
    0xb0, 0xd4, 0x08, 0xfe, 0x68, 0x07, 0x7a, 0x58, 0x92, 0xbd, 0x57, 0x03, 0x59, 0x85, 0xab, 0x14};

static const struct { char name[8]; uint32_t va; } n3kb_secs[7] = {
    {".text", 0x1000},     {".rdata", 0x38E2000}, {".data", 0x44B4000}, {".pdata", 0x4E28000},
    {"_RDATA", 0x50DB000}, {".rsrc", 0x5100000},  {".reloc", 0x523A000}};

/* ---- minimal SHA-256 ---- */
static const uint32_t n3kb_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define N3KB_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static inline void n3kb_block(uint32_t h[8], const uint8_t* p) {
    uint32_t w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;
    for (i = 0; i < 16; ++i) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = N3KB_ROR(w[i - 15], 7) ^ N3KB_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = N3KB_ROR(w[i - 2], 17) ^ N3KB_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; ++i) {
        t1 = hh + (N3KB_ROR(e, 6) ^ N3KB_ROR(e, 11) ^ N3KB_ROR(e, 25)) + ((e & f) ^ (~e & g)) + n3kb_k[i] + w[i];
        t2 = (N3KB_ROR(a, 2) ^ N3KB_ROR(a, 13) ^ N3KB_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
/* sha256 of n bytes; n must be a multiple of 64 (the 64 KB window is) */
static inline void n3kb_sha256(const uint8_t* p, uint32_t n, uint8_t out[32]) {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t pad[64];
    uint64_t bits = (uint64_t)n * 8;
    uint32_t i;
    int k;
    for (i = 0; i + 64 <= n; i += 64) n3kb_block(h, p + i);
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (k = 0; k < 8; ++k) pad[63 - k] = (uint8_t)(bits >> (8 * k));
    n3kb_block(h, pad);
    for (k = 0; k < 32; ++k) out[k] = (uint8_t)(h[k / 4] >> (24 - 8 * (k % 4)));
}

/* Classifies a mapped image (base = Nioh3.exe module). Safe from DllMain: reads the mapped headers and 64 KB of .text. */
static inline int n3kb_classify(uintptr_t base) {
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    const IMAGE_NT_HEADERS64* nt;
    const IMAGE_SECTION_HEADER* sh;
    uint8_t got[32];
    unsigned i;
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return N3KB_UNKNOWN;
    nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp != N3KB_STAMP) return N3KB_UNKNOWN;
    if (nt->OptionalHeader.SizeOfImage == N3KB_SIZE_STEAM) return N3KB_STEAM;
    if (nt->OptionalHeader.SizeOfImage != N3KB_SIZE_STRIPPED || nt->FileHeader.NumberOfSections != 7) return N3KB_UNKNOWN;
    sh = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < 7; ++i) {
        if (memcmp(sh[i].Name, n3kb_secs[i].name, 8) != 0 || sh[i].VirtualAddress != n3kb_secs[i].va) return N3KB_UNKNOWN;
    }
    if (sh[0].Misc.VirtualSize < N3KB_HASH_RVA - 0x1000 + N3KB_HASH_LEN) return N3KB_UNKNOWN;
    n3kb_sha256((const uint8_t*)(base + N3KB_HASH_RVA), N3KB_HASH_LEN, got);
    return memcmp(got, n3kb_text_sha, 32) == 0 ? N3KB_STRIPPED : N3KB_UNKNOWN;
}

static inline const char* n3kb_name(int k) {
    return k == N3KB_STEAM ? "known build v2.02 (Steam exe)" : k == N3KB_STRIPPED ? "known-stripped: v2.02 without the Steam DRM wrapper"
                                                                                  : "unknown build";
}
