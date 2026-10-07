== func 1FE5D8C-1FE5EFE
01FE5D8C  48895C2418               mov [rsp+18h],rbx
01FE5D91  4889742420               mov [rsp+20h],rsi
01FE5D96  55                       push rbp
01FE5D97  57                       push rdi
01FE5D98  4156                     push r14
01FE5D9A  488DAC2430FEFFFF         lea rbp,[rsp-1D0h]
01FE5DA2  4881ECD0020000           sub rsp,2D0h
01FE5DA9  488B0580214D02           mov rax,[rel R_44B7F30]
01FE5DB0  4833C4                   xor rax,rsp
01FE5DB3  488985C0010000           mov [rbp+1C0h],rax
01FE5DBA  4863FA                   movsxd rdi,edx
01FE5DBD  E80AB7FFFF               call R_1FE14CC
01FE5DC2  33DB                     xor ebx,ebx
01FE5DC4  488BF0                   mov rsi,rax
01FE5DC7  4885C0                   test rax,rax
01FE5DCA  7442                     je short R_1FE5E0E
01FE5DCC  41BE04010000             mov r14d,104h
01FE5DD2  488D4DA0                 lea rcx,[rbp-60h]
01FE5DD6  458BC6                   mov r8d,r14d
01FE5DD9  33D2                     xor edx,edx
01FE5DDB  E820E1BAFE               call R_B93F00
01FE5DE0  488B4608                 mov rax,[rsi+8]
01FE5DE4  4885C0                   test rax,rax
01FE5DE7  744E                     je short R_1FE5E37
01FE5DE9  85FF                     test edi,edi
01FE5DEB  781C                     js short R_1FE5E09
01FE5DED  3B7E24                   cmp edi,[rsi+24h]
01FE5DF0  7D17                     jge short R_1FE5E09
01FE5DF2  4C8B0CF8                 mov r9,[rax+rdi*8]
01FE5DF6  4C8D05434A9001           lea r8,[rel R_38EA840]
01FE5DFD  498BD6                   mov rdx,r14
01FE5E00  488D4DA0                 lea rcx,[rbp-60h]
01FE5E04  E877DF12FE               call R_113D80
01FE5E09  385DA0                   cmp [rbp-60h],bl
01FE5E0C  755C                     jne short R_1FE5E6A
01FE5E0E  32C0                     xor al,al
01FE5E10  488B8DC0010000           mov rcx,[rbp+1C0h]
01FE5E17  4833CC                   xor rcx,rsp
01FE5E1A  E8C1B4BAFE               call R_B912E0
01FE5E1F  4C8D9C24D0020000         lea r11,[rsp+2D0h]
01FE5E27  498B5B30                 mov rbx,[r11+30h]
01FE5E2B  498B7338                 mov rsi,[r11+38h]
01FE5E2F  498BE3                   mov rsp,r11
01FE5E32  415E                     pop r14
01FE5E34  5F                       pop rdi
01FE5E35  5D                       pop rbp
01FE5E36  C3                       ret
01FE5E37  397E28                   cmp [rsi+28h],edi
01FE5E3A  7FCD                     jg short R_1FE5E09
01FE5E3C  3B7E2C                   cmp edi,[rsi+2Ch]
01FE5E3F  7FC8                     jg short R_1FE5E09
01FE5E41  0F57C0                   xorps xmm0,xmm0
01FE5E44  488D15A9E59001           lea rdx,[rel R_38F43F4]
01FE5E4B  448BC7                   mov r8d,edi
01FE5E4E  488D4D90                 lea rcx,[rbp-70h]
01FE5E52  0F114590                 movups [rbp-70h],xmm0
01FE5E56  E81510FFFF               call R_1FD6E70
01FE5E5B  0F104590                 movups xmm0,[rbp-70h]
01FE5E5F  4C8D4E10                 lea r9,[rsi+10h]
01FE5E63  F3410F7F01               movdqu [r9],xmm0
01FE5E68  EB8C                     jmp short R_1FE5DF6
01FE5E6A  488D4C2420               lea rcx,[rsp+20h]
01FE5E6F  E8001BFFFF               call R_1FD7974
01FE5E74  E8074DFDFE               call R_FBAB80
01FE5E79  488BD0                   mov rdx,rax
01FE5E7C  488D4C2420               lea rcx,[rsp+20h]
01FE5E81  E83ED39900               call R_29831C4
01FE5E86  84C0                     test al,al
01FE5E88  7457                     je short R_1FE5EE1
01FE5E8A  4D8BC6                   mov r8,r14
01FE5E8D  488D8DB0000000           lea rcx,[rbp+0B0h]
01FE5E94  33D2                     xor edx,edx
01FE5E96  E865E0BAFE               call R_B93F00
01FE5E9B  4C8B0E                   mov r9,[rsi]
01FE5E9E  4C8D05B346AB01           lea r8,[rel R_3A9A558]
01FE5EA5  498BD6                   mov rdx,r14
01FE5EA8  488D8DB0000000           lea rcx,[rbp+0B0h]
01FE5EAF  E8CCDE12FE               call R_113D80
01FE5EB4  4C8D85B0000000           lea r8,[rbp+0B0h]
01FE5EBB  488D55A0                 lea rdx,[rbp-60h]
01FE5EBF  488D4C2420               lea rcx,[rsp+20h]
01FE5EC4  E897719A00               call R_298D060
01FE5EC9  84C0                     test al,al
01FE5ECB  7414                     je short R_1FE5EE1
01FE5ECD  E8AE4CFDFE               call R_FBAB80
01FE5ED2  488BD0                   mov rdx,rax
01FE5ED5  488D4C2420               lea rcx,[rsp+20h]
01FE5EDA  E895F19900               call R_2985074
01FE5EDF  8AD8                     mov bl,al
01FE5EE1  488D0530540402           lea rax,[rel R_402B318]
01FE5EE8  488D4C2420               lea rcx,[rsp+20h]
01FE5EED  4889442420               mov [rsp+20h],rax
01FE5EF2  E85137FDFE               call R_FB9648
01FE5EF7  8AC3                     mov al,bl
01FE5EF9  E912FFFFFF               jmp R_1FE5E10
== func 1FE6064-1FE6379
01FE6064  48895C2408               mov [rsp+8],rbx
01FE6069  4889742410               mov [rsp+10h],rsi
01FE606E  48897C2418               mov [rsp+18h],rdi
01FE6073  4154                     push r12
01FE6075  4156                     push r14
01FE6077  4157                     push r15
01FE6079  4881EC60010000           sub rsp,160h
01FE6080  33DB                     xor ebx,ebx
01FE6082  4C8D3DA74E5D02           lea r15,[rel R_45BAF30]
01FE6089  4183CCFF                 or r12d,0FFFFFFFFh
01FE608D  4C8BF1                   mov r14,rcx
01FE6090  418B3E                   mov edi,[r14]
01FE6093  8BCB                     mov ecx,ebx
01FE6095  E85EC2FFFF               call R_1FE22F8
01FE609A  3BC7                     cmp eax,edi
01FE609C  0F8480020000             je near R_1FE6322
01FE60A2  8BD7                     mov edx,edi
01FE60A4  8BCB                     mov ecx,ebx
01FE60A6  E845DFFFFF               call R_1FE3FF0
01FE60AB  84C0                     test al,al
01FE60AD  0F846F020000             je near R_1FE6322
01FE60B3  4863F3                   movsxd rsi,ebx
01FE60B6  41893CB7                 mov [r15+rsi*4],edi
01FE60BA  85DB                     test ebx,ebx
01FE60BC  7518                     jne short R_1FE60D6
01FE60BE  443925075C5D02           cmp [rel R_45BBCCC],r12d
01FE60C5  0F8557020000             jne near R_1FE6322
01FE60CB  893DFB5B5D02             mov [rel R_45BBCCC],edi
01FE60D1  E94C020000               jmp R_1FE6322
01FE60D6  8D43FA                   lea eax,[rbx-6]
01FE60D9  A9FDFFFFFF               test eax,0FFFFFFFDh
01FE60DE  7508                     jne short R_1FE60E8
01FE60E0  498BCF                   mov rcx,r15
01FE60E3  E8A857FDFE               call R_FBB890
01FE60E8  83FB09                   cmp ebx,9
01FE60EB  7405                     je short R_1FE60F2
01FE60ED  83FB22                   cmp ebx,22h
01FE60F0  7505                     jne short R_1FE60F7
01FE60F2  E84147FDFE               call R_FBA838
01FE60F7  83FB01                   cmp ebx,1
01FE60FA  751D                     jne short R_1FE6119
01FE60FC  8BCF                     mov ecx,edi
01FE60FE  E8D1DEFFFF               call R_1FE3FD4
01FE6103  84C0                     test al,al
01FE6105  0F8517020000             jne near R_1FE6322
01FE610B  E814C3FFFF               call R_1FE2424
01FE6110  418904B7                 mov [r15+rsi*4],eax
01FE6114  E909020000               jmp R_1FE6322
01FE6119  83FB02                   cmp ebx,2
01FE611C  0F85DC000000             jne near R_1FE61FE
01FE6122  E8619EFFFF               call R_1FDFF88
01FE6127  0FA3F8                   bt eax,edi
01FE612A  7209                     jb short R_1FE6135
01FE612C  E82FAAFFFF               call R_1FE0B60
01FE6131  418904B7                 mov [r15+rsi*4],eax
01FE6135  E846180000               call R_1FE7980
01FE613A  4D631CB7                 movsxd r11,[r15+rsi*4]
01FE613E  488D4C2420               lea rcx,[rsp+20h]
01FE6143  4969C338010000           imul rax,r11,138h
01FE614A  BA02000000               mov edx,2
01FE614F  480560010000             add rax,160h
01FE6155  4903C7                   add rax,r15
01FE6158  0F1000                   movups xmm0,[rax]
01FE615B  0F104810                 movups xmm1,[rax+10h]
01FE615F  488D8080000000           lea rax,[rax+80h]
01FE6166  0F1101                   movups [rcx],xmm0
01FE6169  0F1040A0                 movups xmm0,[rax-60h]
01FE616D  0F114910                 movups [rcx+10h],xmm1
01FE6171  0F1048B0                 movups xmm1,[rax-50h]
01FE6175  0F114120                 movups [rcx+20h],xmm0
01FE6179  0F1040C0                 movups xmm0,[rax-40h]
01FE617D  0F114930                 movups [rcx+30h],xmm1
01FE6181  0F1048D0                 movups xmm1,[rax-30h]
01FE6185  0F114140                 movups [rcx+40h],xmm0
01FE6189  0F1040E0                 movups xmm0,[rax-20h]
01FE618D  0F114950                 movups [rcx+50h],xmm1
01FE6191  0F1048F0                 movups xmm1,[rax-10h]
01FE6195  0F114160                 movups [rcx+60h],xmm0
01FE6199  488D8980000000           lea rcx,[rcx+80h]
01FE61A0  0F1149F0                 movups [rcx-10h],xmm1
01FE61A4  4883EA01                 sub rdx,1
01FE61A8  75AE                     jne short R_1FE6158
01FE61AA  0F1000                   movups xmm0,[rax]
01FE61AD  0F104810                 movups xmm1,[rax+10h]
01FE61B1  0F1101                   movups [rcx],xmm0
01FE61B4  0F104020                 movups xmm0,[rax+20h]
01FE61B8  488B4030                 mov rax,[rax+30h]
01FE61BC  0F114910                 movups [rcx+10h],xmm1
01FE61C0  0F114120                 movups [rcx+20h],xmm0
01FE61C4  48894130                 mov [rcx+30h],rax
01FE61C8  81BC242001000086800000   cmp dword [rsp+120h],8086h
01FE61D3  0F9405145B5D02           sete [rel R_45BBCEE]
01FE61DA  418B0CB7                 mov ecx,[r15+rsi*4]
01FE61DE  E80DDAFFFF               call R_1FE3BF0
01FE61E3  84C0                     test al,al
01FE61E5  0F8437010000             je near R_1FE6322
01FE61EB  8325C2324D0200           and dword [rel R_44B94B4],0
01FE61F2  8325B7324D0200           and dword [rel R_44B94B0],0
01FE61F9  E924010000               jmp R_1FE6322
01FE61FE  83FB0B                   cmp ebx,0Bh
01FE6201  7522                     jne short R_1FE6225
01FE6203  443925CA5A5D02           cmp [rel R_45BBCD4],r12d
01FE620A  7515                     jne short R_1FE6221
01FE620C  E8038EFFFF               call R_1FDF014
01FE6211  84C0                     test al,al
01FE6213  750C                     jne short R_1FE6221
01FE6215  33C9                     xor ecx,ecx
01FE6217  E82C81FFFF               call R_1FDE348
01FE621C  E901010000               jmp R_1FE6322
01FE6221  8BCF                     mov ecx,edi
01FE6223  EBF2                     jmp short R_1FE6217
01FE6225  83FB0C                   cmp ebx,0Ch
01FE6228  750C                     jne short R_1FE6236
01FE622A  8BCF                     mov ecx,edi
01FE622C  E8F380FFFF               call R_1FDE324
01FE6231  E9EC000000               jmp R_1FE6322
01FE6236  83FB0D                   cmp ebx,0Dh
01FE6239  750C                     jne short R_1FE6247
01FE623B  8BCF                     mov ecx,edi
01FE623D  E86E80FFFF               call R_1FDE2B0
01FE6242  E9DB000000               jmp R_1FE6322
01FE6247  83FB0E                   cmp ebx,0Eh
01FE624A  750C                     jne short R_1FE6258
01FE624C  8BCF                     mov ecx,edi
01FE624E  E82981FFFF               call R_1FDE37C
01FE6253  E9CA000000               jmp R_1FE6322
01FE6258  83FB0F                   cmp ebx,0Fh
01FE625B  750C                     jne short R_1FE6269
01FE625D  8BCF                     mov ecx,edi
01FE625F  E86C80FFFF               call R_1FDE2D0
01FE6264  E9B9000000               jmp R_1FE6322
01FE6269  83FB10                   cmp ebx,10h
01FE626C  750C                     jne short R_1FE627A
01FE626E  8BCF                     mov ecx,edi
01FE6270  E88780FFFF               call R_1FDE2FC
01FE6275  E9A8000000               jmp R_1FE6322
01FE627A  83FB11                   cmp ebx,11h
01FE627D  7514                     jne short R_1FE6293
01FE627F  83FF64                   cmp edi,64h
01FE6282  8D4321                   lea eax,[rbx+21h]
01FE6285  0F46C7                   cmovbe eax,edi
01FE6288  8905425A5D02             mov [rel R_45BBCD0],eax
01FE628E  E98F000000               jmp R_1FE6322
01FE6293  83FB2F                   cmp ebx,2Fh
01FE6296  7533                     jne short R_1FE62CB
01FE6298  8B0D4E4D5D02             mov ecx,[rel R_45BAFEC]
01FE629E  E891DCFFFF               call R_1FE3F34
01FE62A3  84C0                     test al,al
01FE62A5  757B                     jne short R_1FE6322
01FE62A7  8B0DA74C5D02             mov ecx,[rel R_45BAF54]
01FE62AD  E872A7FFFF               call R_1FE0A24
01FE62B2  8BC8                     mov ecx,eax
01FE62B4  448BC8                   mov r9d,eax
01FE62B7  E878DCFFFF               call R_1FE3F34
01FE62BC  F6D8                     neg al
01FE62BE  1BC9                     sbb ecx,ecx
01FE62C0  4123C9                   and ecx,r9d
01FE62C3  890D234D5D02             mov [rel R_45BAFEC],ecx
01FE62C9  EB57                     jmp short R_1FE6322
01FE62CB  83FB13                   cmp ebx,13h
01FE62CE  750E                     jne short R_1FE62DE
01FE62D0  85FF                     test edi,edi
01FE62D2  410F44FC                 cmove edi,r12d
01FE62D6  893DA04C5D02             mov [rel R_45BAF7C],edi
01FE62DC  EB44                     jmp short R_1FE6322
01FE62DE  83FB30                   cmp ebx,30h
01FE62E1  7509                     jne short R_1FE62EC
01FE62E3  8BCF                     mov ecx,edi
01FE62E5  E83E7CFFFF               call R_1FDDF28
01FE62EA  EB36                     jmp short R_1FE6322
01FE62EC  83FB31                   cmp ebx,31h
01FE62EF  7504                     jne short R_1FE62F5
01FE62F1  8BCF                     mov ecx,edi
01FE62F3  EB28                     jmp short R_1FE631D
01FE62F5  83FB32                   cmp ebx,32h
01FE62F8  7528                     jne short R_1FE6322
01FE62FA  8BCF                     mov ecx,edi
01FE62FC  E8B781FFFF               call R_1FDE4B8
01FE6301  8B0D494C5D02             mov ecx,[rel R_45BAF50]
01FE6307  E8DC80FFFF               call R_1FDE3E8
01FE630C  8B0DDE4C5D02             mov ecx,[rel R_45BAFF0]
01FE6312  E8117CFFFF               call R_1FDDF28
01FE6317  8B0DD74C5D02             mov ecx,[rel R_45BAFF4]
01FE631D  E8527CFFFF               call R_1FDDF74
01FE6322  FFC3                     inc ebx
01FE6324  4983C604                 add r14,4
01FE6328  83FB3C                   cmp ebx,3Ch
01FE632B  0F825FFDFFFF             jb near R_1FE6090
01FE6331  833DF84B5D0200           cmp dword [rel R_45BAF30],0
01FE6338  7521                     jne short R_1FE635B
01FE633A  8B0D084C5D02             mov ecx,[rel R_45BAF48]
01FE6340  E83FD6FFFF               call R_1FE3984
01FE6345  84C0                     test al,al
01FE6347  7512                     jne short R_1FE635B
01FE6349  E8FAA6FFFF               call R_1FE0A48
01FE634E  8BC8                     mov ecx,eax
01FE6350  8905F24B5D02             mov [rel R_45BAF48],eax
01FE6356  E8A981FFFF               call R_1FDE504
01FE635B  4C8D9C2460010000         lea r11,[rsp+160h]
01FE6363  498B5B20                 mov rbx,[r11+20h]
01FE6367  498B7328                 mov rsi,[r11+28h]
01FE636B  498B7B30                 mov rdi,[r11+30h]
01FE636F  498BE3                   mov rsp,r11
01FE6372  415F                     pop r15
01FE6374  415E                     pop r14
01FE6376  415C                     pop r12
01FE6378  C3                       ret
