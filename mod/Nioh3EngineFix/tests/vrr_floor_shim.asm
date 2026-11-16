; vrr_floor_shim.asm - calls caveFps with its register contract (rdi = G, r8d = tier, eax = G[620]). Test only.
OPTION CASEMAP:NONE
EXTERN caveFps:PROC
PUBLIC CallFps
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
END
