== func 298D6C8-298DF02
0298D6C8  488BC4                   mov rax,rsp
0298D6CB  48895810                 mov [rax+10h],rbx
0298D6CF  48897018                 mov [rax+18h],rsi
0298D6D3  48897820                 mov [rax+20h],rdi
0298D6D7  55                       push rbp
0298D6D8  4154                     push r12
0298D6DA  4155                     push r13
0298D6DC  4156                     push r14
0298D6DE  4157                     push r15
0298D6E0  488D68A1                 lea rbp,[rax-5Fh]
0298D6E4  4881EC00010000           sub rsp,100h
0298D6EB  4533ED                   xor r13d,r13d
0298D6EE  488D0513BA1F01           lea rax,[rel R_3B89108]
0298D6F5  4C896918                 mov [rcx+18h],r13
0298D6F9  4C8BE2                   mov r12,rdx
0298D6FC  4C896920                 mov [rcx+20h],r13
0298D700  488BF1                   mov rsi,rcx
0298D703  488901                   mov [rcx],rax
0298D706  488D053BBA1F01           lea rax,[rel R_3B89148]
0298D70D  48894108                 mov [rcx+8],rax
0298D711  488D05E0B91F01           lea rax,[rel R_3B890F8]
0298D718  48894110                 mov [rcx+10h],rax
0298D71C  4C896928                 mov [rcx+28h],r13
0298D720  4C896930                 mov [rcx+30h],r13
0298D724  8A02                     mov al,[rdx]
0298D726  884138                   mov [rcx+38h],al
0298D729  8A82C0000000             mov al,[rdx+0C0h]
0298D72F  884139                   mov [rcx+39h],al
0298D732  8A82C2000000             mov al,[rdx+0C2h]
0298D738  88413A                   mov [rcx+3Ah],al
0298D73B  8A82C4000000             mov al,[rdx+0C4h]
0298D741  88413B                   mov [rcx+3Bh],al
0298D744  8A82C5000000             mov al,[rdx+0C5h]
0298D74A  88413C                   mov [rcx+3Ch],al
0298D74D  8A82C6000000             mov al,[rdx+0C6h]
0298D753  88413D                   mov [rcx+3Dh],al
0298D756  8A82C7000000             mov al,[rdx+0C7h]
0298D75C  88413E                   mov [rcx+3Eh],al
0298D75F  8A82C8000000             mov al,[rdx+0C8h]
0298D765  88413F                   mov [rcx+3Fh],al
0298D768  8A82C9000000             mov al,[rdx+0C9h]
0298D76E  884140                   mov [rcx+40h],al
0298D771  8A82CA000000             mov al,[rdx+0CAh]
0298D777  884141                   mov [rcx+41h],al
0298D77A  8B82D8000000             mov eax,[rdx+0D8h]
0298D780  894144                   mov [rcx+44h],eax
0298D783  8B82BC000000             mov eax,[rdx+0BCh]
0298D789  894148                   mov [rcx+48h],eax
0298D78C  8A82DC000000             mov al,[rdx+0DCh]
0298D792  88414C                   mov [rcx+4Ch],al
0298D795  8A82DE000000             mov al,[rdx+0DEh]
0298D79B  88414D                   mov [rcx+4Dh],al
0298D79E  8A82DF000000             mov al,[rdx+0DFh]
0298D7A4  88414E                   mov [rcx+4Eh],al
0298D7A7  8A82E0000000             mov al,[rdx+0E0h]
0298D7AD  88414F                   mov [rcx+4Fh],al
0298D7B0  8A82E1000000             mov al,[rdx+0E1h]
0298D7B6  884150                   mov [rcx+50h],al
0298D7B9  8A82E2000000             mov al,[rdx+0E2h]
0298D7BF  884151                   mov [rcx+51h],al
0298D7C2  8A82E3000000             mov al,[rdx+0E3h]
0298D7C8  884152                   mov [rcx+52h],al
0298D7CB  8B82E4000000             mov eax,[rdx+0E4h]
0298D7D1  894154                   mov [rcx+54h],eax
0298D7D4  8B82E8000000             mov eax,[rdx+0E8h]
0298D7DA  894158                   mov [rcx+58h],eax
0298D7DD  8B82EC000000             mov eax,[rdx+0ECh]
0298D7E3  89415C                   mov [rcx+5Ch],eax
0298D7E6  8A82F0000000             mov al,[rdx+0F0h]
0298D7EC  884160                   mov [rcx+60h],al
0298D7EF  8A82F1000000             mov al,[rdx+0F1h]
0298D7F5  884161                   mov [rcx+61h],al
0298D7F8  8A82F2000000             mov al,[rdx+0F2h]
0298D7FE  884162                   mov [rcx+62h],al
0298D801  8A82F3000000             mov al,[rdx+0F3h]
0298D807  884163                   mov [rcx+63h],al
0298D80A  8A82F4000000             mov al,[rdx+0F4h]
0298D810  884164                   mov [rcx+64h],al
0298D813  8A82F5000000             mov al,[rdx+0F5h]
0298D819  884165                   mov [rcx+65h],al
0298D81C  8A82F6000000             mov al,[rdx+0F6h]
0298D822  884166                   mov [rcx+66h],al
0298D825  8A82F7000000             mov al,[rdx+0F7h]
0298D82B  884167                   mov [rcx+67h],al
0298D82E  8A82F8000000             mov al,[rdx+0F8h]
0298D834  884168                   mov [rcx+68h],al
0298D837  8A82F9000000             mov al,[rdx+0F9h]
0298D83D  884169                   mov [rcx+69h],al
0298D840  8A82FA000000             mov al,[rdx+0FAh]
0298D846  88416A                   mov [rcx+6Ah],al
0298D849  4489696C                 mov [rcx+6Ch],r13d
0298D84D  6644896970               mov [rcx+70h],r13w
0298D852  44886972                 mov [rcx+72h],r13b
0298D856  4C8DB1B0000000           lea r14,[rcx+0B0h]
0298D85D  8B8200010000             mov eax,[rdx+100h]
0298D863  4C8DB9B8000000           lea r15,[rcx+0B8h]
0298D86A  894174                   mov [rcx+74h],eax
0298D86D  8A8204010000             mov al,[rdx+104h]
0298D873  884178                   mov [rcx+78h],al
0298D876  8A8205010000             mov al,[rdx+105h]
0298D87C  884179                   mov [rcx+79h],al
0298D87F  8A8206010000             mov al,[rdx+106h]
0298D885  88417A                   mov [rcx+7Ah],al
0298D888  488B8208010000           mov rax,[rdx+108h]
0298D88F  48898180000000           mov [rcx+80h],rax
0298D896  8B8220010000             mov eax,[rdx+120h]
0298D89C  898188000000             mov [rcx+88h],eax
0298D8A2  4C89A998000000           mov [rcx+98h],r13
0298D8A9  4C89A990000000           mov [rcx+90h],r13
0298D8B0  4C89A9A0000000           mov [rcx+0A0h],r13
0298D8B7  48C781A800000007000000   mov qword [rcx+0A8h],7
0298D8C2  4C89A9C0000000           mov [rcx+0C0h],r13
0298D8C9  4D892E                   mov [r14],r13
0298D8CC  4D892F                   mov [r15],r13
0298D8CF  664489A9C8000000         mov [rcx+0C8h],r13w
0298D8D7  4C89A9D0000000           mov [rcx+0D0h],r13
0298D8DE  4881C1D8000000           add rcx,0D8h
0298D8E5  E84238DCFD               call R_75112C
0298D8EA  488D8E40010000           lea rcx,[rsi+140h]
0298D8F1  4C89AE28010000           mov [rsi+128h],r13
0298D8F8  4C8D0DB15910FE           lea r9,[rel R_A932B0]
0298D8FF  4C89AE30010000           mov [rsi+130h],r13
0298D906  418D5510                 lea edx,[r13+10h]
0298D90A  4C89AE38010000           mov [rsi+138h],r13
0298D911  458D4505                 lea r8d,[r13+5]
0298D915  E856A47DFD               call R_167D70
0298D91A  C6869001000001           mov byte [rsi+190h],1
0298D921  488DBED0010000           lea rdi,[rsi+1D0h]
0298D928  83C9FF                   or ecx,0FFFFFFFFh
0298D92B  0F57C0                   xorps xmm0,xmm0
0298D92E  0F118694010000           movups [rsi+194h],xmm0
0298D935  4C89AEA4010000           mov [rsi+1A4h],r13
0298D93C  33D2                     xor edx,edx
0298D93E  664489AEAC010000         mov [rsi+1ACh],r13w
0298D946  41B8A0000000             mov r8d,0A0h
0298D94C  8B05E2D5C201             mov eax,[rel R_45BAF34]
0298D952  8986B0010000             mov [rsi+1B0h],eax
0298D958  8B05D2D5C201             mov eax,[rel R_45BAF30]
0298D95E  8986B4010000             mov [rsi+1B4h],eax
0298D964  8B05DED5C201             mov eax,[rel R_45BAF48]
0298D96A  8986B8010000             mov [rsi+1B8h],eax
0298D970  8B05E6D5C201             mov eax,[rel R_45BAF5C]
0298D976  8986BC010000             mov [rsi+1BCh],eax
0298D97C  8B05BAD5C201             mov eax,[rel R_45BAF3C]
0298D982  898EC4010000             mov [rsi+1C4h],ecx
0298D988  8986C0010000             mov [rsi+1C0h],eax
0298D98E  8B05E8D5C201             mov eax,[rel R_45BAF7C]
0298D994  898ECC010000             mov [rsi+1CCh],ecx
0298D99A  488BCF                   mov rcx,rdi
0298D99D  8986C8010000             mov [rsi+1C8h],eax
0298D9A3  E8586520FE               call R_B93F00
0298D9A8  4C89AE88020000           mov [rsi+288h],r13
0298D9AF  488D9E70020000           lea rbx,[rsi+270h]
0298D9B6  4C89AE90020000           mov [rsi+290h],r13
0298D9BD  4C89AE98020000           mov [rsi+298h],r13
0298D9C4  4C89AEA0020000           mov [rsi+2A0h],r13
0298D9CB  4C89AEA8020000           mov [rsi+2A8h],r13
0298D9D2  4C89AEB0020000           mov [rsi+2B0h],r13
0298D9D9  44892B                   mov [rbx],r13d
0298D9DC  4C89AE74020000           mov [rsi+274h],r13
0298D9E3  4489AE7C020000           mov [rsi+27Ch],r13d
0298D9EA  664489AE80020000         mov [rsi+280h],r13w
0298D9F2  4488AE82020000           mov [rsi+282h],r13b
0298D9F9  4489AE84020000           mov [rsi+284h],r13d
0298DA00  410F10442402             movups xmm0,[r12+2]
0298DA06  0F1107                   movups [rdi],xmm0
0298DA09  410F104C2412             movups xmm1,[r12+12h]
0298DA0F  0F114F10                 movups [rdi+10h],xmm1
0298DA13  410F10442422             movups xmm0,[r12+22h]
0298DA19  0F114720                 movups [rdi+20h],xmm0
0298DA1D  410F104C2432             movups xmm1,[r12+32h]
0298DA23  0F114F30                 movups [rdi+30h],xmm1
0298DA27  410F10442442             movups xmm0,[r12+42h]
0298DA2D  0F114740                 movups [rdi+40h],xmm0
0298DA31  B980000000               mov ecx,80h
0298DA36  410F104C2452             movups xmm1,[r12+52h]
0298DA3C  0F114F50                 movups [rdi+50h],xmm1
0298DA40  410F10442462             movups xmm0,[r12+62h]
0298DA46  0F114760                 movups [rdi+60h],xmm0
0298DA4A  410F10442472             movups xmm0,[r12+72h]
0298DA50  0F11440FF0               movups [rdi+rcx-10h],xmm0
0298DA55  410F104C0C02             movups xmm1,[r12+rcx+2]
0298DA5B  0F110C0F                 movups [rdi+rcx],xmm1
0298DA5F  410F10440C12             movups xmm0,[r12+rcx+12h]
0298DA65  0F11440F10               movups [rdi+rcx+10h],xmm0
0298DA6A  488BCE                   mov rcx,rsi
0298DA6D  E836130000               call R_298EDA8
0298DA72  E8A9B8C2FD               call R_5B9320
0298DA77  488BD3                   mov rdx,rbx
0298DA7A  488BC8                   mov rcx,rax
0298DA7D  E8525565FF               call R_1FE2FD4
0298DA82  E875C8AD00               call R_346A2FC
0298DA87  488BCE                   mov rcx,rsi
0298DA8A  E8E1180000               call R_298F370
0298DA8F  418D4D1F                 lea ecx,[r13+1Fh]
0298DA93  C745B707000200           mov dword [rbp-49h],20007h
0298DA9A  4C896DC7                 mov [rbp-39h],r13
0298DA9E  4C896DCF                 mov [rbp-31h],r13
0298DAA2  48C745BB00010000         mov qword [rbp-45h],100h
0298DAAA  E855B8C2FD               call R_5B9304
0298DAAF  8B4808                   mov ecx,[rax+8]
0298DAB2  E8257B91FF               call R_22A55DC
0298DAB7  8945C3                   mov [rbp-3Dh],eax
0298DABA  E8098FFDFF               call R_29669C8
0298DABF  488BD8                   mov rbx,rax
0298DAC2  4885C0                   test rax,rax
0298DAC5  7430                     je short R_298DAF7
0298DAC7  488B10                   mov rdx,[rax]
0298DACA  488BC8                   mov rcx,rax
0298DACD  FF5208                   call qword [rdx+8]
0298DAD0  4C8D4597                 lea r8,[rbp-69h]
0298DAD4  C7459735000000           mov dword [rbp-69h],35h
0298DADB  48894630                 mov [rsi+30h],rax
0298DADF  488BD0                   mov rdx,rax
0298DAE2  488B0B                   mov rcx,[rbx]
0298DAE5  4C896D9F                 mov [rbp-61h],r13
0298DAE9  4C8B4928                 mov r9,[rcx+28h]
0298DAED  488BCB                   mov rcx,rbx
0298DAF0  41FFD1                   call r9
0298DAF3  48894628                 mov [rsi+28h],rax
0298DAF7  E82C8EFDFF               call R_2966928
0298DAFC  488BC8                   mov rcx,rax
0298DAFF  488D55B7                 lea rdx,[rbp-49h]
0298DB03  E81C8AF3FE               call R_18C6524
0298DB08  488D5567                 lea rdx,[rbp+67h]
0298DB0C  48894567                 mov [rbp+67h],rax
0298DB10  498BCE                   mov rcx,r14
0298DB13  E874080000               call R_298E38C
0298DB18  488D4D67                 lea rcx,[rbp+67h]
0298DB1C  E8E7060000               call R_298E208
0298DB21  4D392E                   cmp [r14],r13
0298DB24  750C                     jne short R_298DB32
0298DB26  488D0D6388F500           lea rcx,[rel R_38E6390]
0298DB2D  E8820D0000               call R_298E8B4
0298DB32  498BD4                   mov rdx,r12
0298DB35  488BCE                   mov rcx,rsi
0298DB38  E8DF170000               call R_298F31C
0298DB3D  498BD4                   mov rdx,r12
0298DB40  C6467101                 mov byte [rsi+71h],1
0298DB44  488BCE                   mov rcx,rsi
0298DB47  E864170000               call R_298F2B0
0298DB4C  488D4DB7                 lea rcx,[rbp-49h]
0298DB50  E8CB991AFE               call R_B37520
0298DB55  E80A8FFDFF               call R_2966A64
0298DB5A  488BD0                   mov rdx,rax
0298DB5D  488D4DB7                 lea rcx,[rbp-49h]
0298DB61  E8CAA01AFE               call R_B37C30
0298DB66  488D4DB7                 lea rcx,[rbp-49h]
0298DB6A  E8019B1AFE               call R_B37670
0298DB6F  E8F08EFDFF               call R_2966A64
0298DB74  488D4D67                 lea rcx,[rbp+67h]
0298DB78  48890519712102           mov [rel R_4BA4C98],rax
0298DB7F  48897567                 mov [rbp+67h],rsi
0298DB83  E8E8239CFE               call R_134FF70
0298DB88  498D8C2418010000         lea rcx,[r12+118h]
0298DB90  48894567                 mov [rbp+67h],rax
0298DB94  488D5567                 lea rdx,[rbp+67h]
0298DB98  E8BF070000               call R_298E35C
0298DB9D  488BD0                   mov rdx,rax
0298DBA0  498BCF                   mov rcx,r15
0298DBA3  E8ACAA64FF               call R_1FD8654
0298DBA8  488D4D67                 lea rcx,[rbp+67h]
0298DBAC  E857A764FF               call R_1FD8308
0298DBB1  4D392F                   cmp [r15],r13
0298DBB4  0F8424030000             je near R_298DEDE
0298DBBA  498B06                   mov rax,[r14]
0298DBBD  488945B7                 mov [rbp-49h],rax
0298DBC1  4C896DBF                 mov [rbp-41h],r13
0298DBC5  44896DC7                 mov [rbp-39h],r13d
0298DBC9  E8968EFDFF               call R_2966A64
0298DBCE  488D4DB7                 lea rcx,[rbp-49h]
0298DBD2  488945CF                 mov [rbp-31h],rax
0298DBD6  E861F2FFFF               call R_298CE3C
0298DBDB  E8FCD98DFF               call R_226B5DC
0298DBE0  E8BB6565FF               call R_1FE41A0
0298DBE5  488BCE                   mov rcx,rsi
0298DBE8  E867170000               call R_298F354
0298DBED  8B053DD3C201             mov eax,[rel R_45BAF30]
0298DBF3  0F57C0                   xorps xmm0,xmm0
0298DBF6  488B8E80000000           mov rcx,[rsi+80h]
0298DBFD  8986B4010000             mov [rsi+1B4h],eax
0298DC03  8B052BD3C201             mov eax,[rel R_45BAF34]
0298DC09  8986B0010000             mov [rsi+1B0h],eax
0298DC0F  488D053EA4EEFD           lea rax,[rel R_878054]
0298DC16  488945EF                 mov [rbp-11h],rax
0298DC1A  8A463C                   mov al,[rsi+3Ch]
0298DC1D  F6D8                     neg al
0298DC1F  44896C2428               mov [rsp+28h],r13d
0298DC24  4C896DF7                 mov [rbp-9],r13
0298DC28  481BDB                   sbb rbx,rbx
0298DC2B  660F7F4517               movdqa [rbp+17h],xmm0
0298DC30  4533C9                   xor r9d,r9d
0298DC33  C745E750000000           mov dword [rbp-19h],50h
0298DC3A  83E307                   and ebx,7
0298DC3D  C745EB03000000           mov dword [rbp-15h],3
0298DC44  48894DFF                 mov [rbp-1],rcx
0298DC48  44896C2420               mov [rsp+20h],r13d
0298DC4D  458D4101                 lea r8d,[r9+1]
0298DC51  488D5365                 lea rdx,[rbx+65h]
0298DC55  FF152D4EF500             call qword [rel R_38E2A88]
0298DC5B  48894507                 mov [rbp+7],rax
0298DC5F  BA007F0000               mov edx,7F00h
0298DC64  488D05F9AE1F01           lea rax,[rel R_3B88B64]
0298DC6B  33C9                     xor ecx,ecx
0298DC6D  48894527                 mov [rbp+27h],rax
0298DC71  FF15294EF500             call qword [rel R_38E2AA0]
0298DC77  488B8E80000000           mov rcx,[rsi+80h]
0298DC7E  488D5365                 lea rdx,[rbx+65h]
0298DC82  41B910000000             mov r9d,10h
0298DC88  44896C2428               mov [rsp+28h],r13d
0298DC8D  4889450F                 mov [rbp+0Fh],rax
0298DC91  C744242010000000         mov dword [rsp+20h],10h
0298DC99  458D41F1                 lea r8d,[r9-0Fh]
0298DC9D  FF15E54DF500             call qword [rel R_38E2A88]
0298DCA3  488D4DE7                 lea rcx,[rbp-19h]
0298DCA7  4889452F                 mov [rbp+2Fh],rax
0298DCAB  FF15CF4DF500             call qword [rel R_38E2A80]
0298DCB1  668986C8000000           mov [rsi+0C8h],ax
0298DCB8  6685C0                   test ax,ax
0298DCBB  0F841D020000             je near R_298DEDE
0298DCC1  8B8EB0010000             mov ecx,[rsi+1B0h]
0298DCC7  4C8D4D67                 lea r9,[rbp+67h]
0298DCCB  488B054ED3C201           mov rax,[rel R_45BB020]
0298DCD2  33D2                     xor edx,edx
0298DCD4  448B86B4010000           mov r8d,[rsi+1B4h]
0298DCDB  894C2420                 mov [rsp+20h],ecx
0298DCDF  488D4DB7                 lea rcx,[rbp-49h]
0298DCE3  48894567                 mov [rbp+67h],rax
0298DCE7  E884369CFE               call R_1351370
0298DCEC  0F104810                 movups xmm1,[rax+10h]
0298DCF0  0F1000                   movups xmm0,[rax]
0298DCF3  0F114DA7                 movups [rbp-59h],xmm1
0298DCF7  488B45A7                 mov rax,[rbp-59h]
0298DCFB  48898628010000           mov [rsi+128h],rax
0298DD02  488B0517D3C201           mov rax,[rel R_45BB020]
0298DD09  48898630010000           mov [rsi+130h],rax
0298DD10  0F114597                 movups [rbp-69h],xmm0
0298DD14  4439AEB4010000           cmp [rsi+1B4h],r13d
0298DD1B  0F85D8000000             jne near R_298DDF9
0298DD21  448B3518D2C201           mov r14d,[rel R_45BAF40]
0298DD28  B800000080               mov eax,80000000h
0298DD2D  448B3D10D2C201           mov r15d,[rel R_45BAF44]
0298DD34  443BF0                   cmp r14d,eax
0298DD37  7408                     je short R_298DD41
0298DD39  418ACD                   mov cl,r13b
0298DD3C  443BF8                   cmp r15d,eax
0298DD3F  7502                     jne short R_298DD43
0298DD41  B101                     mov cl,1
0298DD43  4585F6                   test r14d,r14d
0298DD46  7408                     je short R_298DD50
0298DD48  418AC5                   mov al,r13b
0298DD4B  4585FF                   test r15d,r15d
0298DD4E  7502                     jne short R_298DD52
0298DD50  B001                     mov al,1
0298DD52  84C9                     test cl,cl
0298DD54  0F859F000000             jne near R_298DDF9
0298DD5A  84C0                     test al,al
0298DD5C  0F8597000000             jne near R_298DDF9
0298DD62  41BD4C000000             mov r13d,4Ch
0298DD68  418BCD                   mov ecx,r13d
0298DD6B  FF15EF4CF500             call qword [rel R_38E2A60]
0298DD71  418D4D01                 lea ecx,[r13+1]
0298DD75  894567                   mov [rbp+67h],eax
0298DD78  FF15E24CF500             call qword [rel R_38E2A60]
0298DD7E  418D4D02                 lea ecx,[r13+2]
0298DD82  8BF8                     mov edi,eax
0298DD84  FF15D64CF500             call qword [rel R_38E2A60]
0298DD8A  418BCD                   mov ecx,r13d
0298DD8D  8BD8                     mov ebx,eax
0298DD8F  FF15CB4CF500             call qword [rel R_38E2A60]
0298DD95  B94F000000               mov ecx,4Fh
0298DD9A  448D2C03                 lea r13d,[rbx+rax]
0298DD9E  FF15BC4CF500             call qword [rel R_38E2A60]
0298DDA4  B94D000000               mov ecx,4Dh
0298DDA9  8BD8                     mov ebx,eax
0298DDAB  FF15AF4CF500             call qword [rel R_38E2A60]
0298DDB1  448B4D9F                 mov r9d,[rbp-61h]
0298DDB5  8D0C03                   lea ecx,[rbx+rax]
0298DDB8  438D0431                 lea eax,[r9+r14]
0298DDBC  413BFF                   cmp edi,r15d
0298DDBF  7F09                     jg short R_298DDCA
0298DDC1  443BF9                   cmp r15d,ecx
0298DDC4  7D04                     jge short R_298DDCA
0298DDC6  B101                     mov cl,1
0298DDC8  EB02                     jmp short R_298DDCC
0298DDCA  32C9                     xor cl,cl
0298DDCC  8B5567                   mov edx,[rbp+67h]
0298DDCF  413BD6                   cmp edx,r14d
0298DDD2  7D05                     jge short R_298DDD9
0298DDD4  453BF5                   cmp r14d,r13d
0298DDD7  7C09                     jl short R_298DDE2
0298DDD9  3BD0                     cmp edx,eax
0298DDDB  7D0C                     jge short R_298DDE9
0298DDDD  413BC5                   cmp eax,r13d
0298DDE0  7D07                     jge short R_298DDE9
0298DDE2  B001                     mov al,1
0298DDE4  4533ED                   xor r13d,r13d
0298DDE7  EB06                     jmp short R_298DDEF
0298DDE9  4533ED                   xor r13d,r13d
0298DDEC  418AC5                   mov al,r13b
0298DDEF  84C9                     test cl,cl
0298DDF1  740A                     je short R_298DDFD
0298DDF3  84C0                     test al,al
0298DDF5  7406                     je short R_298DDFD
0298DDF7  EB0C                     jmp short R_298DE05
0298DDF9  448B4D9F                 mov r9d,[rbp-61h]
0298DDFD  448B7D9B                 mov r15d,[rbp-65h]
0298DE01  448B7597                 mov r14d,[rbp-69h]
0298DE05  44386E3C                 cmp [rsi+3Ch],r13b
0298DE09  488D0574AC1F01           lea rax,[rel R_3B88A84]
0298DE10  488B5527                 mov rdx,[rbp+27h]
0298DE14  4C8D053DAD1F01           lea r8,[rel R_3B88B58]
0298DE1B  8B4DB3                   mov ecx,[rbp-4Dh]
0298DE1E  4C0F44C0                 cmove r8,rax
0298DE22  488B8680000000           mov rax,[rsi+80h]
0298DE29  4889742458               mov [rsp+58h],rsi
0298DE2E  4889442450               mov [rsp+50h],rax
0298DE33  8B45A3                   mov eax,[rbp-5Dh]
0298DE36  4C896C2448               mov [rsp+48h],r13
0298DE3B  4C896C2440               mov [rsp+40h],r13
0298DE40  89442438                 mov [rsp+38h],eax
0298DE44  44894C2430               mov [rsp+30h],r9d
0298DE49  448B4DAF                 mov r9d,[rbp-51h]
0298DE4D  44897C2428               mov [rsp+28h],r15d
0298DE52  4489742420               mov [rsp+20h],r14d
0298DE57  FF151B4CF500             call qword [rel R_38E2A78]
0298DE5D  4989842410010000         mov [r12+110h],rax
0298DE65  488986D0000000           mov [rsi+0D0h],rax
0298DE6C  4885C0                   test rax,rax
0298DE6F  746D                     je short R_298DEDE
0298DE71  BA68000000               mov edx,68h
0298DE76  48897567                 mov [rbp+67h],rsi
0298DE7A  488D4D67                 lea rcx,[rbp+67h]
0298DE7E  E86D1E9CFE               call R_134FCF0
0298DE83  BA6A000000               mov edx,6Ah
0298DE88  488D4D67                 lea rcx,[rbp+67h]
0298DE8C  E85F1E9CFE               call R_134FCF0
0298DE91  BA6B000000               mov edx,6Bh
0298DE96  488D4D67                 lea rcx,[rbp+67h]
0298DE9A  E8511E9CFE               call R_134FCF0
0298DE9F  BA67000000               mov edx,67h
0298DEA4  488D4D67                 lea rcx,[rbp+67h]
0298DEA8  E8431E9CFE               call R_134FCF0
0298DEAD  BA69000000               mov edx,69h
0298DEB2  488D4D67                 lea rcx,[rbp+67h]
0298DEB6  E8351E9CFE               call R_134FCF0
0298DEBB  498BD4                   mov rdx,r12
0298DEBE  488D0D7B6C1C02           lea rcx,[rel R_4B54B40]
0298DEC5  E82AC2FDFF               call R_296A0F4
0298DECA  498BCC                   mov rcx,r12
0298DECD  E8468965FF               call R_1FE6818
0298DED2  C60567C6C20101           mov byte [rel R_45BA540],1
0298DED9  E85E5FFEFF               call R_2973E3C
0298DEDE  4C8D9C2400010000         lea r11,[rsp+100h]
0298DEE6  488BC6                   mov rax,rsi
0298DEE9  498B5B38                 mov rbx,[r11+38h]
0298DEED  498B7340                 mov rsi,[r11+40h]
0298DEF1  498B7B48                 mov rdi,[r11+48h]
0298DEF5  498BE3                   mov rsp,r11
0298DEF8  415F                     pop r15
0298DEFA  415E                     pop r14
0298DEFC  415D                     pop r13
0298DEFE  415C                     pop r12
0298DF00  5D                       pop rbp
0298DF01  C3                       ret
