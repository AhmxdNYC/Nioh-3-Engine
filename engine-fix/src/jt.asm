; jt.asm - JT job-timing probe caves (jt.cpp, [Profiler] JT = 0 by default; notes\gq-jt-probes.md). MEASUREMENT ONLY.
;
; Sites (patches.json group JT, written by dllmain's frozen ApplyBatch only when JT = 1; nothing is patched otherwise):
;   JTc  40E4E  `mov rcx,rbx; call rdx` (JobWorkerPoll 40800: the job call, rbx = job, rdx = [[job]+8])  -> call caveJtJob
;   JTw  B616B7 `call [SleepConditionVariableSRW]` (STL condition_variable win7 wait, INFINITE)        -> nop; call caveJtWait
;   JTx  B616F7 `call [SleepConditionVariableSRW]` (the same, wait_for)                                 -> nop; call caveJtWait
;   JTn1 B61674 `jmp [WakeConditionVariable]`      (notify_one, after `add rcx,8`)                     -> jmp caveJtWake1; nop2
;   JTn2 B6165C `jmp [WakeAllConditionVariable]`   (notify_all, after `add rcx,8`)                     -> jmp caveJtWakeAll; nop2
; Every site is entered through dllmain's near trampoline (`jmp [rip+0]`), so the caves see the game's stack and registers.
;
; Register contracts
;   caveJtJob: entry = a normal call from 40E4E (rsp = 8 mod 16, [rsp] = 40E53). Does `mov rcx,rbx`, calls the job function
;     rdx with the caller's rcx / rdx / r8 / r9, returns its rax. Nonvolatile registers: untouched (rbx stays the job; 40E58
;     uses it). Volatile registers other than rax: clobbered, as by the original `call rdx`. Flags: dead at 40E53.
;     JtJobBegin / JtJobEnd (C++) stamp QPC around it. The frame has unwind info (an exception may pass through the job).
;   caveJtWait: a normal call in place of `call [SleepConditionVariableSRW]` (rcx = CONDITION_VARIABLE*, rdx = SRWLOCK*,
;     r8d = ms, r9d = flags). Fast path (any thread but the watched one): tail-jump through the IAT slot (rax, flags
;     clobbered: volatile at a call, not arguments). Watched thread: JtWaitBegin, the real wait, JtWaitEnd; eax = its result.
;   caveJtWake1 / caveJtWakeAll: entered by jmp in place of `jmp [Wake(All)ConditionVariable]` (function-entry state,
;     rcx = CONDITION_VARIABLE*). Fast path: rcx != the CV the watched thread sleeps on -> tail-jump through the IAT slot.
;     Else JtOnWake records the waker, then the same tail-jump.

OPTION CASEMAP:NONE

EXTERN JtJobBegin:PROC
EXTERN JtJobEnd:PROC
EXTERN JtWaitBegin:PROC
EXTERN JtWaitEnd:PROC
EXTERN JtOnWake:PROC

PUBLIC caveJtJob, caveJtWait, caveJtWake1, caveJtWakeAll
PUBLIC jt_waitTid, jt_waitCv, jt_iatSleepSrw, jt_iatWake, jt_iatWakeAll

jtdata SEGMENT ALIGN(64) READ WRITE 'DATA'
jt_waitCv       DQ 0            ; CONDITION_VARIABLE* the watched thread sleeps on right now (0 = none)
jt_waitTid      DD 0            ; the watched thread (TEB ClientId.UniqueThread, gs:[48h]); 0 = none yet
                DD 0
jt_iatSleepSrw  DQ 0            ; &IAT slot SleepConditionVariableSRW
jt_iatWake      DQ 0            ; &IAT slot WakeConditionVariable
jt_iatWakeAll   DQ 0            ; &IAT slot WakeAllConditionVariable
jtdata ENDS

.code

; ---- JTc ---------------------------------------------------------------------------------------------------------------
caveJtJob PROC FRAME
    sub     rsp, 38h                    ; entry rsp = 8 mod 16 -> 0 mod 16
    .allocstack 38h
    .endprolog
    mov     qword ptr [rsp+20h], rdx    ; job function
    mov     qword ptr [rsp+28h], r8
    mov     qword ptr [rsp+30h], r9
    mov     rcx, rbx                    ; JtJobBegin(job, fn)
    call    JtJobBegin
    mov     rcx, rbx                    ; the displaced `mov rcx,rbx`
    mov     rdx, qword ptr [rsp+20h]
    mov     r8, qword ptr [rsp+28h]
    mov     r9, qword ptr [rsp+30h]
    call    qword ptr [rsp+20h]         ; the original `call rdx`
    mov     qword ptr [rsp+28h], rax
    mov     rcx, rbx                    ; JtJobEnd(job)
    call    JtJobEnd
    mov     rax, qword ptr [rsp+28h]
    add     rsp, 38h
    ret
caveJtJob ENDP

; ---- JTw / JTx ---------------------------------------------------------------------------------------------------------
caveJtWait PROC
    mov     eax, dword ptr gs:[48h]     ; current thread id
    cmp     eax, dword ptr [jt_waitTid]
    je      jtWaitSlow
    mov     rax, qword ptr [jt_iatSleepSrw]
    jmp     qword ptr [rax]
caveJtWait ENDP

jtWaitSlow PROC FRAME
    sub     rsp, 48h                    ; entry rsp = 8 mod 16 -> 0 mod 16
    .allocstack 48h
    .endprolog
    mov     qword ptr [rsp+20h], rcx
    mov     qword ptr [rsp+28h], rdx
    mov     qword ptr [rsp+30h], r8
    mov     qword ptr [rsp+38h], r9
    mov     edx, r8d                    ; JtWaitBegin(cv, ms)
    call    JtWaitBegin
    mov     rcx, qword ptr [rsp+20h]
    mov     rdx, qword ptr [rsp+28h]
    mov     r8, qword ptr [rsp+30h]
    mov     r9, qword ptr [rsp+38h]
    mov     rax, qword ptr [jt_iatSleepSrw]
    call    qword ptr [rax]
    mov     qword ptr [rsp+40h], rax
    mov     rcx, qword ptr [rsp+20h]    ; JtWaitEnd(cv, result)
    mov     edx, eax
    call    JtWaitEnd
    mov     rax, qword ptr [rsp+40h]
    add     rsp, 48h
    ret
jtWaitSlow ENDP

; ---- JTn1 / JTn2 -------------------------------------------------------------------------------------------------------
caveJtWake1 PROC
    cmp     rcx, qword ptr [jt_waitCv]
    je      jtWake1Slow
    mov     rax, qword ptr [jt_iatWake]
    jmp     qword ptr [rax]
caveJtWake1 ENDP

caveJtWakeAll PROC
    cmp     rcx, qword ptr [jt_waitCv]
    je      jtWakeAllSlow
    mov     rax, qword ptr [jt_iatWakeAll]
    jmp     qword ptr [rax]
caveJtWakeAll ENDP

jtWake1Slow PROC FRAME
    push    rcx
    .pushreg rcx
    sub     rsp, 20h                    ; entry 8 mod 16, +8 push, +20h -> 0 mod 16
    .allocstack 20h
    .endprolog
    xor     edx, edx                    ; JtOnWake(cv, 0 = notify_one)
    call    JtOnWake
    mov     rax, qword ptr [jt_iatWake]
    add     rsp, 20h
    pop     rcx
    jmp     qword ptr [rax]
jtWake1Slow ENDP

jtWakeAllSlow PROC FRAME
    push    rcx
    .pushreg rcx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    mov     edx, 1                      ; JtOnWake(cv, 1 = notify_all)
    call    JtOnWake
    mov     rax, qword ptr [jt_iatWakeAll]
    add     rsp, 20h
    pop     rcx
    jmp     qword ptr [rax]
jtWakeAllSlow ENDP

END
