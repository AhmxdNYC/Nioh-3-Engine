; mpcskip.asm - MPCS (1.7.47, [Patches] MPCS = 0): MPC tail skip in MeshletPreculling 5301FC (notes\meshlet-frontend.md 3).
; With the proven `mpc` build of shader_dx12\0x29ee0902.file (dllmain refuses MPCS without it), MPC #23 leaves the FINAL state of the
; #23 -> #28 -> #29 -> #30 -> #31 chain when nVSDrawPrm.y = cnt <= 1024, the replacement #28-#31 then return at once, and #23 also
; writes the value #26 writes (u9[1]); gpu-shaders\culling\verify_culling.py check B proves "#23 alone (+ #28-#31 only if cnt > 1024),
; no #26" == the shipped chain for every cnt >= 1. The caves drop the recording of those no-op stages (dispatch + its UAV barrier
; tokens) by jumping to the engine's own bind-failed paths:
;   MPCSa 530FD3 (end of the #23 block, #23 recorded)     -> mark this invocation (rsp) when 1 <= cnt <= 32768, then the decision
;   MPCSb 530FE4 (#28 block; now only the #23-failed path) -> drop a stale mark of this stack slot, then the tail decision
;   tail decision: switch on and 1 <= ebx (= cnt) <= 1024  -> 5313C5 (#24 section, = the #31 bind-failed path), else stock
;   MPCSc 531520 (#26 block)                               -> mark of this invocation present (#23 was dispatched) and switch on
;                                                             -> 5315C5 (the #26 bind-failed path), else stock
; A mark is the invocation's rsp in slot (rsp >> 4) & 63: concurrent recorder threads have different stacks; a slot another thread
; overwrote only makes cave C miss (no skip = stock), never skip wrongly. Every mark is consumed by cave C of the same invocation (the
; #26 block is reached whenever the #23 block was). Volatile registers clobbered: rax, rcx, rdx - all dead at the sites
; and their targets (re\scripts_re\hook_liveness.py after 5313C5 / 5315C5 / 530FEC / 531528), flags dead (next insns set them).
OPTION CASEMAP:NONE

PUBLIC caveMpcsA, caveMpcsB, caveMpcsC
PUBLIC mpcs_enable, mpcs_resB, mpcs_skipB, mpcs_resC, mpcs_skipC, mpcs_n28, mpcs_n26, mpcs_seen, mpcs_marks

mpdata SEGMENT ALIGN(64) READ WRITE 'DATA'
mpcs_enable DD 0                ; 1 = skip (set only with the mpc build installed)
ALIGN 8
mpcs_resB   DQ 0                ; Nioh3+530FEC: mov rcx,rsi; call [rax+0A8h] (#28 PSO bind)
mpcs_skipB  DQ 0                ; Nioh3+5313C5: xor ebx,ebx; the #24 section
mpcs_resC   DQ 0                ; Nioh3+531528: mov rcx,rsi; call [rax+0A8h] (#26 PSO bind)
mpcs_skipC  DQ 0                ; Nioh3+5315C5: mov r12d,8Ah; the #27 block
mpcs_n28    DQ 0                ; #28-#31 tails skipped
mpcs_n26    DQ 0                ; #26 dispatches skipped
mpcs_seen   DQ 0                ; tail decisions taken (chains with the #23 block recorded)
ALIGN 64
mpcs_marks  DQ 64 DUP (0)
mpdata ENDS

.code
mpcsDecide PROC
    lock inc qword ptr [mpcs_seen]
    cmp     dword ptr [mpcs_enable], 0
    je      md_stock
    lea     eax, [rbx-1]
    cmp     eax, 1023
    ja      md_stock                    ; unsigned: ebx = cnt outside 1..1024
    lock inc qword ptr [mpcs_n28]
    jmp     qword ptr [mpcs_skipB]
md_stock:
    mov     rax, [rsi]                  ; displaced 530FE4: mov rax,[rsi]; mov edx,0Eh
    mov     edx, 0Eh
    jmp     qword ptr [mpcs_resB]
mpcsDecide ENDP

caveMpcsA PROC
    add     qword ptr [rdi+3C40h], 8    ; displaced 530FD3 (the jmp short 530FE4 after it becomes the decision)
    lea     edx, [rbx-1]                ; #23 was recorded as Dispatch((cnt + 3FFh) >> 10, 1, 1): with cnt 0 it got no group and
    cmp     edx, 32767                  ; wrote nothing, so #26 must stay; marked for 1 <= cnt <= 32768 (verify_culling.py check B
    ja      caveMpcsB                   ; covers cnt 1..33000); otherwise as the bind-failed path (drop a stale mark, decide)
    mov     rax, rsp
    shr     rax, 4
    and     eax, 63
    lea     rcx, mpcs_marks
    mov     [rcx+rax*8], rsp
    jmp     mpcsDecide
caveMpcsA ENDP

caveMpcsB PROC
    mov     rax, rsp
    shr     rax, 4
    and     eax, 63
    lea     rcx, mpcs_marks
    cmp     [rcx+rax*8], rsp
    jne     mb_go
    mov     qword ptr [rcx+rax*8], 0    ; #23 not dispatched in this invocation: a mark of this stack slot is stale
mb_go:
    jmp     mpcsDecide
caveMpcsB ENDP

caveMpcsC PROC
    mov     rax, rsp
    shr     rax, 4
    and     eax, 63
    lea     rdx, mpcs_marks
    cmp     [rdx+rax*8], rsp
    jne     mc_stock
    mov     qword ptr [rdx+rax*8], 0    ; consume the mark (also with the switch off)
    cmp     dword ptr [mpcs_enable], 0
    je      mc_stock
    lock inc qword ptr [mpcs_n26]
    jmp     qword ptr [mpcs_skipC]
mc_stock:
    mov     rax, [rsi]                  ; displaced 531520: mov rax,[rsi]; mov edx,0Bh
    mov     edx, 0Bh
    jmp     qword ptr [mpcs_resC]
caveMpcsC ENDP

END
