; samelook.asm - register shims for samelook.cpp (1.7.49; notes\same-look-package.md).
;   caveVsmaBind  VSMA3 1F8AF2: `call VsmTileBindViewport 1F96E0` (rcx = clip, rdx = ctx) in VsmClipmapRender's batch loop.
;                 Live there (re\scripts_shadow, asm 1F8A8B..1F8AF2, deps-checked): rbx = tile entry {clip|flags, x, y, ..},
;                 r14 = page manager, edi = VsmTilesPerFrameBudget. Passes them to VsmaBindC(clip, ctx, tile, pm, budget) and
;                 returns its al (bound / not bound) to 1F8AF7 (xor r11d,r11d; test al,al). Volatile registers are dead after the
;                 original call as well (it is an ordinary call); rbx / r14 / edi are callee-saved in VsmaBindC.
;   caveRco       RCO1 39B7CA: `call qword ptr [rax+4B8h]` (6 bytes -> E8 rel32 + nop) right after ApplyGraphicsSettings in
;                 RenderFrameSettingsApply 39B570: rcx = rsi = the frame's RenderConfig S, rdx = [r15], rax = S's vtable.
;                 Calls RcoApplyC(S), restores rcx / rdx / r8 / r9 / rax and tail-jumps into the original vfunc, which returns
;                 to the nop after our E8 (39B7CF) and on to 39B7D0 as before.
OPTION CASEMAP:NONE

EXTERN VsmaBindC:PROC
EXTERN RcoApplyC:PROC
PUBLIC caveVsmaBind, caveRco

.code
caveVsmaBind PROC
    sub     rsp, 38h                    ; entry rsp = 8 mod 16 -> 0 mod 16; [rsp+20h] = 5th argument slot (our own frame)
    mov     dword ptr [rsp+20h], edi    ; budget
    mov     r8, rbx                     ; tile entry
    mov     r9, r14                     ; page manager
    call    VsmaBindC
    add     rsp, 38h
    ret
caveVsmaBind ENDP

caveRco PROC
    push    rcx
    push    rdx
    push    r8
    push    r9
    push    rax                         ; entry 8 mod 16, 5 pushes -> 0 mod 16
    sub     rsp, 20h
    call    RcoApplyC                   ; rcx = S
    add     rsp, 20h
    pop     rax
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    jmp     qword ptr [rax+4B8h]
caveRco ENDP

END
