; drawprep.asm - caves for drawprep.cpp (1.7.32; notes\remaining-work.md 2.5, notes\mainthread-drawprep.md D,
; notes\meshlet-mainthread.md P1E7). Reached from a near trampoline (jmp [rip]) that drawprep.cpp writes; each cave
; leaves through an absolute jmp to a resume address set by drawprep.cpp. Live switches: one dword each (0 = stock).
;
;   caveDq    site 1A74C45 `comiss xmm6,xmm0; ja 1A74BD0` (5 B -> E9 rel32): the [App]Game main loop's blind retry spin
;             `while ((QPC - start) / freq < xmm6) ;` with xmm6 = 0.001f (loaded once at 1A744E4, before the loop, so a
;             live patch of that load would never take effect). Switch on: the exit test uses dp_dq_quantum instead of
;             xmm6 (default 0.0002f). Same ordering semantics as the original (`ja`: loop only if quantum > elapsed and
;             both ordered). Writes the flags only (the original comiss writes them too); every register is preserved.
;   caveP1e7  site 1E72C6 `mov r14,[r12]; mov r8,[r11+1C8h]` (11 B -> E9 rel32 + nop6) in the texture loop of 1E7050:
;             prefetch the streaming object of the next texture (5 lines 128EF0 / 1290B8 stall on) and the object of the
;             texture after that. r12 = &tex[k], r15d = k, rdx = group (count at +18h), r11 = this. Scratch rcx, r10 and
;             the flags (dead at 1E72D1: patcher\meshlet_caves.fsx proveDead, re-checked with hook_liveness.py). Only
;             loads + prefetches; the one real load ([tex[k+1]+68h]) is what 1E72D6 loads one iteration later under the
;             same bound and null check. Port of re\scripts_meshlet\meshlet_caves.fsx cP1E7, byte for byte in behaviour.
;   caveW1b   site 61BAC (22 B -> E9 rel32 + int3 x17; resume 61BC2) in the glyph padding clear 61B1C (notes\w1b-rpa.md):
;             switch on = the game's original loop with its write-combined read-modify-write `and dword [rbx+rax*4],0`
;             turned into the plain store `mov dword [rbx+rax*4],0`, i.e. no WC read (W1's aim) and no `rep stosd`
;             start-up per row (W1's cost: count = pad = 1). Same stores in the same order, same final registers and
;             flags as the original. Switch off = whatever the site held at install: W1's bytes (dp_w1b_w1 = 1) or the
;             original loop.
;   caveRpa   site 247950 RenderParamsAssign entry `mov [rsp+8],rbx` (5 B -> E9 rel32; resume 247955). Switch on: copies
;             exactly the byte ranges the original copies (derived offline from the original code, tools\gen_rpa_copy.py;
;             padding between fields untouched) with 16-byte moves instead of ~2.4k dword moves from 29.6 KB of code.
;             rcx = dst, rdx = src; returns rax = dst; uses only rax, r8, xmm0, xmm1 (volatile). Off: the displaced
;             instruction, then the original.
OPTION CASEMAP:NONE

PUBLIC caveDq, caveP1e7, caveW1b, caveRpa
PUBLIC dp_dq_enable, dp_p1e7_enable, dp_dq_quantum, dp_dq_loop, dp_dq_exit, dp_p1e7_resume, dp_dq_early
PUBLIC dp_w1b_enable, dp_w1b_w1, dp_rpa_enable, dp_w1b_resume, dp_rpa_resume

dpdata SEGMENT ALIGN(64) READ WRITE 'DATA'
dp_dq_enable    DD 0
dp_p1e7_enable  DD 0
dp_dq_quantum   REAL4 0.0002
dp_dq_early     DD 0            ; retries that came earlier than the stock 1 ms quantum (enabled path only)
dp_dq_loop      DQ 0            ; Nioh3+1A74BD0 (spin loop head)
dp_dq_exit      DQ 0            ; Nioh3+1A74C4A (`jmp 1A744F1`: retry the frame function)
dp_p1e7_resume  DQ 0            ; Nioh3+1E72D1 (test r14,r14)
dp_w1b_resume   DQ 0            ; Nioh3+61BC2 (movsxd r8,[rsi+80h])
dp_rpa_resume   DQ 0            ; Nioh3+247955 (push rdi)
dp_w1b_enable   DD 0
dp_w1b_w1       DD 0            ; 1 = the site held W1's bytes at install (switch off runs W1), 0 = the original loop
dp_rpa_enable   DD 0
dpdata ENDS

.code
caveDq PROC
    cmp     dword ptr [dp_dq_enable], 0
    je      dqStock
    comiss  xmm0, dword ptr [dp_dq_quantum]   ; elapsed vs quantum
    jp      dqExit                            ; unordered: the original (ja) exits too
    jb      dqLoop                            ; elapsed < quantum: keep spinning
    comiss  xmm6, xmm0                        ; would the stock 1 ms spin still be waiting?
    jbe     dqExit
    lock inc dword ptr [dp_dq_early]
    jmp     dqExit
dqStock:
    comiss  xmm6, xmm0                        ; the displaced original
    ja      dqLoop
dqExit:
    jmp     qword ptr [dp_dq_exit]
dqLoop:
    jmp     qword ptr [dp_dq_loop]
caveDq ENDP

caveP1e7 PROC
    cmp     dword ptr [dp_p1e7_enable], 0
    je      pfEnd
    lea     ecx, [r15+2]
    cmp     ecx, dword ptr [rdx+18h]
    jge     pf1
    mov     rcx, qword ptr [r12+10h]          ; tex[k+2]
    prefetcht0 byte ptr [rcx+68h]
pf1:
    lea     ecx, [r15+1]
    cmp     ecx, dword ptr [rdx+18h]
    jge     pfEnd
    mov     rcx, qword ptr [r12+8]            ; tex[k+1]
    test    rcx, rcx
    je      pfEnd
    mov     r10, qword ptr [rcx+68h]          ; what 1E72D6 loads next iteration
    test    r10, r10
    je      pfEnd
    prefetcht0 byte ptr [r10+12Eh]            ; 128EF0 first load (+108h+26h)
    prefetcht0 byte ptr [r10+150h]            ; 128EF0 list node +140h..+158h
    prefetcht0 byte ptr [r10+90h]             ; 1290B8 first load
    prefetcht0 byte ptr [r10+178h]            ; 1290B8 second load
    prefetcht0 byte ptr [r10+1B0h]            ; 1290B8 +188h stamp / +1B0h
pfEnd:
    mov     r14, qword ptr [r12]              ; displaced
    mov     r8, qword ptr [r11+1C8h]          ; displaced
    jmp     qword ptr [dp_p1e7_resume]
caveP1e7 ENDP

caveW1b PROC
    cmp     dword ptr [dp_w1b_enable], 0
    je      w1bOff
    xor     ecx, ecx
w1bLoop:                                      ; 61BAE..61BC0 with `and [m],0` -> `mov [m],0`
    movzx   eax, byte ptr [r14]
    add     eax, r15d
    add     eax, ecx
    inc     ecx
    mov     dword ptr [rbx+rax*4], 0
    cmp     ecx, r15d
    jb      w1bLoop
    jmp     qword ptr [dp_w1b_resume]
w1bOff:
    cmp     dword ptr [dp_w1b_w1], 0
    je      w1bOrig
    movzx   eax, byte ptr [r14]               ; W1 (patches.json W1 new), byte for byte in behaviour
    add     eax, r15d
    lea     rdi, [rbx+rax*4]
    mov     ecx, r15d
    xor     eax, eax
    rep stosd
    jmp     qword ptr [dp_w1b_resume]
w1bOrig:
    xor     ecx, ecx                          ; the game's original 61BAC..61BC1
w1bOrigLoop:
    movzx   eax, byte ptr [r14]
    add     eax, r15d
    add     eax, ecx
    inc     ecx
    and     dword ptr [rbx+rax*4], 0
    cmp     ecx, r15d
    jb      w1bOrigLoop
    jmp     qword ptr [dp_w1b_resume]
caveW1b ENDP

caveRpa PROC
    cmp     dword ptr [dp_rpa_enable], 0
    je      rpaStock
; ---- BEGIN GENERATED rpa_copy (tools\gen_rpa_copy.py from tests\rpa_ranges.txt; do not edit by hand) ----
    ; [10h, 164h) 340 bytes
    mov     rax, -140h
rpaL1:
    movups  xmm0, xmmword ptr [rdx+rax+150h]
    movups  xmm1, xmmword ptr [rdx+rax+160h]
    movups  xmmword ptr [rcx+rax+150h], xmm0
    movups  xmmword ptr [rcx+rax+160h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+170h]
    movups  xmm1, xmmword ptr [rdx+rax+180h]
    movups  xmmword ptr [rcx+rax+170h], xmm0
    movups  xmmword ptr [rcx+rax+180h], xmm1
    add     rax, 64
    jnz     rpaL1
    movups  xmm0, xmmword ptr [rdx+150h]
    movups  xmm1, xmmword ptr [rdx+154h]
    movups  xmmword ptr [rcx+150h], xmm0
    movups  xmmword ptr [rcx+154h], xmm1
    ; [170h, 354h) 484 bytes
    mov     rax, -1C0h
rpaL2:
    movups  xmm0, xmmword ptr [rdx+rax+330h]
    movups  xmm1, xmmword ptr [rdx+rax+340h]
    movups  xmmword ptr [rcx+rax+330h], xmm0
    movups  xmmword ptr [rcx+rax+340h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+350h]
    movups  xmm1, xmmword ptr [rdx+rax+360h]
    movups  xmmword ptr [rcx+rax+350h], xmm0
    movups  xmmword ptr [rcx+rax+360h], xmm1
    add     rax, 64
    jnz     rpaL2
    movups  xmm0, xmmword ptr [rdx+330h]
    movups  xmm1, xmmword ptr [rdx+340h]
    movups  xmmword ptr [rcx+330h], xmm0
    movups  xmmword ptr [rcx+340h], xmm1
    movups  xmm0, xmmword ptr [rdx+344h]
    movups  xmmword ptr [rcx+344h], xmm0
    ; [360h, 0EA4h) 2884 bytes
    mov     rax, -0B40h
rpaL3:
    movups  xmm0, xmmword ptr [rdx+rax+0EA0h]
    movups  xmm1, xmmword ptr [rdx+rax+0EB0h]
    movups  xmmword ptr [rcx+rax+0EA0h], xmm0
    movups  xmmword ptr [rcx+rax+0EB0h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+0EC0h]
    movups  xmm1, xmmword ptr [rdx+rax+0ED0h]
    movups  xmmword ptr [rcx+rax+0EC0h], xmm0
    movups  xmmword ptr [rcx+rax+0ED0h], xmm1
    add     rax, 64
    jnz     rpaL3
    movups  xmm0, xmmword ptr [rdx+0E94h]
    movups  xmmword ptr [rcx+0E94h], xmm0
    ; [0EB0h, 0F06h) 86 bytes
    movups  xmm0, xmmword ptr [rdx+0EB0h]
    movups  xmm1, xmmword ptr [rdx+0EC0h]
    movups  xmmword ptr [rcx+0EB0h], xmm0
    movups  xmmword ptr [rcx+0EC0h], xmm1
    movups  xmm0, xmmword ptr [rdx+0ED0h]
    movups  xmm1, xmmword ptr [rdx+0EE0h]
    movups  xmmword ptr [rcx+0ED0h], xmm0
    movups  xmmword ptr [rcx+0EE0h], xmm1
    movups  xmm0, xmmword ptr [rdx+0EF0h]
    movups  xmm1, xmmword ptr [rdx+0EF6h]
    movups  xmmword ptr [rcx+0EF0h], xmm0
    movups  xmmword ptr [rcx+0EF6h], xmm1
    ; [0F08h, 1EF5h) 4077 bytes
    mov     rax, -0FC0h
rpaL4:
    movups  xmm0, xmmword ptr [rdx+rax+1EC8h]
    movups  xmm1, xmmword ptr [rdx+rax+1ED8h]
    movups  xmmword ptr [rcx+rax+1EC8h], xmm0
    movups  xmmword ptr [rcx+rax+1ED8h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+1EE8h]
    movups  xmm1, xmmword ptr [rdx+rax+1EF8h]
    movups  xmmword ptr [rcx+rax+1EE8h], xmm0
    movups  xmmword ptr [rcx+rax+1EF8h], xmm1
    add     rax, 64
    jnz     rpaL4
    movups  xmm0, xmmword ptr [rdx+1EC8h]
    movups  xmm1, xmmword ptr [rdx+1ED8h]
    movups  xmmword ptr [rcx+1EC8h], xmm0
    movups  xmmword ptr [rcx+1ED8h], xmm1
    movups  xmm0, xmmword ptr [rdx+1EE5h]
    movups  xmmword ptr [rcx+1EE5h], xmm0
    ; [1EF8h, 1F0Eh) 22 bytes
    movups  xmm0, xmmword ptr [rdx+1EF8h]
    movups  xmm1, xmmword ptr [rdx+1EFEh]
    movups  xmmword ptr [rcx+1EF8h], xmm0
    movups  xmmword ptr [rcx+1EFEh], xmm1
    ; [1F10h, 1FA5h) 149 bytes
    mov     rax, -80h
rpaL5:
    movups  xmm0, xmmword ptr [rdx+rax+1F90h]
    movups  xmm1, xmmword ptr [rdx+rax+1FA0h]
    movups  xmmword ptr [rcx+rax+1F90h], xmm0
    movups  xmmword ptr [rcx+rax+1FA0h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+1FB0h]
    movups  xmm1, xmmword ptr [rdx+rax+1FC0h]
    movups  xmmword ptr [rcx+rax+1FB0h], xmm0
    movups  xmmword ptr [rcx+rax+1FC0h], xmm1
    add     rax, 64
    jnz     rpaL5
    movups  xmm0, xmmword ptr [rdx+1F90h]
    movups  xmm1, xmmword ptr [rdx+1F95h]
    movups  xmmword ptr [rcx+1F90h], xmm0
    movups  xmmword ptr [rcx+1F95h], xmm1
    ; [1FA8h, 22FEh) 854 bytes
    mov     rax, -340h
rpaL6:
    movups  xmm0, xmmword ptr [rdx+rax+22E8h]
    movups  xmm1, xmmword ptr [rdx+rax+22F8h]
    movups  xmmword ptr [rcx+rax+22E8h], xmm0
    movups  xmmword ptr [rcx+rax+22F8h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+2308h]
    movups  xmm1, xmmword ptr [rdx+rax+2318h]
    movups  xmmword ptr [rcx+rax+2308h], xmm0
    movups  xmmword ptr [rcx+rax+2318h], xmm1
    add     rax, 64
    jnz     rpaL6
    movups  xmm0, xmmword ptr [rdx+22E8h]
    movups  xmm1, xmmword ptr [rdx+22EEh]
    movups  xmmword ptr [rcx+22E8h], xmm0
    movups  xmmword ptr [rcx+22EEh], xmm1
    ; [2300h, 2335h) 53 bytes
    movups  xmm0, xmmword ptr [rdx+2300h]
    movups  xmm1, xmmword ptr [rdx+2310h]
    movups  xmmword ptr [rcx+2300h], xmm0
    movups  xmmword ptr [rcx+2310h], xmm1
    movups  xmm0, xmmword ptr [rdx+2320h]
    movups  xmm1, xmmword ptr [rdx+2325h]
    movups  xmmword ptr [rcx+2320h], xmm0
    movups  xmmword ptr [rcx+2325h], xmm1
    ; [2338h, 233Dh) 5 bytes
    mov     eax, dword ptr [rdx+2338h]
    mov     r8d, dword ptr [rdx+2339h]
    mov     dword ptr [rcx+2338h], eax
    mov     dword ptr [rcx+2339h], r8d
    ; [2340h, 2345h) 5 bytes
    mov     eax, dword ptr [rdx+2340h]
    mov     r8d, dword ptr [rdx+2341h]
    mov     dword ptr [rcx+2340h], eax
    mov     dword ptr [rcx+2341h], r8d
    ; [2348h, 2352h) 10 bytes
    mov     rax, qword ptr [rdx+2348h]
    mov     r8, qword ptr [rdx+234Ah]
    mov     qword ptr [rcx+2348h], rax
    mov     qword ptr [rcx+234Ah], r8
    ; [2354h, 235Dh) 9 bytes
    mov     rax, qword ptr [rdx+2354h]
    mov     r8, qword ptr [rdx+2355h]
    mov     qword ptr [rcx+2354h], rax
    mov     qword ptr [rcx+2355h], r8
    ; [2360h, 2386h) 38 bytes
    movups  xmm0, xmmword ptr [rdx+2360h]
    movups  xmm1, xmmword ptr [rdx+2370h]
    movups  xmmword ptr [rcx+2360h], xmm0
    movups  xmmword ptr [rcx+2370h], xmm1
    movups  xmm0, xmmword ptr [rdx+2376h]
    movups  xmmword ptr [rcx+2376h], xmm0
    ; [2388h, 2399h) 17 bytes
    movups  xmm0, xmmword ptr [rdx+2388h]
    movups  xmm1, xmmword ptr [rdx+2389h]
    movups  xmmword ptr [rcx+2388h], xmm0
    movups  xmmword ptr [rcx+2389h], xmm1
    ; [239Ch, 23E9h) 77 bytes
    movups  xmm0, xmmword ptr [rdx+239Ch]
    movups  xmm1, xmmword ptr [rdx+23ACh]
    movups  xmmword ptr [rcx+239Ch], xmm0
    movups  xmmword ptr [rcx+23ACh], xmm1
    movups  xmm0, xmmword ptr [rdx+23BCh]
    movups  xmm1, xmmword ptr [rdx+23CCh]
    movups  xmmword ptr [rcx+23BCh], xmm0
    movups  xmmword ptr [rcx+23CCh], xmm1
    movups  xmm0, xmmword ptr [rdx+23D9h]
    movups  xmmword ptr [rcx+23D9h], xmm0
    ; [23ECh, 23FDh) 17 bytes
    movups  xmm0, xmmword ptr [rdx+23ECh]
    movups  xmm1, xmmword ptr [rdx+23EDh]
    movups  xmmword ptr [rcx+23ECh], xmm0
    movups  xmmword ptr [rcx+23EDh], xmm1
    ; [2400h, 2DD8h) 2520 bytes
    mov     rax, -9C0h
rpaL7:
    movups  xmm0, xmmword ptr [rdx+rax+2DC0h]
    movups  xmm1, xmmword ptr [rdx+rax+2DD0h]
    movups  xmmword ptr [rcx+rax+2DC0h], xmm0
    movups  xmmword ptr [rcx+rax+2DD0h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+2DE0h]
    movups  xmm1, xmmword ptr [rdx+rax+2DF0h]
    movups  xmmword ptr [rcx+rax+2DE0h], xmm0
    movups  xmmword ptr [rcx+rax+2DF0h], xmm1
    add     rax, 64
    jnz     rpaL7
    movups  xmm0, xmmword ptr [rdx+2DC0h]
    movups  xmm1, xmmword ptr [rdx+2DC8h]
    movups  xmmword ptr [rcx+2DC0h], xmm0
    movups  xmmword ptr [rcx+2DC8h], xmm1
    ; [2DE0h, 2DE4h) 4 bytes
    mov     eax, dword ptr [rdx+2DE0h]
    mov     r8d, dword ptr [rdx+2DE0h]
    mov     dword ptr [rcx+2DE0h], eax
    mov     dword ptr [rcx+2DE0h], r8d
    ; [2DF0h, 3574h) 1924 bytes
    mov     rax, -780h
rpaL8:
    movups  xmm0, xmmword ptr [rdx+rax+3570h]
    movups  xmm1, xmmword ptr [rdx+rax+3580h]
    movups  xmmword ptr [rcx+rax+3570h], xmm0
    movups  xmmword ptr [rcx+rax+3580h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+3590h]
    movups  xmm1, xmmword ptr [rdx+rax+35A0h]
    movups  xmmword ptr [rcx+rax+3590h], xmm0
    movups  xmmword ptr [rcx+rax+35A0h], xmm1
    add     rax, 64
    jnz     rpaL8
    movups  xmm0, xmmword ptr [rdx+3564h]
    movups  xmmword ptr [rcx+3564h], xmm0
    ; [3580h, 3D38h) 1976 bytes
    mov     rax, -780h
rpaL9:
    movups  xmm0, xmmword ptr [rdx+rax+3D00h]
    movups  xmm1, xmmword ptr [rdx+rax+3D10h]
    movups  xmmword ptr [rcx+rax+3D00h], xmm0
    movups  xmmword ptr [rcx+rax+3D10h], xmm1
    movups  xmm0, xmmword ptr [rdx+rax+3D20h]
    movups  xmm1, xmmword ptr [rdx+rax+3D30h]
    movups  xmmword ptr [rcx+rax+3D20h], xmm0
    movups  xmmword ptr [rcx+rax+3D30h], xmm1
    add     rax, 64
    jnz     rpaL9
    movups  xmm0, xmmword ptr [rdx+3D00h]
    movups  xmm1, xmmword ptr [rdx+3D10h]
    movups  xmmword ptr [rcx+3D00h], xmm0
    movups  xmmword ptr [rcx+3D10h], xmm1
    movups  xmm0, xmmword ptr [rdx+3D20h]
    movups  xmm1, xmmword ptr [rdx+3D28h]
    movups  xmmword ptr [rcx+3D20h], xmm0
    movups  xmmword ptr [rcx+3D28h], xmm1
; ---- END GENERATED rpa_copy ----
    mov     rax, rcx
    ret
rpaStock:
    mov     qword ptr [rsp+8], rbx            ; displaced 247950
    jmp     qword ptr [dp_rpa_resume]
caveRpa ENDP

END
