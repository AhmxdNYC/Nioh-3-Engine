; drawprep_harness.asm - register-exact runner for the drawprep.asm caves (tests\drawprep_harness.cpp).
; RunCave(target): saves the host's non-volatiles, loads every GPR (but rsp) and xmm0-15 from h_in, jumps to target.
; The cave leaves through an absolute jmp that the harness pointed at LandA / LandB / LandC; each records its id in h_land,
; stores every GPR and xmm0-15 into h_out (no register or flag is changed before the store) and returns to the host.
; Regs layout: 16 GPRs in encoding order (rax rcx rdx rbx rsp rbp rsi rdi r8..r15) x 8, then xmm0..xmm15 x 16 = 384 B.
OPTION CASEMAP:NONE
PUBLIC RunCave, LandA, LandB, LandC, h_in, h_out, h_land

hdata SEGMENT ALIGN(64) READ WRITE 'DATA'
h_in     DB 384 DUP(0)
h_out    DB 384 DUP(0)
h_land   DD 0
         DD 0
h_rsp    DQ 0
h_target DQ 0
hdata ENDS

.code
RunCave PROC
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 0A8h
    movdqu  xmmword ptr [rsp+00h], xmm6
    movdqu  xmmword ptr [rsp+10h], xmm7
    movdqu  xmmword ptr [rsp+20h], xmm8
    movdqu  xmmword ptr [rsp+30h], xmm9
    movdqu  xmmword ptr [rsp+40h], xmm10
    movdqu  xmmword ptr [rsp+50h], xmm11
    movdqu  xmmword ptr [rsp+60h], xmm12
    movdqu  xmmword ptr [rsp+70h], xmm13
    movdqu  xmmword ptr [rsp+80h], xmm14
    movdqu  xmmword ptr [rsp+90h], xmm15
    mov     qword ptr [h_target], rcx
    mov     qword ptr [h_rsp], rsp
    mov     dword ptr [h_land], 0
    movdqu  xmm0, xmmword ptr [h_in+128+00h]
    movdqu  xmm1, xmmword ptr [h_in+128+10h]
    movdqu  xmm2, xmmword ptr [h_in+128+20h]
    movdqu  xmm3, xmmword ptr [h_in+128+30h]
    movdqu  xmm4, xmmword ptr [h_in+128+40h]
    movdqu  xmm5, xmmword ptr [h_in+128+50h]
    movdqu  xmm6, xmmword ptr [h_in+128+60h]
    movdqu  xmm7, xmmword ptr [h_in+128+70h]
    movdqu  xmm8, xmmword ptr [h_in+128+80h]
    movdqu  xmm9, xmmword ptr [h_in+128+90h]
    movdqu  xmm10, xmmword ptr [h_in+128+0A0h]
    movdqu  xmm11, xmmword ptr [h_in+128+0B0h]
    movdqu  xmm12, xmmword ptr [h_in+128+0C0h]
    movdqu  xmm13, xmmword ptr [h_in+128+0D0h]
    movdqu  xmm14, xmmword ptr [h_in+128+0E0h]
    movdqu  xmm15, xmmword ptr [h_in+128+0F0h]
    mov     rcx, qword ptr [h_in+08h]
    mov     rdx, qword ptr [h_in+10h]
    mov     rbx, qword ptr [h_in+18h]
    mov     rbp, qword ptr [h_in+28h]
    mov     rsi, qword ptr [h_in+30h]
    mov     rdi, qword ptr [h_in+38h]
    mov     r8, qword ptr [h_in+40h]
    mov     r9, qword ptr [h_in+48h]
    mov     r10, qword ptr [h_in+50h]
    mov     r11, qword ptr [h_in+58h]
    mov     r12, qword ptr [h_in+60h]
    mov     r13, qword ptr [h_in+68h]
    mov     r14, qword ptr [h_in+70h]
    mov     r15, qword ptr [h_in+78h]
    mov     rax, qword ptr [h_in+00h]
    jmp     qword ptr [h_target]
LandA::
    mov     dword ptr [h_land], 1
    jmp     Snap
LandB::
    mov     dword ptr [h_land], 2
    jmp     Snap
LandC::
    mov     dword ptr [h_land], 3
Snap:
    mov     qword ptr [h_out+00h], rax
    mov     qword ptr [h_out+08h], rcx
    mov     qword ptr [h_out+10h], rdx
    mov     qword ptr [h_out+18h], rbx
    mov     qword ptr [h_out+20h], rsp
    mov     qword ptr [h_out+28h], rbp
    mov     qword ptr [h_out+30h], rsi
    mov     qword ptr [h_out+38h], rdi
    mov     qword ptr [h_out+40h], r8
    mov     qword ptr [h_out+48h], r9
    mov     qword ptr [h_out+50h], r10
    mov     qword ptr [h_out+58h], r11
    mov     qword ptr [h_out+60h], r12
    mov     qword ptr [h_out+68h], r13
    mov     qword ptr [h_out+70h], r14
    mov     qword ptr [h_out+78h], r15
    movdqu  xmmword ptr [h_out+128+00h], xmm0
    movdqu  xmmword ptr [h_out+128+10h], xmm1
    movdqu  xmmword ptr [h_out+128+20h], xmm2
    movdqu  xmmword ptr [h_out+128+30h], xmm3
    movdqu  xmmword ptr [h_out+128+40h], xmm4
    movdqu  xmmword ptr [h_out+128+50h], xmm5
    movdqu  xmmword ptr [h_out+128+60h], xmm6
    movdqu  xmmword ptr [h_out+128+70h], xmm7
    movdqu  xmmword ptr [h_out+128+80h], xmm8
    movdqu  xmmword ptr [h_out+128+90h], xmm9
    movdqu  xmmword ptr [h_out+128+0A0h], xmm10
    movdqu  xmmword ptr [h_out+128+0B0h], xmm11
    movdqu  xmmword ptr [h_out+128+0C0h], xmm12
    movdqu  xmmword ptr [h_out+128+0D0h], xmm13
    movdqu  xmmword ptr [h_out+128+0E0h], xmm14
    movdqu  xmmword ptr [h_out+128+0F0h], xmm15
    mov     rsp, qword ptr [h_rsp]
    movdqu  xmm6, xmmword ptr [rsp+00h]
    movdqu  xmm7, xmmword ptr [rsp+10h]
    movdqu  xmm8, xmmword ptr [rsp+20h]
    movdqu  xmm9, xmmword ptr [rsp+30h]
    movdqu  xmm10, xmmword ptr [rsp+40h]
    movdqu  xmm11, xmmword ptr [rsp+50h]
    movdqu  xmm12, xmmword ptr [rsp+60h]
    movdqu  xmm13, xmmword ptr [rsp+70h]
    movdqu  xmm14, xmmword ptr [rsp+80h]
    movdqu  xmm15, xmmword ptr [rsp+90h]
    add     rsp, 0A8h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    ret
RunCave ENDP

END
