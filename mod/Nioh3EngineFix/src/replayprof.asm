; replayprof.asm - RPF replay-thread profiler thunks (replayprof.cpp, notes\renderer\replay-profile.md). MEASUREMENT ONLY.
;
; 1. Token thunks rp_tok_I (I = 0..184): the KTGL token handler table entry I points here while RPF is on.
;      mov r8d, I ; jmp RpTok           (RpTok(ctx, cursor, op) in replayprof.cpp times the original handler)
;    r8 is volatile and not an argument of a handler (void h(SynCtx*, u8**)); the replay loops reload it every iteration.
;
; 2. D3D12 method thunks rp_t_ID (ID = set * 128 + slot): vtable slot `slot` of hooked vtable `set` points here.
;      mov r11d, ID ; jmp rp_check
;    rp_check (leaf, rsp untouched): unless the caller is the replay thread (TEB ClientId.UniqueThread == rp_tid), RPF is on
;    (rp_on) and no hooked method is already running on it (rp_busy), it tail-jumps to rp_orig[ID] with every register and
;    the stack exactly as at the method entry. Otherwise rp_body (full x64 unwind info) copies 8 stack arguments (the
;    largest D3D12 method used here has 7), reads the TSC, calls the original, reads the TSC again and adds the ticks to
;    rp_id[ID] {ticks, calls} and to rp_mat[rp_curOp][rp_grp[ID]] {ticks, calls} (rp_curOp = the token handler running now,
;    185 = none). The return value (rax) is passed back; xmm registers are not touched.

OPTION CASEMAP:NONE

EXTERN RpTok:PROC

PUBLIC rp_pk, rp_psoRed, rp_orig, rp_grp, rp_id, rp_mat, rp_curOp, rp_tid, rp_busy, rp_on, rp_thunks, rp_tokthunks

RP_SETS   EQU 10
RP_SLOTS  EQU 128
RP_IDS    EQU RP_SETS * RP_SLOTS
RP_GROUPS EQU 24
RP_OPS    EQU 186
RP_TOKENS EQU 185

.data
ALIGN 16
rp_tid    dd 0                          ; replay thread id (0 = not learned yet: everything passes through)
rp_curOp  dd 185                        ; token handler running on the replay thread (185 = none)
rp_busy   db 0                          ; 1 while a hooked method runs on the replay thread (nested calls pass through)
rp_on     db 0                          ; master switch
ALIGN 16
rp_orig   dq RP_IDS dup(0)              ; original method per ID
rp_id     dq RP_IDS * 2 dup(0)          ; {ticks, calls} per ID (replay thread only)
rp_mat    dq RP_OPS * RP_GROUPS * 2 dup(0)  ; {ticks, calls} per (token handler, call group)
rp_grp    db RP_IDS dup(0)              ; call group per ID
rp_pk     db RP_IDS dup(0)              ; 1 = list SetPipelineState, 2 = list Reset / ClearState
ALIGN 16
rp_psoList dq 0                         ; list of the last SetPipelineState (0 after a Reset / ClearState)
rp_psoLast dq 0                         ; its PSO
rp_psoRed  dq 2 dup(0)                  ; {ticks, calls} of redundant SetPipelineState (same list, same PSO)

.code

rp_body PROC FRAME
    sub     rsp, 88h                    ; entry rsp = 8 mod 16 -> 0 mod 16 for the call below
    .allocstack 88h
    .endprolog
    ; frame: [rsp+0..1Fh] home area of the call, [rsp+20h..5Fh] stack args 5..12, [rsp+60h] ID, [rsp+68h] t0, [rsp+70h] rax,
    ;        [rsp+78h] 1 = redundant SetPipelineState
    mov     qword ptr [rsp+60h], r11
    ; SetPipelineState redundancy (rp_pk[ID]: 1 = SetPipelineState, 2 = Reset / ClearState): same list (rcx) and same PSO (rdx)
    ; as the last SetPipelineState, with no Reset / ClearState of a list in between
    mov     qword ptr [rsp+78h], 0
    lea     rax, rp_pk
    movzx   eax, byte ptr [rax + r11]
    test    eax, eax
    jz      pk_done
    cmp     eax, 1
    jne     pk_reset
    cmp     rcx, qword ptr [rp_psoList]
    jne     pk_new
    cmp     rdx, qword ptr [rp_psoLast]
    jne     pk_new
    mov     qword ptr [rsp+78h], 1
    jmp     pk_done
pk_new:
    mov     qword ptr [rp_psoList], rcx
    mov     qword ptr [rp_psoLast], rdx
    jmp     pk_done
pk_reset:
    mov     qword ptr [rp_psoList], 0
pk_done:
    mov     rax, qword ptr [rsp+0B0h]   ; entry rsp + 28h = caller's 5th argument
    mov     qword ptr [rsp+20h], rax
    mov     rax, qword ptr [rsp+0B8h]
    mov     qword ptr [rsp+28h], rax
    mov     rax, qword ptr [rsp+0C0h]
    mov     qword ptr [rsp+30h], rax
    mov     rax, qword ptr [rsp+0C8h]
    mov     qword ptr [rsp+38h], rax
    mov     rax, qword ptr [rsp+0D0h]
    mov     qword ptr [rsp+40h], rax
    mov     rax, qword ptr [rsp+0D8h]
    mov     qword ptr [rsp+48h], rax
    mov     rax, qword ptr [rsp+0E0h]
    mov     qword ptr [rsp+50h], rax
    mov     rax, qword ptr [rsp+0E8h]
    mov     qword ptr [rsp+58h], rax
    mov     r10, rdx                    ; rdtsc writes edx
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     qword ptr [rsp+68h], rax
    mov     byte ptr [rp_busy], 1
    mov     rdx, r10
    lea     rax, rp_orig
    call    qword ptr [rax + r11*8]
    mov     qword ptr [rsp+70h], rax
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, qword ptr [rsp+68h]    ; ticks
    mov     r11, qword ptr [rsp+60h]
    mov     r10, r11
    shl     r10, 4
    lea     rdx, rp_id
    add     qword ptr [rdx + r10], rax
    add     qword ptr [rdx + r10 + 8], 1
    lea     rdx, rp_grp
    movzx   r10d, byte ptr [rdx + r11]
    mov     ecx, dword ptr [rp_curOp]
    imul    ecx, ecx, RP_GROUPS
    add     r10d, ecx
    shl     r10, 4
    lea     rdx, rp_mat
    add     qword ptr [rdx + r10], rax
    add     qword ptr [rdx + r10 + 8], 1
    cmp     qword ptr [rsp+78h], 0
    je      no_red
    add     qword ptr [rp_psoRed], rax
    add     qword ptr [rp_psoRed + 8], 1
no_red:
    mov     byte ptr [rp_busy], 0
    mov     rax, qword ptr [rsp+70h]
    add     rsp, 88h
    ret
rp_body ENDP

rp_thunk_code PROC
rp_check LABEL PROC
    mov     eax, dword ptr gs:[48h]     ; TEB ClientId.UniqueThread
    cmp     eax, dword ptr [rp_tid]
    jne     rp_pass
    cmp     byte ptr [rp_on], 0
    je      rp_pass
    cmp     byte ptr [rp_busy], 0
    jne     rp_pass
    jmp     rp_body
rp_pass:
    lea     rax, rp_orig
    jmp     qword ptr [rax + r11*8]
rp_thunk_code ENDP

THUNK MACRO I
rp_t_&I LABEL PROC
    mov     r11d, I
    jmp     rp_check
ENDM

TOKTHUNK MACRO I
rp_tok_&I LABEL PROC
    mov     r8d, I
    jmp     RpTok
ENDM

rp_thunks_code PROC
cnt = 0
REPT RP_IDS
    THUNK %cnt
    cnt = cnt + 1
ENDM
cnt = 0
REPT RP_TOKENS
    TOKTHUNK %cnt
    cnt = cnt + 1
ENDM
    ret
rp_thunks_code ENDP

TENTRY MACRO I
    dq rp_t_&I
ENDM
TOKENTRY MACRO I
    dq rp_tok_&I
ENDM

.data
ALIGN 8
rp_thunks LABEL QWORD
cnt = 0
REPT RP_IDS
    TENTRY %cnt
    cnt = cnt + 1
ENDM
rp_tokthunks LABEL QWORD
cnt = 0
REPT RP_TOKENS
    TOKENTRY %cnt
    cnt = cnt + 1
ENDM

END
