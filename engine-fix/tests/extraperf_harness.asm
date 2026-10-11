; extraperf_harness.asm - register-level drivers for the EPF caves (tests/run_extraperf_harness.py).
;   fxSimProbe(float v)  -> xmm1 = v, rax = 0x1122334455667788, call caveFxSim; returns xmm1 (must be v), checks rax kept
;   fxDrawProbe(float v) -> same with xmm2 / caveFxDraw
;   both return 0.0 - 1.0 = -1.0 if rax changed (the test then fails)
OPTION CASEMAP:NONE
EXTERN caveFxSim:PROC
EXTERN caveFxDraw:PROC
PUBLIC fxSimProbe, fxDrawProbe

.data
ALIGN 4
minus1 REAL4 -1.0

.code
fxSimProbe PROC
    push    rbx
    sub     rsp, 20h
    movaps  xmm1, xmm0
    mov     rax, 1122334455667788h
    mov     rbx, rax
    call    caveFxSim
    movaps  xmm0, xmm1
    cmp     rax, rbx
    je      @F
    movss   xmm0, dword ptr [minus1]
@@: add     rsp, 20h
    pop     rbx
    ret
fxSimProbe ENDP

fxDrawProbe PROC
    push    rbx
    sub     rsp, 20h
    movaps  xmm2, xmm0
    mov     rax, 1122334455667788h
    mov     rbx, rax
    call    caveFxDraw
    movaps  xmm0, xmm2
    cmp     rax, rbx
    je      @F
    movss   xmm0, dword ptr [minus1]
@@: add     rsp, 20h
    pop     rbx
    ret
fxDrawProbe ENDP

END
