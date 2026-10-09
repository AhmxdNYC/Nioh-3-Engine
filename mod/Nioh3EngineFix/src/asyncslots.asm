; asyncslots.asm - AS live switch for the engine's async-compute slots (asyncslots.cpp, notes\gpu-async-wins.md).
;
; The producer call sites of AsyncSlotGetCtx 52BE1C (`call 52BE1C`, E8 rel32) are re-pointed at near trampolines that
; jump to caveAsK. Each caveAsK counts the call, then does what 52BE1C's first instruction does (`mov eax,[rcx+1CCh]`,
; the slot mask) with the live AND-mask applied, and continues in the game's own function at 52BE22 (`bt eax,edx`), whose
; `ret` returns to the producer. So a masked slot reads as "not created" at that producer only, and the producer takes its
; own synchronous path (graphics stream), exactly as on a GPU where the slot is off.
;   switch on  : eax = mask & as_and_on   (as_and_on  = ~(slots forced synchronous by AS_Slots "-N"))
;   switch off : eax = mask & as_and_off  (as_and_off = ~(slots AS added at boot)) = the stock slot set
; Registers: rax (as the original), flags (dead across a call). rcx / rdx / everything else untouched. No stack use.
OPTION CASEMAP:NONE

PUBLIC caveAs0, caveAs1, caveAs2, caveAs3, caveAs4, caveAs5
PUBLIC as_enable, as_and_on, as_and_off, as_resume, as_calls

asdata SEGMENT ALIGN(64) READ WRITE 'DATA'
as_enable   DD 0
as_and_on   DD 0FFFFFFFFh
as_and_off  DD 0FFFFFFFFh
            DD 0
as_resume   DQ 0            ; Nioh3+52BE22 (`bt eax,edx` in AsyncSlotGetCtx)
as_calls    DD 8 DUP(0)     ; producer calls per patched site (stats)
asdata ENDS

.code
caveAs0 PROC
    lock inc dword ptr [as_calls+00h]
    jmp     asCommon
caveAs0 ENDP
caveAs1 PROC
    lock inc dword ptr [as_calls+04h]
    jmp     asCommon
caveAs1 ENDP
caveAs2 PROC
    lock inc dword ptr [as_calls+08h]
    jmp     asCommon
caveAs2 ENDP
caveAs3 PROC
    lock inc dword ptr [as_calls+0Ch]
    jmp     asCommon
caveAs3 ENDP
caveAs4 PROC
    lock inc dword ptr [as_calls+10h]
    jmp     asCommon
caveAs4 ENDP
caveAs5 PROC
    lock inc dword ptr [as_calls+14h]
    jmp     asCommon
caveAs5 ENDP

asCommon PROC
    mov     eax, dword ptr [rcx+1CCh]       ; the displaced instruction of 52BE1C
    cmp     dword ptr [as_enable], 0
    je      asOff
    and     eax, dword ptr [as_and_on]
    jmp     qword ptr [as_resume]
asOff:
    and     eax, dword ptr [as_and_off]
    jmp     qword ptr [as_resume]
asCommon ENDP

END
