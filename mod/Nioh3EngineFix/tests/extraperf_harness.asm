; extraperf_harness.asm - register-level drivers for the EPF caves (tests/run_extraperf_harness.py).
;   fxSimProbe(float v)  -> xmm1 = v, rax = 0x1122334455667788, call caveFxSim; returns xmm1 (must be v), checks rax kept
;   fxDrawProbe(float v) -> same with xmm2 / caveFxDraw
;   both return 0.0 - 1.0 = -1.0 if rax changed (the test then fails)
OPTION CASEMAP:NONE
EXTERN caveFxSim:PROC
EXTERN caveFxDraw:PROC
EXTERN caveCharLod:PROC
EXTERN cavePropDist:PROC
EXTERN ep_charFall:QWORD
EXTERN ep_charTake:QWORD
PUBLIC fxSimProbe, fxDrawProbe, charLodProbe, charLodScaled, propProbe

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

; charLodProbe(float limit in xmm0, float camera in xmm1) -> eax 1 = fall (visible), 2 = take (culled).
; charLodScaled is xmm8 after caveCharLod multiplies the limit.
PUBLIC charLodScaled
.data
ALIGN 4
charLodScaled REAL4 0.0
.code
charLodProbe PROC
    push    rbp
    sub     rsp, 40h
    movaps  xmmword ptr [rsp], xmm6
    movaps  xmmword ptr [rsp+10h], xmm8
    movaps  xmm8, xmm0
    movaps  xmm6, xmm1
    lea     rax, rdFall
    mov     qword ptr [ep_charFall], rax
    lea     rax, rdTake
    mov     qword ptr [ep_charTake], rax
    jmp     caveCharLod
rdFall:
    mov     eax, 1
    jmp     rdDone
rdTake:
    mov     eax, 2
rdDone:
    movss   dword ptr [charLodScaled], xmm8
    movaps  xmm8, xmmword ptr [rsp+10h]
    movaps  xmm6, xmmword ptr [rsp]
    add     rsp, 40h
    pop     rbp
    ret
charLodProbe ENDP

; propProbe(dst, src, n, key). The key is what PropTableReadParamsBatch16 compared at [rsi].
propProbe PROC
    push    rsi
    sub     rsp, 20h
    mov     dword ptr [rsp+18h], r9d
    lea     rsi, [rsp+18h]
    call    cavePropDist
    add     rsp, 20h
    pop     rsi
    ret
propProbe ENDP

END
