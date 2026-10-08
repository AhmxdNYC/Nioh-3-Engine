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
OPTION CASEMAP:NONE

PUBLIC caveDq, caveP1e7
PUBLIC dp_dq_enable, dp_p1e7_enable, dp_dq_quantum, dp_dq_loop, dp_dq_exit, dp_p1e7_resume, dp_dq_early

dpdata SEGMENT ALIGN(64) READ WRITE 'DATA'
dp_dq_enable    DD 0
dp_p1e7_enable  DD 0
dp_dq_quantum   REAL4 0.0002
dp_dq_early     DD 0            ; retries that came earlier than the stock 1 ms quantum (enabled path only)
dp_dq_loop      DQ 0            ; Nioh3+1A74BD0 (spin loop head)
dp_dq_exit      DQ 0            ; Nioh3+1A74C4A (`jmp 1A744F1`: retry the frame function)
dp_p1e7_resume  DQ 0            ; Nioh3+1E72D1 (test r14,r14)
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

END
