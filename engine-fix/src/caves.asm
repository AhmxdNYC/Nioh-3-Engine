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
PUBLIC tg_cap

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
tg_cap          dd 0            ; TG: limiter cap from the tick governor (0 = stock / not engaged)

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
; TG (1.6.0, opt-in): while VRR is armed and the tick governor publishes a cap (tg_cap != 0, always below the tier
; cap), the limiter target becomes tg_cap. eax and the flags only, as before (P4 is followed by `mov [rsp+20h],eax;
; call`, so the flags are dead).
caveLim PROC
    mov     eax, dword ptr [rdi+614h]
    cmp     dword ptr [vrr_fpsOut], 0
    je      lR
    mov     eax, dword ptr [vrr_capCur]
    cmp     dword ptr [tg_cap], 0
    je      lR
    cmp     eax, dword ptr [tg_cap]
    jbe     lR
    mov     eax, dword ptr [tg_cap]
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
    ; WS (1.3.0): split the wake checks by the caller of JobWorkerPoll 40800. Its frame is 8 pushes + 68h, so its
    ; return address is at site rsp + 0A8h = [rsp + 0D8h] here (call 8 + pushfq + 4 pushes = 30h).
    cmp     dword ptr [ws_enable], 0
    je      wsNoSplit
    mov     rax, qword ptr [rsp+0D8h]
    mov     edx, 3
    cmp     rax, qword ptr [ws_ret]
    jne     @F
    xor     edx, edx
@@:
    cmp     rax, qword ptr [ws_ret+8]
    jne     @F
    mov     edx, 1
@@:
    cmp     rax, qword ptr [ws_ret+10h]
    jne     @F
    mov     edx, 2
@@:
    shl     edx, 6
    lea     rax, ws_chk
    lock inc qword ptr [rax+rdx]
wsNoSplit:
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

; =================================================================================================================
; EXPERIMENTAL CPU-power caves (ini default 0). 1:1 ports of the Iced caves in the live scripts named per cave; the
; only additions are runtime switches where the script had none (B) and absolute jumps back into the game
; (xx_resume / xx_stock / xx_target are absolute addresses set by dllmain before the site is written).
; Every switch is read on each pass, so the mod menu flips them live; switch off = the original instructions.
; =================================================================================================================
PUBLIC caveGallop, cavePP, caveFPC, caveAabb, caveM1, caveX1, caveE1A, caveE1B, caveW6, caveW6Wake
PUBLIC gal_enable, gal_resume
PUBLIC pp_enable, pp_mask, pp_target, pp_ran, pp_skipped
PUBLIC fpc_enable, fpc_tid, fpc_rlo, fpc_rhi, fpc_gaddr, fpc_orig, fpc_epoch, fpc_hits, fpc_misses, fpc_bypass
PUBLIC aabb_enable, aabb_resume
PUBLIC m1_enable, m1_resume, m1_stock, m1_refresh
PUBLIC x1_enable, x1_pmgr, x1_iat, x1_skip, x1_pass
PUBLIC e1_enable, e1_mask, e1_scale, e1_pass, e1_upd, e1_skip, e1_aback, e1_enq
PUBLIC w6_waiting, w6_wakeAll, w6_wakes
EXTERN W6Wait:PROC

; own segment: .data only aligns to 16, the worker-shared lines want 64
expdata SEGMENT ALIGN(64) READ WRITE 'DATA'
; ---- B (patcher\gallop_live.fsx) + PP/FPC (patcher\drawprep_elim.fsx): main render thread only ----
gal_enable  DD 0
pp_enable   DD 0
pp_mask     DD 3                ; PP period - 1 (period 4)
pp_ran      DD 0
pp_skipped  DD 0
fpc_enable  DD 0
fpc_tid     DD 0                ; [App]Game TID (0 = not found yet: every call bypasses the cache)
fpc_last    DD 0                ; last seen g+3C60h
gal_resume  DQ 0                ; Nioh3+3BC1C
pp_target   DQ 0                ; Nioh3+2522C4 (from the original call)
fpc_rlo     DQ 0                ; .rdata start
fpc_rhi     DQ 0                ; .rdata end
fpc_gaddr   DQ 0                ; &g (Nioh3+45BA588)
fpc_orig    DQ 0                ; MaterialFindParam Nioh3+3C768C
fpc_epoch   DQ 1                ; own epoch: bumped whenever g+3C60h changes (table entries never outlive a frame)
fpc_hits    DQ 0
fpc_misses  DQ 0
fpc_bypass  DQ 0                ; main thread, name not in .rdata or g == 0
; ---- AABB (re\scripts_logic\logic_live.fsx): constants must be 16-byte aligned (xorps/andps memory operands) ----
ALIGN 16
aabb_one    DD 3F800000h, 3F800000h, 3F800000h, 3F800000h
aabb_mone   DD 0BF800000h, 0BF800000h, 0BF800000h, 0BF800000h
aabb_s0     DD 80000000h, 0, 0, 0
aabb_s1     DD 0, 80000000h, 0, 0
aabb_s2     DD 0, 0, 80000000h, 0
aabb_enable DD 0
            DD 0
aabb_resume DQ 0                ; Nioh3+6EA55
; ---- M1 (re\scripts_workers\workers_live.fsx): line 0 read-mostly by every worker, line 1 the refresh counter ----
ALIGN 64
m1_enable   DD 0
            DD 0
m1_khead    DQ 0                ; (list-0 head | 1), 0 = cache empty
m1_kval     DQ 0                ; hi32 = rdtsc>>22 tag, lo32 = sum
m1_resume   DQ 0                ; Nioh3+491AC
m1_stock    DQ 0                ; Nioh3+49138
ALIGN 64
m1_refresh  DD 0
ALIGN 64
; ---- X1 (re\scripts_input\input_live.fsx): line 0 read-mostly, line 1 counters ----
x1_enable   DD 0
            DD 0
x1_pmgr     DQ 0                ; &padMgr global (Nioh3+45BA1E0)
x1_iat      DQ 0                ; &IAT slot of XInputGetState (Nioh3+38E2C50) - read at every call: hooks stay chained
ALIGN 64
x1_skip     DD 0
x1_pass     DD 0
ALIGN 64
; ---- E1 (re\scripts_fx\fx_live.fsx): one line, written by the game-update thread only ----
e1_enable   DD 0
e1_mask     DD 1                ; N - 1
e1_scale    REAL4 2.0           ; N
e1_pass     DD 0
e1_upd      DQ 0
e1_skip     DQ 0
e1_aback    DQ 0                ; Nioh3+7E82B9
e1_enq      DQ 0                ; Nioh3+73A6F8 (from the original call)
ALIGN 64
; ---- W6 (present wait): waiter count (W6Wait) and the waker ----
w6_waiting  DD 0
            DD 0
w6_wakeAll  DQ 0                ; WakeByAddressAll
w6_wakes    DQ 0
expdata ENDS

.data?
ALIGN 16
fpc_tab     DB 20000h DUP(?)    ; 2048 x 64 B {material, name, first link, epoch, result}

.code

; ---- B: galloping group-length search. Site 3BC17 `mov r10d,[rsp+28h]` (first instruction of the list-type-3
; path of 3BC0C) -> jmp. Same stack as the function (reached by jmp): [rsp+28h] = n, [rsp+30h] = out*. Probes s, s+1,
; s+3, s+7 ... then a binary search in the last bracket; ends with the original tail (store, restore rbx, ret).
; Switch off: the displaced instruction, then the original binary search at 3BC1C.
caveGallop PROC
    cmp     dword ptr [gal_enable], 0
    je      galStock
    mov     r10d, dword ptr [rsp+28h]
    cmp     r9d, r10d
    jae     galTail
    mov     r11, qword ptr [r8+18h]
    mov     rbx, qword ptr [rdx]
    mov     eax, r9d
    mov     rcx, qword ptr [rbx+rax*8]
    mov     rcx, qword ptr [rcx]
    cmp     r11, qword ptr [rcx+18h]
    jne     galEIsS
    mov     r8d, r9d
    mov     edx, 1
galLoop:
    lea     eax, [r8+rdx]
    cmp     eax, r10d
    jae     galBsN
    mov     rcx, qword ptr [rbx+rax*8]
    mov     rcx, qword ptr [rcx]
    cmp     r11, qword ptr [rcx+18h]
    jne     galBs
    mov     r8d, eax
    add     edx, edx
    jmp     galLoop
galBsN:
    mov     eax, r10d
galBs:
    mov     ecx, eax
    sub     ecx, r8d
    cmp     ecx, 1
    jbe     galFound
    shr     ecx, 1
    add     ecx, r8d
    mov     rdx, qword ptr [rbx+rcx*8]
    mov     rdx, qword ptr [rdx]
    cmp     r11, qword ptr [rdx+18h]
    cmove   r8d, ecx
    cmovne  eax, ecx
    jmp     galBs
galFound:
    mov     r10d, eax
    jmp     galTail
galEIsS:
    mov     r10d, r9d
galTail:
    sub     r10d, r9d
    inc     r10d
    mov     rax, qword ptr [rsp+30h]
    mov     dword ptr [rax], r10d
    mov     rbx, qword ptr [rsp+8]
    ret
galStock:
    mov     r10d, dword ptr [rsp+28h]
    jmp     qword ptr [gal_resume]
caveGallop ENDP

; ---- PP: page-pool usage sweep every N-th frame. Site 252677 `call PagePoolScanAll` -> call here; rcx = pool.
; rax + flags are dead after the call (25267C inc [rdi+D10h]; ...). Skipped frames publish the latest measurement as
; "previous" (B30h = B2Ch, B38h = B34h), so readers see a value at most N-1 frames old.
cavePP PROC
    cmp     dword ptr [pp_enable], 0
    je      ppRun
    cmp     byte ptr [rcx+0B28h], 0
    je      ppRun
    mov     eax, dword ptr [rcx+0D10h]
    test    dword ptr [pp_mask], eax
    je      ppRun
    mov     eax, dword ptr [rcx+0B2Ch]
    mov     dword ptr [rcx+0B30h], eax
    mov     eax, dword ptr [rcx+0B34h]
    mov     dword ptr [rcx+0B38h], eax
    inc     dword ptr [pp_skipped]
    ret
ppRun:
    inc     dword ptr [pp_ran]
    jmp     qword ptr [pp_target]
cavePP ENDP

; ---- FPC: per-frame cache for MaterialFindParam (material vtbl+F8h slot 38FB9D8 points here). rcx = material,
; rdx = name. Cached only on [App]Game, for names inside .rdata (string literals), with g != 0. Key: material, name,
; [material+1338h] (first link), own epoch. A miss calls the original and fills the entry (fpcMiss, below).
caveFPC PROC
    cmp     dword ptr [fpc_enable], 0
    je      fBypass
    db      65h, 8Bh, 04h, 25h, 48h, 00h, 00h, 00h      ; mov eax,gs:[48h] (TEB ClientId.UniqueThread)
    cmp     eax, dword ptr [fpc_tid]
    jne     fBypass
    cmp     rdx, qword ptr [fpc_rlo]
    jb      fBypassMain
    cmp     rdx, qword ptr [fpc_rhi]
    jae     fBypassMain
    mov     r8, qword ptr [fpc_gaddr]
    mov     r8, qword ptr [r8]
    test    r8, r8
    je      fBypassMain
    mov     r8d, dword ptr [r8+3C60h]
    cmp     r8d, dword ptr [fpc_last]
    je      fEpochOk
    mov     dword ptr [fpc_last], r8d
    inc     qword ptr [fpc_epoch]
fEpochOk:
    mov     r8, qword ptr [fpc_epoch]
    mov     r9, qword ptr [rcx+1338h]
    mov     rax, rcx
    shr     rax, 6
    mov     r10, rcx
    shr     r10, 17
    xor     rax, r10
    mov     r10, rdx
    shr     r10, 3
    xor     rax, r10
    and     eax, 2047
    shl     rax, 6
    lea     r10, fpc_tab
    add     r10, rax
    cmp     qword ptr [r10], rcx
    jne     fMiss
    cmp     qword ptr [r10+8], rdx
    jne     fMiss
    cmp     qword ptr [r10+10h], r9
    jne     fMiss
    cmp     qword ptr [r10+18h], r8
    jne     fMiss
    mov     rax, qword ptr [r10+20h]
    inc     qword ptr [fpc_hits]
    ret
fMiss:
    jmp     fpcMiss
fBypassMain:
    inc     qword ptr [fpc_bypass]
fBypass:
    jmp     qword ptr [fpc_orig]
caveFPC ENDP

; entered by jmp from caveFPC with the caller's return address on top (rsp = 16n+8): r10 = entry, r9 = first link,
; r8 = epoch, rcx/rdx = the original arguments
fpcMiss PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rbx, r10
    mov     rsi, rcx
    mov     rdi, rdx
    mov     r12, r9
    mov     r13, r8
    call    qword ptr [fpc_orig]
    mov     qword ptr [rbx], 0                          ; single writer (main thread); invalidate first
    mov     qword ptr [rbx+8], rdi
    mov     qword ptr [rbx+10h], r12
    mov     qword ptr [rbx+18h], r13
    mov     qword ptr [rbx+20h], rax
    mov     qword ptr [rbx], rsi
    inc     qword ptr [fpc_misses]
    add     rsp, 20h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
fpcMiss ENDP

; ---- AABB: ObbToAabb (6EA50) in SSE, same IEEE operations in the same order as the 54 stock vec calls, so the
; output is bit-identical (logic_live.fsx: 300k random boxes, 0 mismatches). Falls back to the original when out
; overlaps the obb or the switch is off: displaced `mov [rsp+8],rbx`, then the original at 6EA55.
AABB_DIR MACRO P, NEG, EOFF
    movaps  xmm2, xmm1
    shufps  xmm2, xmm2, 55h
    blendps xmm2, xmm0, 2
    IF NEG
    xorps   xmm2, xmmword ptr [aabb_s1]
    ENDIF
    movaps  xmm3, xmm1
    shufps  xmm3, xmm3, 0
    blendps xmm3, xmm0, 1
    IF NEG
    xorps   xmm3, xmmword ptr [aabb_s0]
    ENDIF
    addps   xmm2, xmm3
    movaps  xmm3, xmm1
    shufps  xmm3, xmm3, 0AAh
    blendps xmm3, xmm0, 4
    IF NEG
    xorps   xmm3, xmmword ptr [aabb_s2]
    ENDIF
    addps   xmm2, xmm3
    xorps   xmm3, xmm3
    cmpltps xmm3, xmm2
    movaps  xmm2, xmm3
    andps   xmm3, xmmword ptr [aabb_one]
    andnps  xmm2, xmmword ptr [aabb_mone]
    orps    xmm3, xmm2
    movss   xmm2, dword ptr [rcx+EOFF]
    shufps  xmm2, xmm2, 0
    mulps   xmm3, xmm2
    mulps   xmm3, xmm0
    addps   xmm3, P
    movaps  P, xmm3
ENDM
AABB_AXIS MACRO AXOFF, EOFF
    movups  xmm0, xmmword ptr [rcx+AXOFF]
    xorps   xmm1, xmm1
    mulps   xmm1, xmm0
    AABB_DIR xmm4, 0, EOFF
    AABB_DIR xmm5, 1, EOFF
ENDM
caveAabb PROC
    test    byte ptr [aabb_enable], 1
    jz      aabbFallback
    lea     rax, [rcx+4Ch]
    cmp     rdx, rax
    jae     aabbNoAlias
    lea     rax, [rdx+1Ch]
    cmp     rcx, rax
    jb      aabbFallback
aabbNoAlias:
    movups  xmm5, xmmword ptr [rcx]
    movaps  xmm4, xmm5
    AABB_AXIS 10h, 40h
    AABB_AXIS 20h, 44h
    AABB_AXIS 30h, 48h
    movq    qword ptr [rdx], xmm4
    extractps dword ptr [rdx+8], xmm4, 2
    movq    qword ptr [rdx+10h], xmm5
    extractps dword ptr [rdx+18h], xmm5, 2
    ret
aabbFallback:
    mov     qword ptr [rsp+8], rbx
    jmp     qword ptr [aabb_resume]
caveAabb ENDP

; ---- M1: TS_MESHLET_REQ page-pool capacity sum cache. Site 49131 `mov rdx,[rsi+1C8h]` -> jmp; nop2. rsi = meshlet
; manager, r13d = 0. Out: ecx = sum, then 491AC. Clobbers rax/rcx/rdx/flags exactly like 49131..491AB.
; Staleness: one rdtsc>>22 window, and an immediate refresh when the list-0 head changes.
caveM1 PROC
    cmp     dword ptr [m1_enable], 0
    je      m1Stock
    rdtsc
    shrd    eax, edx, 22
    mov     ecx, eax
    mov     rdx, qword ptr [rsi+1C8h]
    mov     rax, qword ptr [rdx+67A8h]
    or      rax, 1
    cmp     rax, qword ptr [m1_khead]
    jne     m1Refresh
    mov     rax, qword ptr [m1_kval]
    shl     rcx, 32
    xor     rcx, rax
    shr     rcx, 32
    jne     m1Refresh
    mov     ecx, eax
    jmp     qword ptr [m1_resume]
m1Refresh:
    mov     rdx, qword ptr [rsi+1C8h]
    mov     ecx, r13d
    mov     rax, qword ptr [rdx+67A8h]
    cmp     byte ptr [rdx+67C0h], cl
    je      m1Three
m1One:
    test    rax, rax
    je      m1Done
    add     ecx, dword ptr [rax+14h]
    mov     rax, qword ptr [rax+30h]
    jmp     m1One
m1Three:
    test    rax, rax
    je      m1W1
    add     ecx, dword ptr [rax+14h]
    mov     rax, qword ptr [rax+30h]
    jmp     m1Three
m1W1:
    mov     rax, qword ptr [rdx+67B0h]
m1W1l:
    test    rax, rax
    je      m1W2
    add     ecx, dword ptr [rax+14h]
    mov     rax, qword ptr [rax+30h]
    jmp     m1W1l
m1W2:
    mov     rax, qword ptr [rdx+67B8h]
m1W2l:
    test    rax, rax
    je      m1Done
    add     ecx, dword ptr [rax+14h]
    mov     rax, qword ptr [rax+30h]
    jmp     m1W2l
m1Done:
    mov     rax, qword ptr [rdx+67A8h]
    or      rax, 1
    mov     qword ptr [m1_khead], rax                   ; key first, then value (x64 TSO keeps the order)
    rdtsc
    shrd    eax, edx, 22
    shl     rax, 32
    or      rax, rcx
    mov     qword ptr [m1_kval], rax
    inc     dword ptr [m1_refresh]
    jmp     qword ptr [m1_resume]
m1Stock:
    mov     rdx, qword ptr [rsi+1C8h]
    jmp     qword ptr [m1_stock]
caveM1 ENDP

; ---- X1: active-pad scan skips slots the game already knows are empty. Site 435BB7 `call [XInputGetState]` ->
; nop; call here (ecx = slot, rdx = state*). Clobbers rax, r11, flags only.
caveX1 PROC
    cmp     dword ptr [x1_enable], 0
    je      x1Pass
    cmp     ecx, 3
    ja      x1Pass
    mov     rax, qword ptr [x1_pmgr]
    mov     rax, qword ptr [rax]
    test    rax, rax
    je      x1Pass
    mov     r11d, ecx
    lea     r11, [r11+r11*4]
    cmp     byte ptr [rax+r11*4+69850h], 0
    jne     x1Pass
    inc     dword ptr [x1_skip]
    mov     eax, 48Fh                                   ; ERROR_DEVICE_NOT_CONNECTED
    ret
x1Pass:
    inc     dword ptr [x1_pass]
    mov     r11, qword ptr [x1_iat]
    jmp     qword ptr [r11]                             ; tail call through the current IAT entry
caveX1 ENDP

; ---- E1: invisible (NoDisp) effects updated every N-th pass with dt*N.
; A: site 7E82B4 (EffectThreadListsReset entry `xor edx,edx; mov r9d,edx`) -> jmp: count the pass.
caveE1A PROC
    inc     dword ptr [e1_pass]
    xor     edx, edx
    mov     r9d, edx
    jmp     qword ptr [e1_aback]
caveE1A ENDP
; B: site 160B83 `call EffectNoDispEnqueue` -> call here. rcx = mgr, edx = list, r8 = effect sys, xmm3 = dt.
; Clobbers rax, r9, r10, xmm3, flags (proven dead at 160B88 by fx_live.fsx).
caveE1B PROC
    cmp     dword ptr [e1_enable], 0
    je      e1Stock
    mov     r9, r8
    shr     r9, 7
    mov     r10, r8
    shr     r10, 13
    xor     r9, r10
    mov     eax, dword ptr [e1_pass]
    add     eax, r9d
    test    dword ptr [e1_mask], eax
    jne     e1Skip
    mulss   xmm3, dword ptr [e1_scale]
    inc     qword ptr [e1_upd]
e1Stock:
    jmp     qword ptr [e1_enq]
e1Skip:
    inc     qword ptr [e1_skip]
    ret
caveE1B ENDP

; ---- W6: present wait. Site 7D9EA2 `xor ecx,ecx; call [Sleep]` inside `while ([ctx+7728h]) Sleep(0)` (7D9E90,
; rbx = ctx) -> call here; nop3. W6Wait (C++) blocks until the flag clears (or Sleep(0)s once when switched off); the
; loop's own re-check at 7D9EAA then exits. Stack: 7D9E90 keeps rsp 16-aligned + 20h home space, so this is a
; normal call into a C function.
caveW6 PROC
    mov     rcx, rbx
    jmp     W6Wait
caveW6 ENDP

; ---- W6 waker: site 1A822D9 `mov byte [rbx+7728h],0` (Present thread, right after swapchain->Present) -> call
; here; nop2. Does the store, then wakes a W6 waiter if there is one. rax is live (1A822E0 test rax,rax): every
; volatile register is kept; flags are dead (that test rewrites them).
caveW6Wake PROC
    mov     byte ptr [rbx+7728h], 0
    mfence                                              ; store flag, then read the waiter count (Dekker with W6Wait)
    cmp     dword ptr [w6_waiting], 0
    jne     w6WakeSlow
    ret
caveW6Wake ENDP

w6WakeSlow PROC FRAME
    push    rax
    .pushreg rax
    push    rcx
    .pushreg rcx
    push    rdx
    .pushreg rdx
    push    r8
    .pushreg r8
    push    r9
    .pushreg r9
    push    r10
    .pushreg r10
    push    r11
    .pushreg r11
    sub     rsp, 80h
    .allocstack 80h
    .endprolog
    movdqu  xmmword ptr [rsp+20h], xmm0
    movdqu  xmmword ptr [rsp+30h], xmm1
    movdqu  xmmword ptr [rsp+40h], xmm2
    movdqu  xmmword ptr [rsp+50h], xmm3
    movdqu  xmmword ptr [rsp+60h], xmm4
    movdqu  xmmword ptr [rsp+70h], xmm5
    lea     rcx, [rbx+7728h]
    call    qword ptr [w6_wakeAll]
    lock inc qword ptr [w6_wakes]
    movdqu  xmm5, xmmword ptr [rsp+70h]
    movdqu  xmm4, xmmword ptr [rsp+60h]
    movdqu  xmm3, xmmword ptr [rsp+50h]
    movdqu  xmm2, xmmword ptr [rsp+40h]
    movdqu  xmm1, xmmword ptr [rsp+30h]
    movdqu  xmm0, xmmword ptr [rsp+20h]
    add     rsp, 80h
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    pop     rax
    ret
w6WakeSlow ENDP

; =================================================================================================================
; INPUT LATENCY (1.2.0): IK late input kick + TS timestamp probes. 1:1 port of patcher\latency_live.fsx
; (notes\input-latency.md 6.1 / 6.3). Engine functions are reached through absolute pointers set by dllmain.
; Probe ids in the ring: 1 K (pad kick), 2 C (pad consumed), 3 W (render begin), 4 B (after the waitable wait),
; 5 P (present handed to the present thread), 6 L (limiter exit).
; =================================================================================================================
PUBLIC caveIkKick, caveIkPadWait, caveTsL, caveTsW, caveTsB, caveTsP
PUBLIC lat_ik, lat_ts, lat_iklatch, lat_cap, lat_qpcNow, lat_inputKick, lat_padWait1, lat_padWaitN, lat_padEvent
PUBLIC lat_latMarker, lat_iatSetEvent, lat_lastkick, lat_agesum, lat_agecnt, lat_agemax, lat_timeouts, lat_ringidx
PUBLIC lat_kstock, lat_kik, lat_ring

latdata SEGMENT ALIGN(64) READ WRITE 'DATA'
lat_ik          DD 0            ; IK switch (live)
lat_ts          DD 0            ; TS switch (live)
lat_iklatch     DD 0            ; IK state latched at the last logic-frame start
                DD 0
lat_cap         DQ 2            ; duration for KtglEventWaitFor (stock pad-stage wait passes 1 = 1 ms)
lat_qpcNow      DQ 0            ; Nioh3+205530 QpcNow
lat_inputKick   DQ 0            ; Nioh3+5AB7B0 InputWorkerKick (callee of the IK site)
lat_padWait1    DQ 0            ; Nioh3+86C018 InputWorkerWaitPadStage (callee of IKc)
lat_padWaitN    DQ 0            ; Nioh3+86C040 KtglEventWaitFor(event*, duration*)
lat_padEvent    DQ 0            ; Nioh3+45C39B0 pad-stage event
lat_latMarker   DQ 0            ; Nioh3+6B8D34 (callee of TSb)
lat_iatSetEvent DQ 0            ; &IAT slot SetEvent (Nioh3+38E2348)
ALIGN 64
lat_lastkick    DQ 0            ; QPC of the last kick
lat_agesum      DQ 0            ; input age K->C (QPC ticks)
lat_agecnt      DQ 0
lat_agemax      DQ 0
lat_timeouts    DQ 0            ; IK pad-stage waits that hit the cap
lat_kstock      DQ 0            ; kicks at frame end (stock position)
lat_kik         DQ 0            ; kicks at frame start (IK)
ALIGN 64
lat_ringidx     DQ 0
latdata ENDS

.data?
ALIGN 16
lat_ring        DB 10000h DUP(?) ; 4096 x {QPC, id}

.code

; stamp: rax = QPC, ecx = id. Clobbers r10, r11, flags only.
latStamp PROC
    cmp     dword ptr [lat_ts], 0
    je      lsOut
    mov     r10d, 1
    lock xadd qword ptr [lat_ringidx], r10
    and     r10d, 4095
    shl     r10, 4
    lea     r11, lat_ring
    add     r10, r11
    mov     qword ptr [r10], rax
    mov     qword ptr [r10+8], rcx
lsOut:
    ret
latStamp ENDP

; K: replaces `call InputWorkerKick` at the end of the logic frame (5AB6CF). IK active this frame -> skip (the kick
; already happened at the frame start).
caveIkKick PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    cmp     dword ptr [lat_iklatch], 0
    jne     kSkip
    call    qword ptr [lat_qpcNow]
    mov     qword ptr [lat_lastkick], rax
    inc     qword ptr [lat_kstock]
    mov     ecx, 1
    call    latStamp
    add     rsp, 28h
    jmp     qword ptr [lat_inputKick]
kSkip:
    add     rsp, 28h
    ret
caveIkKick ENDP

; C: replaces `call InputWorkerWaitPadStage` at the start of the logic frame (5ACAAA, same G+646h gate as the stock
; kick). The switch is latched once per frame: IK path if IK is on now OR was on last frame, so switching IK off still
; polls once here (last frame's end kick was skipped). The frame IK is switched ON polls twice (end kick + here).
caveIkPadWait PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     eax, dword ptr [lat_ik]
    mov     ecx, dword ptr [lat_iklatch]
    mov     dword ptr [lat_iklatch], eax
    or      eax, ecx
    je      cStock
    call    qword ptr [lat_qpcNow]
    mov     qword ptr [lat_lastkick], rax
    inc     qword ptr [lat_kik]
    mov     ecx, 1
    call    latStamp
    call    qword ptr [lat_inputKick]
    cmp     qword ptr [lat_cap], 0                      ; cap 0: don't wait here (GameModeUpdateMain joins the
    je      cMeas                                       ; input worker later anyway, 5AB7E0)
    mov     rcx, qword ptr [lat_padEvent]
    lea     rdx, lat_cap
    call    qword ptr [lat_padWaitN]
    test    al, al
    jne     cMeas
    lock inc qword ptr [lat_timeouts]
    jmp     cMeas
cStock:
    call    qword ptr [lat_padWait1]
cMeas:
    call    qword ptr [lat_qpcNow]
    mov     rdx, qword ptr [lat_lastkick]
    test    rdx, rdx
    je      cNoAge
    mov     rcx, rax
    sub     rcx, rdx
    js      cNoAge
    add     qword ptr [lat_agesum], rcx
    inc     qword ptr [lat_agecnt]
    cmp     rcx, qword ptr [lat_agemax]
    jbe     cNoAge
    mov     qword ptr [lat_agemax], rcx
cNoAge:
    mov     ecx, 2
    call    latStamp
    add     rsp, 28h
    ret
caveIkPadWait ENDP

; L: replaces `mov [r15],rdx; xorps xmm1,xmm1` (5AD2BB, call + nop). rdx = limiter exit QPC. Flags are dead after it
; (5AD2C1 test rbx,rbx).
caveTsL PROC FRAME
    push    rax
    .pushreg rax
    push    rcx
    .pushreg rcx
    push    rdx
    .pushreg rdx
    push    r10
    .pushreg r10
    push    r11
    .pushreg r11
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     qword ptr [r15], rdx
    xorps   xmm1, xmm1
    mov     rax, rdx
    mov     ecx, 6
    call    latStamp
    add     rsp, 20h
    pop     r11
    pop     r10
    pop     rdx
    pop     rcx
    pop     rax
    ret
caveTsL ENDP

; W: replaces `call QpcNow` at RenderFrameBegin entry (4A52D4); returns the QPC in rax.
caveTsW PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    call    qword ptr [lat_qpcNow]
    mov     ecx, 3
    call    latStamp
    add     rsp, 28h
    ret
caveTsW ENDP

; B: replaces `call 6B8D34` right after the frame-latency (waitable) wait (4A535E); keeps rcx/rdx/r8/r9/xmm0-3.
caveTsB PROC
    cmp     dword ptr [lat_ts], 0
    jne     tsBSlow
    jmp     qword ptr [lat_latMarker]
caveTsB ENDP

tsBSlow PROC FRAME
    push    rcx
    .pushreg rcx
    push    rdx
    .pushreg rdx
    push    r8
    .pushreg r8
    push    r9
    .pushreg r9
    sub     rsp, 68h
    .allocstack 68h
    .endprolog
    movdqu  xmmword ptr [rsp+20h], xmm0
    movdqu  xmmword ptr [rsp+30h], xmm1
    movdqu  xmmword ptr [rsp+40h], xmm2
    movdqu  xmmword ptr [rsp+50h], xmm3
    call    qword ptr [lat_qpcNow]
    mov     ecx, 4
    call    latStamp
    movdqu  xmm0, xmmword ptr [rsp+20h]
    movdqu  xmm1, xmmword ptr [rsp+30h]
    movdqu  xmm2, xmmword ptr [rsp+40h]
    movdqu  xmm3, xmmword ptr [rsp+50h]
    add     rsp, 68h
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    jmp     qword ptr [lat_latMarker]
tsBSlow ENDP

; P: replaces `call [SetEvent]` (3D6552, call + nop) in GfxDevicePresent; rcx = event handle.
caveTsP PROC
    cmp     dword ptr [lat_ts], 0
    jne     tsPSlow
tsPGo::
    mov     rax, qword ptr [lat_iatSetEvent]
    jmp     qword ptr [rax]
caveTsP ENDP

tsPSlow PROC FRAME
    push    rcx
    .pushreg rcx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    call    qword ptr [lat_qpcNow]
    mov     ecx, 5
    call    latStamp
    add     rsp, 20h
    pop     rcx
    jmp     tsPGo
tsPSlow ENDP

; =================================================================================================================
; WORKER WAKES (1.3.0, notes\worker-power.md): WS wake counters (measurement only) and ED EffectDisp empty-list skip.
; =================================================================================================================
PUBLIC caveWs0, caveWs1, caveWs2, caveWs3, caveWs4, caveWs5, caveWsSleep, caveEdKick, caveEdJoin
PUBLIC ws_enable, ws_iatSetEvent, ws_iatWait, ws_set, ws_sleeps, ws_sleepPool, ws_chk, ws_ret
PUBLIC ed_enable, ed_kickFn, ed_joinFn, ed_kicked, ed_skipped
PUBLIC wg_installed
EXTERN WgWake:PROC
EXTERN WgSleep:PROC

wsdata SEGMENT ALIGN(64) READ WRITE 'DATA'
ws_enable       DD 0
ed_enable       DD 0
wg_installed    DD 0            ; WG bookkeeping installed (set once before the WS group is written)
                DD 0
ws_iatSetEvent  DQ 0            ; &IAT slot SetEvent
ws_iatWait      DQ 0            ; &IAT slot WaitForSingleObject
ws_ret          DQ 3 DUP(0)     ; return addresses of the 40800 calls in 3FE14 / 403A0 / 207200
ed_kickFn       DQ 0            ; Nioh3+2F6680 (event set)
ed_joinFn       DQ 0            ; Nioh3+3D7CC8 (event wait)
ALIGN 64
ws_set          DQ 48 DUP(0)    ; real SetEvents per site (6 sites, one 64-byte line each)
ws_sleeps       DQ 0            ; worker sleeps (all pools)
ALIGN 64
ws_sleepPool    DQ 64 DUP(0)    ; worker sleeps per pool id (pool+34h & 63)
ws_chk          DQ 32 DUP(0)    ; wake checks at 40A4C by caller of 40800: 3FE14, 403A0, 207200, other (one line each)
ed_kicked       DQ 0
ed_skipped      DQ 0
ed_skip         DB 64 DUP(0)    ; per Disp worker: kick was skipped this pass -> skip its join too
wsdata ENDS

.code

; WS0..WS5: replace `call [SetEvent]` (FF 15, 6 bytes -> nop; call). Count, then tail-call through the IAT slot.
WS_CAVE MACRO NAME, IDX
NAME PROC
    cmp     dword ptr [ws_enable], 0
    je      @F
    lock inc qword ptr [ws_set + IDX*64]
@@:
    mov     rax, qword ptr [ws_iatSetEvent]
    jmp     qword ptr [rax]
NAME ENDP
ENDM
WS_CAVE caveWs0, 0
WS_CAVE caveWs1, 1
WS_CAVE caveWs4, 4
WS_CAVE caveWs5, 5

; WS2 / WS3 (the two wake-one sites) also carry WG (1.6.0): with WG installed (wg_installed) they tail-call
; WgWake(event, slot) (C++, dllmain.cpp) instead of SetEvent. That is a plain Win64 call in place of a call to
; SetEvent: the same argument register, the same volatile registers clobbered, the callers ignore eax (48C71
; `mov al,dil`, 48D0C `mov al,1`). The target's slot is rsi (48BFF..48C0F) / rbx (48CA4..48CAF).
caveWs2 PROC
    cmp     dword ptr [ws_enable], 0
    je      @F
    lock inc qword ptr [ws_set + 2*64]
@@:
    cmp     dword ptr [wg_installed], 0
    jne     ws2Wg
    mov     rax, qword ptr [ws_iatSetEvent]
    jmp     qword ptr [rax]
ws2Wg:
    mov     rdx, rsi
    jmp     WgWake
caveWs2 ENDP

caveWs3 PROC
    cmp     dword ptr [ws_enable], 0
    je      @F
    lock inc qword ptr [ws_set + 3*64]
@@:
    cmp     dword ptr [wg_installed], 0
    jne     ws3Wg
    mov     rax, qword ptr [ws_iatSetEvent]
    jmp     qword ptr [rax]
ws3Wg:
    mov     rdx, rbx
    jmp     WgWake
caveWs3 ENDP

; WSs: replaces `call [WaitForSingleObject]` in the worker sleep (1A6BC35); rbx = pool, rdi = the worker's slot. Uses
; only volatile registers (rax, r8-r11; WaitForSingleObject has two arguments). With WG installed the wait becomes
; WgSleep(event, timeout, pool, slot) (C++), which marks the worker "waiting" around the real wait (WG bookkeeping);
; the result of the wait is dead at 1A6BC48 (xor eax,eax).
caveWsSleep PROC
    cmp     dword ptr [ws_enable], 0
    je      wsSlGo
    mov     r10d, dword ptr [rbx+34h]
    and     r10d, 63
    lea     r11, ws_sleepPool
    lock inc qword ptr [r11+r10*8]
    lock inc qword ptr [ws_sleeps]
wsSlGo:
    cmp     dword ptr [wg_installed], 0
    jne     wsSlWg
    mov     rax, qword ptr [ws_iatWait]
    jmp     qword ptr [rax]
wsSlWg:
    mov     r8, rbx
    mov     r9, rdi
    jmp     WgSleep
caveWsSleep ENDP

; ED kick: replaces `call 2F6680` (event set) in the EffectDisp kick loop (3D6F46). rcx = obj+8, ebx = worker index.
; If the switch is on and the worker's list ([obj+1A0h] -> vector {begin,end}) is empty, the kick is skipped and
; remembered so the matching join is skipped too. Clobbers rax, rdx, flags (volatile at a call).
caveEdKick PROC
    cmp     ebx, 63
    ja      edKickGo
    lea     rax, ed_skip
    mov     byte ptr [rax+rbx], 0
    cmp     dword ptr [ed_enable], 0
    je      edKickGo
    mov     rdx, qword ptr [rcx+198h]                   ; obj+1A0h
    test    rdx, rdx
    je      edKickGo
    mov     rax, qword ptr [rdx]
    cmp     rax, qword ptr [rdx+8]
    jne     edKickGo
    lea     rax, ed_skip
    mov     byte ptr [rax+rbx], 1
    inc     qword ptr [ed_skipped]
    ret
edKickGo:
    inc     qword ptr [ed_kicked]
    jmp     qword ptr [ed_kickFn]
caveEdKick ENDP

; ED join: replaces `call 3D7CC8` (event wait) in the EffectDisp join loop (3D6F8B). Skips exactly the joins whose kick
; was skipped (independent of the switch, so toggling between kick and join stays matched).
caveEdJoin PROC
    cmp     ebx, 63
    ja      edJoinGo
    lea     rax, ed_skip
    cmp     byte ptr [rax+rbx], 0
    je      edJoinGo
    mov     byte ptr [rax+rbx], 0
    ret
edJoinGo:
    jmp     qword ptr [ed_joinFn]
caveEdJoin ENDP

; =================================================================================================================
; GF (1.4.0): GPU-fence Sleep(0) spins -> SetEventOnCompletion + wait (GfWait, C++). Both sites are the
; `xor ecx,ecx; call [Sleep]` inside a `while (fence->GetCompletedValue() < value) Sleep(0)` loop (8 bytes ->
; call; nop3); the loop's own re-check follows, so returning early only costs one more iteration.
; GfWait(ID3D12Fence** f, UINT64 value). Both functions keep rsp 16-aligned with 20h of home space at the site.
; =================================================================================================================
PUBLIC caveGf1, caveGf2
EXTERN GfWait:PROC

; 7D9E66 in FenceWait 7D9E1C (event == NULL path): rsi = &fence (p1+6D8h), rdi = value
caveGf1 PROC
    mov     rcx, rsi
    mov     rdx, rdi
    jmp     GfWait
caveGf1 ENDP

; 117D9F in the worker fence poll 117D6C (p2 == NULL path): fence = [rbx+18h], value = rdi
caveGf2 PROC
    lea     rcx, [rbx+18h]
    mov     rdx, rdi
    jmp     GfWait
caveGf2 ENDP

; =================================================================================================================
; WL (1.5.0, notes\memory-leaks.md 5.3): RenderFrameBegin calls IDXGISwapChain2::GetFrameLatencyWaitableObject every
; frame (4A5338, vtbl+108h) and never closes the handle (DXGI returns a new one per call). Site `call [rax+108h]`
; (6 bytes) -> call here; nop. Gets the handle exactly as before (rcx = swapchain, rax = its vtable), then closes the
; PREVIOUS frame's handle: its wait loop is over (rbp's only use is 4A534C; rbp is overwritten at 4A5461).
; The same value as last time means the runtime does not duplicate: kept. Switch off = stock (nothing closed).
; =================================================================================================================
PUBLIC caveWL, wl_enable, wl_prev, wl_closed, wl_closeHandle

wldata SEGMENT ALIGN(64) READ WRITE 'DATA'
wl_enable       DD 0
                DD 0
wl_prev         DQ 0
wl_closed       DQ 0
wl_closeHandle  DQ 0            ; kernel32!CloseHandle
wldata ENDS

.code
caveWL PROC FRAME
    sub     rsp, 38h
    .allocstack 38h
    .endprolog
    call    qword ptr [rax+108h]                        ; h = GetFrameLatencyWaitableObject()
    mov     qword ptr [rsp+28h], rax
    mov     rcx, rax
    xchg    rcx, qword ptr [wl_prev]                    ; rcx = the previous frame's handle; store h
    test    rcx, rcx
    jz      wlDone
    cmp     rcx, qword ptr [rsp+28h]
    je      wlDone
    cmp     dword ptr [wl_enable], 0
    je      wlDone
    call    qword ptr [wl_closeHandle]
    lock inc qword ptr [wl_closed]
wlDone:
    mov     rax, qword ptr [rsp+28h]
    add     rsp, 38h
    ret
caveWL ENDP

; =================================================================================================================
; DS (1.6.0, opt-in; notes\cpu-hotspots-ally.md 10): the [App]Game main loop's 1 ms "no new frame packet" retry spin
; `while ((QPC - start) / freq < 0.001f) ;` (1A74BD0..1A74C48). Site 1A74BDA `call [QueryPerformanceCounter]` (6 B ->
; nop; call). rcx = &now, rbx = the spin's start (callee-saved, read only). DsQpc (C++) = QueryPerformanceCounter plus,
; when switched on and enough of the millisecond is left, a high-resolution timer sleep first. The loop's own exit test
; is unchanged. Normal Win64 call semantics (same as the QPC call it replaces).
; =================================================================================================================
PUBLIC caveDs
EXTERN DsQpc:PROC
.code
caveDs PROC
    mov     rdx, rbx
    jmp     DsQpc
caveDs ENDP

; =================================================================================================================
; CP / SPF (1.6.0, opt-in; notes\worker-jobs-opt.md 7, patcher\collprefetch_live.fsx / stageprefetch_live.fsx):
; software prefetch at two worker loop heads. No stores, prefetches never fault, only registers dead at the site
; are written (proofs in the notes). Switch off = the displaced instruction, then back into the game.
; =================================================================================================================
PUBLIC caveCP, cp_enable, cp_resume, caveSPF, spf_enable, spf_resume, spf_gworld

pfdata SEGMENT ALIGN(64) READ WRITE 'DATA'
cp_enable   DD 0
spf_enable  DD 0
cp_resume   DQ 0                ; Nioh3+D8187
spf_resume  DQ 0                ; Nioh3+DB8C6
spf_gworld  DQ 0                ; Nioh3+45B9E58 (address of g_CollisionWorld)
pfdata ENDS

.code
; CP: CollisionObjectUpdate loop-head prefetch. Site D8180 `mov rcx,[r10+150h]` -> jmp; nop2.
; r10 = mgr, r13 = i (stride 5). Writes rax/rcx/flags (dead at D8180); rdx/r8 saved. No stores.
caveCP PROC
    cmp     dword ptr [cp_enable], 0
    je      cpStock
    push    rdx
    push    r8
    mov     rcx, qword ptr [r10+150h]
    mov     rax, qword ptr [r10+158h]
    sub     rax, rcx
    sar     rax, 3
    lea     rdx, [r13+15]
    cmp     rdx, rax
    jae     cpB
    mov     rdx, qword ptr [rcx+rdx*8]
    test    rdx, rdx
    je      cpB
    prefetcht0 byte ptr [rdx+8]
    prefetcht0 byte ptr [rdx+88h]
    prefetcht0 byte ptr [rdx+0C8h]
cpB:
    lea     rdx, [r13+10]
    cmp     rdx, rax
    jae     cpC
    mov     rdx, qword ptr [rcx+rdx*8]
    test    rdx, rdx
    je      cpC
    cmp     byte ptr [rdx+0A5h], 0
    je      cpC
    mov     r8, qword ptr [rdx+88h]
    prefetcht0 byte ptr [r8]
    mov     r8, qword ptr [rdx+0C8h]
    prefetcht0 byte ptr [r8]
    mov     r8, qword ptr [rdx+90h]
    prefetcht0 byte ptr [r8]
cpC:
    lea     rdx, [r13+5]
    cmp     rdx, rax
    jae     cpDone
    mov     rdx, qword ptr [rcx+rdx*8]
    test    rdx, rdx
    je      cpDone
    cmp     byte ptr [rdx+0A5h], 0
    je      cpDone
    mov     r8, qword ptr [rdx+0C8h]
    test    r8, r8
    je      cpDone
    mov     eax, dword ptr [r8]
    mov     r8, qword ptr [rdx+88h]
    test    r8, r8
    je      cpDone
    cmp     eax, 4
    jbe     cpK
    mov     eax, 4
cpK:
    test    eax, eax
    je      cpDone
cpLoop:
    mov     rdx, qword ptr [r8]
    test    rdx, rdx
    je      cpNext
    prefetcht0 byte ptr [rdx+62h]
    prefetcht0 byte ptr [rdx+88h]
    prefetcht0 byte ptr [rdx+0B8h]
cpNext:
    add     r8, 8
    dec     eax
    jne     cpLoop
cpDone:
    pop     r8
    pop     rdx
cpStock:
    mov     rcx, qword ptr [r10+150h]
    jmp     qword ptr [cp_resume]
caveCP ENDP

; SPF: placeable-map loop prefetch. Site DB8BF `mov rax,[rel g_CollisionWorld]` -> jmp; nop2.
; rbx = entry (16 B), r14 = end. Writes rax (original value), rsi, flags (rsi/flags dead at DB8C6). No stores.
caveSPF PROC
    cmp     dword ptr [spf_enable], 0
    je      spfStock
    lea     rsi, [rbx+40h]
    cmp     rsi, r14
    jae     spf2
    mov     rsi, qword ptr [rsi+8]
    test    rsi, rsi
    je      spf2
    prefetcht0 byte ptr [rsi+10h]
    prefetcht0 byte ptr [rsi+70h]
    prefetcht0 byte ptr [rsi+98h]
    prefetcht0 byte ptr [rsi+0C8h]
    prefetcht0 byte ptr [rsi+109h]
spf2:
    lea     rsi, [rbx+20h]
    cmp     rsi, r14
    jae     spfStock
    mov     rsi, qword ptr [rsi+8]
    test    rsi, rsi
    je      spfStock
    mov     rsi, qword ptr [rsi+0B0h]
    prefetcht0 byte ptr [rsi]
spfStock:
    mov     rax, qword ptr [spf_gworld]
    mov     rax, qword ptr [rax]
    jmp     qword ptr [spf_resume]
caveSPF ENDP

; =================================================================================================================
; SPR (1.7.2, measurement only; notes\stutter.md, patcher\stutter_probe_live.fsx): timing wrappers around two calls that
; are only slow when the engine misses a cache. SprG = `call 9290D8 CreateGfxPso` in PsoGetOrCreateGfx 119670 (a runtime
; graphics PSO create on the dispatcher thread, PSO-cache CS held; site 11970A). SprU = `call 9C2EE0 CreateDedicatedUpload`
; in UploadHeapSuballoc 62D688 (upload heap ring full / request larger than a heap; site 62D7E7). Both callees take at most
; four register arguments: rcx/rdx/r8/r9 are kept in the caller's home space around the rdtsc, the callee is called with
; them unchanged and its rax is returned. Counters: calls, total ticks, max ticks, calls >= spr_slow ticks (stats only).
; =================================================================================================================
PUBLIC caveSprG, caveSprU, spr_gfxFn, spr_ufFn, spr_slow, spr_gN, spr_gSum, spr_gMax, spr_gSlow, spr_uN, spr_uSum, spr_uMax, spr_uSlow

sprdata SEGMENT ALIGN(64) READ WRITE 'DATA'
spr_gfxFn   DQ 0                ; Nioh3+9290D8
spr_ufFn    DQ 0                ; Nioh3+9C2EE0
spr_slow    DQ 0                ; ticks of the slow threshold (1 ms)
spr_gN      DQ 0
spr_gSum    DQ 0
spr_gMax    DQ 0
spr_gSlow   DQ 0
spr_uN      DQ 0
spr_uSum    DQ 0
spr_uMax    DQ 0
spr_uSlow   DQ 0
sprdata ENDS

.code
SPRCAVE MACRO nm, fn, cn, sm, mx, sl
LOCAL nomax, noslow
nm PROC FRAME
    sub     rsp, 48h
    .allocstack 48h
    .endprolog
    mov     qword ptr [rsp+50h], rcx
    mov     qword ptr [rsp+58h], rdx
    mov     qword ptr [rsp+60h], r8
    mov     qword ptr [rsp+68h], r9
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     qword ptr [rsp+20h], rax
    mov     rcx, qword ptr [rsp+50h]
    mov     rdx, qword ptr [rsp+58h]
    mov     r8, qword ptr [rsp+60h]
    mov     r9, qword ptr [rsp+68h]
    call    qword ptr [fn]
    mov     qword ptr [rsp+28h], rax
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, qword ptr [rsp+20h]
    lock inc qword ptr [cn]
    lock add qword ptr [sm], rax
    cmp     rax, qword ptr [mx]
    jbe     nomax
    mov     qword ptr [mx], rax
nomax:
    cmp     rax, qword ptr [spr_slow]
    jb      noslow
    lock inc qword ptr [sl]
noslow:
    mov     rax, qword ptr [rsp+28h]
    add     rsp, 48h
    ret
nm ENDP
ENDM

SPRCAVE caveSprG, spr_gfxFn, spr_gN, spr_gSum, spr_gMax, spr_gSlow
SPRCAVE caveSprU, spr_ufFn, spr_uN, spr_uSum, spr_uMax, spr_uSlow

; =================================================================================================================
; 1.7.12: MQ / PT1 / DL (opt-in; notes\streaming-anim.md 4, notes\collision-opt.md; ports of patcher\mq_live.fsx,
; pt1_live.fsx, collbook_live.fsx). Each has a live switch; with the switch off every cave is the displaced stock code.
; =================================================================================================================
PUBLIC mq_count, dl_count
PUBLIC caveMQ, mq_enable, mq_resume, mq_calls
PUBLIC capPt1Acq, capPt1Rel, cavePt1B, cavePt1C, cavePt1D
PUBLIC pt1_enable, pt1_owner, pt1_claims, pt1_saved, pt1_pAcq, pt1_pRel, pt1_ret, pt1_resB, pt1_resC, pt1_resD, pt1_jeD
PUBLIC caveDL, dl_enable, dl_fn, dl_resume, dl_skips

xdata SEGMENT ALIGN(64) READ WRITE 'DATA'
mq_enable   DD 0                ; 1 = on
mq_count    DD 0                ; 1 = count calls (CpuStats)
dl_enable   DD 0                ; 1 = on
dl_count    DD 0                ; 1 = count skips (CpuStats)
pt1_enable  DD 0                ; 1 = new textures claim the pool lock
pt1_owner   DD 0                ; thread id that holds the lock across a texture's touches (0 = none)
pt1_held    DQ 0                ; the SRWLOCK held shared by the owner
pt1_claims  DD 0
pt1_saved   DD 0
mq_resume   DQ 0                ; Nioh3+8878A
mq_calls    DQ 0
dl_fn       DQ 0                ; Nioh3+D8740 (CollisionPartMapFind)
dl_resume   DQ 0                ; Nioh3+D8400
dl_skips    DQ 0
pt1_pAcq    DQ 0                ; AcquireSRWLockShared / ReleaseSRWLockShared, copied from the game's IAT
pt1_pRel    DQ 0
pt1_ret     DQ 0                ; Nioh3+1297FE: return address of the phase-2 call to 114C38
pt1_resB    DQ 0                ; Nioh3+129658
pt1_resC    DQ 0                ; Nioh3+129835
pt1_resD    DQ 0                ; Nioh3+1298BD
pt1_jeD     DQ 0                ; Nioh3+129943
ALIGN 16
mq_one      DD 3F800000h, 3F800000h, 3F800000h, 3F800000h
xdata ENDS

.code
; ---- MQ: MatrixFromScaleQuatTrans 88780 (rcx = out[16], rdx = scale[3], r8 = quat[4], r9 = trans[4]); bit-exact SSE.
; Volatile xmm0-5 only, leaf. Switch off or out overlapping s -> displaced prologue, then the original body.
caveMQ PROC
    cmp     dword ptr [mq_enable], 0
    je      mq_fallback
    cmp     dword ptr [mq_count], 0
    je      mq_nocount
    inc     qword ptr [mq_calls]
mq_nocount:
    lea     rax, [rcx+40h]
    cmp     rdx, rax
    jae     mq_ok
    lea     rax, [rdx+0Ch]
    cmp     rcx, rax
    jb      mq_fallback
mq_ok:
    movups  xmm0, xmmword ptr [r8]
    movaps  xmm5, xmm0
    shufps  xmm5, xmm5, 0FFh
    shufps  xmm0, xmm0, 24h
    movaps  xmm1, xmm0
    addps   xmm1, xmm0
    movaps  xmm2, xmm1
    mulps   xmm2, xmm0
    movaps  xmm3, xmm0
    shufps  xmm3, xmm3, 49h
    movaps  xmm4, xmm1
    mulps   xmm4, xmm3
    mulps   xmm1, xmm5
    shufps  xmm1, xmm1, 92h
    movaps  xmm5, xmm4
    addps   xmm5, xmm1
    subps   xmm4, xmm1
    movaps  xmm0, xmmword ptr [mq_one]
    movaps  xmm3, xmm2
    shufps  xmm3, xmm3, 49h
    subps   xmm0, xmm3
    shufps  xmm2, xmm2, 92h
    subps   xmm0, xmm2
    movq    xmm1, qword ptr [rdx]
    insertps xmm1, dword ptr [rdx+8], 20h
    shufps  xmm1, xmm1, 24h
    mulps   xmm0, xmm1
    mulps   xmm5, xmm1
    shufps  xmm1, xmm1, 49h
    mulps   xmm4, xmm1
    movaps  xmm2, xmm0
    insertps xmm2, xmm5, 18h
    insertps xmm2, xmm4, 0A8h
    movaps  xmm3, xmm4
    insertps xmm3, xmm0, 58h
    insertps xmm3, xmm5, 68h
    insertps xmm0, xmm5, 88h
    insertps xmm0, xmm4, 58h
    movups  xmm1, xmmword ptr [r9]
    insertps xmm1, dword ptr [mq_one], 30h
    movups  xmmword ptr [rcx], xmm2
    movups  xmmword ptr [rcx+10h], xmm3
    movups  xmmword ptr [rcx+20h], xmm0
    movups  xmmword ptr [rcx+30h], xmm1
    mov     rax, rcx
    ret
mq_fallback:
    mov     r11, rsp
    sub     rsp, 88h
    jmp     qword ptr [mq_resume]
caveMQ ENDP

; ---- DL: site D83F4 `call D8740` -> `call caveDL` (rcx = part map, rdx = part, [rsp] = D83F9). On the global-owner
; path (sil != 0, r11 != 0) stock re-searches and overwrites everything this first call sets, so skip it: drop the
; return address and continue at the stock code D8400 (rax = 0, r13 = 0, then the sil / r11 tests).
caveDL PROC
    cmp     dword ptr [dl_enable], 0
    je      dl_stock
    test    sil, sil
    jz      dl_stock
    test    r11, r11
    jz      dl_stock
    cmp     dword ptr [dl_count], 0
    je      dl_skip
    inc     qword ptr [dl_skips]
dl_skip:
    lea     rsp, [rsp+8]
    jmp     qword ptr [dl_resume]
dl_stock:
    jmp     qword ptr [dl_fn]
caveDL ENDP

; ---- PT1: TextureStreamRequestTiles holds the pool SRWLOCK (shared) once per texture instead of per touch.
; capPt1Acq / capPt1Rel replace `call [AcquireSRWLockShared]` (114CA8) / `call [ReleaseSRWLockShared]` (114CEE) in 114C38;
; the gate is "owner thread AND 114C38 called from phase 2" ([rsp+70h] = return address of the call).
capPt1Acq PROC
    mov     eax, dword ptr gs:[48h]
    cmp     eax, dword ptr [pt1_owner]
    jne     pa_stock
    mov     rax, qword ptr [rsp+70h]
    cmp     rax, qword ptr [pt1_ret]
    jne     pa_stock
    mov     rax, qword ptr [pt1_held]
    cmp     rax, rcx
    je      pa_skip
    test    rax, rax
    jz      pa_take
    push    rcx
    sub     rsp, 20h
    mov     rcx, rax
    call    qword ptr [pt1_pRel]
    add     rsp, 20h
    pop     rcx
pa_take:
    mov     qword ptr [pt1_held], rcx
pa_stock:
    jmp     qword ptr [pt1_pAcq]
pa_skip:
    inc     dword ptr [pt1_saved]
    ret
capPt1Acq ENDP

capPt1Rel PROC
    mov     eax, dword ptr gs:[48h]
    cmp     eax, dword ptr [pt1_owner]
    jne     pr_stock
    mov     rax, qword ptr [rsp+70h]
    cmp     rax, qword ptr [pt1_ret]
    jne     pr_stock
    cmp     rcx, qword ptr [pt1_held]
    jne     pr_stock
    ret
pr_stock:
    jmp     qword ptr [pt1_pRel]
capPt1Rel ENDP

; S_B 129651: phase 2 begins (displaced `add [r14+7590h], r10d`; rax dead)
cavePt1B PROC
    test    byte ptr [pt1_enable], 1
    jz      pb_skip
    push    rcx
    mov     ecx, dword ptr gs:[48h]
    xor     eax, eax
    lock cmpxchg dword ptr [pt1_owner], ecx
    jne     pb_no
    inc     dword ptr [pt1_claims]
pb_no:
    pop     rcx
pb_skip:
    add     dword ptr [r14+7590h], r10d
    jmp     qword ptr [pt1_resB]
cavePt1B ENDP

; S_C 12982E: touch failed, before `call [rax+28h]`: release first (all volatile GPRs are dead)
cavePt1C PROC
    mov     eax, dword ptr gs:[48h]
    cmp     eax, dword ptr [pt1_owner]
    jne     pc_out
    mov     rcx, qword ptr [pt1_held]
    test    rcx, rcx
    jz      pc_out
    mov     qword ptr [pt1_held], 0
    sub     rsp, 20h
    call    qword ptr [pt1_pRel]
    add     rsp, 20h
pc_out:
    mov     rax, qword ptr [rsi]
    lea     r8, [rbp+60h]
    jmp     qword ptr [pt1_resC]
cavePt1C ENDP

; S_D 1298B3: phase 2 ends (single join): release a held lock, clear the owner; every volatile GPR preserved
cavePt1D PROC
    push    rax
    push    rcx
    push    rdx
    push    r8
    push    r9
    push    r10
    push    r11
    sub     rsp, 28h
    mov     eax, dword ptr gs:[48h]
    cmp     eax, dword ptr [pt1_owner]
    jne     pd_rest
    mov     rcx, qword ptr [pt1_held]
    mov     qword ptr [pt1_held], 0
    test    rcx, rcx
    jz      pd_clear
    call    qword ptr [pt1_pRel]
pd_clear:
    mov     dword ptr [pt1_owner], 0
pd_rest:
    add     rsp, 28h
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    pop     rax
    cmp     byte ptr [rbp+0Ch], 0
    jne     pd_ne
    jmp     qword ptr [pt1_jeD]
pd_ne:
    jmp     qword ptr [pt1_resD]
cavePt1D ENDP

; =================================================================================================================
; 1.7.14: GL1 (software prefetch in the mission task-list walks), LW (loader managers wait on an event instead of
; Sleep(1)). Both opt-in ([Patches] GL1 / LW, default 0); LC (in-flight cap 16) is a plain byte patch, no cave.
; notes\gamelogic.md GL1, notes\loading.md section 3 rows 4-5.
; =================================================================================================================
PUBLIC caveGl1A, caveGl1B, gl1_resA, gl1_endA, gl1_resB, gl1_endB
PUBLIC caveLdWait, caveLdSig, lw_event, lw_pWait, lw_pSleep, lw_pSet, lw_resSig, lw_waiting, lw_signals

gldata SEGMENT ALIGN(64) READ WRITE 'DATA'
gl1_resA    DQ 0                ; Nioh3+E266DB (mov rdi,[rbx+8])
gl1_endA    DQ 0                ; the je target of E266D5 (loop end)
gl1_resB    DQ 0                ; Nioh3+E26695 (mov rcx,[rbx+8])
gl1_endB    DQ 0                ; Nioh3+E266A4 (loop end)
lw_event    DQ 0                ; auto-reset event (0 -> the wait cave falls back to Sleep)
lw_pWait    DQ 0                ; WaitForSingleObject
lw_pSleep   DQ 0                ; Sleep
lw_pSet     DQ 0                ; SetEvent
lw_resSig   DQ 0                ; Nioh3+1343D3D
lw_waiting  DD 0                ; managers currently inside the wait (the signal only costs a syscall when > 0)
lw_signals  DD 0                ; SetEvent calls issued (stats)
gldata ENDS

.code
; ---- GL1 A: E266D1 `cmp rbx,[rbp]` / `je end` (10 bytes) of MissionTaskListProximityPass' loop. rbx = entry (16-byte
; {?, task*}), rbp = &last. Prefetch the task of entry i+3 (3 lines: +40h/+80h/+C0h hold +50h, +97h, +D9h, +F0h..FAh)
; when it lies inside [first,last). r10 is never used by this function. No architectural effect.
caveGl1A PROC
    cmp     rbx, qword ptr [rbp]
    je      ga_end
    lea     r10, [rbx+30h]
    cmp     r10, qword ptr [rbp]
    jae     ga_go
    mov     r10, qword ptr [r10+8]
    prefetcht0 byte ptr [r10+40h]
    prefetcht0 byte ptr [r10+80h]
    prefetcht0 byte ptr [r10+0C0h]
ga_go:
    jmp     qword ptr [gl1_resA]
ga_end:
    jmp     qword ptr [gl1_endA]
caveGl1A ENDP

; ---- GL1 B: E26690 `cmp rbx,[rdi]` / `je end` (5 bytes) of the E26680 loop that calls 2866F0 per entry (rbx = entry,
; rdi = &last). Same prefetch.
caveGl1B PROC
    cmp     rbx, qword ptr [rdi]
    je      gb_end
    lea     r10, [rbx+30h]
    cmp     r10, qword ptr [rdi]
    jae     gb_go
    mov     r10, qword ptr [r10+8]
    prefetcht0 byte ptr [r10+40h]
    prefetcht0 byte ptr [r10+80h]
    prefetcht0 byte ptr [r10+0C0h]
gb_go:
    jmp     qword ptr [gl1_resB]
gb_end:
    jmp     qword ptr [gl1_endB]
caveGl1B ENDP

; ---- LW wait: replaces `call [Sleep]` (ecx = 1) at 1FEE68 / 1FF18C / 1FF371 (ResourceLoadIdlePoll managers A and B).
; WaitForSingleObject(event, 1 ms): the same 1 ms upper bound, but a finished ResourceStepJob wakes it at once.
caveLdWait PROC
    mov     edx, ecx
    mov     rcx, qword ptr [lw_event]
    test    rcx, rcx
    jz      lw_stock
    sub     rsp, 28h
    lock inc dword ptr [lw_waiting]
    call    qword ptr [lw_pWait]
    lock dec dword ptr [lw_waiting]
    add     rsp, 28h
    ret
lw_stock:
    mov     ecx, edx
    jmp     qword ptr [lw_pSleep]
caveLdWait ENDP

; ---- LW signal: 1343D35 in ResourceStepJob_Run (rbx = job), displaced `or [rbx+24h],1` / `mov rax,[rbx+38h]`.
; Only signals while a manager is waiting. rsp is 16-aligned here (jmp from the job body, not a call).
caveLdSig PROC
    or      dword ptr [rbx+24h], 1
    cmp     dword ptr [lw_waiting], 0
    je      ls_out
    mov     rcx, qword ptr [lw_event]
    sub     rsp, 20h
    call    qword ptr [lw_pSet]
    add     rsp, 20h
    lock inc dword ptr [lw_signals]
ls_out:
    mov     rax, qword ptr [rbx+38h]
    jmp     qword ptr [lw_resSig]
caveLdSig ENDP

; =================================================================================================================
; 1.7.40: BPO - BoundsFromPartObbs 71C60 (logic thread; notes\bounds-partobbs.md). 1:1 port of the Iced caves in
; re\scripts_logic\bounds_offline.fsx (offline: whole 71C60 stock vs patched bit-identical, exhaustive half sweep,
; seqlock stress, liveness of everything the caves leave different). bpo_enable = live switch (menu, 0/1); bpo_mask
; picks the parts: 1 Q quaternion memo, 2 L plain ready-word loads, 4 V forwarding-friendly Vec4CopyW0.
; Switch off (or the part's bit clear) = the displaced original instructions.
; =================================================================================================================
PUBLIC caveBpoQA, caveBpoQB, caveBpoL1, caveBpoL2, caveBpoV
PUBLIC bpo_enable, bpo_mask, bpo_imask, bpo_tbl, bpo_resQS, bpo_resQH, bpo_resQB, bpo_resL1, bpo_resL2, bpo_vStock

bpodata SEGMENT ALIGN(64) READ WRITE 'DATA'
bpo_enable  DD 0                ; live switch: 0 = every site runs its original instructions
bpo_mask    DD 7                ; 1 Q, 2 L, 4 V ([Tuning] BPO_Mask)
bpo_imask   DQ 0                ; quaternion table: entries - 1
bpo_tbl     DQ 0                ; 32-byte entries { u32 seq (even = stable, 0 = empty), u32 MXCSR & E040h, u64 key, f32 q[4] }
bpo_one     DD 3F800000h, 0     ; 1.0f (stock xmm1 at 72027)
bpo_resQS   DQ 0                ; Nioh3+71F04 (after the displaced movzx / mov)
bpo_resQH   DQ 0                ; Nioh3+7206F (hit: quaternion done, test rax,rax)
bpo_resQB   DQ 0                ; Nioh3+7206F (after the displaced 4 stores)
bpo_resL1   DQ 0                ; Nioh3+71DB7
bpo_resL2   DQ 0                ; Nioh3+1A7AE6
bpo_vStock  DQ 0                ; Nioh3+174610 Vec4CopyW0
bpodata ENDS

; key (4 halves) -> r9 bits: one unpacked float (stock: sign<<16 | mant<<13 | (exp ? (exp+70h)<<23 : 0)) back to its half
BPO_REPACK MACRO SRC
    mov     ecx, SRC
    mov     r8d, ecx
    shr     r8d, 16
    and     r8d, 8000h
    mov     edx, ecx
    shr     edx, 13
    and     edx, 3FFh
    or      r8d, edx
    shr     ecx, 23
    and     ecx, 0FFh
    lea     edx, [rcx-112]
    shl     edx, 10
    test    ecx, ecx
    cmovz   edx, ecx
    or      r8d, edx
    or      r9, r8
ENDM

; rcx = &table[((key * K) >> 32) & imask]
BPO_HASH MACRO KEY
    mov     rcx, KEY
    mov     rdx, 9E3779B97F4A7C15h
    imul    rcx, rdx
    shr     rcx, 32
    and     rcx, qword ptr [bpo_imask]
    shl     rcx, 5
    add     rcx, qword ptr [bpo_tbl]
ENDM

.code
; ---- QA @71EFE (displaced: movzx eax,word ptr [rbx+6] / mov ecx,eax). rbx = part+4, r14 = obj, rsp = 71C60's frame.
; Hit: the quaternion of the part's 4 halves (rbx+4) from the table into xmm11/xmm12/xmm10/xmm9 and [rsp+40h] exactly as
; the stock code leaves them at 7206F, plus xmm1 = 1.0 and rax = [r14+3F8h]. Miss / off: the stock code from 71F04.
caveBpoQA PROC
    cmp     dword ptr [bpo_enable], 0
    je      bpoqa_stock
    test    byte ptr [bpo_mask], 1
    jz      bpoqa_stock
    mov     rax, qword ptr [rbx+4]
    stmxcsr dword ptr [rsp+30h]             ; FTZ / RC / DAZ change the stock result (half denormals unpack to float
    mov     r8d, dword ptr [rsp+30h]        ; denormals): an entry only serves the MXCSR it was made under.
    and     r8d, 0E040h                     ; [rsp+30h] is rewritten by the stock code (71FA2) before any read
    BPO_HASH rax
    mov     edx, dword ptr [rcx]            ; seq 1
    test    dl, 1
    jnz     bpoqa_stock
    test    edx, edx
    jz      bpoqa_stock
    cmp     rax, qword ptr [rcx+8]
    jne     bpoqa_stock
    cmp     r8d, dword ptr [rcx+4]
    jne     bpoqa_stock
    movups  xmm0, xmmword ptr [rcx+10h]
    movss   xmm11, dword ptr [rcx+10h]
    movss   xmm12, dword ptr [rcx+14h]
    movss   xmm10, dword ptr [rcx+18h]
    movss   xmm9, dword ptr [rcx+1Ch]
    cmp     edx, dword ptr [rcx]            ; seq 2 (x86 keeps loads in order)
    jne     bpoqa_stock
    movups  xmmword ptr [rsp+40h], xmm0
    movss   xmm1, dword ptr [bpo_one]
    mov     rax, qword ptr [r14+3F8h]
    jmp     qword ptr [bpo_resQH]
bpoqa_stock:
    movzx   eax, word ptr [rbx+6]
    mov     ecx, eax
    jmp     qword ptr [bpo_resQS]
caveBpoQA ENDP

; ---- QB @72053 (displaced: the 4 quaternion stores 72053..7206E). Inserts the stock result, keyed by the halves
; re-packed from the 4 unpacked floats the stock code used ([rbp-20h] x, [rbp-1Ch] y, [rbp-18h] z, [rsp+30h] angle).
; Seqlock writer: odd seq while writing; a busy or already-filled entry is left alone. rax (= [r14+3F8h]) preserved.
caveBpoQB PROC
    movss   dword ptr [rsp+40h], xmm11
    movss   dword ptr [rsp+44h], xmm12
    movss   dword ptr [rsp+4Ch], xmm9
    movss   dword ptr [rsp+48h], xmm10
    cmp     dword ptr [bpo_enable], 0
    je      bpoqb_done
    test    byte ptr [bpo_mask], 1
    jz      bpoqb_done
    xor     r9d, r9d
    BPO_REPACK dword ptr [rsp+30h]
    shl     r9, 16
    BPO_REPACK dword ptr [rbp-18h]
    shl     r9, 16
    BPO_REPACK dword ptr [rbp-1Ch]
    shl     r9, 16
    BPO_REPACK dword ptr [rbp-20h]
    BPO_HASH r9
    mov     r8d, dword ptr [rcx]
    test    r8b, 1
    jnz     bpoqb_done
    test    r8d, r8d
    jz      bpoqb_try
    cmp     r9, qword ptr [rcx+8]
    je      bpoqb_done
bpoqb_try:
    lea     r11d, [r8+1]
    push    rax
    mov     eax, r8d
    lock cmpxchg dword ptr [rcx], r11d
    pop     rax
    jnz     bpoqb_done
    stmxcsr dword ptr [rbp-30h]             ; [rbp-30h] (SinCos sine) is dead after 71FC1
    mov     edx, dword ptr [rbp-30h]
    and     edx, 0E040h
    mov     dword ptr [rcx+4], edx
    mov     qword ptr [rcx+8], r9
    movss   dword ptr [rcx+10h], xmm11
    movss   dword ptr [rcx+14h], xmm12
    movss   dword ptr [rcx+18h], xmm10
    movss   dword ptr [rcx+1Ch], xmm9
    add     r11d, 1
    mov     dword ptr [rcx], r11d
bpoqb_done:
    jmp     qword ptr [bpo_resQB]
caveBpoQB ENDP

; ---- L1 @71DB2 (displaced: lock cmpxchg [rcx],r10d with eax = r10d = 0, i.e. an atomic read of the ready word)
caveBpoL1 PROC
    cmp     dword ptr [bpo_enable], 0
    je      bpol1_stock
    test    byte ptr [bpo_mask], 2
    jz      bpol1_stock
    mov     eax, dword ptr [rcx]
    jmp     qword ptr [bpo_resL1]
bpol1_stock:
    lock cmpxchg dword ptr [rcx], r10d
    jmp     qword ptr [bpo_resL1]
caveBpoL1 ENDP

; ---- L2 @1A7AE0 in ObjResTreeAllReady 1A7AAC (displaced: xor eax,eax / lock cmpxchg [rdx],ebx with ebx = 0)
caveBpoL2 PROC
    xor     eax, eax
    cmp     dword ptr [bpo_enable], 0
    je      bpol2_stock
    test    byte ptr [bpo_mask], 2
    jz      bpol2_stock
    mov     eax, dword ptr [rdx]
    jmp     qword ptr [bpo_resL2]
bpol2_stock:
    lock cmpxchg dword ptr [rdx], ebx
    jmp     qword ptr [bpo_resL2]
caveBpoL2 ENDP

; ---- V: called instead of Vec4CopyW0 174610 at its 6 call sites in 71C60 (dst rcx, src rdx; returns rax = dst,
; xmm0 = src as loaded). 4-byte loads forward from the 4-byte stores that just wrote src; one 16-byte store (w = 0)
; forwards to ObbToAabb's 16-byte loads. Same memory, rax, xmm0; only flags differ (dead at all 6 return sites).
caveBpoV PROC
    cmp     dword ptr [bpo_enable], 0
    je      bpov_stock
    test    byte ptr [bpo_mask], 4
    jz      bpov_stock
    movss   xmm0, dword ptr [rdx]
    insertps xmm0, dword ptr [rdx+4], 10h
    insertps xmm0, dword ptr [rdx+8], 20h
    movups  xmmword ptr [rcx], xmm0
    insertps xmm0, dword ptr [rdx+0Ch], 30h
    mov     rax, rcx
    ret
bpov_stock:
    jmp     qword ptr [bpo_vStock]
caveBpoV ENDP

; =================================================================================================================
; 1.7.44 (branch fgproton) - FSR frame generation under Proton (notes\fg-proton-fix.md)
; FGC: the game's FidelityFX backend (KTGL) copy-job executor 3718DB4 calls KtglTextureNative 4548C4(tex, 0) on the
;      source and destination textures WITHOUT the null check its clear (37187EE) and compute (371899D) executors have.
;      When an FG internal texture was not created (CreateResource 3715CCC swallows a failed create and returns FFX_OK
;      with a null entry), the first frame-interpolation dispatch reads [0+90h] -> 0xC0000005 on [App]Game: the crash
;      in vkd3d-proton issue 2845 (Nioh3+BA2AC <- 356DD58 <- 356D0C6 <- FI dispatch 35DC9EE in that build = 4548C4 /
;      3718DFC / 371816A / 3789A76 in v2.02). The caves return 0 for a null texture (what 4548C4 returns for a texture
;      without a native resource); 3718DFF then skips the copy. Non-null: tail jump to 4548C4, bit-identical.
;      fgc_src counts every copy job = one per FI dispatch = one per real frame while FG interpolates.
; FGP: replaces the call of the menu-open getter 5A37D4 in ApplyGraphicsSettings (39A865). fgp_pause = 1 answers
;      "menu open" -> the engine's own FG pause (RenderConfig S+2055h = 1): FSR FG stops interpolating without a
;      swapchain rebuild. fgp_pause = 0: tail jump to 5A37D4 (stock).
; =================================================================================================================
PUBLIC caveFgcSrc, caveFgcDst, caveFgp
PUBLIC fgc_target, fgc_src, fgc_srcNull, fgc_dstNull, fgp_target, fgp_pause, fgp_hits

fgdata SEGMENT ALIGN(64) READ WRITE 'DATA'
fgc_target  DQ 0                ; Nioh3+4548C4 KtglTextureNative
fgp_target  DQ 0                ; Nioh3+5A37D4 menu-open getter
fgc_src     DD 0                ; copy jobs executed (source lookups)
fgc_srcNull DD 0                ; copy jobs whose source texture was null (skipped instead of crashing)
fgc_dstNull DD 0                ; copy jobs whose destination texture was null (skipped instead of crashing)
fgp_pause   DD 0                ; live switch: 1 = FSR FG paused by the mod
fgp_hits    DD 0                ; frames answered "paused" by the cave
fgdata ENDS

.code
caveFgcSrc PROC
    lock inc dword ptr [fgc_src]
    test    rcx, rcx
    jz      fgcSrcNull
    jmp     qword ptr [fgc_target]
fgcSrcNull:
    lock inc dword ptr [fgc_srcNull]
    xor     eax, eax
    ret
caveFgcSrc ENDP

caveFgcDst PROC
    test    rcx, rcx
    jz      fgcDstNull
    jmp     qword ptr [fgc_target]
fgcDstNull:
    lock inc dword ptr [fgc_dstNull]
    xor     eax, eax
    ret
caveFgcDst ENDP

caveFgp PROC
    cmp     dword ptr [fgp_pause], 0
    je      fgpStock
    lock inc dword ptr [fgp_hits]
    mov     eax, 1
    ret
fgpStock:
    jmp     qword ptr [fgp_target]
caveFgp ENDP

END