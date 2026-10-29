; jt_harness.asm - calls caveJtJob exactly like the patched JTc site in JobWorkerPoll (40E4E): rbx = job, rdx = job function,
; `call caveJtJob` with rsp 16-aligned. Records rbx after the call (must still be the job: 40E58 uses it).
OPTION CASEMAP:NONE
EXTERN caveJtJob:PROC
PUBLIC JtCallJob, jth_rbxAfter

.data
ALIGN 8
jth_rbxAfter DQ 0

.code
; uint64_t JtCallJob(void* job, void* fn)
JtCallJob PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     rbx, rcx
    mov     rcx, 0BADC0DEh              ; garbage: the cave must load rcx from rbx itself
    call    caveJtJob
    mov     qword ptr [jth_rbxAfter], rbx
    add     rsp, 20h
    pop     rbx
    ret
JtCallJob ENDP
END
