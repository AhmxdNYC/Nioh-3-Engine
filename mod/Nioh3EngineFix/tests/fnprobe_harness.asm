; fnprobe_harness.asm - register-exact runner for the FnProbe entry thunk (src\fnprobe_core.h, tests\fnprobe_harness.cpp).
; RunProbe(fn, in): saves the host's non-volatiles, loads xmm0-5, the flags and rax rcx rdx r8-r11 from `in`, records the
; entry rsp, and CALLs fn (the thunk). The thunk runs the relocated prologue of TargetA/B/C/D and jumps to its body; the
; body stores every volatile register, the flags and rsp into p_out before changing anything, undoes its prologue, returns.
; Regs layout (8-byte slots): rax rcx rdx r8 r9 r10 r11 rflags, then xmm0..xmm5 (16 B each) = 160 B, then rsp at body.
; Clobber: writes junk into every volatile register, xmm0-5 and the flags (called by the C++ hook).
OPTION CASEMAP:NONE
PUBLIC RunProbe, Clobber, TargetA, TargetB, TargetC, TargetD, p_in, p_out, p_entry_rsp

pdata SEGMENT ALIGN(64) READ WRITE 'DATA'
p_in        DB 160 DUP(0)
p_out       DB 176 DUP(0)
p_entry_rsp DQ 0
pdata ENDS

.code
STORE_OUT MACRO
    mov     qword ptr [p_out+00h], rax
    mov     qword ptr [p_out+08h], rcx
    mov     qword ptr [p_out+10h], rdx
    mov     qword ptr [p_out+18h], r8
    mov     qword ptr [p_out+20h], r9
    mov     qword ptr [p_out+28h], r10
    mov     qword ptr [p_out+30h], r11
    pushfq
    pop     qword ptr [p_out+38h]
    movdqu  xmmword ptr [p_out+40h], xmm0
    movdqu  xmmword ptr [p_out+50h], xmm1
    movdqu  xmmword ptr [p_out+60h], xmm2
    movdqu  xmmword ptr [p_out+70h], xmm3
    movdqu  xmmword ptr [p_out+80h], xmm4
    movdqu  xmmword ptr [p_out+90h], xmm5
    mov     qword ptr [p_out+0A0h], rsp
ENDM

; A: mov [rsp+8],rbx (5 B, the FP1-type prologue)
TargetA PROC
    DB 48h, 89h, 5Ch, 24h, 08h
    STORE_OUT
    mov     rbx, qword ptr [rsp+8]
    ret
TargetA ENDP

; B: push rbp; push rbx; push rsi; push rdi (REX push + 3 pushes = 5 B, the 471E68 / 23BF1C prologue)
TargetB PROC
    DB 40h, 55h, 53h, 56h, 57h
    STORE_OUT
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
TargetB ENDP

; C: mov rax,rsp; mov [rax+8],rbx (7 B, the 59F498-type prologue; rax = entry rsp afterwards)
TargetC PROC
    DB 48h, 8Bh, 0C4h, 48h, 89h, 58h, 08h
    STORE_OUT
    mov     rbx, qword ptr [rsp+8]
    ret
TargetC ENDP

; D: push rbx (REX); sub rsp,40h (6 B, the 7D48D4 prologue; writes the flags itself)
TargetD PROC
    DB 40h, 53h, 48h, 83h, 0ECh, 40h
    STORE_OUT
    add     rsp, 40h
    pop     rbx
    ret
TargetD ENDP

RunProbe PROC
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 28h
    mov     r12, rcx
    mov     r13, rdx
    movdqu  xmm0, xmmword ptr [r13+40h]
    movdqu  xmm1, xmmword ptr [r13+50h]
    movdqu  xmm2, xmmword ptr [r13+60h]
    movdqu  xmm3, xmmword ptr [r13+70h]
    movdqu  xmm4, xmmword ptr [r13+80h]
    movdqu  xmm5, xmmword ptr [r13+90h]
    lea     rax, [rsp-8]
    mov     qword ptr [p_entry_rsp], rax
    push    qword ptr [r13+38h]
    popfq
    mov     rax, qword ptr [r13+00h]
    mov     rcx, qword ptr [r13+08h]
    mov     r8,  qword ptr [r13+18h]
    mov     r9,  qword ptr [r13+20h]
    mov     r10, qword ptr [r13+28h]
    mov     r11, qword ptr [r13+30h]
    mov     rdx, qword ptr [r13+10h]
    call    r12
    add     rsp, 28h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    ret
RunProbe ENDP

Clobber PROC
    mov     rax, 0DEAD0000DEAD0001h
    mov     rcx, 0DEAD0000DEAD0002h
    mov     rdx, 0DEAD0000DEAD0003h
    mov     r8,  0DEAD0000DEAD0004h
    mov     r9,  0DEAD0000DEAD0005h
    mov     r10, 0DEAD0000DEAD0006h
    mov     r11, 0DEAD0000DEAD0007h
    movq    xmm0, rax
    movq    xmm1, rcx
    movq    xmm2, rdx
    movq    xmm3, r8
    movq    xmm4, r9
    movq    xmm5, r10
    xor     eax, eax                ; ZF=1 PF=1, CF=OF=SF=0
    stc
    ret
Clobber ENDP
END
