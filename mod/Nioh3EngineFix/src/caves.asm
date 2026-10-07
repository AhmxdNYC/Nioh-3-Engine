; caves.asm - Nioh3EngineFix code caves (ml64). 1:1 port of the live-tested Iced caves in
; patcher\ft_vrr_patch.fsx (v2 + P9) and patcher\cpu_caves.fsx (L1 entry; W3 is C++ hookPeek).
;
; Every patched site does `call <near trampoline>`; the trampoline (allocated within +-2 GB of Nioh3.exe)
; is `jmp qword ptr [rip+0]` to the procedures below, so they run with the game's stack and registers exactly
; as the original caves did. Data the game reads RIP-relatively (ticks120 / ticks60 / r1Timeout) lives in the
; near page; vrr_pNear points at it (offsets NEAR_*). Everything else lives here in .data.
;
; Register contracts (from the scripts / notes\frame-timing.md, notes\cpu-profile.md):
;   caveFps   replaces `mov [rdi+614h],r8d` @P1   rdi=G, r8d=tier fps, eax=old G[620]; preserves all GPRs + xmm0-2
;   caveScale replaces `movss [rdi+624h],xmm2` @P2 (clobbers only xmm2, as the original store's consumer allows)
;   caveLim   replaces `mov eax,[rdi+614h]` @P4   (eax only)
;   caveTier  replaces `call GetFps` @P8a-d and `mov eax,[rdi+614h]` @P9 (eax only)
;   caveMenu  replaces `call GetDelta60` @P7 (optional), normal call semantics
;   caveL1    replaces `xor edx,edx; mov rcx,rsi; call 916EBC` @L1, normal call semantics;
;             rdx=now, r12=last frame QPC, rdi=ticks/frame, rbx=QPC frequency, rsi=scheduler

OPTION CASEMAP:NONE

NEAR_TICKS120 EQU 0
NEAR_TICKS60  EQU 4

EXTERN L1Wait:PROC

PUBLIC caveFps, caveScale, caveLim, caveTier, caveMenu, caveL1
PUBLIC vrr_minNs, vrr_maxNs, vrr_pNear, vrr_getDelta60, l1_callee
PUBLIC vrr_enable, vrr_tier, vrr_capCur, vrr_arm, vrr_cap, vrr_fpsOut, vrr_scale, vrr_last
PUBLIC caveWakeGate, wk_enable, wk_skipped, wk_kept

.data
ALIGN 16
vrr_prev        dq 0            ; previous frame start (ns, G+7D8)
vrr_acc120      REAL8 0.0       ; 120 Hz tick carry
vrr_acc60       REAL8 0.0       ; 60 Hz tick carry
vrr_last        dq 0            ; last clamped interval (ns), diagnostics
c_1e9           REAL8 1.0e9
c_120           REAL8 120.0
c_60            REAL8 60.0
vrr_minNs       dq 3 dup(0)     ; per tier (30/60/120): shortest interval = 1e9 / max fps
vrr_maxNs       dq 3 dup(0)     ; per tier: longest interval = 1e9 / min fps
vrr_pNear       dq 0            ; -> near data page (ticks120, ticks60, r1Timeout)
vrr_getDelta60  dq 0            ; GetDelta60 (taken from the P7 original call)
l1_callee       dq 0            ; 916EBC (taken from the L1 original call)
vrr_enable      dd 1
vrr_scale       REAL4 1.0       ; scaleCorr = fps_i * interval
vrr_fpsOut      dd 0            ; 0 = not armed this frame
vrr_tier        dd 60           ; tier seen by P1 (stock-like default until the first frame)
vrr_capCur      dd 60
vrr_arm         dd 3 dup(0)
vrr_cap         dd 3 dup(0)
c_4f            REAL4 4.0
c_025f          REAL4 0.25
vrr_mcarry      REAL4 0.0

.code

; ---- caveFps (P1) ---------------------------------------------------------------------------------------------
caveFps PROC FRAME
    push    rax
    .pushreg rax
    push    rcx
    .pushreg rcx
    push    rdx
    .pushreg rdx
    push    r9
    .pushreg r9
    sub     rsp, 38h
    .allocstack 38h
    .endprolog
    movdqu  xmmword ptr [rsp], xmm0
    movdqu  xmmword ptr [rsp+10h], xmm1
    movdqu  xmmword ptr [rsp+20h], xmm2
    mov     rcx, qword ptr [rdi+7D8h]           ; this frame's start (ns)
    xchg    qword ptr [vrr_prev], rcx           ; rcx = previous frame start
    mov     dword ptr [vrr_tier], r8d
    cmp     dword ptr [vrr_enable], 0
    je      lStock
    xor     r9d, r9d
    cmp     r8d, 30
    je      lIdx
    mov     r9d, 1
    cmp     r8d, 60
    je      lIdx
    mov     r9d, 2
    cmp     r8d, 120
    jne     lStock
lIdx:
    lea     rdx, vrr_arm
    cmp     dword ptr [rdx+r9*4], 0
    je      lStock
    cmp     eax, r8d                            ; event / time-effect override (G[620] != tier) -> stock
    jne     lStock
    test    rcx, rcx
    je      lStock
    mov     rax, qword ptr [rdi+7D8h]
    sub     rax, rcx
    lea     rdx, vrr_minNs
    cmp     rax, qword ptr [rdx+r9*8]
    jge     a1
    mov     rax, qword ptr [rdx+r9*8]
a1:
    lea     rdx, vrr_maxNs
    cmp     rax, qword ptr [rdx+r9*8]
    jle     a2
    mov     rax, qword ptr [rdx+r9*8]
a2:
    mov     qword ptr [vrr_last], rax
    lea     rdx, vrr_cap
    mov     edx, dword ptr [rdx+r9*4]
    mov     dword ptr [vrr_capCur], edx
    cvtsi2sd xmm1, rax
    divsd   xmm1, qword ptr [c_1e9]             ; xmm1 = interval seconds
    movsd   xmm0, qword ptr [c_1e9]
    cvtsi2sd xmm2, rax
    divsd   xmm0, xmm2
    cvtsd2si ecx, xmm0                          ; fps_i = round(1/interval)
    cmp     ecx, 1
    jge     a3
    mov     ecx, 1
a3:
    mov     dword ptr [vrr_fpsOut], ecx
    mov     dword ptr [rdi+614h], ecx           ; measured fps -> G[614]
    cvtsi2sd xmm0, ecx
    mulsd   xmm0, xmm1
    cvtsd2ss xmm0, xmm0
    movss   dword ptr [vrr_scale], xmm0         ; scaleCorr = fps_i * interval
    movapd  xmm0, xmm1
    mulsd   xmm0, qword ptr [c_120]
    addsd   xmm0, qword ptr [vrr_acc120]
    cvttsd2si ecx, xmm0
    cvtsi2sd xmm2, ecx
    subsd   xmm0, xmm2
    movsd   qword ptr [vrr_acc120], xmm0
    mov     rdx, qword ptr [vrr_pNear]
    mov     dword ptr [rdx+NEAR_TICKS120], ecx  ; 120 Hz ticks with carry
    cmp     r9d, 2
    je      l60z                                ; stock 60/120 == 0 in the 120 tier
    movapd  xmm0, xmm1
    mulsd   xmm0, qword ptr [c_60]
    addsd   xmm0, qword ptr [vrr_acc60]
    cvttsd2si ecx, xmm0
    cvtsi2sd xmm2, ecx
    subsd   xmm0, xmm2
    movsd   qword ptr [vrr_acc60], xmm0
    mov     dword ptr [rdx+NEAR_TICKS60], ecx   ; 60 Hz ticks with carry
    jmp     lDone
l60z:
    mov     dword ptr [rdx+NEAR_TICKS60], 0
    jmp     lDone
lStock:
    mov     dword ptr [vrr_scale], 3F800000h
    mov     dword ptr [vrr_fpsOut], 0
    mov     dword ptr [vrr_capCur], r8d
    mov     dword ptr [rdi+614h], r8d           ; the replaced instruction
    mov     ecx, r8d
    test    ecx, ecx
    je      lDone
    mov     r9, qword ptr [vrr_pNear]
    mov     eax, 120
    xor     edx, edx
    div     ecx
    mov     dword ptr [r9+NEAR_TICKS120], eax   ; stock 120/fps
    mov     eax, 60
    xor     edx, edx
    div     ecx
    mov     dword ptr [r9+NEAR_TICKS60], eax    ; stock 60/fps
lDone:
    movdqu  xmm2, xmmword ptr [rsp+20h]
    movdqu  xmm1, xmmword ptr [rsp+10h]
    movdqu  xmm0, xmmword ptr [rsp]
    add     rsp, 38h
    pop     r9
    pop     rdx
    pop     rcx
    pop     rax
    ret
caveFps ENDP

; ---- caveScale (P2) -------------------------------------------------------------------------------------------
caveScale PROC
    mulss   xmm2, dword ptr [vrr_scale]
    movss   dword ptr [rdi+624h], xmm2
    ret
caveScale ENDP

; ---- caveLim (P4): the limiter keeps the tier cap while armed -----------------------------------------------------
caveLim PROC
    mov     eax, dword ptr [rdi+614h]
    cmp     dword ptr [vrr_fpsOut], 0
    je      lR
    mov     eax, dword ptr [vrr_capCur]
lR:
    ret
caveLim ENDP

; ---- caveTier (P8a-d, P9): integer value must stay the tier -------------------------------------------------------
caveTier PROC
    mov     eax, dword ptr [vrr_tier]
    ret
caveTier ENDP

; ---- caveMenu (P7, optional): GetDelta60 for menu hold timers, quantised to 0.25 with carry while armed -----------
; Unlike the (never-applied) script version this keeps the Win64 ABI for the inner call (16-byte alignment and
; 32 bytes of home space), so GetDelta60 cannot overwrite our return address.
caveMenu PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    call    qword ptr [vrr_getDelta60]
    add     rsp, 28h
    cmp     dword ptr [vrr_fpsOut], 0
    je      lM
    movaps  xmm1, xmm0
    addss   xmm1, dword ptr [vrr_mcarry]
    movaps  xmm0, xmm1
    mulss   xmm0, dword ptr [c_4f]
    roundss xmm0, xmm0, 0
    mulss   xmm0, dword ptr [c_025f]
    subss   xmm1, xmm0
    movss   dword ptr [vrr_mcarry], xmm1
lM:
    ret
caveMenu ENDP

; ---- caveL1: limiter slow path. L1Wait (C++) sleeps on a high-resolution waitable timer when enough slack is left
; and returns 1; otherwise run the original `xor edx,edx; mov rcx,rsi; call 916EBC` as a tail call (916EBC returns
; straight to the site's return address).
caveL1 PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     rcx, rdx                            ; now
    mov     rdx, r12                            ; last frame QPC
    mov     r8, rdi                             ; ticks per frame
    mov     r9, rbx                             ; QPC frequency
    call    L1Wait
    add     rsp, 28h
    test    eax, eax
    jz      lOrig
    ret
lOrig:
    xor     edx, edx
    mov     rcx, rsi
    jmp     qword ptr [l1_callee]
caveL1 ENDP

; ---- WK: job-system wake gate (notes\job-system.md 2a). Site 40A4C `mov rcx,[r15+440h]` (7 B) -> call + nop2.
; Skips the SetEvent of the "wake one more worker" step when no queue of this pool holds work (the woken worker
; would find nothing). Skip path = the stock "no sleeper found" path: resumes at 40B10 with r8 = 0.
; Keep path executes the displaced instruction. Preserves every register and the flags.
.data
ALIGN 4
wk_enable  DD 0
wk_skipped DD 0
wk_kept    DD 0
.code
caveWakeGate PROC
    pushfq
    push    rax
    push    rdx
    push    r8
    push    r9
    cmp     dword ptr [wk_enable], 0
    je      wkKeep
    mov     rax, [r15+440h]
    mov     r8d, dword ptr [rax+28h]
    mov     rax, [rax+10h]
    test    r8d, r8d
    jle     wkKeepC
wkScan:
    mov     rdx, [rax+448h]
    test    rdx, rdx
    je      wkNext
    mov     r9d, dword ptr [rdx]
    or      r9d, dword ptr [rdx+4]
    or      r9d, dword ptr [rdx+8]
    or      r9d, dword ptr [rdx+0Ch]
    jne     wkKeepC
wkNext:
    add     rax, 458h
    dec     r8d
    jne     wkScan
    lock inc dword ptr [wk_skipped]
    add     qword ptr [rsp+28h], 0BFh          ; return 40A51 -> 40B10
    mov     qword ptr [rsp+8], 0               ; saved r8 := 0 (its value on the stock path)
    pop     r9
    pop     r8
    pop     rdx
    pop     rax
    popfq
    ret
wkKeepC:
    lock inc dword ptr [wk_kept]
wkKeep:
    pop     r9
    pop     r8
    pop     rdx
    pop     rax
    popfq
    mov     rcx, [r15+440h]
    ret
caveWakeGate ENDP

END