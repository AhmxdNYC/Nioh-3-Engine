// fnprobe_core.h - game-independent parts of FnProbe (fnprobe.cpp), header-only so tests\fnprobe_harness.cpp can run them:
//   PrologueLen  - length of the whole-instruction prologue run (>= 5 bytes) that the entry thunk relocates, or -1 if an
//                  instruction in it is not one of the plain forms below (rip-relative, branches, calls, anything else).
//   BuildThunk   - writes the entry thunk (fast path: one locked add; slow path: C++ hook with every volatile saved).
//   IsHashish    - "looks like a 32-bit name hash" hint for the arg dump.
#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace fnprobe {

// Decodes ONE instruction of the restricted prologue set. Returns its length, or -1.
// Accepted: [66|F2|F3] [REX] then
//   50-57 push r | 88 89 8A 8B 8D (mov/lea r,m) 31 33 29 2B 85 39 3B (xor/sub/test/cmp) with ModRM
//   83 /x ib | 81 /x id | C7 /0 id | B8-BF mov r,imm (imm64 with REX.W) | 0F 10/11/28/29/6F/7F (movups/movaps/movdqu/movdqa)
// ModRM memory forms must not be rip-relative (mod 00, rm 101) and must have a base register (SIB base 101 with mod 00 is refused).
inline int InsnLen(const uint8_t* p) {
    int i = 0;
    const bool operand16 = p[i] == 0x66;
    if (operand16 || p[i] == 0xF2 || p[i] == 0xF3) ++i;
    bool rexW = false;
    if ((p[i] & 0xF0) == 0x40) {
        rexW = (p[i] & 8) != 0;
        ++i;
    }
    const uint8_t op = p[i++];
    int imm = 0;
    bool modrm = false;
    bool twoByte = false;
    if (op >= 0x50 && op <= 0x57) return i;
    if (op >= 0xB8 && op <= 0xBF) return i + (rexW ? 8 : operand16 ? 2 : 4);
    switch (op) {
        case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8D:
        case 0x31: case 0x33: case 0x29: case 0x2B: case 0x85: case 0x39: case 0x3B:
            modrm = true;
            break;
        case 0x83: modrm = true; imm = 1; break;
        case 0x81: modrm = true; imm = operand16 && !rexW ? 2 : 4; break;
        case 0xC7: modrm = true; imm = operand16 && !rexW ? 2 : 4; break;
        case 0x0F: {
            const uint8_t o2 = p[i++];
            if (o2 != 0x10 && o2 != 0x11 && o2 != 0x28 && o2 != 0x29 && o2 != 0x6F && o2 != 0x7F) return -1;
            modrm = true;
            twoByte = true;
            break;
        }
        default:
            return -1;
    }
    (void)twoByte;
    if (!modrm) return -1;
    const uint8_t m = p[i++];
    const int mod = m >> 6, rm = m & 7;
    if (op == 0xC7 && ((m >> 3) & 7) != 0) return -1;
    if (op == 0x8D && mod == 3) return -1;
    if (mod != 3) {
        if (rm == 4) {
            const uint8_t sib = p[i++];
            if (mod == 0 && (sib & 7) == 5) return -1;  // no base (disp32 absolute)
        } else if (mod == 0 && rm == 5) {
            return -1;  // rip-relative
        }
        if (mod == 1) i += 1;
        if (mod == 2) i += 4;
    }
    return i + imm;
}

// Whole instructions covering >= 5 bytes (the E9 rel32), at most 16. -1 if not relocatable.
inline int PrologueLen(const uint8_t* p) {
    int n = 0;
    while (n < 5) {
        const int l = InsnLen(p + n);
        if (l <= 0) return -1;
        n += l;
        if (n > 16) return -1;
    }
    return n;
}

inline bool IsHashish(uint32_t v) {
    if (v < 0x01000000u) return false;
    int pc = 0;
    for (uint32_t x = v; x; x &= x - 1) ++pc;
    if (pc < 10 || pc > 22) return false;
    const uint32_t e = (v >> 23) & 0xFF;
    if (e >= 117 && e <= 141 && (v & 0xFFF) == 0) return false;  // round float in 2^-10..2^14
    return true;
}

constexpr size_t kThunkMax = 256;

// Entry thunk. `entry` = function entry (its first n bytes are copied; the caller patches entry with E9 -> thunk afterwards).
// Fast path (most calls): pushfq; push rax; push rcx; lock xadd [counter],1; if old >= firstN and (old & mask) != 0:
// restore and run the original prologue. Slow path (the first firstN hits, then 1 in mask+1): also saves rdx, r8-r11,
// xmm0-xmm5 and calls hook(rcx, rdx, r8, r9, entry_rsp, old) with the ORIGINAL argument registers, then restores all.
// Every register and the flags are as at entry when the relocated prologue runs (notes\hook-register-audit.md).
// Returns the thunk size (<= kThunkMax).
inline size_t BuildThunk(uint8_t* t, const uint8_t* entry, int n, volatile uint64_t* counter, uint32_t firstN, uint32_t mask,
                         const void* hook) {
    uint8_t* w = t;
    auto put = [&](std::initializer_list<uint8_t> b) {
        for (uint8_t x : b) *w++ = x;
    };
    auto put64 = [&](uint64_t v) {
        memcpy(w, &v, 8);
        w += 8;
    };
    auto put32 = [&](uint32_t v) {
        memcpy(w, &v, 4);
        w += 4;
    };
    put({0x9C, 0x50, 0x51});  // pushfq; push rax; push rcx
    put({0x48, 0xB9});        // mov rcx, imm64 (&counter)
    put64((uint64_t)(uintptr_t)counter);
    put({0xB8, 0x01, 0x00, 0x00, 0x00});        // mov eax, 1
    put({0xF0, 0x48, 0x0F, 0xC1, 0x01});        // lock xadd [rcx], rax
    put({0x48, 0x3D});                          // cmp rax, imm32
    put32(firstN);
    put({0x0F, 0x82});                          // jb SLOW
    uint8_t* jb = w;
    put32(0);
    put({0xA9});                                // test eax, imm32
    put32(mask);
    put({0x0F, 0x84});                          // jz SLOW
    uint8_t* jz = w;
    put32(0);
    uint8_t* fast = w;
    put({0x59, 0x58, 0x9D});                    // pop rcx; pop rax; popfq
    memcpy(w, entry, (size_t)n);                // relocated prologue (rsp at its entry value)
    w += n;
    put({0xFF, 0x25, 0, 0, 0, 0});              // jmp [rip+0]
    put64((uint64_t)(uintptr_t)(entry + n));
    uint8_t* slow = w;
    int32_t r = (int32_t)(slow - (jb + 4));
    memcpy(jb, &r, 4);
    r = (int32_t)(slow - (jz + 4));
    memcpy(jz, &r, 4);
    put({0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});  // push rdx, r8, r9, r10, r11 (entry rsp - 64)
    put({0x48, 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00});              // sub rsp, 98h -> 16-aligned at the call
    for (uint8_t i = 0; i < 6; ++i) {                             // movdqu [rsp+30h+10h*i], xmm_i
        put({0xF3, 0x0F, 0x7F, (uint8_t)(0x84 | (i << 3)), 0x24});
        put32(0x30u + 0x10u * i);
    }
    put({0x48, 0x89, 0x44, 0x24, 0x28});                          // mov [rsp+28h], rax   (6th arg: old count)
    put({0x48, 0x8D, 0x84, 0x24, 0xD8, 0x00, 0x00, 0x00});        // lea rax, [rsp+0D8h]   (entry rsp)
    put({0x48, 0x89, 0x44, 0x24, 0x20});                          // mov [rsp+20h], rax   (5th arg)
    put({0x48, 0x8B, 0x8C, 0x24, 0xC0, 0x00, 0x00, 0x00});        // mov rcx, [rsp+0C0h]  (original rcx)
    put({0x48, 0xB8});                                            // mov rax, imm64 (hook)
    put64((uint64_t)(uintptr_t)hook);
    put({0xFF, 0xD0});                                            // call rax
    for (uint8_t i = 0; i < 6; ++i) {                             // movdqu xmm_i, [rsp+30h+10h*i]
        put({0xF3, 0x0F, 0x6F, (uint8_t)(0x84 | (i << 3)), 0x24});
        put32(0x30u + 0x10u * i);
    }
    put({0x48, 0x81, 0xC4, 0x98, 0x00, 0x00, 0x00});              // add rsp, 98h
    put({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A});  // pop r11, r10, r9, r8, rdx
    put({0xE9});                                                  // jmp FAST (pop rcx, rax, flags; prologue)
    r = (int32_t)(fast - (w + 4));
    put32((uint32_t)r);
    return (size_t)(w - t);
}

}  // namespace fnprobe
