; gpuopt_thunks.asm - generic vtable thunks for ID3D12GraphicsCommandList (gpuopt.cpp, notes\gpu-barriers.md).
;
; One thunk per vtable slot and per hooked vtable ("set" 0/1: the DIRECT and, if different, the COMPUTE list class).
; Fast path (no stack change, a leaf): pass straight to the original method unless
;   * tracing is on (gpuopt_trace != 0), or
;   * this slot flushes (gpuopt_flush[slot] != 0) AND `this` (rcx) is a list with deferred barriers (gpuopt_pend[0..3]).
; Slow path: the thunk calls gpuopt_slowN, which saves the four argument registers + xmm0-3, calls
;   GpuOptSlow(self = rcx, setSlot = set*256 + slot, a1 = rdx, a2 = r8, a3 = r9)
; (emit the list's deferred barriers as one ResourceBarrier(n) call and/or record a trace entry), restores everything
; and returns into the thunk, which tail-jumps to the original method with the caller's exact registers and stack
; (stack arguments of e.g. ExecuteIndirect / ClearDepthStencilView are untouched: rsp is back at its entry value).
; gpuopt_slowN has full x64 unwind info (PROC FRAME) and a plain ret epilogue; the thunks never move rsp except for
; that call, so they unwind as leaves. Stack walks and exceptions pass through correctly.

OPTION CASEMAP:NONE

EXTERN GpuOptSlow:PROC

PUBLIC gpuopt_pend, gpuopt_flush, gpuopt_trace, gpuopt_orig0, gpuopt_orig1, gpuopt_thunks0, gpuopt_thunks1

GPUOPT_SLOTS EQU 96

.data
ALIGN 16
gpuopt_pend   dq 4 dup(0)                 ; lists that currently hold deferred barriers (one per deferring thread)
gpuopt_orig0  dq GPUOPT_SLOTS dup(0)      ; original methods, vtable set 0
gpuopt_orig1  dq GPUOPT_SLOTS dup(0)      ; original methods, vtable set 1
gpuopt_flush  db GPUOPT_SLOTS dup(0)      ; 1 = this method must see the deferred barriers first (work / sync / unwrap)
gpuopt_trace  db 0                        ; 1 = every hooked call goes to GpuOptSlow (trace mode)

.code

; ---- slow routines (one per set) --------------------------------------------------------------------------------------
; in: r11d = slot, everything else as at the method's entry
SLOWBODY MACRO S
gpuopt_slow&S PROC FRAME
    push    rcx
    .pushreg rcx
    push    rdx
    .pushreg rdx
    push    r8
    .pushreg r8
    push    r9
    .pushreg r9
    sub     rsp, 80h                        ; entry rsp = 0 mod 16 (called from a thunk): stays 16-aligned
    .allocstack 80h
    movdqa  xmmword ptr [rsp+30h], xmm0
    .savexmm128 xmm0, 30h
    movdqa  xmmword ptr [rsp+40h], xmm1
    .savexmm128 xmm1, 40h
    movdqa  xmmword ptr [rsp+50h], xmm2
    .savexmm128 xmm2, 50h
    movdqa  xmmword ptr [rsp+60h], xmm3
    .savexmm128 xmm3, 60h
    .endprolog
    ; frame: [rsp+0..1F] shadow, [rsp+20h] 5th arg, [rsp+30h..6F] xmm0-3,
    ;        [rsp+80h] r9, [rsp+88h] r8, [rsp+90h] rdx, [rsp+98h] rcx, [rsp+0A0h] return address (into the thunk)
    mov     rax, qword ptr [rsp+80h]        ; a3 = original r9
    mov     qword ptr [rsp+20h], rax
    mov     r9, qword ptr [rsp+88h]         ; a2 = original r8
    mov     r8, qword ptr [rsp+90h]         ; a1 = original rdx
    mov     edx, r11d
    or      edx, S * 256                    ; setSlot
    ; rcx = self (unchanged)
    call    GpuOptSlow
    movdqa  xmm0, xmmword ptr [rsp+30h]
    movdqa  xmm1, xmmword ptr [rsp+40h]
    movdqa  xmm2, xmmword ptr [rsp+50h]
    movdqa  xmm3, xmmword ptr [rsp+60h]
    add     rsp, 80h
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    ret                                     ; back into the thunk, which tail-jumps to the original
gpuopt_slow&S ENDP
ENDM

SLOWBODY 0
SLOWBODY 1

; ---- per-slot thunks --------------------------------------------------------------------------------------------------
THUNK MACRO S, I
    LOCAL slow, pass
gpuopt_t&S&_&I LABEL PROC
    cmp     byte ptr [gpuopt_trace], 0
    jne     slow
    cmp     byte ptr [gpuopt_flush + I], 0
    je      pass
    cmp     rcx, qword ptr [gpuopt_pend]
    je      slow
    cmp     rcx, qword ptr [gpuopt_pend + 8]
    je      slow
    cmp     rcx, qword ptr [gpuopt_pend + 16]
    je      slow
    cmp     rcx, qword ptr [gpuopt_pend + 24]
    je      slow
pass:
    jmp     qword ptr [gpuopt_orig&S + I*8]
slow:
    mov     r11d, I
    call    gpuopt_slow&S                   ; the thunk itself never moves rsp, so it unwinds as a leaf
    jmp     qword ptr [gpuopt_orig&S + I*8]
ENDM

gpuopt_thunk_code PROC
    ret                                     ; never called; keeps the thunks inside a PROC for the assembler
gpuopt_thunk_code ENDP

cnt = 0
REPT GPUOPT_SLOTS
    THUNK 0, %cnt
    cnt = cnt + 1
ENDM

cnt = 0
REPT GPUOPT_SLOTS
    THUNK 1, %cnt
    cnt = cnt + 1
ENDM

; ---- thunk address tables ---------------------------------------------------------------------------------------------
TENTRY MACRO S, I
    dq gpuopt_t&S&_&I
ENDM

.data
ALIGN 8
gpuopt_thunks0 LABEL QWORD
cnt = 0
REPT GPUOPT_SLOTS
    TENTRY 0, %cnt
    cnt = cnt + 1
ENDM
gpuopt_thunks1 LABEL QWORD
cnt = 0
REPT GPUOPT_SLOTS
    TENTRY 1, %cnt
    cnt = cnt + 1
ENDM

END
