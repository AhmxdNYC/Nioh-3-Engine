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
EXTERN ExtraPerfResC:PROC
EXTERN ep_resOrig:QWORD
EXTERN ep_giOrig:QWORD
EXTERN ep_lodOrig:QWORD
EXTERN ep_pSimFrac:QWORD
EXTERN ep_pDrawFrac:QWORD
EXTERN ep_giMul:DWORD
EXTERN ep_giMin:DWORD
EXTERN ep_fxMul:DWORD
EXTERN ep_giLastIn:DWORD
EXTERN ep_giLastOut:DWORD
EXTERN ep_drawOrig:QWORD
EXTERN ep_placeOrig:QWORD
EXTERN ep_rdMul:DWORD
EXTERN ep_charFall:QWORD
EXTERN ep_charTake:QWORD
EXTERN ExtraPerfPropC:PROC
PUBLIC caveGiScale, caveMeshLod, caveFxSim, caveFxDraw, caveRenderDims
PUBLIC caveDrawLimit, cavePlaceDist, caveCharLod, cavePropDist

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

; #67206 XRES: XRS1-4 `call RenderDimsGet 5AC428` (rcx = out {u32 w, u32 h}, returns rax = out). Calls the original, then
; ExtraPerfResC(out) scales w / h by ep_resMul (1.0 = untouched).
caveRenderDims PROC
    ; #67915: the game keeps live values in volatile registers across this call (FsrConfigApply 2998D48 keeps its config
    ; pointer in r10 and writes [r10+20B8] right after; RenderDimsGet itself only touches rax/rcx/rdx/r8/r9). ExtraPerfResC
    ; is C code and clobbers r10/r11/xmm, which crashed Preset 4 at boot (write to 0x20F8 = r10 0x40 + 20B8). So every
    ; volatile register except rax is saved around the whole cave; rax = out on return like the original.
    push    rcx
    push    rdx
    push    r8
    push    r9
    push    r10
    push    r11                         ; entry 8 mod 16 - 48 -> 8 mod 16
    sub     rsp, 98h                    ; -> 0 mod 16: 20h shadow + 6 xmm (60h) + rax slot (80h) + pad
    movups  xmmword ptr [rsp+20h], xmm0
    movups  xmmword ptr [rsp+30h], xmm1
    movups  xmmword ptr [rsp+40h], xmm2
    movups  xmmword ptr [rsp+50h], xmm3
    movups  xmmword ptr [rsp+60h], xmm4
    movups  xmmword ptr [rsp+70h], xmm5
    mov     rcx, qword ptr [rsp+98h+28h] ; the caller's rcx (out), untouched so far - reload for clarity
    call    qword ptr [ep_resOrig]
    mov     qword ptr [rsp+80h], rax
    mov     rcx, rax
    call    ExtraPerfResC
    movups  xmm0, xmmword ptr [rsp+20h]
    movups  xmm1, xmmword ptr [rsp+30h]
    movups  xmm2, xmmword ptr [rsp+40h]
    movups  xmm3, xmmword ptr [rsp+50h]
    movups  xmm4, xmmword ptr [rsp+60h]
    movups  xmm5, xmmword ptr [rsp+70h]
    mov     rax, qword ptr [rsp+80h]
    add     rsp, 98h
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    ret
caveRenderDims ENDP

; #67225 RenderDistance. Both call caves return the original xmm0 multiplied by ep_rdMul (1.0 = stock).
; Callers of GetDrawDistanceLimit / GetPlaceableDrawDistance consume xmm0 as a distance and keep nothing else.
caveDrawLimit PROC
    sub     rsp, 28h
    call    qword ptr [ep_drawOrig]
    mulss   xmm0, dword ptr [ep_rdMul]
    add     rsp, 28h
    ret
caveDrawLimit ENDP

cavePlaceDist PROC
    sub     rsp, 28h
    call    qword ptr [ep_placeOrig]
    mulss   xmm0, dword ptr [ep_rdMul]
    add     rsp, 28h
    ret
cavePlaceDist ENDP

; Replaces `comiss xmm6, xmm8; ja rel8` (6 bytes). xmm8 is the character draw-distance limit; xmm6 is the camera
; distance. Scale the limit, then take the original branch. Continuations are absolute so another build's rel8 still
; lands on the right instruction. jmp does not clobber the comiss flags.
caveCharLod PROC
    mulss   xmm8, dword ptr [ep_rdMul]
    comiss  xmm6, xmm8
    ja      rdCharTake
    jmp     qword ptr [ep_charFall]
rdCharTake:
    jmp     qword ptr [ep_charTake]
caveCharLod ENDP

; Replaces the property-record memcpy inside PropTableReadParamsBatch16. rcx/rdx/r8 are the memcpy args; [rsi] is the
; property hash (the site's cmp [rsi], eax). ExtraPerfPropC copies, then scales the destination for the distance keys.
cavePropDist PROC
    sub     rsp, 28h
    mov     r9d, dword ptr [rsi]
    call    ExtraPerfPropC
    add     rsp, 28h
    ret
cavePropDist ENDP

END
