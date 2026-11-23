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
    // 433A4C (pad state update, called from 435546; GitHub #123 fault 433ADF): three walks of [rsi+10h]
    {0x433AD6,  9, 3, kRbx, {0x48,0x3B,0xD8,0x0F,0x84,0x71,0x03,0x00,0x00},       4, {0x4C,0x8B,0x66,0x10}},             // #123 fault 433ADF
    {0x433E58,  9, 3, kRbx, {0x49,0x3B,0xDC,0x0F,0x84,0xB0,0x00,0x00,0x00},       5, {0x0F,0x28,0x74,0x24,0x60}},
    {0x433EDE,  5, 3, kRdi, {0x49,0x3B,0xFF,0x74,0x26},                          3, {0x48,0x8B,0x1B}},
};
// clang-format on
constexpr int kNumSites = (int)(sizeof(kWalks) / sizeof(kWalks[0]));
static_assert(kNumSites <= 16, "the page holds 16 site blocks, counters and unwind entries");

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

// ---- map miss (GitHub #96) ----
// The same pad walks look each listed pad up in a per-mode unordered_map with at() (431690). When the other thread has just
// added / removed that pad, the key is missing and at() throws std::out_of_range ("invalid unordered_map<K, T> key") from its
// miss path at 4316F9 (`lea rcx, msg ; call 0xB5FB84`); #96 died there 12 s after start. The miss path is replaced by a jump
// to a cave that, ONLY when at() was called from one of the pad walks below (return address check), returns a zeroed scratch
// value instead: every one of those callers reads flag bytes / dwords at +0..+0xD of it, and all-zero is "no input, nothing
// to do" for that pad this frame (most of them branch straight back to the walk on [v+2] == [v+3] == 0). Any other caller of
// at() gets the original throw, unchanged. The cave never moves rsp before its own `add rsp, 28h ; ret` (at()'s epilogue).
constexpr uint32_t kAtFn = 0x431690;      // unordered_map<uint32, T>::at
constexpr uint32_t kAtMiss = 0x4316F9;    // its miss path: lea rcx, [rip+msg] ; call 0xB5FB84
constexpr uint8_t kAtMissLen = 12;
inline constexpr uint8_t kAtMissOrig[kAtMissLen] = {0x48, 0x8D, 0x0D, 0x70, 0xAD, 0x65, 0x03, 0xE8, 0x7F, 0xE4, 0x72, 0x00};
// the pad-walk calls of at() (E8 rel32 to kAtFn); the return address is the call + 5
inline constexpr uint32_t kAtCalls[] = {0x42FC11, 0x42FD92, 0x4301A2, 0x43028F, 0x430330, 0x4303C2, 0x4304C2, 0x43138F, 0x43866E};
constexpr int kNumAtCalls = (int)(sizeof(kAtCalls) / sizeof(kAtCalls[0]));
constexpr size_t kMapCave = 0x800, kMapRets = 0x880, kMapScratch = 0x900, kMapCounter = 0x940, kMapUnwind = 0x960;
constexpr size_t kMapScratchLen = 0x40;
static_assert(kTable + 12 * (kNumSites + 2) <= kMapCave && kTable + 16 * 12 <= kMapCave && kMapRets + 8 * 16 <= kMapScratch && kMapUnwind + 16 <= 0x1000, "map page layout");

// ---- second map (GitHub #115 / #118 / #63 / #51) ----
// The pad manager keeps a second unordered_map<uint32, 8-byte pad state> with its own at() copy (431E6C, same FNV-1a lookup,
// same `sub rsp,28h` frame, value = node+14h). Its miss path 431ED5 (also reached from its cold chunk 19A9265) throws the same
// out_of_range; #115/#118/#63 died there at boot (return 431EE1, callers 433E7E / 433B1B). Its seven callers all treat the value
// as one pad's 8-byte state: they read [v+2] (zero = skip that pad), clear it, or write this frame's bits into it. A zeroed
// scratch value therefore means "no input for that pad this frame"; writes land in the scratch and are zeroed on the next hit.
// It gets its own block (cave, return table, scratch, counter, unwind) so the two maps never share a scratch value.
constexpr uint32_t kAt2Fn = 0x431E6C;     // second unordered_map<uint32, T>::at
constexpr uint32_t kAt2Miss = 0x431ED5;   // its miss path: lea rcx, [rip+msg] ; call 0xB5FB84
inline constexpr uint8_t kAt2MissOrig[kAtMissLen] = {0x48, 0x8D, 0x0D, 0x94, 0xA5, 0x65, 0x03, 0xE8, 0xA3, 0xDC, 0x72, 0x00};
inline constexpr uint32_t kAt2Calls[] = {0x433B16, 0x433E79, 0x433EFA, 0x19A9246, 0x2978718, 0x2978891, 0x2978924};
constexpr int kNumAt2Calls = (int)(sizeof(kAt2Calls) / sizeof(kAt2Calls[0]));

// where one map-miss block lives in the guard page
struct MapLayout {
    size_t cave, rets, scratch, counter, unwind;
};
inline constexpr MapLayout kMap1 = {kMapCave, kMapRets, kMapScratch, kMapCounter, kMapUnwind};
inline constexpr MapLayout kMap2 = {0xA00, 0xA80, 0xB00, 0xB40, 0xB60};
static_assert(kMapUnwind + 16 <= kMap2.cave && kMap2.rets + 8 * 16 <= kMap2.scratch && kMap2.scratch + kMapScratchLen <= kMap2.counter &&
                  kMap2.counter + 4 <= kMap2.unwind && kMap2.unwind + 16 <= 0x1000,
              "second map page layout");

// rel32 target of the E8 call at `at`
inline uintptr_t CallTarget(uintptr_t at) {
    int32_t d;
    memcpy(&d, (const void*)(at + 1), 4);
    return at + 5 + (intptr_t)d;
}

// Writes the map-miss cave at page + L.cave and its return-address table (L = kMap1 for #96, kMap2 for #115). msg = absolute address of the original lea's
// string, thrower = the original call target. Returns the code size, 0 when a rel32 does not reach.
inline size_t BuildMapMiss(uint8_t* page, const uintptr_t* rets, int nRets, uintptr_t msg, uintptr_t thrower,
                           const MapLayout& L = kMap1) {
    if (nRets < 1 || nRets > 16) return 0;
    uint8_t* out = page + L.cave;
    size_t n = 0;
    auto rel = [&](uintptr_t target, size_t insnEnd) -> bool {
        const intptr_t d = (intptr_t)target - (intptr_t)((uintptr_t)out + insnEnd);
        if (d < INT32_MIN || d > INT32_MAX) return false;
        const int32_t v = (int32_t)d;
        memcpy(out + n, &v, 4);
        n += 4;
        return true;
    };
    memset(out, 0xCC, L.rets - L.cave);
    memcpy(page + L.rets, rets, sizeof(uintptr_t) * (size_t)nRets);
    out[n++] = 0x48, out[n++] = 0x8B, out[n++] = 0x44, out[n++] = 0x24, out[n++] = 0x28;  // 0   mov rax, [rsp+28h] (return)
    out[n++] = 0x48, out[n++] = 0x8D, out[n++] = 0x0D;                                    // 5   lea rcx, [rets]
    if (!rel((uintptr_t)page + L.rets, n + 4)) return 0;
    out[n++] = 0xBA, out[n++] = (uint8_t)nRets, out[n++] = 0, out[n++] = 0, out[n++] = 0;  // 12  mov edx, nRets
    const size_t loop = n;                                                                // 17  loop:
    out[n++] = 0x48, out[n++] = 0x3B, out[n++] = 0x01;                                    //     cmp rax, [rcx]
    out[n++] = 0x74, out[n++] = 0;                                                        // 20  je hit (patched below)
    const size_t jeHit = n - 1;
    out[n++] = 0x48, out[n++] = 0x83, out[n++] = 0xC1, out[n++] = 0x08;                   // 22  add rcx, 8
    out[n++] = 0xFF, out[n++] = 0xCA;                                                     // 26  dec edx
    out[n++] = 0x75, out[n++] = (uint8_t)(int8_t)((intptr_t)loop - (intptr_t)(n + 1));   // 28  jnz loop
    out[n++] = 0x48, out[n++] = 0xB9;                                                     // 30  mov rcx, msg (the original throw)
    memcpy(out + n, &msg, 8), n += 8;
    out[n++] = 0x48, out[n++] = 0xB8;                                                     // 40  mov rax, thrower
    memcpy(out + n, &thrower, 8), n += 8;
    out[n++] = 0xFF, out[n++] = 0xD0;                                                     // 50  call rax (never returns)
    out[n++] = 0xCC;                                                                      // 52  int3
    out[jeHit] = (uint8_t)(n - (jeHit + 1));                                              //     hit:
    out[n++] = 0xF0, out[n++] = 0xFF, out[n++] = 0x05;                                    // 53  lock inc dword [counter]
    if (!rel((uintptr_t)page + L.counter, n + 4)) return 0;
    out[n++] = 0x48, out[n++] = 0x8D, out[n++] = 0x05;                                    // 60  lea rax, [scratch]
    if (!rel((uintptr_t)page + L.scratch, n + 4)) return 0;
    out[n++] = 0x31, out[n++] = 0xC9;                                                     // 67  xor ecx, ecx
    for (uint8_t o = 0; o < kMapScratchLen; o += 8) {                                     //     zero the scratch value
        out[n++] = 0x48, out[n++] = 0x89, out[n++] = o ? 0x48 : 0x08;
        if (o) out[n++] = o;
    }
    out[n++] = 0x48, out[n++] = 0x83, out[n++] = 0xC4, out[n++] = 0x28;                   //     add rsp, 28h (at()'s epilogue)
    out[n++] = 0xC3;                                                                      //     ret
    return n;
}

}  // namespace padguard_cave
