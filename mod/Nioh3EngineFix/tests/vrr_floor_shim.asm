; vrr_floor_shim.asm - calls caveFps with its register contract (rdi = G, r8d = tier, eax = G[620]). Test only.
OPTION CASEMAP:NONE
EXTERN caveFps:PROC
EXTERN caveHold:PROC
PUBLIC CallFps, CallFpsOvr, CallHold
.code
; uint32_t CallFps(void* g, uint32_t tier) -> returns 0
CallFps PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rdi, rcx
    mov     r8d, edx
    mov     eax, edx
    call    caveFps
    xor     eax, eax
    add     rsp, 20h
    pop     rdi
    ret
CallFps ENDP
; uint32_t CallFpsOvr(void* g, uint32_t tier, uint32_t g620) -> returns 0 (GH #155: the game's tier override in eax)
CallFpsOvr PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rdi, rcx
    mov     eax, r8d
    mov     r8d, edx
    call    caveFps
    xor     eax, eax
    add     rsp, 20h
    pop     rdi
    ret
CallFpsOvr ENDP
; uint32_t CallHold(void* entry) -> ecx after caveHold (rbx = entry, the P10 contract); rax must survive
CallHold PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rbx, rcx
    mov     eax, 0A5A5A5A5h
    call    caveHold
    cmp     eax, 0A5A5A5A5h
    jne     hBad
    mov     eax, ecx
    jmp     hOut
hBad:
    mov     eax, 0FFFFFFFFh
hOut:
    add     rsp, 20h
    pop     rbx
    ret
CallHold ENDP
END
