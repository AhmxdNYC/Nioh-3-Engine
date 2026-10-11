; replayprof_harness.asm - fake dispatcher loop + fake loop wait for tests\replayprof_harness.cpp (same call shapes as
; SynergisticDispatcherLoop 1A7C3E0: call [table + op*8] with rcx = ctx, rdx = &cursor; call [WaitForSingleObject import]).
OPTION CASEMAP:NONE
PUBLIC hl_loop, hl_loop_ret, hl_wait, hl_wait_ret

.code
; void hl_loop(void** table, void* ctx, uint8_t** cursor /* {cur, end} */)
hl_loop PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rbx, rcx
    mov     rdi, rdx
    mov     rsi, r8
next:
    mov     rax, qword ptr [rsi]
    cmp     rax, qword ptr [rsi+8]
    jae     done
    movzx   eax, word ptr [rax]
    mov     rcx, rdi
    mov     rdx, rsi
    call    qword ptr [rbx + rax*8]
hl_loop_ret LABEL PROC
    jmp     next
done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
hl_loop ENDP

; DWORD hl_wait(void** importSlot, HANDLE h, DWORD ms)
hl_wait PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     rax, rcx
    mov     rcx, rdx
    mov     edx, r8d
    call    qword ptr [rax]
hl_wait_ret LABEL PROC
    add     rsp, 28h
    ret
hl_wait ENDP

END
