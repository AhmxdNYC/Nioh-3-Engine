== func 1C33174-1C33548
01C33174  48895C2410               mov [rsp+10h],rbx
01C33179  4889742418               mov [rsp+18h],rsi
01C3317E  55                       push rbp
01C3317F  57                       push rdi
01C33180  4156                     push r14
01C33182  488DAC24D0F9FFFF         lea rbp,[rsp-630h]
01C3318A  4881EC30070000           sub rsp,730h
01C33191  488B05984D8802           mov rax,[rel R_44B7F30]
01C33198  4833C4                   xor rax,rsp
01C3319B  48898520060000           mov [rbp+620h],rax
01C331A2  8B5110                   mov edx,[rcx+10h]
01C331A5  488BF1                   mov rsi,rcx
01C331A8  83FA06                   cmp edx,6
01C331AB  0F8F05030000             jg near R_1C334B6
01C331B1  0F847C020000             je near R_1C33433
01C331B7  33FF                     xor edi,edi
01C331B9  85D2                     test edx,edx
01C331BB  0F8459020000             je near R_1C3341A
01C331C1  83EA01                   sub edx,1
01C331C4  0F8417020000             je near R_1C333E1
01C331CA  83EA02                   sub edx,2
01C331CD  7435                     je short R_1C33204
01C331CF  83EA01                   sub edx,1
01C331D2  0F8449030000             je near R_1C33521
01C331D8  83FA01                   cmp edx,1
01C331DB  0F8540030000             jne near R_1C33521
01C331E1  E8AEE04BFE               call R_F1294
01C331E6  84C0                     test al,al
01C331E8  0F8533030000             jne near R_1C33521
01C331EE  BABD770000               mov edx,77BDh
01C331F3  E8C410C000               call R_28342BC
01C331F8  C7461006000000           mov dword [rsi+10h],6
01C331FF  E91D030000               jmp R_1C33521
01C33204  E837A0BF00               call R_282D240
01C33209  84C0                     test al,al
01C3320B  0F8510030000             jne near R_1C33521
01C33211  40387E14                 cmp [rsi+14h],dil
01C33215  0F85B0010000             jne near R_1C333CB
01C3321B  B912000000               mov ecx,12h
01C33220  E8C7E13A00               call R_1FE13EC
01C33225  8BC8                     mov ecx,eax
01C33227  E808AE3A00               call R_1FDE034
01C3322C  393DEA7D9802             cmp [rel R_45BB01C],edi
01C33232  7507                     jne short R_1C3323B
01C33234  33C9                     xor ecx,ecx
01C33236  E845B23A00               call R_1FDE480
01C3323B  E868243B00               call R_1FE56A8
01C33240  33D2                     xor edx,edx
01C33242  C6461401                 mov byte [rsi+14h],1
01C33246  41B800040000             mov r8d,400h
01C3324C  488D8D20020000           lea rcx,[rbp+220h]
01C33253  E8A80CF6FE               call R_B93F00
01C33258  B91BFCEB03               mov ecx,3EBFC1Bh
01C3325D  E8A21897FE               call R_5A4B04
01C33262  B900020000               mov ecx,200h
01C33267  48897D00                 mov [rbp],rdi
01C3326B  488BD8                   mov rbx,rax
01C3326E  48894D08                 mov [rbp+8],rcx
01C33272  488D8520020000           lea rax,[rbp+220h]
01C33279  48894DF8                 mov [rbp-8],rcx
01C3327D  488945E8                 mov [rbp-18h],rax
01C33281  4C8D35B0A4CC01           lea r14,[rel R_38FD738]
01C33288  488D8520020000           lea rax,[rbp+220h]
01C3328F  48897DF0                 mov [rbp-10h],rdi
01C33293  B932F6C603               mov ecx,3C6F632h
01C33298  48894510                 mov [rbp+10h],rax
01C3329C  4C8975E0                 mov [rbp-20h],r14
01C332A0  E85F1897FE               call R_5A4B04
01C332A5  4889442430               mov [rsp+30h],rax
01C332AA  488BCF                   mov rcx,rdi
01C332AD  0F10442430               movups xmm0,[rsp+30h]
01C332B2  488D45C0                 lea rax,[rbp-40h]
01C332B6  48895C2420               mov [rsp+20h],rbx
01C332BB  0F104C2420               movups xmm1,[rsp+20h]
01C332C0  4889442428               mov [rsp+28h],rax
01C332C5  488D056CD3CD01           lea rax,[rel R_3910638]
01C332CC  4889442430               mov [rsp+30h],rax
01C332D1  F30F7F45C0               movdqu [rbp-40h],xmm0
01C332D6  48C7442420CC000000       mov qword [rsp+20h],0CCh
01C332DF  F30F7F4DD0               movdqu [rbp-30h],xmm1
01C332E4  48FFC1                   inc rcx
01C332E7  488D4002                 lea rax,[rax+2]
01C332EB  663938                   cmp [rax],di
01C332EE  75F4                     jne short R_1C332E4
01C332F0  0F28442420               movaps xmm0,[rsp+20h]
01C332F5  4C8D442420               lea r8,[rsp+20h]
01C332FA  48894C2438               mov [rsp+38h],rcx
01C332FF  488D542430               lea rdx,[rsp+30h]
01C33304  0F284C2430               movaps xmm1,[rsp+30h]
01C33309  488D4DE0                 lea rcx,[rbp-20h]
01C3330D  660F7F4C2430             movdqa [rsp+30h],xmm1
01C33313  660F7F442420             movdqa [rsp+20h],xmm0
01C33319  E88AAF6DFE               call R_30E2A8
01C3331E  B9FF010000               mov ecx,1FFh
01C33323  4C8975E0                 mov [rbp-20h],r14
01C33327  483BC1                   cmp rax,rcx
01C3332A  480F47C1                 cmova rax,rcx
01C3332E  488D4DE0                 lea rcx,[rbp-20h]
01C33332  6689BC4520020000         mov [rbp+rax*2+220h],di
01C3333A  E80950D8FE               call R_9B8348
01C3333F  B93185C603               mov ecx,3C68531h
01C33344  C7442420687465C8         mov dword [rsp+20h],0C8657468h
01C3334C  C74424248A527DA1         mov dword [rsp+24h],0A17D528Ah
01C33354  C74424284AB42D72         mov dword [rsp+28h],722DB44Ah
01C3335C  C744242C5C6E9075         mov dword [rsp+2Ch],75906E5Ch
01C33364  E89B1797FE               call R_5A4B04
01C33369  488BD0                   mov rdx,rax
01C3336C  488D4DC0                 lea rcx,[rbp-40h]
01C33370  E8FFCF6DFE               call R_310374
01C33375  488BD8                   mov rbx,rax
01C33378  48897588                 mov [rbp-78h],rsi
01C3337C  488D05AD8BE501           lea rax,[rel R_3A8BF30]
01C33383  48894580                 mov [rbp-80h],rax
01C33387  488D9520020000           lea rdx,[rbp+220h]
01C3338E  488D4580                 lea rax,[rbp-80h]
01C33392  488D4C2440               lea rcx,[rsp+40h]
01C33397  488945B8                 mov [rbp-48h],rax
01C3339B  E8D4CF6DFE               call R_310374
01C333A0  0F28442420               movaps xmm0,[rsp+20h]
01C333A5  4C8D4C2430               lea r9,[rsp+30h]
01C333AA  4C8BC3                   mov r8,rbx
01C333AD  660F7F442430             movdqa [rsp+30h],xmm0
01C333B3  488D5580                 lea rdx,[rbp-80h]
01C333B7  488BC8                   mov rcx,rax
01C333BA  E89544BD00               call R_2807854
01C333BF  C7461004000000           mov dword [rsi+10h],4
01C333C6  E956010000               jmp R_1C33521
01C333CB  BAB7710000               mov edx,71B7h
01C333D0  E8E70EC000               call R_28342BC
01C333D5  C7461005000000           mov dword [rsi+10h],5
01C333DC  E940010000               jmp R_1C33521
01C333E1  E8C69EBF00               call R_282D2AC
01C333E6  84C0                     test al,al
01C333E8  0F8533010000             jne near R_1C33521
01C333EE  E899073B00               call R_1FE3B8C
01C333F3  84C0                     test al,al
01C333F5  740A                     je short R_1C33401
01C333F7  B901000000               mov ecx,1
01C333FC  E847AF3A00               call R_1FDE348
01C33401  E8C249C000               call R_2837DC8
01C33406  84C0                     test al,al
01C33408  0F8413010000             je near R_1C33521
01C3340E  C7461003000000           mov dword [rsi+10h],3
01C33415  E907010000               jmp R_1C33521
01C3341A  E8A54AC000               call R_2837EC4
01C3341F  84C0                     test al,al
01C33421  0F84FA000000             je near R_1C33521
01C33427  C7461001000000           mov dword [rsi+10h],1
01C3342E  E9EE000000               jmp R_1C33521
01C33433  E85CDE4BFE               call R_F1294
01C33438  84C0                     test al,al
01C3343A  0F85E1000000             jne near R_1C33521
01C33440  488D05B98AE501           lea rax,[rel R_3A8BF00]
01C33447  C7442420687465C8         mov dword [rsp+20h],0C8657468h
01C3344F  48894580                 mov [rbp-80h],rax
01C33453  B93330C503               mov ecx,3C53033h
01C33458  488D4580                 lea rax,[rbp-80h]
01C3345C  C74424248A527DA1         mov dword [rsp+24h],0A17D528Ah
01C33464  488945B8                 mov [rbp-48h],rax
01C33468  C74424284AB42D72         mov dword [rsp+28h],722DB44Ah
01C33470  C744242C5C6E9075         mov dword [rsp+2Ch],75906E5Ch
01C33478  48897588                 mov [rbp-78h],rsi
01C3347C  E8831697FE               call R_5A4B04
01C33481  488BD0                   mov rdx,rax
01C33484  488D4C2440               lea rcx,[rsp+40h]
01C33489  E8E6CE6DFE               call R_310374
01C3348E  0F28442420               movaps xmm0,[rsp+20h]
01C33493  4C8D442430               lea r8,[rsp+30h]
01C33498  4533C9                   xor r9d,r9d
01C3349B  660F7F442430             movdqa [rsp+30h],xmm0
01C334A1  488D5580                 lea rdx,[rbp-80h]
01C334A5  488BC8                   mov rcx,rax
01C334A8  E83B4B71FE               call R_347FE8
01C334AD  C7461007000000           mov dword [rsi+10h],7
01C334B4  EB6B                     jmp short R_1C33521
01C334B6  83EA07                   sub edx,7
01C334B9  7466                     je short R_1C33521
01C334BB  83EA01                   sub edx,1
01C334BE  7445                     je short R_1C33505
01C334C0  83EA01                   sub edx,1
01C334C3  7413                     je short R_1C334D8
01C334C5  83FA01                   cmp edx,1
01C334C8  7557                     jne short R_1C33521
01C334CA  0F57C0                   xorps xmm0,xmm0
01C334CD  0F11442448               movups [rsp+48h],xmm0
01C334D2  89542450                 mov [rsp+50h],edx
01C334D6  EB08                     jmp short R_1C334E0
01C334D8  0F57C0                   xorps xmm0,xmm0
01C334DB  0F11442448               movups [rsp+48h],xmm0
01C334E0  33C0                     xor eax,eax
01C334E2  C744244001000000         mov dword [rsp+40h],1
01C334EA  488D4C2440               lea rcx,[rsp+40h]
01C334EF  4889442478               mov [rsp+78h],rax
01C334F4  0F11442468               movups [rsp+68h],xmm0
01C334F9  0F11442458               movups [rsp+58h],xmm0
01C334FE  E859B0FFFF               call R_1C2E55C
01C33503  EB1C                     jmp short R_1C33521
01C33505  488B05744F9902           mov rax,[rel R_45C8480]
01C3350C  33FF                     xor edi,edi
01C3350E  397810                   cmp [rax+10h],edi
01C33511  740E                     je short R_1C33521
01C33513  83781001                 cmp dword [rax+10h],1
01C33517  400F95C7                 setne dil
01C3351B  83C709                   add edi,9
01C3351E  897910                   mov [rcx+10h],edi
01C33521  488B8D20060000           mov rcx,[rbp+620h]
01C33528  4833CC                   xor rcx,rsp
01C3352B  E8B0DDF5FE               call R_B912E0
01C33530  4C8D9C2430070000         lea r11,[rsp+730h]
01C33538  498B5B28                 mov rbx,[r11+28h]
01C3353C  498B7330                 mov rsi,[r11+30h]
01C33540  498BE3                   mov rsp,r11
01C33543  415E                     pop r14
01C33545  5F                       pop rdi
01C33546  5D                       pop rbp
01C33547  C3                       ret
== func 1C33FA8-1C3413B
01C33FA8  48895C2408               mov [rsp+8],rbx
01C33FAD  48896C2410               mov [rsp+10h],rbp
01C33FB2  4889742418               mov [rsp+18h],rsi
01C33FB7  57                       push rdi
01C33FB8  4881EC80000000           sub rsp,80h
01C33FBF  488BEA                   mov rbp,rdx
01C33FC2  488BF9                   mov rdi,rcx
01C33FC5  E892D56900               call R_22D155C
01C33FCA  33DB                     xor ebx,ebx
01C33FCC  84C0                     test al,al
01C33FCE  0F844E010000             je near R_1C34122
01C33FD4  448B4710                 mov r8d,[rdi+10h]
01C33FD8  4585C0                   test r8d,r8d
01C33FDB  0F84C4000000             je near R_1C340A5
01C33FE1  4183E801                 sub r8d,1
01C33FE5  0F8493000000             je near R_1C3407E
01C33FEB  4183F801                 cmp r8d,1
01C33FEF  0F850F010000             jne near R_1C34104
01C33FF5  F30F100DE301FA01         movss xmm1,[rel R_3BD41E0]
01C33FFD  0F57C0                   xorps xmm0,xmm0
01C34000  0F28D1                   movaps xmm2,xmm1
01C34003  F30F11442438             movss [rsp+38h],xmm0
01C34009  E82E896300               call R_226C93C
01C3400E  E841C03A00               call R_1FE0054
01C34013  8D7301                   lea esi,[rbx+1]
01C34016  83F804                   cmp eax,4
01C34019  742B                     je short R_1C34046
01C3401B  488B0DEE58B202           mov rcx,[rel R_4759910]
01C34022  4885C9                   test rcx,rcx
01C34025  7406                     je short R_1C3402D
01C34027  898184020000             mov [rcx+284h],eax
01C3402D  2BC6                     sub eax,esi
01C3402F  740F                     je short R_1C34040
01C34031  2BC6                     sub eax,esi
01C34033  7409                     je short R_1C3403E
01C34035  BB02000000               mov ebx,2
01C3403A  3BC6                     cmp eax,esi
01C3403C  7402                     je short R_1C34040
01C3403E  8BDE                     mov ebx,esi
01C34040  891DD66F9802             mov [rel R_45BB01C],ebx
01C34046  488D0D636DAD02           lea rcx,[rel R_470ADB0]
01C3404D  E81E2A3A00               call R_1FD6A70
01C34052  0F57C0                   xorps xmm0,xmm0
01C34055  89742440                 mov [rsp+40h],esi
01C34059  33C0                     xor eax,eax
01C3405B  488D4C2440               lea rcx,[rsp+40h]
01C34060  0F11442448               movups [rsp+48h],xmm0
01C34065  4889442478               mov [rsp+78h],rax
01C3406A  0F11442458               movups [rsp+58h],xmm0
01C3406F  0F11442468               movups [rsp+68h],xmm0
01C34074  E8E3A4FFFF               call R_1C2E55C
01C34079  E986000000               jmp R_1C34104
01C3407E  488B0523449902           mov rax,[rel R_45C84A8]
01C34085  488B8898050000           mov rcx,[rax+598h]
01C3408C  4885C9                   test rcx,rcx
01C3408F  7473                     je short R_1C34104
01C34091  488B01                   mov rax,[rcx]
01C34094  FF5048                   call qword [rax+48h]
01C34097  84C0                     test al,al
01C34099  7569                     jne short R_1C34104
01C3409B  BB02000000               mov ebx,2
01C340A0  895F10                   mov [rdi+10h],ebx
01C340A3  EB5F                     jmp short R_1C34104
01C340A5  488B05FC439902           mov rax,[rel R_45C84A8]
01C340AC  488BB098050000           mov rsi,[rax+598h]
01C340B3  4885F6                   test rsi,rsi
01C340B6  744C                     je short R_1C34104
01C340B8  488B06                   mov rax,[rsi]
01C340BB  488BCE                   mov rcx,rsi
01C340BE  FF9000010000             call qword [rax+100h]
01C340C4  84C0                     test al,al
01C340C6  743C                     je short R_1C34104
01C340C8  488B06                   mov rax,[rsi]
01C340CB  488BCE                   mov rcx,rsi
01C340CE  FF5048                   call qword [rax+48h]
01C340D1  84C0                     test al,al
01C340D3  752F                     jne short R_1C34104
01C340D5  F30F100D0301FA01         movss xmm1,[rel R_3BD41E0]
01C340DD  0F57C0                   xorps xmm0,xmm0
01C340E0  0F57D2                   xorps xmm2,xmm2
01C340E3  F30F11442438             movss [rsp+38h],xmm0
01C340E9  E84E886300               call R_226C93C
01C340EE  385F18                   cmp [rdi+18h],bl
01C340F1  488BCE                   mov rcx,rsi
01C340F4  0F94C2                   sete dl
01C340F7  E81449BA00               call R_27D8A10
01C340FC  BE01000000               mov esi,1
01C34101  897710                   mov [rdi+10h],esi
01C34104  8B4510                   mov eax,[rbp+10h]
01C34107  0F57C0                   xorps xmm0,xmm0
01C3410A  F30F104D14               movss xmm1,[rbp+14h]
01C3410F  F3480F2AC0               cvtsi2ss xmm0,rax
01C34114  F30F5EC8                 divss xmm1,xmm0
01C34118  F30F584F14               addss xmm1,[rdi+14h]
01C3411D  F30F114F14               movss [rdi+14h],xmm1
01C34122  4C8D9C2480000000         lea r11,[rsp+80h]
01C3412A  498B5B10                 mov rbx,[r11+10h]
01C3412E  498B6B18                 mov rbp,[r11+18h]
01C34132  498B7320                 mov rsi,[r11+20h]
01C34136  498BE3                   mov rsp,r11
01C34139  5F                       pop rdi
01C3413A  C3                       ret
== func 298EDA8-298F18B
0298EDA8  488BC4                   mov rax,rsp
0298EDAB  48895810                 mov [rax+10h],rbx
0298EDAF  48897018                 mov [rax+18h],rsi
0298EDB3  48897820                 mov [rax+20h],rdi
0298EDB7  55                       push rbp
0298EDB8  4154                     push r12
0298EDBA  4155                     push r13
0298EDBC  4156                     push r14
0298EDBE  4157                     push r15
0298EDC0  488D68A1                 lea rbp,[rax-5Fh]
0298EDC4  4881ECF0000000           sub rsp,0F0h
0298EDCB  488B055E91B201           mov rax,[rel R_44B7F30]
0298EDD2  4833C4                   xor rax,rsp
0298EDD5  48894527                 mov [rbp+27h],rax
0298EDD9  4533ED                   xor r13d,r13d
0298EDDC  4C8D442434               lea r8,[rsp+34h]
0298EDE1  488BF9                   mov rdi,rcx
0298EDE4  44896C2434               mov [rsp+34h],r13d
0298EDE9  B9FFFF0000               mov ecx,0FFFFh
0298EDEE  33D2                     xor edx,edx
0298EDF0  418BF5                   mov esi,r13d
0298EDF3  458BF5                   mov r14d,r13d
0298EDF6  458BFD                   mov r15d,r13d
0298EDF9  458BE5                   mov r12d,r13d
0298EDFC  FF15AE36F500             call qword [rel R_38E24B0]
0298EE02  85C0                     test eax,eax
0298EE04  0F858F020000             jne near R_298F099
0298EE0A  FF158835F500             call qword [rel R_38E2398]
0298EE10  83F87A                   cmp eax,7Ah
0298EE13  0F8580020000             jne near R_298F099
0298EE19  8B542434                 mov edx,[rsp+34h]
0298EE1D  458D4508                 lea r8d,[r13+8]
0298EE21  488D4C2438               lea rcx,[rsp+38h]
0298EE26  E86946C4FD               call R_5D3494
0298EE2B  488B5C2438               mov rbx,[rsp+38h]
0298EE30  4885DB                   test rbx,rbx
0298EE33  0F8446030000             je near R_298F17F
0298EE39  4C8D442434               lea r8,[rsp+34h]
0298EE3E  488BD3                   mov rdx,rbx
0298EE41  B9FFFF0000               mov ecx,0FFFFh
0298EE46  FF156436F500             call qword [rel R_38E24B0]
0298EE4C  85C0                     test eax,eax
0298EE4E  0F842B030000             je near R_298F17F
0298EE54  448B4C2434               mov r9d,[rsp+34h]
0298EE59  458D5501                 lea r10d,[r13+1]
0298EE5D  418BCD                   mov ecx,r13d
0298EE60  4585C9                   test r9d,r9d
0298EE63  7451                     je short R_298EEB6
0298EE65  8BC1                     mov eax,ecx
0298EE67  4803C3                   add rax,rbx
0298EE6A  44396804                 cmp [rax+4],r13d
0298EE6E  0F860B030000             jbe near R_298F17F
0298EE74  034804                   add ecx,[rax+4]
0298EE77  443928                   cmp [rax],r13d
0298EE7A  7424                     je short R_298EEA0
0298EE7C  833804                   cmp dword [rax],4
0298EE7F  7530                     jne short R_298EEB1
0298EE81  0FB7500A                 movzx edx,word [rax+0Ah]
0298EE85  4885D2                   test rdx,rdx
0298EE88  7427                     je short R_298EEB1
0298EE8A  4C8D4021                 lea r8,[rax+21h]
0298EE8E  410FB600                 movzx eax,byte [r8]
0298EE92  4D8D4030                 lea r8,[r8+30h]
0298EE96  4403F0                   add r14d,eax
0298EE99  492BD2                   sub rdx,r10
0298EE9C  75F0                     jne short R_298EE8E
0298EE9E  EB11                     jmp short R_298EEB1
0298EEA0  4103F2                   add esi,r10d
0298EEA3  44386809                 cmp [rax+9],r13b
0298EEA7  7605                     jbe short R_298EEAE
0298EEA9  4503FA                   add r15d,r10d
0298EEAC  EB03                     jmp short R_298EEB1
0298EEAE  4503E2                   add r12d,r10d
0298EEB1  413BC9                   cmp ecx,r9d
0298EEB4  72AF                     jb short R_298EE65
0298EEB6  488D4C2438               lea rcx,[rsp+38h]
0298EEBB  E8A033C4FD               call R_5D2260
0298EEC0  85F6                     test esi,esi
0298EEC2  0F8ED1010000             jle near R_298F099
0298EEC8  4585F6                   test r14d,r14d
0298EECB  0F8EC8010000             jle near R_298F099
0298EED1  4489B774020000           mov [rdi+274h],r14d
0298EED8  41BE01000000             mov r14d,1
0298EEDE  4488B780020000           mov [rdi+280h],r14b
0298EEE5  89B770020000             mov [rdi+270h],esi
0298EEEB  4489BF78020000           mov [rdi+278h],r15d
0298EEF2  4489A77C020000           mov [rdi+27Ch],r12d
0298EEF9  8B0D1DC1C201             mov ecx,[rel R_45BB01C]
0298EEFF  85C9                     test ecx,ecx
0298EF01  7417                     je short R_298EF1A
0298EF03  412BCE                   sub ecx,r14d
0298EF06  740B                     je short R_298EF13
0298EF08  413BCE                   cmp ecx,r14d
0298EF0B  7506                     jne short R_298EF13
0298EF0D  418D4602                 lea eax,[r14+2]
0298EF11  EB0A                     jmp short R_298EF1D
0298EF13  B802000000               mov eax,2
0298EF18  EB03                     jmp short R_298EF1D
0298EF1A  418BC6                   mov eax,r14d
0298EF1D  33D2                     xor edx,edx
0298EF1F  898784020000             mov [rdi+284h],eax
0298EF25  488D4DA4                 lea rcx,[rbp-5Ch]
0298EF29  448D4240                 lea r8d,[rdx+40h]
0298EF2D  E8CE4F20FE               call R_B93F00
0298EF32  33C9                     xor ecx,ecx
0298EF34  6644896DE4               mov [rbp-1Ch],r13w
0298EF39  33C0                     xor eax,eax
0298EF3B  4C896DE7                 mov [rbp-19h],r13
0298EF3F  0FA2                     cpuid
0298EF41  0F57C0                   xorps xmm0,xmm0
0298EF44  89559B                   mov [rbp-65h],edx
0298EF47  894D9F                   mov [rbp-61h],ecx
0298EF4A  488D15F79B1F01           lea rdx,[rel R_3B88B48]
0298EF51  0F114587                 movups [rbp-79h],xmm0
0298EF55  488D4D97                 lea rcx,[rbp-69h]
0298EF59  894587                   mov [rbp-79h],eax
0298EF5C  895D97                   mov [rbp-69h],ebx
0298EF5F  44886DA3                 mov [rbp-5Dh],r13b
0298EF63  E84802F6FE               call R_18EF1B0
0298EF68  85C0                     test eax,eax
0298EF6A  488D151F9F1F01           lea rdx,[rel R_3B88E90]
0298EF71  488D4D97                 lea rcx,[rbp-69h]
0298EF75  400F94C6                 sete sil
0298EF79  408875E4                 mov [rbp-1Ch],sil
0298EF7D  E82E02F6FE               call R_18EF1B0
0298EF82  85C0                     test eax,eax
0298EF84  410FB6CD                 movzx ecx,r13b
0298EF88  418BC6                   mov eax,r14d
0298EF8B  410F44CE                 cmove ecx,r14d
0298EF8F  884DE5                   mov [rbp-1Bh],cl
0298EF92  33C9                     xor ecx,ecx
0298EF94  0FA2                     cpuid
0298EF96  894D8F                   mov [rbp-71h],ecx
0298EF99  448BC0                   mov r8d,eax
0298EF9C  895593                   mov [rbp-6Dh],edx
0298EF9F  8BC8                     mov ecx,eax
0298EFA1  8BD0                     mov edx,eax
0298EFA3  C1E908                   shr ecx,8
0298EFA6  C1EA04                   shr edx,4
0298EFA9  83E10F                   and ecx,0Fh
0298EFAC  41C1E810                 shr r8d,10h
0298EFB0  83E20F                   and edx,0Fh
0298EFB3  C1E814                   shr eax,14h
0298EFB6  4183E00F                 and r8d,0Fh
0298EFBA  895D8B                   mov [rbp-75h],ebx
0298EFBD  0FB6C0                   movzx eax,al
0298EFC0  83F90F                   cmp ecx,0Fh
0298EFC3  0F85A2010000             jne near R_298F16B
0298EFC9  03C1                     add eax,ecx
0298EFCB  8945E7                   mov [rbp-19h],eax
0298EFCE  41C1E004                 shl r8d,4
0298EFD2  440BC2                   or r8d,edx
0298EFD5  448945EB                 mov [rbp-15h],r8d
0298EFD9  33C9                     xor ecx,ecx
0298EFDB  B800000080               mov eax,80000000h
0298EFE0  0FA2                     cpuid
0298EFE2  895D8B                   mov [rbp-75h],ebx
0298EFE5  894D8F                   mov [rbp-71h],ecx
0298EFE8  895593                   mov [rbp-6Dh],edx
0298EFEB  3D04000080               cmp eax,80000004h
0298EFF0  726A                     jb short R_298F05C
0298EFF2  33C9                     xor ecx,ecx
0298EFF4  44886DE3                 mov [rbp-1Dh],r13b
0298EFF8  0F57C0                   xorps xmm0,xmm0
0298EFFB  B802000080               mov eax,80000002h
0298F000  0FA2                     cpuid
0298F002  0F114507                 movups [rbp+7],xmm0
0298F006  0F1145F7                 movups [rbp-9],xmm0
0298F00A  8945F7                   mov [rbp-9],eax
0298F00D  B803000080               mov eax,80000003h
0298F012  0F114517                 movups [rbp+17h],xmm0
0298F016  894DFF                   mov [rbp-1],ecx
0298F019  33C9                     xor ecx,ecx
0298F01B  895DFB                   mov [rbp-5],ebx
0298F01E  895503                   mov [rbp+3],edx
0298F021  0F1045F7                 movups xmm0,[rbp-9]
0298F025  0FA2                     cpuid
0298F027  894507                   mov [rbp+7],eax
0298F02A  B804000080               mov eax,80000004h
0298F02F  894D0F                   mov [rbp+0Fh],ecx
0298F032  33C9                     xor ecx,ecx
0298F034  895D0B                   mov [rbp+0Bh],ebx
0298F037  895513                   mov [rbp+13h],edx
0298F03A  0F104D07                 movups xmm1,[rbp+7]
0298F03E  0FA2                     cpuid
0298F040  894517                   mov [rbp+17h],eax
0298F043  895D1B                   mov [rbp+1Bh],ebx
0298F046  894D1F                   mov [rbp+1Fh],ecx
0298F049  895523                   mov [rbp+23h],edx
0298F04C  0F1145A4                 movups [rbp-5Ch],xmm0
0298F050  0F104517                 movups xmm0,[rbp+17h]
0298F054  0F114DB4                 movups [rbp-4Ch],xmm1
0298F058  0F1145C4                 movups [rbp-3Ch],xmm0
0298F05C  4084F6                   test sil,sil
0298F05F  7418                     je short R_298F079
0298F061  488D15209E1F01           lea rdx,[rel R_3B88E88]
0298F068  488D4DA4                 lea rcx,[rbp-5Ch]
0298F06C  E8974820FE               call R_B93908
0298F071  4885C0                   test rax,rax
0298F074  418AC6                   mov al,r14b
0298F077  7503                     jne short R_298F07C
0298F079  418AC5                   mov al,r13b
0298F07C  888781020000             mov [rdi+281h],al
0298F082  84C0                     test al,al
0298F084  7413                     je short R_298F099
0298F086  837DE717                 cmp dword [rbp-19h],17h
0298F08A  750D                     jne short R_298F099
0298F08C  837DEB30                 cmp dword [rbp-15h],30h
0298F090  7307                     jae short R_298F099
0298F092  4488B782020000           mov [rdi+282h],r14b
0298F099  FF150134F500             call qword [rel R_38E24A0]
0298F09F  4C8D442430               lea r8,[rsp+30h]
0298F0A4  44896C2420               mov [rsp+20h],r13d
0298F0A9  4C8BC8                   mov r9,rax
0298F0AC  33D2                     xor edx,edx
0298F0AE  33C9                     xor ecx,ecx
0298F0B0  488BD8                   mov rbx,rax
0298F0B3  FF15FF33F500             call qword [rel R_38E24B8]
0298F0B9  8B4C2430                 mov ecx,[rsp+30h]
0298F0BD  E89A2A20FE               call R_B91B5C
0298F0C2  8B542430                 mov edx,[rsp+30h]
0298F0C6  4C8D442430               lea r8,[rsp+30h]
0298F0CB  4C8BCB                   mov r9,rbx
0298F0CE  48894587                 mov [rbp-79h],rax
0298F0D2  488BC8                   mov rcx,rax
0298F0D5  44896C2420               mov [rsp+20h],r13d
0298F0DA  4C8BF0                   mov r14,rax
0298F0DD  FF15D533F500             call qword [rel R_38E24B8]
0298F0E3  85C0                     test eax,eax
0298F0E5  744E                     je short R_298F135
0298F0E7  837C243020               cmp dword [rsp+30h],20h
0298F0EC  418BF5                   mov esi,r13d
0298F0EF  7244                     jb short R_298F135
0298F0F1  498D5E08                 lea rbx,[r14+8]
0298F0F5  44396BFC                 cmp [rbx-4],r13d
0298F0F9  7524                     jne short R_298F11F
0298F0FB  488D542438               lea rdx,[rsp+38h]
0298F100  8B03                     mov eax,[rbx]
0298F102  488D8F88020000           lea rcx,[rdi+288h]
0298F109  89442438                 mov [rsp+38h],eax
0298F10D  44386B0A                 cmp [rbx+0Ah],r13b
0298F111  7707                     ja short R_298F11A
0298F113  488D8FA0020000           lea rcx,[rdi+2A0h]
0298F11A  E8F5E3FFFF               call R_298D514
0298F11F  8B442430                 mov eax,[rsp+30h]
0298F123  83C620                   add esi,20h
0298F126  8BCE                     mov ecx,esi
0298F128  4883C320                 add rbx,20h
0298F12C  4883C120                 add rcx,20h
0298F130  483BC8                   cmp rcx,rax
0298F133  76C0                     jbe short R_298F0F5
0298F135  488D4D87                 lea rcx,[rbp-79h]
0298F139  E8E2F0FFFF               call R_298E220
0298F13E  488B4D27                 mov rcx,[rbp+27h]
0298F142  4833CC                   xor rcx,rsp
0298F145  E8962120FE               call R_B912E0
0298F14A  4C8D9C24F0000000         lea r11,[rsp+0F0h]
0298F152  498B5B38                 mov rbx,[r11+38h]
0298F156  498B7340                 mov rsi,[r11+40h]
0298F15A  498B7B48                 mov rdi,[r11+48h]
0298F15E  498BE3                   mov rsp,r11
0298F161  415F                     pop r15
0298F163  415E                     pop r14
0298F165  415D                     pop r13
0298F167  415C                     pop r12
0298F169  5D                       pop rbp
0298F16A  C3                       ret
0298F16B  894DE7                   mov [rbp-19h],ecx
0298F16E  83F906                   cmp ecx,6
0298F171  0F8457FEFFFF             je near R_298EFCE
0298F177  8955EB                   mov [rbp-15h],edx
0298F17A  E95AFEFFFF               jmp R_298EFD9
0298F17F  488D4C2438               lea rcx,[rsp+38h]
0298F184  E8D730C4FD               call R_5D2260
0298F189  EBB3                     jmp short R_298F13E
