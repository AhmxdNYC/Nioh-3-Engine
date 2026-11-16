// padguard_cave.h - PadListGuard ([General] PadListGuard = 1; GitHub #79 / #55): the site table and the
// cave builder, shared by padguard.cpp and tests\padguard_harness.cpp (the test builds and runs these exact bytes).
//
// The pad manager's per-frame update (PadMgrUpdate 43671C -> 4385xx) walks intrusive circular lists of pad entries:
//   node = [head]; loop: cmp node, head ; je done ; ...[node+10h]... ; node = [node] ; jmp loop
// Another thread relinks those lists while the walk runs (#79: [r14] was non-null again in the dump taken a moment after the
// fault), so the walker can read a null `next` link and fault on [0+10h] (#79 at 42FB43, #55 at 430A3E). Each site below is
// such a loop head `cmp node, head ; je done`. Its cave does
//   test node,node ; jz null ; <the original cmp> ; je done ; jmp back (the instruction after the je)
//   null: lock inc [hits] ; cmp node,node ; jmp done
// so a null link ends that walk exactly like reaching the head (same exit, same flags: cmp x,x == the original equal compare);
// the next frame walks the list again. Non-null nodes take the original compare unchanged. Every exit was checked to reload
// or never read the node register before writing it (tests\test_padguard.py, capstone over the decrypted dump).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace padguard_cave {

constexpr uint8_t kRbx = 3, kRdi = 7;  // node register (ModRM number)

struct Site {
    uint32_t rva;         // v2.02 RVA of the loop-head `cmp node, head` (patches.json PGn, rvas.json)
    uint8_t len;          // cmp + je bytes replaced by `jmp cave` + nop pad
    uint8_t cmpLen;       // leading cmp bytes, copied verbatim into the cave (none is rip-relative)
    uint8_t node;         // kRbx / kRdi
    uint8_t orig[10];     // exact original bytes (cmp ; je rel8 / je rel32)
    uint8_t exitLen;      // first instruction at the je target, verified before patching
    uint8_t exitBytes[7];
};

// clang-format off
inline constexpr Site kWalks[] = {
    // 42FAC8 (PadMgr mode 3, tail-jumped from 438752): seven walks of the pad list [obj+18h]
    {0x42FB3E,  5, 3, kRbx, {0x49,0x3B,0xDE,0x74,0x73},                          4, {0x48,0x8B,0x46,0x18}},             // #79 fault 42FB43
    {0x42FBD6,  9, 3, kRbx, {0x48,0x3B,0xD8,0x0F,0x84,0x97,0x05,0x00,0x00},       4, {0x48,0x8B,0x5E,0x18}},
    {0x430189,  9, 3, kRbx, {0x48,0x3B,0xDF,0x0F,0x84,0x7D,0x01,0x00,0x00},       3, {0x48,0x8B,0x3F}},
    {0x430220, 10, 4, kRdi, {0x48,0x3B,0x7E,0x18,0x0F,0x84,0xDC,0x00,0x00,0x00},  4, {0x48,0x8B,0x5D,0xA7}},
    {0x430316, 10, 4, kRdi, {0x48,0x3B,0x7E,0x18,0x0F,0x84,0x50,0x02,0x00,0x00},  4, {0x48,0x8D,0x55,0xC7}},
    {0x430399, 10, 4, kRbx, {0x48,0x3B,0x5E,0x18,0x0F,0x84,0xC4,0x00,0x00,0x00},  4, {0x48,0x8B,0x7D,0x6F}},
    {0x430499, 10, 4, kRbx, {0x48,0x3B,0x5E,0x18,0x0F,0x84,0xC4,0x00,0x00,0x00},  4, {0x48,0x8B,0x7D,0x6F}},
    // 4309F8 (called from 4385F8): the walk of [obj+58h]
    {0x430A38,  6, 4, kRbx, {0x48,0x3B,0x5E,0x58,0x74,0x6F},                     7, {0x48,0x8D,0x86,0xB0,0x00,0x00,0x00}}, // #55 fault 430A3E
    // 438604 (called from 4385F0): the walk of [obj+18h]
    {0x438659,  5, 3, kRbx, {0x49,0x3B,0xDE,0x74,0xCC},                          5, {0x48,0x8B,0x5C,0x24,0x48}},
    // 430B58 (PadMgr mode 1, called from 43873A): four walks of [obj+18h]
    {0x430BC1,  9, 3, kRbx, {0x49,0x3B,0xDC,0x0F,0x84,0xDE,0x00,0x00,0x00},       4, {0x49,0x8B,0x46,0x18}},
    {0x430CB7,  9, 3, kRbx, {0x48,0x3B,0xD8,0x0F,0x84,0xB2,0x04,0x00,0x00},       3, {0x49,0x8B,0xCE}},
    {0x43118D,  9, 3, kRdi, {0x48,0x3B,0xFB,0x0F,0x84,0x5F,0x01,0x00,0x00},       4, {0x4C,0x8B,0x45,0xB8}},
    {0x43126F, 10, 4, kRbx, {0x49,0x3B,0x5E,0x18,0x0F,0x84,0x80,0x01,0x00,0x00},  4, {0x48,0x8B,0x7D,0xA8}},
};
// clang-format on
constexpr int kNumSites = (int)(sizeof(kWalks) / sizeof(kWalks[0]));

// page layout: one kBlock per site, then the per-site hit counters, then unwind data
constexpr size_t kBlock = 0x40, kSlotCont = 0x30, kSlotExit = 0x38;
constexpr size_t kCounters = 0x400;   // long[kNumSites]
constexpr size_t kUnwind = 0x500;     // chained UNWIND_INFO, 16 bytes per site
constexpr size_t kTable = 0x700;      // RUNTIME_FUNCTION[kNumSites]
constexpr size_t kPageUsed = kTable + 12 * 16;
static_assert(kCounters >= kBlock * 16 && kUnwind >= kCounters + 4 * 16 && kTable >= kUnwind + 16 * 16, "page layout");

// je target of the original bytes at `site`
inline uintptr_t ExitOf(const Site& s, uintptr_t site) {
    const uint8_t* j = s.orig + s.cmpLen;
    if (j[0] == 0x74) return site + s.len + (intptr_t)(int8_t)j[1];
    int32_t d;
    memcpy(&d, j + 2, 4);
    return site + s.len + (intptr_t)d;
}

// Shape of one table entry: cmp (3B /r with REX.W) then je rel8 / je rel32 ending exactly at len.
inline bool Shaped(const Site& s) {
    const uint8_t* j = s.orig + s.cmpLen;
    const bool rel8 = j[0] == 0x74 && s.len == s.cmpLen + 2, rel32 = j[0] == 0x0F && j[1] == 0x84 && s.len == s.cmpLen + 6;
    return (s.orig[0] == 0x48 || s.orig[0] == 0x49) && s.orig[1] == 0x3B && (s.cmpLen == 3 || s.cmpLen == 4) && (rel8 || rel32) &&
           (s.node == kRbx || s.node == kRdi) && s.len <= sizeof s.orig && s.exitLen <= sizeof s.exitBytes;
}

// Writes the cave for `s` at `out` (kBlock bytes). cont / exit are absolute addresses (back into the game: the instruction after
// the je / the je target). counter is the absolute address of this site's hit counter; it must be within rel32 of `out`.
// Returns the code size (the jump slots sit at kSlotCont / kSlotExit), 0 when the counter is out of reach.
inline size_t Build(uint8_t* out, const Site& s, uintptr_t cont, uintptr_t exit, uintptr_t counter) {
    const uint8_t r = s.node, modrm = (uint8_t)(0xC0 | (r << 3) | r);
    const size_t c = s.cmpLen;
    size_t n = 0;
    auto rel = [&](uintptr_t target, size_t insnEnd) -> bool {
        const intptr_t d = (intptr_t)target - (intptr_t)((uintptr_t)out + insnEnd);
        if (d < INT32_MIN || d > INT32_MAX) return false;
        const int32_t v = (int32_t)d;
        memcpy(out + n, &v, 4);
        n += 4;
        return true;
    };
    memset(out, 0xCC, kBlock);
    out[n++] = 0x48, out[n++] = 0x85, out[n++] = modrm;                 // 0     test node, node
    out[n++] = 0x74, out[n++] = (uint8_t)(16 + c);                      // 3     jz null (21+c)
    memcpy(out + n, s.orig, c), n += c;                                 // 5     cmp node, head (original)
    out[n++] = 0x74, out[n++] = 7;                                      // 5+c   je exitj
    out[n++] = 0x40, out[n++] = 0xFF, out[n++] = 0x25;                  // 7+c   jmp [slotCont] (REX 40: never read as an epilogue
    rel((uintptr_t)out + kSlotCont, n + 4);                             //       by the unwinder, see padguard.cpp)
    out[n++] = 0x40, out[n++] = 0xFF, out[n++] = 0x25;                  // 14+c  exitj: jmp [slotExit]
    rel((uintptr_t)out + kSlotExit, n + 4);
    out[n++] = 0xF0, out[n++] = 0xFF, out[n++] = 0x05;                  // 21+c  null: lock inc dword [counter]
    if (!rel(counter, n + 4)) return 0;
    out[n++] = 0x48, out[n++] = 0x3B, out[n++] = modrm;                 // 28+c  cmp node, node (flags = the original equal exit)
    out[n++] = 0x40, out[n++] = 0xFF, out[n++] = 0x25;                  // 31+c  jmp [slotExit]
    rel((uintptr_t)out + kSlotExit, n + 4);
    memcpy(out + kSlotCont, &cont, 8);
    memcpy(out + kSlotExit, &exit, 8);
    return n;                                                           // 38+c <= 42 < kSlotCont
}

// The site bytes: jmp rel32 to the cave, nop pad to len. false when the cave is out of rel32 reach.
inline bool SiteBytes(uint8_t* out, const Site& s, uintptr_t site, uintptr_t cave) {
    const intptr_t d = (intptr_t)cave - (intptr_t)(site + 5);
    if (d < INT32_MIN || d > INT32_MAX) return false;
    const int32_t v = (int32_t)d;
    out[0] = 0xE9;
    memcpy(out + 1, &v, 4);
    for (size_t i = 5; i < s.len; ++i) out[i] = 0x90;
    return true;
}

// Chained unwind info for a cave: no codes of its own, chained to the host function's RUNTIME_FUNCTION, so a stack walk that
// stops inside the cave unwinds exactly like one stopped in the host's loop (the cave never touches rsp). `hostBegin/End/Unwind`
// are absolute addresses; `tableBase` the BaseAddress given to RtlAddFunctionTable (<= every address, all within 4 GB of it).
// unwindRaw keeps the low bit of a RUNTIME_FUNCTION_INDIRECT UnwindData.
inline void WriteChained(uint8_t* at, uintptr_t tableBase, uintptr_t hostBegin, uintptr_t hostEnd, uintptr_t hostUnwind,
                         uint32_t indirectBit) {
    at[0] = 1 | (0x4 << 3);  // version 1, UNW_FLAG_CHAININFO
    at[1] = 0;               // SizeOfProlog
    at[2] = 0;               // CountOfCodes
    at[3] = 0;               // FrameRegister / offset
    const uint32_t rf[3] = {(uint32_t)(hostBegin - tableBase), (uint32_t)(hostEnd - tableBase),
                            (uint32_t)(hostUnwind - tableBase) | indirectBit};
    memcpy(at + 4, rf, 12);
}

}  // namespace padguard_cave
