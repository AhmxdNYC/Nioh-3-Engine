; mpcs_harness.asm - runs the real MPCS caves (src\mpcskip.asm) the way MeshletPreculling 5301FC reaches them: one function frame per
; invocation (rsp constant between the sites, as in the engine), rbx = cnt, rsi = shader object ([rsi] = its vtable), rdi = token
; context (+3C40h = write pointer). Site A (end of the #23 block) or site B (#23 bind failed) is entered by jmp, the cave jumps to one
; of two labels of this frame (stock #28 bind / skip to #24); then, when r12 is set, site C (#26 block) is entered by jmp and the cave
; jumps to stock #26 bind / skip to #27. At each landing the harness checks what the stock code there needs: rax = [rsi] and edx =
; 0Eh / 0Bh on the stock paths, rbx / rsi / rdi / rsp unchanged everywhere.
OPTION CASEMAP:NONE
EXTERN caveMpcsA:PROC, caveMpcsB:PROC, caveMpcsC:PROC
EXTERN mpcs_resB:QWORD, mpcs_skipB:QWORD, mpcs_resC:QWORD, mpcs_skipC:QWORD
PUBLIC MpcsInvoke, mph_obj, mph_ctx, mph_tok

.data
ALIGN 16
mph_obj   DQ 1122334455667788h          ; [rsi]
mph_ctx   DB 3C40h DUP (0)
mph_tok   DQ 0                          ; ctx+3C40h
mph_rsp   DQ 0
mph_bits  DD 0
mph_bad   DD 0

.code
; uint32_t MpcsInvoke(uint32_t cnt, uint32_t unused, uint32_t reachC, uint32_t depth); mph_bindA = 1: enter at site A (#23 bound)
; returns bits: 1 stock #28, 2 skip #28-#31, 4 stock #26, 8 skip #26, 0x100 = a register check failed; depth = extra stack (16 B units)
MpcsInvoke PROC FRAME
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
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    mov     r13, rsp                    ; frame base (restored at the end)
    shl     r9, 4
    sub     rsp, r9                     ; vary the invocation's rsp (other stack depth = other mark slot / key)
    and     rsp, -16
    mov     ebx, ecx
    lea     rsi, mph_obj
    lea     rdi, mph_ctx
    mov     r12d, r8d
    mov     dword ptr [mph_bits], 0
    mov     dword ptr [mph_bad], 0
    mov     qword ptr [mph_rsp], rsp
    lea     rax, L_r28
    mov     qword ptr [mpcs_resB], rax
    lea     rax, L_s28
    mov     qword ptr [mpcs_skipB], rax
    lea     rax, L_r26
    mov     qword ptr [mpcs_resC], rax
    lea     rax, L_s26
    mov     qword ptr [mpcs_skipC], rax
    mov     dword ptr [mph_cnt], ebx
    mov     rax, 0BADBADBADh            ; garbage in the volatile registers the caves may use
    mov     rcx, rax
    mov     rdx, rax
    cmp     dword ptr [mph_bindA], 0
    je      L_B
    jmp     caveMpcsA
L_B:
    jmp     caveMpcsB

L_r28:
    or      dword ptr [mph_bits], 1
    cmp     rax, qword ptr [mph_obj]
    jne     L_bad1
    cmp     edx, 0Eh
    jne     L_bad1
    jmp     L_tail
L_s28:
    or      dword ptr [mph_bits], 2
L_tail:
    call    Check
    test    r12d, r12d
    jz      L_end
    mov     rax, 0BADBADBADh
    mov     rdx, rax
    jmp     caveMpcsC
L_r26:
    or      dword ptr [mph_bits], 4
    cmp     rax, qword ptr [mph_obj]
    jne     L_bad2
    cmp     edx, 0Bh
    jne     L_bad2
    jmp     L_end
L_s26:
    or      dword ptr [mph_bits], 8
L_end:
    call    Check
    mov     eax, dword ptr [mph_bits]
    or      eax, dword ptr [mph_bad]
    mov     rsp, r13
    add     rsp, 40h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
L_bad1:
    mov     dword ptr [mph_bad], 100h
    jmp     L_tail
L_bad2:
    mov     dword ptr [mph_bad], 100h
    jmp     L_end
MpcsInvoke ENDP

; rbx / rsi / rdi / rsp of the frame must be untouched by the caves (call pushes 8: compare rsp + 8)
Check PROC
    lea     rax, [rsp+8]
    cmp     rax, qword ptr [mph_rsp]
    jne     ck_bad
    lea     rax, mph_obj
    cmp     rsi, rax
    jne     ck_bad
    lea     rax, mph_ctx
    cmp     rdi, rax
    jne     ck_bad
    cmp     ebx, dword ptr [mph_cnt]
    jne     ck_bad
    ret
ck_bad:
    mov     dword ptr [mph_bad], 100h
    ret
Check ENDP

.data
PUBLIC mph_bindA
mph_bindA DD 0
mph_cnt   DD 0
END
