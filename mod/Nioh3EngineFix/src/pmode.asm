; pmode.asm - PM group (1.7.44, [Debug] PresentModeLog; notes\vrr-present.md): who writes the engine's Present
; SyncInterval GfxDevice+424Ch. Two of its three writers are store instructions we replace (6 bytes each ->
; `call trampoline; nop`); each cave performs the ORIGINAL store first, then records value / device / sequence.
; Flags are never modified (mov / lea only) and every register is preserved, so the game sees exactly the original
; instruction. MEASUREMENT ONLY: values written to the game are unchanged.
;
;   caveSiW  DisplaySettingsWatcher 1350210, site 13503A3 `mov [rcx+424Ch],eax` (rcx = GfxDevice, eax = VSync slot
;            clamped >= 0). Runs only when the VSync slot differs from the watcher's cache and its guard is clear.
;   caveSiS  setter 2EEBB50 (no static caller: reached through a pointer), site 2EEBB68 `mov [rcx+424Ch],edx`.
;            The setter's own caller = [rsp+38h] inside the cave (saved rax 8 + cave return 8 + setter frame 28h).
; The third writer (GfxDevice ctor 2EDFBBD `mov dword [rdi+424Ch],1`) runs once per device and is not hooked: the
; C++ side attributes a value it sees on a new device (or before any cave hit) to it (patches.json deps check it).

PUBLIC caveSiW, caveSiS, pm_wSeq, pm_wVal, pm_wDev, pm_sSeq, pm_sVal, pm_sDev, pm_sRet

pmdata SEGMENT ALIGN(64) READ WRITE 'DATA'
pm_wSeq     DQ 0            ; watcher writes (count)
pm_wDev     DQ 0            ; GfxDevice of the last watcher write
pm_wVal     DD 0            ; value of the last watcher write
            DD 0
pm_sSeq     DQ 0            ; setter writes (count)
pm_sDev     DQ 0
pm_sRet     DQ 0            ; return address of the setter's caller (absolute)
pm_sVal     DD 0
            DD 0
pmdata ENDS

.code
caveSiW PROC FRAME
    push    rax
    .pushreg rax
    .endprolog
    mov     dword ptr [rcx+424Ch], eax              ; the original instruction
    mov     dword ptr [pm_wVal], eax
    mov     qword ptr [pm_wDev], rcx
    mov     rax, qword ptr [pm_wSeq]
    lea     rax, [rax+1]                            ; no flags touched
    mov     qword ptr [pm_wSeq], rax
    pop     rax
    ret
caveSiW ENDP

caveSiS PROC FRAME
    push    rax
    .pushreg rax
    .endprolog
    mov     dword ptr [rcx+424Ch], edx              ; the original instruction
    mov     dword ptr [pm_sVal], edx
    mov     qword ptr [pm_sDev], rcx
    mov     rax, qword ptr [rsp+38h]                ; the setter's caller
    mov     qword ptr [pm_sRet], rax
    mov     rax, qword ptr [pm_sSeq]
    lea     rax, [rax+1]
    mov     qword ptr [pm_sSeq], rax
    pop     rax
    ret
caveSiS ENDP

END
