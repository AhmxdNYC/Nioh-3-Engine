; extraperf.asm - caves of the "Extra performance" options (1.7.59; extraperf.h, notes\extra-performance-settings.md).
;   caveGiScale  GIS1-4 (18745A / 187464 / 187471 / 18747F in GiRenderScalesApply 1872C4): `call GiScaleLookup 187A50`
;                (ecx = which, result xmm0). Calls the original, then xmm0 = max(xmm0 * ep_giMul, ep_giMin). At level 0
;                ep_giMul = 1.0 and ep_giMin = 0.0 (the scales are > 0): the original value. Clobbers what the original
;                clobbers (rax rcx + volatile); the caller keeps nothing in rdx / r8-r11 / xmm1-5 across these calls.
;   caveMeshLod  EPL1 233A49 (GameUpdateTick): `call ApplyMeshletLodScales 2353C0` (rcx = object, rdx = ctx, void). Calls the
;                original, then ExtraPerfLodC(object) scales the 8 floats it wrote at +0xA108. rbx is saved; the caller
;                reloads rcx / rdx after the call and does not read rax.
;   caveFxSim    EPF1 4A5B76: `movss dword ptr [g_ViewGpuEffectSimFraction], xmm1` (8 bytes -> E8 rel32 + 3-byte nop).
;   caveFxDraw   EPF2 4A5B7E: `movss dword ptr [g_ViewGpuEffectDrawFraction], xmm2`.
;                Both store xmm * ep_fxMul to the global (address decoded from the original instruction at install) and
;                leave every register and the flags as they were (xmm1 / xmm2 saved whole, rax saved; lea, not sub/add, moves rsp).
OPTION CASEMAP:NONE

EXTERN ExtraPerfLodC:PROC
EXTERN ep_giOrig:QWORD
EXTERN ep_lodOrig:QWORD
EXTERN ep_pSimFrac:QWORD
EXTERN ep_pDrawFrac:QWORD
EXTERN ep_giMul:DWORD
EXTERN ep_giMin:DWORD
EXTERN ep_fxMul:DWORD
EXTERN ep_giLastIn:DWORD
EXTERN ep_giLastOut:DWORD
PUBLIC caveGiScale, caveMeshLod, caveFxSim, caveFxDraw

.code
caveGiScale PROC
    sub     rsp, 28h                    ; entry 8 mod 16 -> 0 mod 16, 20h shadow space
    call    qword ptr [ep_giOrig]
    movss   dword ptr [ep_giLastIn], xmm0     ; read-back for the stats line
    mulss   xmm0, dword ptr [ep_giMul]
    maxss   xmm0, dword ptr [ep_giMin]
    movss   dword ptr [ep_giLastOut], xmm0
    add     rsp, 28h
    ret
caveGiScale ENDP

caveMeshLod PROC
    push    rbx                         ; entry 8 mod 16 -> 0 mod 16
    sub     rsp, 20h
    mov     rbx, rcx
    call    qword ptr [ep_lodOrig]
    mov     rcx, rbx
    call    ExtraPerfLodC
    add     rsp, 20h
    pop     rbx
    ret
caveMeshLod ENDP

caveFxSim PROC
    lea     rsp, [rsp-28h]              ; lea: flags untouched (the replaced movss sets none)
    movups  xmmword ptr [rsp], xmm1
    mov     qword ptr [rsp+10h], rax
    mulss   xmm1, dword ptr [ep_fxMul]
    mov     rax, qword ptr [ep_pSimFrac]
    movss   dword ptr [rax], xmm1
    mov     rax, qword ptr [rsp+10h]
    movups  xmm1, xmmword ptr [rsp]
    lea     rsp, [rsp+28h]
    ret
caveFxSim ENDP

caveFxDraw PROC
    lea     rsp, [rsp-28h]              ; lea: flags untouched (the replaced movss sets none)
    movups  xmmword ptr [rsp], xmm2
    mov     qword ptr [rsp+10h], rax
    mulss   xmm2, dword ptr [ep_fxMul]
    mov     rax, qword ptr [ep_pDrawFrac]
    movss   dword ptr [rax], xmm2
    mov     rax, qword ptr [rsp+10h]
    movups  xmm2, xmmword ptr [rsp]
    lea     rsp, [rsp+28h]
    ret
caveFxDraw ENDP

END
