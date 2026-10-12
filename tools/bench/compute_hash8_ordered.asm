; Offline candidate only. Windows x64 call ABI of1199A4:
; RCX=context, RDX=key bytes, R8D=length; RAX=zero-extended hash.
; Non-eight lengths tail-call supplied original (test stub offline).
; No nonvolatile register or stack modification. No game installation code.
option casemap:none
EXTERN ComputeHash8Original:QWORD
.code
ComputeHash8Ordered PROC
    cmp r8d,8
    jne fallback
    cmp byte ptr [rcx+2490h],0
    je xor_mode
    mov eax,dword ptr [rdx]
    mov r9d,dword ptr [rdx+4]
    shl r9,32
    or r9,rax
    mov eax,0FFFFFFFFh
    crc32 rax,r9
    ret
xor_mode:
    mov eax,dword ptr [rdx]
    xor eax,dword ptr [rdx+4]
    ret
fallback:
    jmp qword ptr [ComputeHash8Original]
ComputeHash8Ordered ENDP
END
