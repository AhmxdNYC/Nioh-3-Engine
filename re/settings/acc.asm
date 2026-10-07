== func 1FE22F8-1FE2313
01FE22F8  4883EC28                 sub rsp,28h
01FE22FC  E8CBF1FFFF               call R_1FE14CC
01FE2301  4885C0                   test rax,rax
01FE2304  7405                     je short R_1FE230B
01FE2306  8B4020                   mov eax,[rax+20h]
01FE2309  EB03                     jmp short R_1FE230E
01FE230B  83C8FF                   or eax,0FFFFFFFFh
01FE230E  4883C428                 add rsp,28h
01FE2312  C3                       ret
== func 1FE2314-1FE23A9
01FE2314  48895C2418               mov [rsp+18h],rbx
01FE2319  57                       push rdi
01FE231A  4883EC40                 sub rsp,40h
01FE231E  488B050B5C4D02           mov rax,[rel R_44B7F30]
01FE2325  4833C4                   xor rax,rsp
01FE2328  4889442430               mov [rsp+30h],rax
01FE232D  4863DA                   movsxd rbx,edx
01FE2330  E897F1FFFF               call R_1FE14CC
01FE2335  488BF8                   mov rdi,rax
01FE2338  4885C0                   test rax,rax
01FE233B  744D                     je short R_1FE238A
01FE233D  488B4808                 mov rcx,[rax+8]
01FE2341  4885C9                   test rcx,rcx
01FE2344  740F                     je short R_1FE2355
01FE2346  85DB                     test ebx,ebx
01FE2348  7840                     js short R_1FE238A
01FE234A  3B5824                   cmp ebx,[rax+24h]
01FE234D  7D3B                     jge short R_1FE238A
01FE234F  488B04D9                 mov rax,[rcx+rbx*8]
01FE2353  EB3C                     jmp short R_1FE2391
01FE2355  395828                   cmp [rax+28h],ebx
01FE2358  7F30                     jg short R_1FE238A
01FE235A  3B582C                   cmp ebx,[rax+2Ch]
01FE235D  7F2B                     jg short R_1FE238A
01FE235F  0F57C0                   xorps xmm0,xmm0
01FE2362  488D158B209101           lea rdx,[rel R_38F43F4]
01FE2369  448BC3                   mov r8d,ebx
01FE236C  488D4C2420               lea rcx,[rsp+20h]
01FE2371  0F11442420               movups [rsp+20h],xmm0
01FE2376  E8F54AFFFF               call R_1FD6E70
01FE237B  0F10442420               movups xmm0,[rsp+20h]
01FE2380  488D4710                 lea rax,[rdi+10h]
01FE2384  F30F7F00                 movdqu [rax],xmm0
01FE2388  EB07                     jmp short R_1FE2391
01FE238A  488D05FF3F9001           lea rax,[rel R_38E6390]
01FE2391  488B4C2430               mov rcx,[rsp+30h]
01FE2396  4833CC                   xor rcx,rsp
01FE2399  E842EFBAFE               call R_B912E0
01FE239E  488B5C2460               mov rbx,[rsp+60h]
01FE23A3  4883C440                 add rsp,40h
01FE23A7  5F                       pop rdi
01FE23A8  C3                       ret
== func 1FE3FF0-1FE402A
01FE3FF0  4053                     push rbx
01FE3FF2  4883EC20                 sub rsp,20h
01FE3FF6  8BDA                     mov ebx,edx
01FE3FF8  E8CFD4FFFF               call R_1FE14CC
01FE3FFD  488BC8                   mov rcx,rax
01FE4000  33C0                     xor eax,eax
01FE4002  4885C9                   test rcx,rcx
01FE4005  741D                     je short R_1FE4024
01FE4007  48394108                 cmp [rcx+8],rax
01FE400B  740B                     je short R_1FE4018
01FE400D  85DB                     test ebx,ebx
01FE400F  7813                     js short R_1FE4024
01FE4011  3B5924                   cmp ebx,[rcx+24h]
01FE4014  7D0E                     jge short R_1FE4024
01FE4016  EB0A                     jmp short R_1FE4022
01FE4018  395928                   cmp [rcx+28h],ebx
01FE401B  7F07                     jg short R_1FE4024
01FE401D  3B592C                   cmp ebx,[rcx+2Ch]
01FE4020  7F02                     jg short R_1FE4024
01FE4022  B001                     mov al,1
01FE4024  4883C420                 add rsp,20h
01FE4028  5B                       pop rbx
01FE4029  C3                       ret
== func 1FE41A0-1FE4244
01FE41A0  48895C2408               mov [rsp+8],rbx
01FE41A5  4889742410               mov [rsp+10h],rsi
01FE41AA  57                       push rdi
01FE41AB  4881EC20010000           sub rsp,120h
01FE41B2  488B05773D4D02           mov rax,[rel R_44B7F30]
01FE41B9  4833C4                   xor rax,rsp
01FE41BC  4889842410010000         mov [rsp+110h],rax
01FE41C4  33D2                     xor edx,edx
01FE41C6  488D4C2420               lea rcx,[rsp+20h]
01FE41CB  41B8F0000000             mov r8d,0F0h
01FE41D1  E82AFDBAFE               call R_B93F00
01FE41D6  33DB                     xor ebx,ebx
01FE41D8  488D7C2420               lea rdi,[rsp+20h]
01FE41DD  488D742420               lea rsi,[rsp+20h]
01FE41E2  488BD6                   mov rdx,rsi
01FE41E5  8BCB                     mov ecx,ebx
01FE41E7  E858000000               call R_1FE4244
01FE41EC  84C0                     test al,al
01FE41EE  7516                     jne short R_1FE4206
01FE41F0  8BCB                     mov ecx,ebx
01FE41F2  E8D5D2FFFF               call R_1FE14CC
01FE41F7  4885C0                   test rax,rax
01FE41FA  7407                     je short R_1FE4203
01FE41FC  8B4020                   mov eax,[rax+20h]
01FE41FF  8907                     mov [rdi],eax
01FE4201  EB03                     jmp short R_1FE4206
01FE4203  830FFF                   or dword [rdi],0FFFFFFFFh
01FE4206  FFC3                     inc ebx
01FE4208  4883C604                 add rsi,4
01FE420C  4883C704                 add rdi,4
01FE4210  83FB3C                   cmp ebx,3Ch
01FE4213  72CD                     jb short R_1FE41E2
01FE4215  488D4C2420               lea rcx,[rsp+20h]
01FE421A  E8451E0000               call R_1FE6064
01FE421F  488B8C2410010000         mov rcx,[rsp+110h]
01FE4227  4833CC                   xor rcx,rsp
01FE422A  E8B1D0BAFE               call R_B912E0
01FE422F  4C8D9C2420010000         lea r11,[rsp+120h]
01FE4237  498B5B10                 mov rbx,[r11+10h]
01FE423B  498B7318                 mov rsi,[r11+18h]
01FE423F  498BE3                   mov rsp,r11
01FE4242  5F                       pop rdi
01FE4243  C3                       ret
== func 1FE4244-1FE43D8
01FE4244  48895C2418               mov [rsp+18h],rbx
01FE4249  4889742420               mov [rsp+20h],rsi
01FE424E  55                       push rbp
01FE424F  57                       push rdi
01FE4250  4156                     push r14
01FE4252  488DAC2440FFFFFF         lea rbp,[rsp-0C0h]
01FE425A  4881ECC0010000           sub rsp,1C0h
01FE4261  488B05C83C4D02           mov rax,[rel R_44B7F30]
01FE4268  4833C4                   xor rax,rsp
01FE426B  488985B0000000           mov [rbp+0B0h],rax
01FE4272  4C8BF2                   mov r14,rdx
01FE4275  E852D2FFFF               call R_1FE14CC
01FE427A  33DB                     xor ebx,ebx
01FE427C  488BF0                   mov rsi,rax
01FE427F  4885C0                   test rax,rax
01FE4282  0F8429010000             je near R_1FE43B1
01FE4288  488D4C2430               lea rcx,[rsp+30h]
01FE428D  E8E236FFFF               call R_1FD7974
01FE4292  E8E968FDFE               call R_FBAB80
01FE4297  488BD0                   mov rdx,rax
01FE429A  488D4C2430               lea rcx,[rsp+30h]
01FE429F  E820EF9900               call R_29831C4
01FE42A4  84C0                     test al,al
01FE42A6  0F84E1000000             je near R_1FE438D
01FE42AC  BF04010000               mov edi,104h
01FE42B1  488D4DA0                 lea rcx,[rbp-60h]
01FE42B5  448BC7                   mov r8d,edi
01FE42B8  33D2                     xor edx,edx
01FE42BA  E841FCBAFE               call R_B93F00
01FE42BF  4C8B0E                   mov r9,[rsi]
01FE42C2  4C8D058F62AB01           lea r8,[rel R_3A9A558]
01FE42C9  8BD7                     mov edx,edi
01FE42CB  488D4DA0                 lea rcx,[rbp-60h]
01FE42CF  E8ACFA12FE               call R_113D80
01FE42D4  4C8D45A0                 lea r8,[rbp-60h]
01FE42D8  48895C2420               mov [rsp+20h],rbx
01FE42DD  488D542420               lea rdx,[rsp+20h]
01FE42E2  488D4C2430               lea rcx,[rsp+30h]
01FE42E7  E8CC899A00               call R_298CCB8
01FE42EC  84C0                     test al,al
01FE42EE  0F8499000000             je near R_1FE438D
01FE42F4  4C8B5E08                 mov r11,[rsi+8]
01FE42F8  4D85DB                   test r11,r11
01FE42FB  7449                     je short R_1FE4346
01FE42FD  448B5624                 mov r10d,[rsi+24h]
01FE4301  8BCB                     mov ecx,ebx
01FE4303  4585D2                   test r10d,r10d
01FE4306  0F848D000000             je near R_1FE4399
01FE430C  BF01000000               mov edi,1
01FE4311  488B542420               mov rdx,[rsp+20h]
01FE4316  8BC1                     mov eax,ecx
01FE4318  4D8B0CC3                 mov r9,[r11+rax*8]
01FE431C  4C2BCA                   sub r9,rdx
01FE431F  0FB602                   movzx eax,byte [rdx]
01FE4322  460FB6040A               movzx r8d,byte [rdx+r9]
01FE4327  412BC0                   sub eax,r8d
01FE432A  7508                     jne short R_1FE4334
01FE432C  4803D7                   add rdx,rdi
01FE432F  4585C0                   test r8d,r8d
01FE4332  75EB                     jne short R_1FE431F
01FE4334  85C0                     test eax,eax
01FE4336  7409                     je short R_1FE4341
01FE4338  03CF                     add ecx,edi
01FE433A  413BCA                   cmp ecx,r10d
01FE433D  72D2                     jb short R_1FE4311
01FE433F  EB58                     jmp short R_1FE4399
01FE4341  41890E                   mov [r14],ecx
01FE4344  EB4A                     jmp short R_1FE4390
01FE4346  488B542420               mov rdx,[rsp+20h]
01FE434B  448BC3                   mov r8d,ebx
01FE434E  BF01000000               mov edi,1
01FE4353  8A0A                     mov cl,[rdx]
01FE4355  8D41D0                   lea eax,[rcx-30h]
01FE4358  3C09                     cmp al,9
01FE435A  760C                     jbe short R_1FE4368
01FE435C  84C9                     test cl,cl
01FE435E  7414                     je short R_1FE4374
01FE4360  80E92B                   sub cl,2Bh
01FE4363  F6C1FD                   test cl,0FDh
01FE4366  752B                     jne short R_1FE4393
01FE4368  4403C7                   add r8d,edi
01FE436B  4803D7                   add rdx,rdi
01FE436E  4183F808                 cmp r8d,8
01FE4372  72DF                     jb short R_1FE4353
01FE4374  488B4C2420               mov rcx,[rsp+20h]
01FE4379  E85E14BEFE               call R_BC57DC
01FE437E  394628                   cmp [rsi+28h],eax
01FE4381  7F16                     jg short R_1FE4399
01FE4383  3B462C                   cmp eax,[rsi+2Ch]
01FE4386  7F11                     jg short R_1FE4399
01FE4388  418906                   mov [r14],eax
01FE438B  EB09                     jmp short R_1FE4396
01FE438D  408AFB                   mov dil,bl
01FE4390  408ADF                   mov bl,dil
01FE4393  408AFB                   mov dil,bl
01FE4396  408ADF                   mov bl,dil
01FE4399  488D05786F0402           lea rax,[rel R_402B318]
01FE43A0  488D4C2430               lea rcx,[rsp+30h]
01FE43A5  4889442430               mov [rsp+30h],rax
01FE43AA  E89952FDFE               call R_FB9648
01FE43AF  8AC3                     mov al,bl
01FE43B1  488B8DB0000000           mov rcx,[rbp+0B0h]
01FE43B8  4833CC                   xor rcx,rsp
01FE43BB  E820CFBAFE               call R_B912E0
01FE43C0  4C8D9C24C0010000         lea r11,[rsp+1C0h]
01FE43C8  498B5B30                 mov rbx,[r11+30h]
01FE43CC  498B7338                 mov rsi,[r11+38h]
01FE43D0  498BE3                   mov rsp,r11
01FE43D3  415E                     pop r14
01FE43D5  5F                       pop rdi
01FE43D6  5D                       pop rbp
01FE43D7  C3                       ret
== func AE0BC0-AE0D84
00AE0BC0  48895C2418               mov [rsp+18h],rbx
00AE0BC5  48894C2408               mov [rsp+8],rcx
00AE0BCA  55                       push rbp
00AE0BCB  56                       push rsi
00AE0BCC  57                       push rdi
00AE0BCD  4883EC20                 sub rsp,20h
00AE0BD1  488D3558A3AD03           lea rsi,[rel R_45BAF30]
00AE0BD8  33D2                     xor edx,edx
00AE0BDA  488BCE                   mov rcx,rsi
00AE0BDD  41B8F0000000             mov r8d,0F0h
00AE0BE3  E818330B00               call R_B93F00
00AE0BE8  F30F1005F0350F03         movss xmm0,[rel R_3BD41E0]
00AE0BF0  488D0D49A4AD03           lea rcx,[rel R_45BB040]
00AE0BF7  33ED                     xor ebp,ebp
00AE0BF9  F30F110537A4AD03         movss [rel R_45BB038],xmm0
00AE0C01  33D2                     xor edx,edx
00AE0C03  48892D16A4AD03           mov [rel R_45BB020],rbp
00AE0C0A  41B8800C0000             mov r8d,0C80h
00AE0C10  48892D11A4AD03           mov [rel R_45BB028],rbp
00AE0C17  48892D12A4AD03           mov [rel R_45BB030],rbp
00AE0C1E  E8DD320B00               call R_B93F00
00AE0C23  F30F1005AD350F03         movss xmm0,[rel R_3BD41D8]
00AE0C2B  8BDD                     mov ebx,ebp
00AE0C2D  F30F11058BB0AD03         movss [rel R_45BBCC0],xmm0
00AE0C35  488BFE                   mov rdi,rsi
00AE0C38  660F6F05B0651203         movdqa xmm0,[rel R_3C071F0]
00AE0C40  660F7F0588B0AD03         movdqa [rel R_45BBCD0],xmm0
00AE0C48  48892D75B0AD03           mov [rel R_45BBCC4],rbp
00AE0C4F  C70573B0AD03FFFFFFFF     mov dword [rel R_45BBCCC],0FFFFFFFFh
00AE0C59  892D81B0AD03             mov [rel R_45BBCE0],ebp
00AE0C5F  C7057BB0AD03FFFFFFFF     mov dword [rel R_45BBCE4],0FFFFFFFFh
00AE0C69  C70575B0AD0308000000     mov dword [rel R_45BBCE8],8
00AE0C73  66892D72B0AD03           mov [rel R_45BBCEC],bp
00AE0C7A  40882D6DB0AD03           mov [rel R_45BBCEE],bpl
00AE0C81  8BCB                     mov ecx,ebx
00AE0C83  E844085001               call R_1FE14CC
00AE0C88  4885C0                   test rax,rax
00AE0C8B  7405                     je short R_AE0C92
00AE0C8D  8B4020                   mov eax,[rax+20h]
00AE0C90  EB05                     jmp short R_AE0C97
00AE0C92  B8FFFFFFFF               mov eax,0FFFFFFFFh
00AE0C97  8907                     mov [rdi],eax
00AE0C99  FFC3                     inc ebx
00AE0C9B  4883C704                 add rdi,4
00AE0C9F  83FB3C                   cmp ebx,3Ch
00AE0CA2  72DD                     jb short R_AE0C81
00AE0CA4  4C8D0D95A3AD03           lea r9,[rel R_45BB040]
00AE0CAB  33D2                     xor edx,edx
00AE0CAD  4C8D050CAB4D00           lea r8,[rel R_FBB7C0]
00AE0CB4  33C9                     xor ecx,ecx
00AE0CB6  FF15F41CE002             call qword [rel R_38E29B0]
00AE0CBC  488D542440               lea rdx,[rsp+40h]
00AE0CC1  48896C2440               mov [rsp+40h],rbp
00AE0CC6  488D0DDBADFB02           lea rcx,[rel R_3A9BAA8]
00AE0CCD  FF155D21E002             call qword [rel R_38E2E30]
00AE0CD3  85C0                     test eax,eax
00AE0CD5  7929                     jns short R_AE0D00
00AE0CD7  488B4C2440               mov rcx,[rsp+40h]
00AE0CDC  4885C9                   test rcx,rcx
00AE0CDF  0F848F000000             je near R_AE0D74
00AE0CE5  48896C2440               mov [rsp+40h],rbp
00AE0CEA  488B11                   mov rdx,[rcx]
00AE0CED  FF5210                   call qword [rdx+10h]
00AE0CF0  488BC6                   mov rax,rsi
00AE0CF3  488B5C2450               mov rbx,[rsp+50h]
00AE0CF8  4883C420                 add rsp,20h
00AE0CFC  5F                       pop rdi
00AE0CFD  5E                       pop rsi
00AE0CFE  5D                       pop rbp
00AE0CFF  C3                       ret
00AE0D00  8BDD                     mov ebx,ebp
00AE0D02  488D3D87A3AD03           lea rdi,[rel R_45BB090]
00AE0D09  0F1F8000000000           nop dword [rax]
00AE0D10  488B4C2440               mov rcx,[rsp+40h]
00AE0D15  4C8D442448               lea r8,[rsp+48h]
00AE0D1A  48896C2448               mov [rsp+48h],rbp
00AE0D1F  8BD3                     mov edx,ebx
00AE0D21  488B01                   mov rax,[rcx]
00AE0D24  FF5060                   call qword [rax+60h]
00AE0D27  85C0                     test eax,eax
00AE0D29  7818                     js short R_AE0D43
00AE0D2B  488B4C2448               mov rcx,[rsp+48h]
00AE0D30  8BC3                     mov eax,ebx
00AE0D32  4869D038010000           imul rdx,rax,138h
00AE0D39  4C8B01                   mov r8,[rcx]
00AE0D3C  4803D7                   add rdx,rdi
00AE0D3F  41FF5050                 call qword [r8+50h]
00AE0D43  488B4C2448               mov rcx,[rsp+48h]
00AE0D48  4885C9                   test rcx,rcx
00AE0D4B  740B                     je short R_AE0D58
00AE0D4D  48896C2448               mov [rsp+48h],rbp
00AE0D52  488B01                   mov rax,[rcx]
00AE0D55  FF5010                   call qword [rax+10h]
00AE0D58  FFC3                     inc ebx
00AE0D5A  83FB0A                   cmp ebx,0Ah
00AE0D5D  72B1                     jb short R_AE0D10
00AE0D5F  488B4C2440               mov rcx,[rsp+40h]
00AE0D64  4885C9                   test rcx,rcx
00AE0D67  740B                     je short R_AE0D74
00AE0D69  48896C2440               mov [rsp+40h],rbp
00AE0D6E  488B01                   mov rax,[rcx]
00AE0D71  FF5010                   call qword [rax+10h]
00AE0D74  488B5C2450               mov rbx,[rsp+50h]
00AE0D79  488BC6                   mov rax,rsi
00AE0D7C  4883C420                 add rsp,20h
00AE0D80  5F                       pop rdi
00AE0D81  5E                       pop rsi
00AE0D82  5D                       pop rbp
00AE0D83  C3                       ret
== func FB9A10-FBA835
00FB9A10  4055                     push rbp
00FB9A12  53                       push rbx
00FB9A13  56                       push rsi
00FB9A14  57                       push rdi
00FB9A15  4154                     push r12
00FB9A17  4156                     push r14
00FB9A19  4157                     push r15
00FB9A1B  488D6C24D0               lea rbp,[rsp-30h]
00FB9A20  4881EC30010000           sub rsp,130h
00FB9A27  488B0502E54F03           mov rax,[rel R_44B7F30]
00FB9A2E  4833C4                   xor rax,rsp
00FB9A31  48894520                 mov [rbp+20h],rax
00FB9A35  488BD9                   mov rbx,rcx
00FB9A38  4D63F0                   movsxd r14,r8d
00FB9A3B  8BCA                     mov ecx,edx
00FB9A3D  E88A7A0201               call R_1FE14CC
00FB9A42  488BF0                   mov rsi,rax
00FB9A45  4885C0                   test rax,rax
00FB9A48  0F84C70D0000             je near R_FBA815
00FB9A4E  4883780800               cmp qword [rax+8],0
00FB9A53  0F8492060000             je near R_FBA0EB
00FB9A59  4585F6                   test r14d,r14d
00FB9A5C  0F88B30D0000             js near R_FBA815
00FB9A62  443B7024                 cmp r14d,[rax+24h]
00FB9A66  0F8DA90D0000             jge near R_FBA815
00FB9A6C  488B10                   mov rdx,[rax]
00FB9A6F  4C8D3DD2E87103           lea r15,[rel R_46D8348]
00FB9A76  33FF                     xor edi,edi
00FB9A78  48C7442428FFFFFFFF       mov qword [rsp+28h],0FFFFFFFFFFFFFFFFh
00FB9A81  488D05086B0D03           lea rax,[rel R_4090590]
00FB9A88  48897C2430               mov [rsp+30h],rdi
00FB9A8D  4889442470               mov [rsp+70h],rax
00FB9A92  4C8D25976B0D03           lea r12,[rel R_4090630]
00FB9A99  488D442470               lea rax,[rsp+70h]
00FB9A9E  4C897C2458               mov [rsp+58h],r15
00FB9AA3  488945A8                 mov [rbp-58h],rax
00FB9AA7  0F57C0                   xorps xmm0,xmm0
00FB9AAA  488D442470               lea rax,[rsp+70h]
00FB9AAF  48897C2438               mov [rsp+38h],rdi
00FB9AB4  488945A0                 mov [rbp-60h],rax
00FB9AB8  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FB9ABF  C744244001000000         mov dword [rsp+40h],1
00FB9AC7  660F7F442460             movdqa [rsp+60h],xmm0
00FB9ACD  4C89642420               mov [rsp+20h],r12
00FB9AD2  48C7442478FFFFFFFF       mov qword [rsp+78h],0FFFFFFFFFFFFFFFFh
00FB9ADB  48897D80                 mov [rbp-80h],rdi
00FB9ADF  4C897D90                 mov [rbp-70h],r15
00FB9AE3  4C897D98                 mov [rbp-68h],r15
00FB9AE7  48897D88                 mov [rbp-78h],rdi
00FB9AEB  48897C2450               mov [rsp+50h],rdi
00FB9AF0  48897C2448               mov [rsp+48h],rdi
00FB9AF5  49FFC0                   inc r8
00FB9AF8  42383C02                 cmp [rdx+r8],dil
00FB9AFC  75F7                     jne short R_FB9AF5
00FB9AFE  488D4C2458               lea rcx,[rsp+58h]
00FB9B03  E8BC4A3900               call R_134E5C4
00FB9B08  488D05A1190703           lea rax,[rel R_402B4B0]
00FB9B0F  0F57C0                   xorps xmm0,xmm0
00FB9B12  4889442420               mov [rsp+20h],rax
00FB9B17  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FB9B1E  488B4608                 mov rax,[rsi+8]
00FB9B22  4A8B14F0                 mov rdx,[rax+r14*8]
00FB9B26  488D0573180703           lea rax,[rel R_402B3A0]
00FB9B2D  488945B0                 mov [rbp-50h],rax
00FB9B31  48C745B8FFFFFFFF         mov qword [rbp-48h],0FFFFFFFFFFFFFFFFh
00FB9B39  48897DC0                 mov [rbp-40h],rdi
00FB9B3D  4C897DE8                 mov [rbp-18h],r15
00FB9B41  48897DC8                 mov [rbp-38h],rdi
00FB9B45  C745D004000000           mov dword [rbp-30h],4
00FB9B4C  48897DD8                 mov [rbp-28h],rdi
00FB9B50  48897DE0                 mov [rbp-20h],rdi
00FB9B54  660F7F45F0               movdqa [rbp-10h],xmm0
00FB9B59  0F1F8000000000           nop dword [rax]
00FB9B60  49FFC0                   inc r8
00FB9B63  42383C02                 cmp [rdx+r8],dil
00FB9B67  75F7                     jne short R_FB9B60
00FB9B69  488D4DE8                 lea rcx,[rbp-18h]
00FB9B6D  E8524A3900               call R_134E5C4
00FB9B72  488B742450               mov rsi,[rsp+50h]
00FB9B77  488D05AA180703           lea rax,[rel R_402B428]
00FB9B7E  488945B0                 mov [rbp-50h],rax
00FB9B82  40887D00                 mov [rbp],dil
00FB9B86  4885F6                   test rsi,rsi
00FB9B89  7423                     je short R_FB9BAE
00FB9B8B  0F1F440000               nop dword [rax+rax]
00FB9B90  488B06                   mov rax,[rsi]
00FB9B93  488BCE                   mov rcx,rsi
00FB9B96  FF5058                   call qword [rax+58h]
00FB9B99  4C8BF0                   mov r14,rax
00FB9B9C  4885C0                   test rax,rax
00FB9B9F  753B                     jne short R_FB9BDC
00FB9BA1  488B7640                 mov rsi,[rsi+40h]
00FB9BA5  4885F6                   test rsi,rsi
00FB9BA8  75E6                     jne short R_FB9B90
00FB9BAA  488B45B0                 mov rax,[rbp-50h]
00FB9BAE  397DD0                   cmp [rbp-30h],edi
00FB9BB1  0F85D8000000             jne near R_FB9C8F
00FB9BB7  488D742420               lea rsi,[rsp+20h]
00FB9BBC  0F1F4000                 nop dword [rax]
00FB9BC0  488B06                   mov rax,[rsi]
00FB9BC3  488BCE                   mov rcx,rsi
00FB9BC6  FF5020                   call qword [rax+20h]
00FB9BC9  4885C0                   test rax,rax
00FB9BCC  7551                     jne short R_FB9C1F
00FB9BCE  488B7618                 mov rsi,[rsi+18h]
00FB9BD2  4885F6                   test rsi,rsi
00FB9BD5  75E9                     jne short R_FB9BC0
00FB9BD7  E9B0010000               jmp R_FB9D8C
00FB9BDC  488B55E8                 mov rdx,[rbp-18h]
00FB9BE0  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FB9BE7  4883C210                 add rdx,10h
00FB9BEB  0F1F440000               nop dword [rax+rax]
00FB9BF0  49FFC0                   inc r8
00FB9BF3  42383C02                 cmp [rdx+r8],dil
00FB9BF7  75F7                     jne short R_FB9BF0
00FB9BF9  488D4838                 lea rcx,[rax+38h]
00FB9BFD  E8C2493900               call R_134E5C4
00FB9C02  488B45C0                 mov rax,[rbp-40h]
00FB9C06  49894610                 mov [r14+10h],rax
00FB9C0A  488B45B8                 mov rax,[rbp-48h]
00FB9C0E  49894608                 mov [r14+8],rax
00FB9C12  0FB64500                 movzx eax,byte [rbp]
00FB9C16  41884650                 mov [r14+50h],al
00FB9C1A  E96D010000               jmp R_FB9D8C
00FB9C1F  488B06                   mov rax,[rsi]
00FB9C22  488BCE                   mov rcx,rsi
00FB9C25  FF5020                   call qword [rax+20h]
00FB9C28  4885C0                   test rax,rax
00FB9C2B  0F845B010000             je near R_FB9D8C
00FB9C31  488D742420               lea rsi,[rsp+20h]
00FB9C36  488B06                   mov rax,[rsi]
00FB9C39  488BCE                   mov rcx,rsi
00FB9C3C  FF5020                   call qword [rax+20h]
00FB9C3F  4885C0                   test rax,rax
00FB9C42  750E                     jne short R_FB9C52
00FB9C44  488B7618                 mov rsi,[rsi+18h]
00FB9C48  4885F6                   test rsi,rsi
00FB9C4B  75E9                     jne short R_FB9C36
00FB9C4D  488BF7                   mov rsi,rdi
00FB9C50  EB0C                     jmp short R_FB9C5E
00FB9C52  488B06                   mov rax,[rsi]
00FB9C55  488BCE                   mov rcx,rsi
00FB9C58  FF5020                   call qword [rax+20h]
00FB9C5B  488BF0                   mov rsi,rax
00FB9C5E  40387E50                 cmp [rsi+50h],dil
00FB9C62  0F8524010000             jne near R_FB9D8C
00FB9C68  488B157995C202           mov rdx,[rel R_3BE31E8]
00FB9C6F  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FB9C76  C6465001                 mov byte [rsi+50h],1
00FB9C7A  C746540F000000           mov dword [rsi+54h],0Fh
00FB9C81  49FFC0                   inc r8
00FB9C84  42383C02                 cmp [rdx+r8],dil
00FB9C88  75F7                     jne short R_FB9C81
00FB9C8A  E9BA000000               jmp R_FB9D49
00FB9C8F  488D4DB0                 lea rcx,[rbp-50h]
00FB9C93  FF5078                   call qword [rax+78h]
00FB9C96  488BC8                   mov rcx,rax
00FB9C99  4885C0                   test rax,rax
00FB9C9C  0F84EA000000             je near R_FB9D8C
00FB9CA2  397820                   cmp [rax+20h],edi
00FB9CA5  0F85B1000000             jne near R_FB9D5C
00FB9CAB  4C8B00                   mov r8,[rax]
00FB9CAE  BA01000000               mov edx,1
00FB9CB3  41FF10                   call qword [r8]
00FB9CB6  488D742420               lea rsi,[rsp+20h]
00FB9CBB  0F1F440000               nop dword [rax+rax]
00FB9CC0  488B06                   mov rax,[rsi]
00FB9CC3  488BCE                   mov rcx,rsi
00FB9CC6  FF5020                   call qword [rax+20h]
00FB9CC9  4885C0                   test rax,rax
00FB9CCC  750E                     jne short R_FB9CDC
00FB9CCE  488B7618                 mov rsi,[rsi+18h]
00FB9CD2  4885F6                   test rsi,rsi
00FB9CD5  75E9                     jne short R_FB9CC0
00FB9CD7  E9B0000000               jmp R_FB9D8C
00FB9CDC  488B06                   mov rax,[rsi]
00FB9CDF  488BCE                   mov rcx,rsi
00FB9CE2  FF5020                   call qword [rax+20h]
00FB9CE5  4885C0                   test rax,rax
00FB9CE8  0F849E000000             je near R_FB9D8C
00FB9CEE  488D742420               lea rsi,[rsp+20h]
00FB9CF3  488B06                   mov rax,[rsi]
00FB9CF6  488BCE                   mov rcx,rsi
00FB9CF9  FF5020                   call qword [rax+20h]
00FB9CFC  4885C0                   test rax,rax
00FB9CFF  750E                     jne short R_FB9D0F
00FB9D01  488B7618                 mov rsi,[rsi+18h]
00FB9D05  4885F6                   test rsi,rsi
00FB9D08  75E9                     jne short R_FB9CF3
00FB9D0A  488BF7                   mov rsi,rdi
00FB9D0D  EB0C                     jmp short R_FB9D1B
00FB9D0F  488B06                   mov rax,[rsi]
00FB9D12  488BCE                   mov rcx,rsi
00FB9D15  FF5020                   call qword [rax+20h]
00FB9D18  488BF0                   mov rsi,rax
00FB9D1B  40387E50                 cmp [rsi+50h],dil
00FB9D1F  756B                     jne short R_FB9D8C
00FB9D21  488B15C094C202           mov rdx,[rel R_3BE31E8]
00FB9D28  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FB9D2F  C6465001                 mov byte [rsi+50h],1
00FB9D33  C746540F000000           mov dword [rsi+54h],0Fh
00FB9D3A  660F1F440000             nop word [rax+rax]
00FB9D40  49FFC0                   inc r8
00FB9D43  42383C02                 cmp [rdx+r8],dil
00FB9D47  75F7                     jne short R_FB9D40
00FB9D49  488D4E58                 lea rcx,[rsi+58h]
00FB9D4D  E872483900               call R_134E5C4
00FB9D52  48C74664FFFFFFFF         mov qword [rsi+64h],0FFFFFFFFFFFFFFFFh
00FB9D5A  EB30                     jmp short R_FB9D8C
00FB9D5C  488D442420               lea rax,[rsp+20h]
00FB9D61  48894118                 mov [rcx+18h],rax
00FB9D65  488B442450               mov rax,[rsp+50h]
00FB9D6A  48894140                 mov [rcx+40h],rax
00FB9D6E  48897948                 mov [rcx+48h],rdi
00FB9D72  488B442450               mov rax,[rsp+50h]
00FB9D77  4885C0                   test rax,rax
00FB9D7A  7406                     je short R_FB9D82
00FB9D7C  48894848                 mov [rax+48h],rcx
00FB9D80  EB05                     jmp short R_FB9D87
00FB9D82  48894C2448               mov [rsp+48h],rcx
00FB9D87  48894C2450               mov [rsp+50h],rcx
00FB9D8C  397C2440                 cmp [rsp+40h],edi
00FB9D90  0F8569010000             jne near R_FB9EFF
00FB9D96  488BF3                   mov rsi,rbx
00FB9D99  4885DB                   test rbx,rbx
00FB9D9C  741D                     je short R_FB9DBB
00FB9D9E  6690                     xchg ax,ax
00FB9DA0  488B06                   mov rax,[rsi]
00FB9DA3  488BCE                   mov rcx,rsi
00FB9DA6  FF5020                   call qword [rax+20h]
00FB9DA9  4885C0                   test rax,rax
00FB9DAC  0F85CD000000             jne near R_FB9E7F
00FB9DB2  488B7618                 mov rsi,[rsi+18h]
00FB9DB6  4885F6                   test rsi,rsi
00FB9DB9  75E5                     jne short R_FB9DA0
00FB9DBB  488B5DD8                 mov rbx,[rbp-28h]
00FB9DBF  4C8D35F2680D03           lea r14,[rel R_40906B8]
00FB9DC6  4C8975B0                 mov [rbp-50h],r14
00FB9DCA  4885DB                   test rbx,rbx
00FB9DCD  7417                     je short R_FB9DE6
00FB9DCF  90                       nop
00FB9DD0  488BCB                   mov rcx,rbx
00FB9DD3  BA01000000               mov edx,1
00FB9DD8  488B5B48                 mov rbx,[rbx+48h]
00FB9DDC  488B01                   mov rax,[rcx]
00FB9DDF  FF10                     call qword [rax]
00FB9DE1  4885DB                   test rbx,rbx
00FB9DE4  75EA                     jne short R_FB9DD0
00FB9DE6  488B4DE8                 mov rcx,[rbp-18h]
00FB9DEA  493BCF                   cmp rcx,r15
00FB9DED  7405                     je short R_FB9DF4
00FB9DEF  E854FF0101               call R_1FD9D48
00FB9DF4  488B5C2448               mov rbx,[rsp+48h]
00FB9DF9  488D3598EBBC02           lea rsi,[rel R_3B88998]
00FB9E00  488975B0                 mov [rbp-50h],rsi
00FB9E04  4C89642420               mov [rsp+20h],r12
00FB9E09  4885DB                   test rbx,rbx
00FB9E0C  7418                     je short R_FB9E26
00FB9E0E  6690                     xchg ax,ax
00FB9E10  488BCB                   mov rcx,rbx
00FB9E13  BA01000000               mov edx,1
00FB9E18  488B5B48                 mov rbx,[rbx+48h]
00FB9E1C  488B01                   mov rax,[rcx]
00FB9E1F  FF10                     call qword [rax]
00FB9E21  4885DB                   test rbx,rbx
00FB9E24  75EA                     jne short R_FB9E10
00FB9E26  48897C2448               mov [rsp+48h],rdi
00FB9E2B  48897C2450               mov [rsp+50h],rdi
00FB9E30  488B45A8                 mov rax,[rbp-58h]
00FB9E34  0F1F4000                 nop dword [rax]
00FB9E38  0F1F840000000000         nop dword [rax+rax]
00FB9E40  488D4C2470               lea rcx,[rsp+70h]
00FB9E45  483BC1                   cmp rax,rcx
00FB9E48  0F8448020000             je near R_FBA096
00FB9E4E  4885C0                   test rax,rax
00FB9E51  0F843F020000             je near R_FBA096
00FB9E57  4C8BC0                   mov r8,rax
00FB9E5A  488BC8                   mov rcx,rax
00FB9E5D  0F1F00                   nop dword [rax]
00FB9E60  488B5138                 mov rdx,[rcx+38h]
00FB9E64  483BC8                   cmp rcx,rax
00FB9E67  0F84EE010000             je near R_FBA05B
00FB9E6D  488BCA                   mov rcx,rdx
00FB9E70  488D542470               lea rdx,[rsp+70h]
00FB9E75  483BCA                   cmp rcx,rdx
00FB9E78  75E6                     jne short R_FB9E60
00FB9E7A  E9FC010000               jmp R_FBA07B
00FB9E7F  488B06                   mov rax,[rsi]
00FB9E82  488BCE                   mov rcx,rsi
00FB9E85  FF5020                   call qword [rax+20h]
00FB9E88  4885C0                   test rax,rax
00FB9E8B  0F842AFFFFFF             je near R_FB9DBB
00FB9E91  488B03                   mov rax,[rbx]
00FB9E94  488BCB                   mov rcx,rbx
00FB9E97  FF5020                   call qword [rax+20h]
00FB9E9A  4885C0                   test rax,rax
00FB9E9D  750E                     jne short R_FB9EAD
00FB9E9F  488B5B18                 mov rbx,[rbx+18h]
00FB9EA3  4885DB                   test rbx,rbx
00FB9EA6  75E9                     jne short R_FB9E91
00FB9EA8  488BDF                   mov rbx,rdi
00FB9EAB  EB0C                     jmp short R_FB9EB9
00FB9EAD  488B03                   mov rax,[rbx]
00FB9EB0  488BCB                   mov rcx,rbx
00FB9EB3  FF5020                   call qword [rax+20h]
00FB9EB6  488BD8                   mov rbx,rax
00FB9EB9  40387B50                 cmp [rbx+50h],dil
00FB9EBD  0F85F8FEFFFF             jne near R_FB9DBB
00FB9EC3  488B151E93C202           mov rdx,[rel R_3BE31E8]
00FB9ECA  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FB9ED1  C6435001                 mov byte [rbx+50h],1
00FB9ED5  C743540F000000           mov dword [rbx+54h],0Fh
00FB9EDC  0F1F4000                 nop dword [rax]
00FB9EE0  49FFC0                   inc r8
00FB9EE3  42383C02                 cmp [rdx+r8],dil
00FB9EE7  75F7                     jne short R_FB9EE0
00FB9EE9  488D4B58                 lea rcx,[rbx+58h]
00FB9EED  E8D2463900               call R_134E5C4
00FB9EF2  48C74364FFFFFFFF         mov qword [rbx+64h],0FFFFFFFFFFFFFFFFh
00FB9EFA  E9BCFEFFFF               jmp R_FB9DBB
00FB9EFF  488B442420               mov rax,[rsp+20h]
00FB9F04  488D4C2420               lea rcx,[rsp+20h]
00FB9F09  FF5078                   call qword [rax+78h]
00FB9F0C  4885C0                   test rax,rax
00FB9F0F  0F84A6FEFFFF             je near R_FB9DBB
00FB9F15  488BD0                   mov rdx,rax
00FB9F18  488BCB                   mov rcx,rbx
00FB9F1B  E858909C01               call R_2982F78
00FB9F20  4885C0                   test rax,rax
00FB9F23  0F8492FEFFFF             je near R_FB9DBB
00FB9F29  488B5DD8                 mov rbx,[rbp-28h]
00FB9F2D  4C8D3584670D03           lea r14,[rel R_40906B8]
00FB9F34  4C8975B0                 mov [rbp-50h],r14
00FB9F38  4885DB                   test rbx,rbx
00FB9F3B  7419                     je short R_FB9F56
00FB9F3D  0F1F00                   nop dword [rax]
00FB9F40  488BCB                   mov rcx,rbx
00FB9F43  BA01000000               mov edx,1
00FB9F48  488B5B48                 mov rbx,[rbx+48h]
00FB9F4C  488B01                   mov rax,[rcx]
00FB9F4F  FF10                     call qword [rax]
00FB9F51  4885DB                   test rbx,rbx
00FB9F54  75EA                     jne short R_FB9F40
00FB9F56  488B4DE8                 mov rcx,[rbp-18h]
00FB9F5A  493BCF                   cmp rcx,r15
00FB9F5D  7405                     je short R_FB9F64
00FB9F5F  E8E4FD0101               call R_1FD9D48
00FB9F64  488B5C2448               mov rbx,[rsp+48h]
00FB9F69  488D3528EABC02           lea rsi,[rel R_3B88998]
00FB9F70  488975B0                 mov [rbp-50h],rsi
00FB9F74  4C89642420               mov [rsp+20h],r12
00FB9F79  4885DB                   test rbx,rbx
00FB9F7C  7418                     je short R_FB9F96
00FB9F7E  6690                     xchg ax,ax
00FB9F80  488BCB                   mov rcx,rbx
00FB9F83  BA01000000               mov edx,1
00FB9F88  488B5B48                 mov rbx,[rbx+48h]
00FB9F8C  488B01                   mov rax,[rcx]
00FB9F8F  FF10                     call qword [rax]
00FB9F91  4885DB                   test rbx,rbx
00FB9F94  75EA                     jne short R_FB9F80
00FB9F96  48897C2448               mov [rsp+48h],rdi
00FB9F9B  48897C2450               mov [rsp+50h],rdi
00FB9FA0  488B45A8                 mov rax,[rbp-58h]
00FB9FA4  488D4C2470               lea rcx,[rsp+70h]
00FB9FA9  483BC1                   cmp rax,rcx
00FB9FAC  745E                     je short R_FBA00C
00FB9FAE  4885C0                   test rax,rax
00FB9FB1  7459                     je short R_FBA00C
00FB9FB3  4C8BC0                   mov r8,rax
00FB9FB6  488BC8                   mov rcx,rax
00FB9FB9  0F1F8000000000           nop dword [rax]
00FB9FC0  488B5138                 mov rdx,[rcx+38h]
00FB9FC4  483BC8                   cmp rcx,rax
00FB9FC7  740F                     je short R_FB9FD8
00FB9FC9  488BCA                   mov rcx,rdx
00FB9FCC  488D542470               lea rdx,[rsp+70h]
00FB9FD1  483BCA                   cmp rcx,rdx
00FB9FD4  75EA                     jne short R_FB9FC0
00FB9FD6  EB20                     jmp short R_FB9FF8
00FB9FD8  488B4130                 mov rax,[rcx+30h]
00FB9FDC  48895038                 mov [rax+38h],rdx
00FB9FE0  488B4130                 mov rax,[rcx+30h]
00FB9FE4  488B5138                 mov rdx,[rcx+38h]
00FB9FE8  48894230                 mov [rdx+30h],rax
00FB9FEC  48897938                 mov [rcx+38h],rdi
00FB9FF0  48897930                 mov [rcx+30h],rdi
00FB9FF4  488B45A8                 mov rax,[rbp-58h]
00FB9FF8  4D85C0                   test r8,r8
00FB9FFB  74A7                     je short R_FB9FA4
00FB9FFD  498B00                   mov rax,[r8]
00FBA000  BA01000000               mov edx,1
00FBA005  498BC8                   mov rcx,r8
00FBA008  FF10                     call qword [rax]
00FBA00A  EB94                     jmp short R_FB9FA0
00FBA00C  488B4D98                 mov rcx,[rbp-68h]
00FBA010  493BCF                   cmp rcx,r15
00FBA013  7405                     je short R_FBA01A
00FBA015  E82EFD0101               call R_1FD9D48
00FBA01A  488B4D90                 mov rcx,[rbp-70h]
00FBA01E  493BCF                   cmp rcx,r15
00FBA021  7405                     je short R_FBA028
00FBA023  E820FD0101               call R_1FD9D48
00FBA028  488B5C2448               mov rbx,[rsp+48h]
00FBA02D  4889742470               mov [rsp+70h],rsi
00FBA032  4C89742420               mov [rsp+20h],r14
00FBA037  4885DB                   test rbx,rbx
00FBA03A  0F8426070000             je near R_FBA766
00FBA040  488BCB                   mov rcx,rbx
00FBA043  BA01000000               mov edx,1
00FBA048  488B5B48                 mov rbx,[rbx+48h]
00FBA04C  488B01                   mov rax,[rcx]
00FBA04F  FF10                     call qword [rax]
00FBA051  4885DB                   test rbx,rbx
00FBA054  75EA                     jne short R_FBA040
00FBA056  E90B070000               jmp R_FBA766
00FBA05B  488B4130                 mov rax,[rcx+30h]
00FBA05F  48895038                 mov [rax+38h],rdx
00FBA063  488B4130                 mov rax,[rcx+30h]
00FBA067  488B5138                 mov rdx,[rcx+38h]
00FBA06B  48894230                 mov [rdx+30h],rax
00FBA06F  48897938                 mov [rcx+38h],rdi
00FBA073  48897930                 mov [rcx+30h],rdi
00FBA077  488B45A8                 mov rax,[rbp-58h]
00FBA07B  4D85C0                   test r8,r8
00FBA07E  0F84BCFDFFFF             je near R_FB9E40
00FBA084  498B00                   mov rax,[r8]
00FBA087  BA01000000               mov edx,1
00FBA08C  498BC8                   mov rcx,r8
00FBA08F  FF10                     call qword [rax]
00FBA091  E99AFDFFFF               jmp R_FB9E30
00FBA096  488B4D98                 mov rcx,[rbp-68h]
00FBA09A  493BCF                   cmp rcx,r15
00FBA09D  7405                     je short R_FBA0A4
00FBA09F  E8A4FC0101               call R_1FD9D48
00FBA0A4  488B4D90                 mov rcx,[rbp-70h]
00FBA0A8  493BCF                   cmp rcx,r15
00FBA0AB  7405                     je short R_FBA0B2
00FBA0AD  E896FC0101               call R_1FD9D48
00FBA0B2  488B5C2448               mov rbx,[rsp+48h]
00FBA0B7  4889742470               mov [rsp+70h],rsi
00FBA0BC  4C89742420               mov [rsp+20h],r14
00FBA0C1  4885DB                   test rbx,rbx
00FBA0C4  0F843C070000             je near R_FBA806
00FBA0CA  660F1F440000             nop word [rax+rax]
00FBA0D0  488BCB                   mov rcx,rbx
00FBA0D3  BA01000000               mov edx,1
00FBA0D8  488B5B48                 mov rbx,[rbx+48h]
00FBA0DC  488B01                   mov rax,[rcx]
00FBA0DF  FF10                     call qword [rax]
00FBA0E1  4885DB                   test rbx,rbx
00FBA0E4  75EA                     jne short R_FBA0D0
00FBA0E6  E91B070000               jmp R_FBA806
00FBA0EB  44397028                 cmp [rax+28h],r14d
00FBA0EF  0F8F20070000             jg near R_FBA815
00FBA0F5  443B702C                 cmp r14d,[rax+2Ch]
00FBA0F9  0F8F16070000             jg near R_FBA815
00FBA0FF  0F57C0                   xorps xmm0,xmm0
00FBA102  488D15EBA29302           lea rdx,[rel R_38F43F4]
00FBA109  458BC6                   mov r8d,r14d
00FBA10C  488D4D10                 lea rcx,[rbp+10h]
00FBA110  0F114510                 movups [rbp+10h],xmm0
00FBA114  E857CD0101               call R_1FD6E70
00FBA119  0FB64510                 movzx eax,byte [rbp+10h]
00FBA11D  4C8D3D24E27103           lea r15,[rel R_46D8348]
00FBA124  884610                   mov [rsi+10h],al
00FBA127  4C8D2502650D03           lea r12,[rel R_4090630]
00FBA12E  0FB64511                 movzx eax,byte [rbp+11h]
00FBA132  33FF                     xor edi,edi
00FBA134  884611                   mov [rsi+11h],al
00FBA137  0F57C0                   xorps xmm0,xmm0
00FBA13A  0FB64512                 movzx eax,byte [rbp+12h]
00FBA13E  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FBA145  884612                   mov [rsi+12h],al
00FBA148  0FB64513                 movzx eax,byte [rbp+13h]
00FBA14C  884613                   mov [rsi+13h],al
00FBA14F  0FB64514                 movzx eax,byte [rbp+14h]
00FBA153  884614                   mov [rsi+14h],al
00FBA156  0FB64515                 movzx eax,byte [rbp+15h]
00FBA15A  884615                   mov [rsi+15h],al
00FBA15D  0FB64516                 movzx eax,byte [rbp+16h]
00FBA161  884616                   mov [rsi+16h],al
00FBA164  0FB64517                 movzx eax,byte [rbp+17h]
00FBA168  884617                   mov [rsi+17h],al
00FBA16B  0FB64518                 movzx eax,byte [rbp+18h]
00FBA16F  884618                   mov [rsi+18h],al
00FBA172  0FB64519                 movzx eax,byte [rbp+19h]
00FBA176  884619                   mov [rsi+19h],al
00FBA179  0FB6451A                 movzx eax,byte [rbp+1Ah]
00FBA17D  88461A                   mov [rsi+1Ah],al
00FBA180  0FB6451B                 movzx eax,byte [rbp+1Bh]
00FBA184  88461B                   mov [rsi+1Bh],al
00FBA187  0FB6451C                 movzx eax,byte [rbp+1Ch]
00FBA18B  88461C                   mov [rsi+1Ch],al
00FBA18E  0FB6451D                 movzx eax,byte [rbp+1Dh]
00FBA192  88461D                   mov [rsi+1Dh],al
00FBA195  0FB6451E                 movzx eax,byte [rbp+1Eh]
00FBA199  88461E                   mov [rsi+1Eh],al
00FBA19C  0FB6451F                 movzx eax,byte [rbp+1Fh]
00FBA1A0  88461F                   mov [rsi+1Fh],al
00FBA1A3  488D05E6630D03           lea rax,[rel R_4090590]
00FBA1AA  488B16                   mov rdx,[rsi]
00FBA1AD  4889442470               mov [rsp+70h],rax
00FBA1B2  488D442470               lea rax,[rsp+70h]
00FBA1B7  488945A8                 mov [rbp-58h],rax
00FBA1BB  488D442470               lea rax,[rsp+70h]
00FBA1C0  488945A0                 mov [rbp-60h],rax
00FBA1C4  48C7442428FFFFFFFF       mov qword [rsp+28h],0FFFFFFFFFFFFFFFFh
00FBA1CD  48897C2430               mov [rsp+30h],rdi
00FBA1D2  4C897C2458               mov [rsp+58h],r15
00FBA1D7  48897C2438               mov [rsp+38h],rdi
00FBA1DC  C744244001000000         mov dword [rsp+40h],1
00FBA1E4  660F7F442460             movdqa [rsp+60h],xmm0
00FBA1EA  4C89642420               mov [rsp+20h],r12
00FBA1EF  48C7442478FFFFFFFF       mov qword [rsp+78h],0FFFFFFFFFFFFFFFFh
00FBA1F8  48897D80                 mov [rbp-80h],rdi
00FBA1FC  4C897D90                 mov [rbp-70h],r15
00FBA200  4C897D98                 mov [rbp-68h],r15
00FBA204  48897D88                 mov [rbp-78h],rdi
00FBA208  48897C2450               mov [rsp+50h],rdi
00FBA20D  48897C2448               mov [rsp+48h],rdi
00FBA212  49FFC0                   inc r8
00FBA215  42383C02                 cmp [rdx+r8],dil
00FBA219  75F7                     jne short R_FBA212
00FBA21B  488D4C2458               lea rcx,[rsp+58h]
00FBA220  E89F433900               call R_134E5C4
00FBA225  488D0584120703           lea rax,[rel R_402B4B0]
00FBA22C  48C745B8FFFFFFFF         mov qword [rbp-48h],0FFFFFFFFFFFFFFFFh
00FBA234  4889442420               mov [rsp+20h],rax
00FBA239  0F57C0                   xorps xmm0,xmm0
00FBA23C  488D055D110703           lea rax,[rel R_402B3A0]
00FBA243  48897DC0                 mov [rbp-40h],rdi
00FBA247  488945B0                 mov [rbp-50h],rax
00FBA24B  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FBA252  4C897DE8                 mov [rbp-18h],r15
00FBA256  48897DC8                 mov [rbp-38h],rdi
00FBA25A  C745D004000000           mov dword [rbp-30h],4
00FBA261  48897DD8                 mov [rbp-28h],rdi
00FBA265  48897DE0                 mov [rbp-20h],rdi
00FBA269  660F7F45F0               movdqa [rbp-10h],xmm0
00FBA26E  6690                     xchg ax,ax
00FBA270  49FFC0                   inc r8
00FBA273  42387C0610               cmp [rsi+r8+10h],dil
00FBA278  75F6                     jne short R_FBA270
00FBA27A  488D5610                 lea rdx,[rsi+10h]
00FBA27E  488D4DE8                 lea rcx,[rbp-18h]
00FBA282  E83D433900               call R_134E5C4
00FBA287  488B742450               mov rsi,[rsp+50h]
00FBA28C  488D0595110703           lea rax,[rel R_402B428]
00FBA293  488945B0                 mov [rbp-50h],rax
00FBA297  40887D00                 mov [rbp],dil
00FBA29B  4885F6                   test rsi,rsi
00FBA29E  741E                     je short R_FBA2BE
00FBA2A0  488B06                   mov rax,[rsi]
00FBA2A3  488BCE                   mov rcx,rsi
00FBA2A6  FF5058                   call qword [rax+58h]
00FBA2A9  4C8BF0                   mov r14,rax
00FBA2AC  4885C0                   test rax,rax
00FBA2AF  753B                     jne short R_FBA2EC
00FBA2B1  488B7640                 mov rsi,[rsi+40h]
00FBA2B5  4885F6                   test rsi,rsi
00FBA2B8  75E6                     jne short R_FBA2A0
00FBA2BA  488B45B0                 mov rax,[rbp-50h]
00FBA2BE  397DD0                   cmp [rbp-30h],edi
00FBA2C1  0F85D8000000             jne near R_FBA39F
00FBA2C7  488D742420               lea rsi,[rsp+20h]
00FBA2CC  0F1F4000                 nop dword [rax]
00FBA2D0  488B06                   mov rax,[rsi]
00FBA2D3  488BCE                   mov rcx,rsi
00FBA2D6  FF5020                   call qword [rax+20h]
00FBA2D9  4885C0                   test rax,rax
00FBA2DC  7551                     jne short R_FBA32F
00FBA2DE  488B7618                 mov rsi,[rsi+18h]
00FBA2E2  4885F6                   test rsi,rsi
00FBA2E5  75E9                     jne short R_FBA2D0
00FBA2E7  E9B0010000               jmp R_FBA49C
00FBA2EC  488B55E8                 mov rdx,[rbp-18h]
00FBA2F0  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FBA2F7  4883C210                 add rdx,10h
00FBA2FB  0F1F440000               nop dword [rax+rax]
00FBA300  49FFC0                   inc r8
00FBA303  42383C02                 cmp [rdx+r8],dil
00FBA307  75F7                     jne short R_FBA300
00FBA309  488D4838                 lea rcx,[rax+38h]
00FBA30D  E8B2423900               call R_134E5C4
00FBA312  488B45C0                 mov rax,[rbp-40h]
00FBA316  49894610                 mov [r14+10h],rax
00FBA31A  488B45B8                 mov rax,[rbp-48h]
00FBA31E  49894608                 mov [r14+8],rax
00FBA322  0FB64500                 movzx eax,byte [rbp]
00FBA326  41884650                 mov [r14+50h],al
00FBA32A  E96D010000               jmp R_FBA49C
00FBA32F  488B06                   mov rax,[rsi]
00FBA332  488BCE                   mov rcx,rsi
00FBA335  FF5020                   call qword [rax+20h]
00FBA338  4885C0                   test rax,rax
00FBA33B  0F845B010000             je near R_FBA49C
00FBA341  488D742420               lea rsi,[rsp+20h]
00FBA346  488B06                   mov rax,[rsi]
00FBA349  488BCE                   mov rcx,rsi
00FBA34C  FF5020                   call qword [rax+20h]
00FBA34F  4885C0                   test rax,rax
00FBA352  750E                     jne short R_FBA362
00FBA354  488B7618                 mov rsi,[rsi+18h]
00FBA358  4885F6                   test rsi,rsi
00FBA35B  75E9                     jne short R_FBA346
00FBA35D  488BF7                   mov rsi,rdi
00FBA360  EB0C                     jmp short R_FBA36E
00FBA362  488B06                   mov rax,[rsi]
00FBA365  488BCE                   mov rcx,rsi
00FBA368  FF5020                   call qword [rax+20h]
00FBA36B  488BF0                   mov rsi,rax
00FBA36E  40387E50                 cmp [rsi+50h],dil
00FBA372  0F8524010000             jne near R_FBA49C
00FBA378  488B15698EC202           mov rdx,[rel R_3BE31E8]
00FBA37F  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FBA386  C6465001                 mov byte [rsi+50h],1
00FBA38A  C746540F000000           mov dword [rsi+54h],0Fh
00FBA391  49FFC0                   inc r8
00FBA394  42383C02                 cmp [rdx+r8],dil
00FBA398  75F7                     jne short R_FBA391
00FBA39A  E9BA000000               jmp R_FBA459
00FBA39F  488D4DB0                 lea rcx,[rbp-50h]
00FBA3A3  FF5078                   call qword [rax+78h]
00FBA3A6  488BC8                   mov rcx,rax
00FBA3A9  4885C0                   test rax,rax
00FBA3AC  0F84EA000000             je near R_FBA49C
00FBA3B2  397820                   cmp [rax+20h],edi
00FBA3B5  0F85B1000000             jne near R_FBA46C
00FBA3BB  4C8B00                   mov r8,[rax]
00FBA3BE  BA01000000               mov edx,1
00FBA3C3  41FF10                   call qword [r8]
00FBA3C6  488D742420               lea rsi,[rsp+20h]
00FBA3CB  0F1F440000               nop dword [rax+rax]
00FBA3D0  488B06                   mov rax,[rsi]
00FBA3D3  488BCE                   mov rcx,rsi
00FBA3D6  FF5020                   call qword [rax+20h]
00FBA3D9  4885C0                   test rax,rax
00FBA3DC  750E                     jne short R_FBA3EC
00FBA3DE  488B7618                 mov rsi,[rsi+18h]
00FBA3E2  4885F6                   test rsi,rsi
00FBA3E5  75E9                     jne short R_FBA3D0
00FBA3E7  E9B0000000               jmp R_FBA49C
00FBA3EC  488B06                   mov rax,[rsi]
00FBA3EF  488BCE                   mov rcx,rsi
00FBA3F2  FF5020                   call qword [rax+20h]
00FBA3F5  4885C0                   test rax,rax
00FBA3F8  0F849E000000             je near R_FBA49C
00FBA3FE  488D742420               lea rsi,[rsp+20h]
00FBA403  488B06                   mov rax,[rsi]
00FBA406  488BCE                   mov rcx,rsi
00FBA409  FF5020                   call qword [rax+20h]
00FBA40C  4885C0                   test rax,rax
00FBA40F  750E                     jne short R_FBA41F
00FBA411  488B7618                 mov rsi,[rsi+18h]
00FBA415  4885F6                   test rsi,rsi
00FBA418  75E9                     jne short R_FBA403
00FBA41A  488BF7                   mov rsi,rdi
00FBA41D  EB0C                     jmp short R_FBA42B
00FBA41F  488B06                   mov rax,[rsi]
00FBA422  488BCE                   mov rcx,rsi
00FBA425  FF5020                   call qword [rax+20h]
00FBA428  488BF0                   mov rsi,rax
00FBA42B  40387E50                 cmp [rsi+50h],dil
00FBA42F  756B                     jne short R_FBA49C
00FBA431  488B15B08DC202           mov rdx,[rel R_3BE31E8]
00FBA438  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FBA43F  C6465001                 mov byte [rsi+50h],1
00FBA443  C746540F000000           mov dword [rsi+54h],0Fh
00FBA44A  660F1F440000             nop word [rax+rax]
00FBA450  49FFC0                   inc r8
00FBA453  42383C02                 cmp [rdx+r8],dil
00FBA457  75F7                     jne short R_FBA450
00FBA459  488D4E58                 lea rcx,[rsi+58h]
00FBA45D  E862413900               call R_134E5C4
00FBA462  48C74664FFFFFFFF         mov qword [rsi+64h],0FFFFFFFFFFFFFFFFh
00FBA46A  EB30                     jmp short R_FBA49C
00FBA46C  488D442420               lea rax,[rsp+20h]
00FBA471  48894118                 mov [rcx+18h],rax
00FBA475  488B442450               mov rax,[rsp+50h]
00FBA47A  48894140                 mov [rcx+40h],rax
00FBA47E  48897948                 mov [rcx+48h],rdi
00FBA482  488B442450               mov rax,[rsp+50h]
00FBA487  4885C0                   test rax,rax
00FBA48A  7406                     je short R_FBA492
00FBA48C  48894848                 mov [rax+48h],rcx
00FBA490  EB05                     jmp short R_FBA497
00FBA492  48894C2448               mov [rsp+48h],rcx
00FBA497  48894C2450               mov [rsp+50h],rcx
00FBA49C  397C2440                 cmp [rsp+40h],edi
00FBA4A0  0F8569010000             jne near R_FBA60F
00FBA4A6  488BF3                   mov rsi,rbx
00FBA4A9  4885DB                   test rbx,rbx
00FBA4AC  741D                     je short R_FBA4CB
00FBA4AE  6690                     xchg ax,ax
00FBA4B0  488B06                   mov rax,[rsi]
00FBA4B3  488BCE                   mov rcx,rsi
00FBA4B6  FF5020                   call qword [rax+20h]
00FBA4B9  4885C0                   test rax,rax
00FBA4BC  0F85CD000000             jne near R_FBA58F
00FBA4C2  488B7618                 mov rsi,[rsi+18h]
00FBA4C6  4885F6                   test rsi,rsi
00FBA4C9  75E5                     jne short R_FBA4B0
00FBA4CB  488B5DD8                 mov rbx,[rbp-28h]
00FBA4CF  4C8D35E2610D03           lea r14,[rel R_40906B8]
00FBA4D6  4C8975B0                 mov [rbp-50h],r14
00FBA4DA  4885DB                   test rbx,rbx
00FBA4DD  7417                     je short R_FBA4F6
00FBA4DF  90                       nop
00FBA4E0  488BCB                   mov rcx,rbx
00FBA4E3  BA01000000               mov edx,1
00FBA4E8  488B5B48                 mov rbx,[rbx+48h]
00FBA4EC  488B01                   mov rax,[rcx]
00FBA4EF  FF10                     call qword [rax]
00FBA4F1  4885DB                   test rbx,rbx
00FBA4F4  75EA                     jne short R_FBA4E0
00FBA4F6  488B4DE8                 mov rcx,[rbp-18h]
00FBA4FA  493BCF                   cmp rcx,r15
00FBA4FD  7405                     je short R_FBA504
00FBA4FF  E844F80101               call R_1FD9D48
00FBA504  488B5C2448               mov rbx,[rsp+48h]
00FBA509  488D3588E4BC02           lea rsi,[rel R_3B88998]
00FBA510  488975B0                 mov [rbp-50h],rsi
00FBA514  4C89642420               mov [rsp+20h],r12
00FBA519  4885DB                   test rbx,rbx
00FBA51C  7418                     je short R_FBA536
00FBA51E  6690                     xchg ax,ax
00FBA520  488BCB                   mov rcx,rbx
00FBA523  BA01000000               mov edx,1
00FBA528  488B5B48                 mov rbx,[rbx+48h]
00FBA52C  488B01                   mov rax,[rcx]
00FBA52F  FF10                     call qword [rax]
00FBA531  4885DB                   test rbx,rbx
00FBA534  75EA                     jne short R_FBA520
00FBA536  48897C2448               mov [rsp+48h],rdi
00FBA53B  48897C2450               mov [rsp+50h],rdi
00FBA540  488B45A8                 mov rax,[rbp-58h]
00FBA544  0F1F4000                 nop dword [rax]
00FBA548  0F1F840000000000         nop dword [rax+rax]
00FBA550  488D4C2470               lea rcx,[rsp+70h]
00FBA555  483BC1                   cmp rax,rcx
00FBA558  0F8459020000             je near R_FBA7B7
00FBA55E  4885C0                   test rax,rax
00FBA561  0F8450020000             je near R_FBA7B7
00FBA567  4C8BC0                   mov r8,rax
00FBA56A  488BC8                   mov rcx,rax
00FBA56D  0F1F00                   nop dword [rax]
00FBA570  488B5138                 mov rdx,[rcx+38h]
00FBA574  483BC8                   cmp rcx,rax
00FBA577  0F84FF010000             je near R_FBA77C
00FBA57D  488BCA                   mov rcx,rdx
00FBA580  488D542470               lea rdx,[rsp+70h]
00FBA585  483BCA                   cmp rcx,rdx
00FBA588  75E6                     jne short R_FBA570
00FBA58A  E90D020000               jmp R_FBA79C
00FBA58F  488B06                   mov rax,[rsi]
00FBA592  488BCE                   mov rcx,rsi
00FBA595  FF5020                   call qword [rax+20h]
00FBA598  4885C0                   test rax,rax
00FBA59B  0F842AFFFFFF             je near R_FBA4CB
00FBA5A1  488B03                   mov rax,[rbx]
00FBA5A4  488BCB                   mov rcx,rbx
00FBA5A7  FF5020                   call qword [rax+20h]
00FBA5AA  4885C0                   test rax,rax
00FBA5AD  750E                     jne short R_FBA5BD
00FBA5AF  488B5B18                 mov rbx,[rbx+18h]
00FBA5B3  4885DB                   test rbx,rbx
00FBA5B6  75E9                     jne short R_FBA5A1
00FBA5B8  488BDF                   mov rbx,rdi
00FBA5BB  EB0C                     jmp short R_FBA5C9
00FBA5BD  488B03                   mov rax,[rbx]
00FBA5C0  488BCB                   mov rcx,rbx
00FBA5C3  FF5020                   call qword [rax+20h]
00FBA5C6  488BD8                   mov rbx,rax
00FBA5C9  40387B50                 cmp [rbx+50h],dil
00FBA5CD  0F85F8FEFFFF             jne near R_FBA4CB
00FBA5D3  488B150E8CC202           mov rdx,[rel R_3BE31E8]
00FBA5DA  49C7C0FFFFFFFF           mov r8,0FFFFFFFFFFFFFFFFh
00FBA5E1  C6435001                 mov byte [rbx+50h],1
00FBA5E5  C743540F000000           mov dword [rbx+54h],0Fh
00FBA5EC  0F1F4000                 nop dword [rax]
00FBA5F0  49FFC0                   inc r8
00FBA5F3  42383C02                 cmp [rdx+r8],dil
00FBA5F7  75F7                     jne short R_FBA5F0
00FBA5F9  488D4B58                 lea rcx,[rbx+58h]
00FBA5FD  E8C23F3900               call R_134E5C4
00FBA602  48C74364FFFFFFFF         mov qword [rbx+64h],0FFFFFFFFFFFFFFFFh
00FBA60A  E9BCFEFFFF               jmp R_FBA4CB
00FBA60F  488B442420               mov rax,[rsp+20h]
00FBA614  488D4C2420               lea rcx,[rsp+20h]
00FBA619  FF5078                   call qword [rax+78h]
00FBA61C  4885C0                   test rax,rax
00FBA61F  0F84A6FEFFFF             je near R_FBA4CB
00FBA625  488BD0                   mov rdx,rax
00FBA628  488BCB                   mov rcx,rbx
00FBA62B  E848899C01               call R_2982F78
00FBA630  4885C0                   test rax,rax
00FBA633  0F8492FEFFFF             je near R_FBA4CB
00FBA639  488B5DD8                 mov rbx,[rbp-28h]
00FBA63D  4C8D3574600D03           lea r14,[rel R_40906B8]
00FBA644  4C8975B0                 mov [rbp-50h],r14
00FBA648  4885DB                   test rbx,rbx
00FBA64B  7419                     je short R_FBA666
00FBA64D  0F1F00                   nop dword [rax]
00FBA650  488BCB                   mov rcx,rbx
00FBA653  BA01000000               mov edx,1
00FBA658  488B5B48                 mov rbx,[rbx+48h]
00FBA65C  488B01                   mov rax,[rcx]
00FBA65F  FF10                     call qword [rax]
00FBA661  4885DB                   test rbx,rbx
00FBA664  75EA                     jne short R_FBA650
00FBA666  488B4DE8                 mov rcx,[rbp-18h]
00FBA66A  493BCF                   cmp rcx,r15
00FBA66D  7405                     je short R_FBA674
00FBA66F  E8D4F60101               call R_1FD9D48
00FBA674  488B5C2448               mov rbx,[rsp+48h]
00FBA679  488D3518E3BC02           lea rsi,[rel R_3B88998]
00FBA680  488975B0                 mov [rbp-50h],rsi
00FBA684  4C89642420               mov [rsp+20h],r12
00FBA689  4885DB                   test rbx,rbx
00FBA68C  7418                     je short R_FBA6A6
00FBA68E  6690                     xchg ax,ax
00FBA690  488BCB                   mov rcx,rbx
00FBA693  BA01000000               mov edx,1
00FBA698  488B5B48                 mov rbx,[rbx+48h]
00FBA69C  488B01                   mov rax,[rcx]
00FBA69F  FF10                     call qword [rax]
00FBA6A1  4885DB                   test rbx,rbx
00FBA6A4  75EA                     jne short R_FBA690
00FBA6A6  48897C2448               mov [rsp+48h],rdi
00FBA6AB  48897C2450               mov [rsp+50h],rdi
00FBA6B0  488B45A8                 mov rax,[rbp-58h]
00FBA6B4  488D4C2470               lea rcx,[rsp+70h]
00FBA6B9  483BC1                   cmp rax,rcx
00FBA6BC  745E                     je short R_FBA71C
00FBA6BE  4885C0                   test rax,rax
00FBA6C1  7459                     je short R_FBA71C
00FBA6C3  4C8BC0                   mov r8,rax
00FBA6C6  488BC8                   mov rcx,rax
00FBA6C9  0F1F8000000000           nop dword [rax]
00FBA6D0  488B5138                 mov rdx,[rcx+38h]
00FBA6D4  483BC8                   cmp rcx,rax
00FBA6D7  740F                     je short R_FBA6E8
00FBA6D9  488BCA                   mov rcx,rdx
00FBA6DC  488D542470               lea rdx,[rsp+70h]
00FBA6E1  483BCA                   cmp rcx,rdx
00FBA6E4  75EA                     jne short R_FBA6D0
00FBA6E6  EB20                     jmp short R_FBA708
00FBA6E8  488B4130                 mov rax,[rcx+30h]
00FBA6EC  48895038                 mov [rax+38h],rdx
00FBA6F0  488B4130                 mov rax,[rcx+30h]
00FBA6F4  488B5138                 mov rdx,[rcx+38h]
00FBA6F8  48894230                 mov [rdx+30h],rax
00FBA6FC  48897938                 mov [rcx+38h],rdi
00FBA700  48897930                 mov [rcx+30h],rdi
00FBA704  488B45A8                 mov rax,[rbp-58h]
00FBA708  4D85C0                   test r8,r8
00FBA70B  74A7                     je short R_FBA6B4
00FBA70D  498B00                   mov rax,[r8]
00FBA710  BA01000000               mov edx,1
00FBA715  498BC8                   mov rcx,r8
00FBA718  FF10                     call qword [rax]
00FBA71A  EB94                     jmp short R_FBA6B0
00FBA71C  488B4D98                 mov rcx,[rbp-68h]
00FBA720  493BCF                   cmp rcx,r15
00FBA723  7405                     je short R_FBA72A
00FBA725  E81EF60101               call R_1FD9D48
00FBA72A  488B4D90                 mov rcx,[rbp-70h]
00FBA72E  493BCF                   cmp rcx,r15
00FBA731  7405                     je short R_FBA738
00FBA733  E810F60101               call R_1FD9D48
00FBA738  488B5C2448               mov rbx,[rsp+48h]
00FBA73D  4889742470               mov [rsp+70h],rsi
00FBA742  4C89742420               mov [rsp+20h],r14
00FBA747  4885DB                   test rbx,rbx
00FBA74A  741A                     je short R_FBA766
00FBA74C  0F1F4000                 nop dword [rax]
00FBA750  488BCB                   mov rcx,rbx
00FBA753  BA01000000               mov edx,1
00FBA758  488B5B48                 mov rbx,[rbx+48h]
00FBA75C  488B01                   mov rax,[rcx]
00FBA75F  FF10                     call qword [rax]
00FBA761  4885DB                   test rbx,rbx
00FBA764  75EA                     jne short R_FBA750
00FBA766  488B4C2458               mov rcx,[rsp+58h]
00FBA76B  493BCF                   cmp rcx,r15
00FBA76E  7405                     je short R_FBA775
00FBA770  E8D3F50101               call R_1FD9D48
00FBA775  B001                     mov al,1
00FBA777  E99B000000               jmp R_FBA817
00FBA77C  488B4130                 mov rax,[rcx+30h]
00FBA780  48895038                 mov [rax+38h],rdx
00FBA784  488B4130                 mov rax,[rcx+30h]
00FBA788  488B5138                 mov rdx,[rcx+38h]
00FBA78C  48894230                 mov [rdx+30h],rax
00FBA790  48897938                 mov [rcx+38h],rdi
00FBA794  48897930                 mov [rcx+30h],rdi
00FBA798  488B45A8                 mov rax,[rbp-58h]
00FBA79C  4D85C0                   test r8,r8
00FBA79F  0F84ABFDFFFF             je near R_FBA550
00FBA7A5  498B00                   mov rax,[r8]
00FBA7A8  BA01000000               mov edx,1
00FBA7AD  498BC8                   mov rcx,r8
00FBA7B0  FF10                     call qword [rax]
00FBA7B2  E989FDFFFF               jmp R_FBA540
00FBA7B7  488B4D98                 mov rcx,[rbp-68h]
00FBA7BB  493BCF                   cmp rcx,r15
00FBA7BE  7405                     je short R_FBA7C5
00FBA7C0  E883F50101               call R_1FD9D48
00FBA7C5  488B4D90                 mov rcx,[rbp-70h]
00FBA7C9  493BCF                   cmp rcx,r15
00FBA7CC  7405                     je short R_FBA7D3
00FBA7CE  E875F50101               call R_1FD9D48
00FBA7D3  488B5C2448               mov rbx,[rsp+48h]
00FBA7D8  4889742470               mov [rsp+70h],rsi
00FBA7DD  4C89742420               mov [rsp+20h],r14
00FBA7E2  4885DB                   test rbx,rbx
00FBA7E5  741F                     je short R_FBA806
00FBA7E7  660F1F840000000000       nop word [rax+rax]
00FBA7F0  488BCB                   mov rcx,rbx
00FBA7F3  BA01000000               mov edx,1
00FBA7F8  488B5B48                 mov rbx,[rbx+48h]
00FBA7FC  488B01                   mov rax,[rcx]
00FBA7FF  FF10                     call qword [rax]
00FBA801  4885DB                   test rbx,rbx
00FBA804  75EA                     jne short R_FBA7F0
00FBA806  488B4C2458               mov rcx,[rsp+58h]
00FBA80B  493BCF                   cmp rcx,r15
00FBA80E  7405                     je short R_FBA815
00FBA810  E833F50101               call R_1FD9D48
00FBA815  32C0                     xor al,al
00FBA817  488B4D20                 mov rcx,[rbp+20h]
00FBA81B  4833CC                   xor rcx,rsp
00FBA81E  E8BD6ABDFF               call R_B912E0
00FBA823  4881C430010000           add rsp,130h
00FBA82A  415F                     pop r15
00FBA82C  415E                     pop r14
00FBA82E  415C                     pop r12
00FBA830  5F                       pop rdi
00FBA831  5E                       pop rsi
00FBA832  5B                       pop rbx
00FBA833  5D                       pop rbp
00FBA834  C3                       ret
