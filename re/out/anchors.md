# Nioh3Fix anchor sites (decompiled)

Generated from the Ghidra project at `re\ghidra_proj` (Nioh3.dump.exe, image base 0x7FF74F410000). All addresses are **RVAs** unless they appear inside
Ghidra output, where `FUN_7ff7xxxxxxxx` / `DAT_7ff7xxxxxxxx` are absolute (subtract 0x7FF74F410000 for the RVA). Labels are the approximate ones from
`notes\nioh3fix-signatures.md`. Re-generate any entry with `re\decomp.cmd <rva> asm:<rva>:10 c:<rva>`.

Interpretation notes (from reading these + their callees, see README for more):

- `0x5af4e4` is the **dynamic-FPS governor**. It reads the REN_FPS cap via `0x5af604` (index @0x45baf54 -> {30,60,120}), and if DYNAMIC_FPS (@0x45baf58) is on, steps the
  target between 120/60/30 and publishes it to the frame object (`*(0x4759910)+0x618`, applied next frame to `+0x614`). Its caller `0x5af410` keeps a 10-sample ring of
  frame timings (+0x08..+0x50) and the average (+0x74). The hook at 0x5af50a is the `cmp [DYNAMIC_FPS],0`; 0x5af5c2 is the commit of the new target.
- `0x433114` adds `FUN_46484()` (= frame delta in 60 Hz units = `timeScale*60/targetFPS`) to three per-ID timers. The hook at 0x433130 is right after that call (XMM0 = delta).
- `0x1fde3e8`/`0x1fde504` are getters on the resolution tables in the config descriptor block (`0x45c8570`, see README "config.xml").

## 0x23554f — LOD / draw distance (Nioh3Fix LOD site)

- containing function: **0x2353c0** (499 bytes, `FUN_7ff74f6453c0`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e3d7a4	DATA	-
0x233a49	UNCONDITIONAL_CALL	FUN_7ff74f6438b0@0x2338b0
```

Disassembly around the anchor (`=>` marks it):

```
   0x23552d   8b 0d 7d 5a 38 04              MOV ECX,dword ptr [0x7ff7539cafb0] ; READ 0x45bafb0 DAT_7ff7539cafb0
   0x235533   85 c9                          TEST ECX,ECX
   0x235535   74 13                          JZ 0x7ff74f64554a ; CONDITIONAL_JUMP 0x23554a LAB_7ff74f64554a
   0x235537   83 e9 01                       SUB ECX,0x1
   0x23553a   74 07                          JZ 0x7ff74f645543 ; CONDITIONAL_JUMP 0x235543 LAB_7ff74f645543
   0x23553c   f3 0f 10 48 0c                 MOVSS XMM1,dword ptr [RAX + 0xc]
   0x235541   eb 0c                          JMP 0x7ff74f64554f ; UNCONDITIONAL_JUMP 0x23554f LAB_7ff74f64554f
   0x235543   f3 0f 10 48 08                 MOVSS XMM1,dword ptr [RAX + 0x8]
   0x235548   eb 05                          JMP 0x7ff74f64554f ; UNCONDITIONAL_JUMP 0x23554f LAB_7ff74f64554f
   0x23554a   f3 0f 10 48 04                 MOVSS XMM1,dword ptr [RAX + 0x4]
=> 0x23554f   f3 0f 59 c1                    MULSS XMM0,XMM1
   0x235553   8d 43 fc                       LEA EAX,[RBX + -0x4]
   0x235556   f3 0f 11 84 9f 08 a1 00 00     MOVSS dword ptr [RDI + RBX*0x4 + 0xa108],XMM0
   0x23555f   a9 fc ff ff ff                 TEST EAX,0xfffffffc
   0x235564   75 05                          JNZ 0x7ff74f64556b ; CONDITIONAL_JUMP 0x23556b LAB_7ff74f64556b
   0x235566   83 fb 06                       CMP EBX,0x6
   0x235569   75 1e                          JNZ 0x7ff74f645589 ; CONDITIONAL_JUMP 0x235589 LAB_7ff74f645589
   0x23556b   48 85 f6                       TEST RSI,RSI
   0x23556e   74 19                          JZ 0x7ff74f645589 ; CONDITIONAL_JUMP 0x235589 LAB_7ff74f645589
   0x235570   f3 0f 58 86 c0 08 00 00        ADDSS XMM0,dword ptr [RSI + 0x8c0]
   0x235578   f3 0f 5d 05 60 f8 99 03        MINSS XMM0,dword ptr [0x7ff752fe4de0] ; READ 0x3bd4de0 DAT_7ff752fe4de0
```

Decompilation:

```c
// function FUN_7ff74f6453c0 @ 0x2353c0  body=499 bytes  pdataStart=0x2353c0
// callers(1): FUN_7ff74f6438b0@0x2338b0 
// callees(12): FUN_7ff74f64617c@0x23617c FUN_7ff74f646240@0x236240 FUN_7ff74f646304@0x236304 FUN_7ff74f44cfd0@0x3cfd0 FUN_7ff74f6460b8@0x2360b8 FUN_7ff74f646458@0x236458 FUN_7ff74f6455e8@0x2355e8 FUN_7ff74f6475d8@0x2375d8 FUN_7ff74f793420@0x383420 FUN_7ff74f645f30@0x235f30 FUN_7ff74f645ff4@0x235ff4 FUN_7ff74f595a1c@0x185a1c 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff74f6453c0(longlong param_1,undefined8 *param_2)

{
  undefined8 uVar1;
  longlong lVar2;
  longlong lVar3;
  float *pfVar4;
  int iVar5;
  ulonglong uVar6;
  undefined4 uVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  longlong local_res10;
  
  local_res10 = FUN_7ff74f44cfd0(*param_2,param_2[1],0x38d04576,8);
  if (local_res10 != 0) {
    lVar3 = FUN_7ff74f793420(local_res10);
    if ((lVar3 != 0) && (*(longlong *)(lVar3 + 0x28) != 0)) {
      uVar1 = *(undefined8 *)(*(longlong *)(lVar3 + 0x28) + 0x6ec0);
      uVar7 = FUN_7ff74f6475d8(uVar1);
      *(undefined4 *)(param_1 + 0xa108) = uVar7;
      uVar7 = FUN_7ff74f645f30(uVar1);
      *(undefined4 *)(param_1 + 0xa10c) = uVar7;
      uVar7 = FUN_7ff74f645ff4(uVar1);
      *(undefined4 *)(param_1 + 0xa110) = uVar7;
      uVar7 = FUN_7ff74f6460b8(uVar1);
      *(undefined4 *)(param_1 + 0xa114) = uVar7;
      uVar7 = FUN_7ff74f646458(uVar1);
      *(undefined4 *)(param_1 + 0xa118) = uVar7;
      uVar7 = FUN_7ff74f64617c(uVar1);
      *(undefined4 *)(param_1 + 0xa11c) = uVar7;
      uVar7 = FUN_7ff74f646240(uVar1);
      *(undefined4 *)(param_1 + 0xa120) = uVar7;
      uVar7 = FUN_7ff74f646304(uVar1);
      lVar3 = _DAT_7ff7539c9e30;
      *(undefined4 *)(param_1 + 0xa124) = uVar7;
      lVar2 = _DAT_7ff7539c9e58;
      if ((lVar3 != 0) && (*(char *)(lVar3 + 0xb08) != '\0')) {
        uVar6 = 0;
        fVar10 = 2.000012;
        do {
          pfVar4 = (float *)FUN_7ff74f6455e8(*(undefined8 *)(lVar3 + 0xa08),uVar6 & 0xff);
          iVar5 = (int)uVar6;
          if (pfVar4 != (float *)0x0) {
            fVar8 = *(float *)(param_1 + 0xa108 + uVar6 * 4);
            if ((1.9999881 <= fVar8) && (fVar8 <= fVar10)) {
              fVar8 = *pfVar4;
              *(float *)(param_1 + 0xa108 + uVar6 * 4) = fVar8;
            }
            if ((iVar5 == 0) && (lVar2 != 0)) {
              fVar8 = *(float *)(lVar2 + 0x8bc);
              *(float *)(param_1 + 0xa108 + uVar6 * 4) = fVar8;
            }
            if (_DAT_7ff7539cafb0 == 0) {
              fVar9 = pfVar4[1];
            }
            else if (_DAT_7ff7539cafb0 == 1) {
              fVar9 = pfVar4[2];
            }
            else {
              fVar9 = pfVar4[3];
            }
            *(float *)(param_1 + 0xa108 + uVar6 * 4) = fVar8 * fVar9;
            if ((((iVar5 - 4U & 0xfffffffc) != 0) || (iVar5 == 6)) && (lVar2 != 0)) {
              fVar8 = fVar8 * fVar9 + *(float *)(lVar2 + 0x8c0);
              if (10.0 <= fVar8) {
                fVar8 = 10.0;
              }
              *(float *)(param_1 + 0xa108 + uVar6 * 4) = fVar8;
            }
          }
          uVar6 = (ulonglong)(iVar5 + 1U);
        } while ((int)(iVar5 + 1U) < 8);
      }
    }
  }
  FUN_7ff74f595a1c(&local_res10);
  return;
}
```

## 0x39a758 — LOD / draw distance

- containing function: **0x39a39c** (1883 bytes, `FUN_7ff74f7aa39c`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e4a320	DATA	-
0x39b7bc	UNCONDITIONAL_CALL	FUN_7ff74f7ab570@0x39b570
0x26f31c0	UNCONDITIONAL_CALL	FUN_7ff751b02d8c@0x26f2d8c
```

Disassembly around the anchor (`=>` marks it):

```
   0x39a721   75 1a                          JNZ 0x7ff74f7aa73d ; CONDITIONAL_JUMP 0x39a73d LAB_7ff74f7aa73d
   0x39a723   48 8b 05 2e f7 21 04           MOV RAX,qword ptr [0x7ff7539c9e58] ; READ 0x45b9e58 DAT_7ff7539c9e58
   0x39a72a   44 38 b8 b9 1b 00 00           CMP byte ptr [RAX + 0x1bb9],R15B
   0x39a731   75 0a                          JNZ 0x7ff74f7aa73d ; CONDITIONAL_JUMP 0x39a73d LAB_7ff74f7aa73d
   0x39a733   c7 83 78 1b 00 00 00 00 80 bf  MOV dword ptr [RBX + 0x1b78],0xbf800000
   0x39a73d   f3 0f 10 05 73 9a 83 03        MOVSS XMM0,dword ptr [0x7ff752fe41b8] ; READ 0x3bd41b8 DAT_7ff752fe41b8
   0x39a745   45 84 e4                       TEST R12B,R12B
   0x39a748   75 24                          JNZ 0x7ff74f7aa76e ; CONDITIONAL_JUMP 0x39a76e LAB_7ff74f7aa76e
   0x39a74a   48 63 05 5b 08 22 04           MOVSXD RAX,dword ptr [0x7ff7539cafac] ; READ 0x45bafac DAT_7ff7539cafac
   0x39a751   48 8d 0d a8 58 c6 ff           LEA RCX,[0x7ff74f410000]
=> 0x39a758   f3 0f 10 84 81 58 7a 97 03     MOVSS XMM0,dword ptr [RCX + RAX*0x4 + 0x3977a58]
   0x39a761   8b 84 81 90 83 97 03           MOV EAX,dword ptr [RCX + RAX*0x4 + 0x3978390]
   0x39a768   89 83 70 19 00 00              MOV dword ptr [RBX + 0x1970],EAX
   0x39a76e   f3 0f 11 83 78 19 00 00        MOVSS dword ptr [RBX + 0x1978],XMM0
   0x39a776   c7 83 cc 15 00 00 00 00 80 3f  MOV dword ptr [RBX + 0x15cc],0x3f800000
   0x39a780   e8 73 03 00 00                 CALL 0x7ff74f7aaaf8 ; UNCONDITIONAL_CALL 0x39aaf8 FUN_7ff74f7aaaf8
   0x39a785   f3 48 0f 2c 50 08              CVTTSS2SI RDX,dword ptr [RAX + 0x8]
   0x39a78b   89 93 f8 18 00 00              MOV dword ptr [RBX + 0x18f8],EDX
   0x39a791   e8 62 03 00 00                 CALL 0x7ff74f7aaaf8 ; UNCONDITIONAL_CALL 0x39aaf8 FUN_7ff74f7aaaf8
   0x39a796   f3 48 0f 2c 48 0c              CVTTSS2SI RCX,dword ptr [RAX + 0xc]
   0x39a79c   3b d1                          CMP EDX,ECX
```

Decompilation:

```c
// function FUN_7ff74f7aa39c @ 0x39a39c  body=1883 bytes  pdataStart=0x39a39c
// callers(2): FUN_7ff751b02d8c@0x26f2d8c FUN_7ff74f7ab570@0x39b570 
// callees(29): FUN_7ff74f9b37d4@0x5a37d4 FUN_7ff74f7ab32c@0x39b32c FUN_7ff74f4ea7c0@0xda7c0 FUN_7ff7518caba0@0x24baba0 FUN_7ff751ac1594@0x26b1594 FUN_7ff74fdb7834@0x9a7834 FUN_7ff74f7ab55c@0x39b55c FUN_7ff74f7aabf4@0x39abf4 FUN_7ff74f971c80@0x561c80 FUN_7ff74f78a00c@0x37a00c FUN_7ff74f692bcc@0x282bcc FUN_7ff74f7ab948@0x39b948 FUN_7ff74f7a9e88@0x399e88 FUN_7ff74f59a87c@0x18a87c floorf@0x18ee308 FUN_7ff751817e60@0x2407e60 FUN_7ff74fac8e88@0x6b8e88 FUN_7ff74f7aaee8@0x39aee8 FUN_7ff74f7ab04c@0x39b04c FUN_7ff74f7aaaf8@0x39aaf8 FUN_7ff74f7ab0e8@0x39b0e8 FUN_7ff74f7ab20c@0x39b20c FUN_7ff74fdbdffc@0x9adffc FUN_7ff74f7ab080@0x39b080 FUN_7ff751da8e08@0x2998e08 FUN_7ff74f501970@0xf1970 FUN_7ff74f4e63c0@0xd63c0 FUN_7ff7510407fc@0x1c307fc FUN_7ff74f495960@0x85960 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff74f7aa39c(longlong param_1,longlong param_2,undefined *param_3)

{
  longlong *plVar1;
  longlong *plVar2;
  char cVar3;
  char cVar4;
  char cVar5;
  char cVar6;
  BOOL BVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  undefined1 *puVar11;
  longlong lVar12;
  uint uVar13;
  ulonglong uVar14;
  ulonglong uVar15;
  uint uVar16;
  char cVar18;
  undefined4 *puVar19;
  bool bVar20;
  undefined4 uVar21;
  float fVar22;
  float fVar23;
  longlong local_res8;
  undefined *local_res18 [2];
  undefined1 local_58 [32];
  ulonglong uVar17;
  
  local_res8 = param_1;
  local_res18[0] = param_3;
  cVar3 = FUN_7ff74f4ea7c0();
  FUN_7ff74fdbdffc();
  FUN_7ff74f7ab948();
  *(undefined4 *)(param_2 + 0x13d8) = 3;
  uVar17 = 0;
  *(undefined4 *)(param_2 + 0xba4) = 0;
  uVar15 = _DAT_7ff7539cb028 & 0xffffffff;
  uVar14 = _DAT_7ff7539cb028 >> 0x20;
  *(undefined8 *)(param_2 + 0x1f44) = 4;
  *(undefined4 *)(param_2 + 0x1f50) = 0x5a0;
  cVar18 = '\x01';
  *(undefined1 *)(param_2 + 0x1f54) = 1;
  *(undefined4 *)(param_2 + 0x1f5c) = 2;
  *(undefined4 *)(param_2 + 0x1f74) = 2;
  *(undefined1 *)(param_2 + 0x1f60) = 0;
  *(undefined4 *)(param_2 + 0x1f62) = 0x1010101;
  *(undefined4 *)(param_2 + 0x1f58) = 0x3f800000;
  *(undefined4 *)(param_2 + 0x1f88) = 0x3f800000;
  *(undefined4 *)(param_2 + 0x1f94) = 0x3f800000;
  *(undefined4 *)(param_2 + 0x1fa0) = 0x3f800000;
  *(undefined4 *)(param_2 + 0x1fa4) = 0x3f800000;
  fVar23 = (float)uVar15 / (float)uVar14;
  *(undefined4 *)(param_2 + 0x1f68) = 0x40000000;
  *(undefined4 *)(param_2 + 0x1fac) = 0x42c80000;
  *(undefined4 *)(param_2 + 0x1fb0) = 1;
  *(undefined4 *)(param_2 + 0x1f78) = 0x3951b717;
  *(undefined4 *)(param_2 + 0x1f7c) = 0x69;
  *(undefined4 *)(param_2 + 0x1f84) = 0x3f8ccccd;
  *(undefined4 *)(param_2 + 0x1f80) = 0x3dcccccd;
  *(undefined4 *)(param_2 + 0x1f8c) = 0x3dcccccd;
  *(undefined4 *)(param_2 + 0x1f98) = 0x3e99999a;
  *(undefined4 *)(param_2 + 0x1f9c) = 0x3e4ccccd;
  *(undefined4 *)(param_2 + 0x1fa8) = 0x3f666666;
  *(undefined1 *)(param_2 + 0x1fb9) = 1;
  DAT_7ff7538c95c0 = 1;
  DAT_7ff7538c95c1 = 1;
  *(int *)(param_2 + 0x1f4c) = (int)(fVar23 * 1440.0);
  *(undefined4 *)(param_2 + 0x1f70) = 0x3f400000;
  *(undefined4 *)(param_2 + 0x1fb4) = 0x3f400000;
  *(undefined4 *)(param_2 + 0x1f90) =
       *(undefined4 *)(*(longlong *)(*(longlong *)(param_1 + 0x530) + 0xf8) + 0x830);
  fVar22 = *(float *)(param_2 + 0x1aa0);
  if (fVar22 <= 1.1920929e-05) {
    fVar22 = 1.1920929e-05;
  }
  fVar22 = floorf(1.0 / fVar22);
  iVar8 = *(int *)(param_2 + 0x188);
  if (*(int *)(param_2 + 0x188) < (int)fVar22) {
    iVar8 = (int)fVar22;
  }
  *(int *)(param_2 + 0x188) = iVar8;
  cVar4 = FUN_7ff74f7ab55c();
  if (cVar4 != '\0') {
    *(undefined1 *)(param_2 + 0x1fb8) = 1;
  }
  _DAT_7ff7539ca704 = 0;
  *(undefined4 *)(param_2 + 8000) = 0;
  DAT_7ff7538c958c = 1;
  *(int *)(param_2 + 0x1f3c) = (int)(fVar23 * 0.0);
  cVar4 = FUN_7ff74f4ea7c0();
  if (cVar4 == '\0') {
    puVar11 = (undefined1 *)FUN_7ff74f7aabf4();
    *puVar11 = 0;
    lVar12 = FUN_7ff74f7aabf4();
    *(undefined1 *)(lVar12 + 8) = 0;
  }
  else {
    FUN_7ff751ac1594(0);
    FUN_7ff751ac1594(1);
  }
  FUN_7ff74f59a87c(param_2,_DAT_7ff7539caf7c);
  *(undefined4 *)(param_2 + 0x20fc) = 0x3dcccccd;
  *(undefined4 *)(param_2 + 0x2100) = 0x40000000;
  puVar19 = (undefined4 *)(param_2 + 0x21a8);
  *(undefined4 *)(param_2 + 0x2104) = 0x3dcccccd;
  *(undefined8 *)(param_2 + 0x10a4) = 1;
  *(undefined4 *)(param_2 + 0x10ac) = 1;
  *(undefined4 *)(param_2 + 0x10d4) = 0xbf800000;
  *(undefined4 *)(param_2 + 0x2108) = 0x3e4ccccd;
  *(undefined1 *)(param_2 + 0x2110) = 1;
  *(undefined4 *)(param_2 + 0x210c) = 0x3f666666;
  *(undefined4 *)(param_2 + 0x2114) = 0x3f19999a;
  *(undefined4 *)(param_2 + 0x2118) = 0x3f266666;
  uVar14 = uVar17;
  do {
    *puVar19 = 0xbf800000;
    cVar4 = FUN_7ff74f4ea7c0();
    uVar15 = _DAT_7ff7539c9e40;
    if (((cVar4 == '\0') && ((int)uVar17 == 4)) &&
       (uVar14 = _DAT_7ff7539c9e40, _DAT_7ff7539c9e40 != 0)) {
      FUN_7ff74f7a9e88(local_58,0);
      cVar4 = FUN_7ff74f78a00c(uVar15);
      uVar14 = 0;
      if (cVar4 != '\0') {
        *puVar19 = 0x3f800000;
      }
    }
    lVar12 = _DAT_7ff7539c9e70;
    uVar16 = (int)uVar17 + 1;
    uVar17 = (ulonglong)uVar16;
    puVar19 = puVar19 + 1;
  } while (uVar16 < 8);
  *(undefined4 *)(param_2 + 0x21a0) = 0x40000000;
  *(undefined4 *)(param_2 + 0x21a4) = 3;
  *(undefined1 *)(param_2 + 0x219c) = 1;
  *(undefined4 *)(param_2 + 0x12f0) = 1;
  cVar4 = (char)uVar14;
  iVar8 = (int)uVar14;
  if (*(char *)(lVar12 + 0x8c) == cVar4) {
    *(int *)(param_2 + 0x188c) = iVar8;
  }
  uVar21 = FUN_7ff74f7ab32c(param_2);
  FUN_7ff74f7ab20c(uVar21,_DAT_7ff7539caf84);
  FUN_7ff74f7ab0e8(param_2,_DAT_7ff7539caf8c);
  FUN_7ff74f7ab080(param_2,_DAT_7ff7539caf9c);
  *(int *)(param_2 + 0x2194) = iVar8;
  *(undefined4 *)(param_2 + 0x2198) = 0xc2480000;
  cVar5 = FUN_7ff74f4ea7c0();
  if ((cVar5 == '\0') && (*(char *)(_DAT_7ff7539c9e58 + 0x1bb9) == cVar4)) {
    *(undefined4 *)(param_2 + 0x1b78) = 0xbf800000;
  }
  uVar21 = 0x3f000000;
  if (cVar3 == '\0') {
    uVar21 = *(undefined4 *)(&DAT_7ff752d87a58 + (longlong)_DAT_7ff7539cafac * 4);
    *(undefined4 *)(param_2 + 0x1970) =
         *(undefined4 *)(&UNK_7ff752d88390 + (longlong)_DAT_7ff7539cafac * 4);
  }
  *(undefined4 *)(param_2 + 0x1978) = uVar21;
  *(undefined4 *)(param_2 + 0x15cc) = 0x3f800000;
  lVar12 = FUN_7ff74f7aaaf8();
  uVar16 = (uint)(longlong)*(float *)(lVar12 + 8);
  *(uint *)(param_2 + 0x18f8) = uVar16;
  lVar12 = FUN_7ff74f7aaaf8();
  uVar13 = (uint)(longlong)*(float *)(lVar12 + 0xc);
  *(uint *)(param_2 + 0x18fc) = uVar13;
  if (uVar16 < uVar13) {
    uVar16 = uVar13;
  }
  *(uint *)(param_2 + 0x18f8) = uVar16;
  if (_DAT_7ff7539cb008 == 1) {
LAB_7ff74f7aa85e:
    *(undefined2 *)(param_2 + 0x20e0) = 1;
  }
  else if (_DAT_7ff7539cb008 == 2) {
    *(undefined2 *)(param_2 + 0x2060) = 0;
    *(undefined4 *)(param_2 + 0x2068) = 0x3e19999a;
    *(undefined4 *)(param_2 + 0x206c) = 0x3e19999a;
    *(undefined1 *)(param_2 + 0x2054) = 1;
    *(undefined8 *)(param_2 + 0x2058) = 3;
    *(char *)(param_2 + 0x2062) = cVar4;
    DAT_7ff753fe2580 = 1;
    *(undefined1 *)(param_2 + 0x20e0) = 1;
    *(undefined2 *)(param_2 + 0x2064) = 1;
    cVar3 = FUN_7ff74f4ea7c0();
    local_res18[0] = &DAT_7ff7538d91e0;
    cVar5 = FUN_7ff7510407fc(local_res18);
    cVar6 = FUN_7ff74f501970();
    if ((((cVar3 == '\0') && (cVar5 == '\0')) && (cVar6 == '\0')) ||
       (*(undefined1 *)(param_2 + 0x2065) = 1, cVar3 == '\0')) {
      *(int *)(param_2 + 0x2070) = iVar8;
    }
  }
  else {
    if (_DAT_7ff7539cb008 == 3) goto LAB_7ff74f7aa85e;
    *(undefined2 *)(param_2 + 0x20e0) = 0;
  }
  cVar3 = FUN_7ff74f9b37d4();
  if ((cVar3 != '\0') || (cVar5 = FUN_7ff74f4e63c0(), cVar3 = cVar4, cVar5 != '\0')) {
    cVar3 = cVar18;
  }
  BVar7 = IsWindowVisible(*(HWND *)(param_1 + 0xd0));
  if ((_DAT_7ff7539cafbc != iVar8) || (cVar6 = FUN_7ff74f4ea7c0(), cVar5 = cVar18, cVar6 == '\0')) {
    cVar5 = cVar4;
  }
  cVar6 = FUN_7ff74f692bcc(1);
  if (cVar6 == '\0') {
    local_res18[0] = &DAT_7ff7538d91f0;
    cVar6 = FUN_7ff74f495960(local_res18);
    if (cVar6 != '\0') goto LAB_7ff74f7aa8cf;
  }
  else {
LAB_7ff74f7aa8cf:
    cVar4 = '\x01';
  }
  iVar8 = FUN_7ff74f7ab04c();
  if ((((cVar3 == '\0') && (BVar7 == 1)) && (cVar5 == '\0')) && (cVar4 == '\0')) {
    local_res18[0] = &DAT_7ff7538d9140;
    cVar3 = FUN_7ff74f495960(local_res18);
    if (cVar3 == '\0') {
      local_res18[0] = &DAT_7ff7538d91a0;
      cVar3 = FUN_7ff74f495960(local_res18);
      if (((cVar3 == '\0') && (cVar3 = FUN_7ff74fdb7834(), cVar3 == '\0')) &&
         ((cVar3 = FUN_7ff74f971c80(), cVar3 == '\0' && (iVar8 != 0x19)))) {
        cVar3 = '\0';
        goto LAB_7ff74f7aa93f;
      }
    }
  }
  uVar14 = 1;
  cVar3 = cVar18;
LAB_7ff74f7aa93f:
  iVar8 = _DAT_7ff7539caff8;
  *(char *)(param_2 + 0x2055) = cVar3;
  if (iVar8 != 2) {
    *(undefined4 *)(param_2 + 0x1f44) = 0;
  }
  if (_DAT_7ff7539cb010 == 0) {
    *(undefined4 *)(param_2 + 0x20a8) = 0;
  }
  else if (_DAT_7ff7539cb010 == 1) {
    *(undefined4 *)(param_2 + 0x20a8) = 1;
  }
  else if (_DAT_7ff7539cb010 == 2) {
    *(undefined4 *)(param_2 + 0x20a8) = 2;
  }
  lVar12 = FUN_7ff74fac8e88();
  if (*(char *)(lVar12 + 0x10) == '\0') {
    *(undefined4 *)(param_2 + 0x20a8) = 0;
  }
  if ((_DAT_7ff7539cb008 == 0) || (iVar9 = 1, _DAT_7ff7539cb008 != 1)) {
    iVar9 = 0;
  }
  *(int *)(param_2 + 0x209c) = iVar9;
  lVar12 = FUN_7ff74fac8e88();
  if (*(char *)(lVar12 + 0xf0) == '\0') {
    *(int *)(param_2 + 0x209c) = 0;
  }
  iVar9 = _DAT_7ff7539cb00c + 1;
  *(char *)(param_2 + 0x20a4) = (char)uVar14;
  *(int *)(param_2 + 0x20a0) = iVar9;
  if ((iVar8 != 1) ||
     (lVar12 = FUN_7ff74fac8e88(), cVar3 = cVar18, *(char *)(lVar12 + 0x20) == '\0')) {
    *(undefined4 *)(param_2 + 0x208c) = 0;
    cVar3 = '\0';
  }
  lVar12 = local_res8;
  if ((DAT_7ff7539d3eaa == '\0') || (_DAT_7ff7539cb008 != 3)) {
    cVar18 = '\0';
  }
  *(char *)(param_2 + 0x20cc) = cVar18;
  iVar9 = 0;
  if ((_DAT_7ff7539cb014 != 0) && (iVar9 = 0, _DAT_7ff7539cb014 == 1)) {
    iVar9 = 1;
  }
  if ((cVar18 != '\0') && (iVar9 == 0)) {
    iVar9 = 1;
  }
  *(int *)(param_2 + 0x20d4) = iVar9;
  *(char *)(param_2 + 0x20cd) = (char)uVar14;
  if (iVar8 != 3) {
    *(undefined4 *)(param_2 + 0x20b8) = 0;
  }
  bVar20 = DAT_7ff7539d3e40 == '\0';
  if (bVar20) {
    *(undefined4 *)(param_2 + 0x20b8) = 0;
  }
  iVar9 = _DAT_7ff7539caff8;
  plVar1 = (longlong *)(local_res8 + 0x608);
  plVar2 = (longlong *)*plVar1;
  if ((plVar2 == (longlong *)0x0) || (iVar10 = (**(code **)(*plVar2 + 8))(), iVar10 != iVar9)) {
    FUN_7ff751da8e08(&local_res8,iVar9);
    if (local_res8 != 0) {
      FUN_7ff751817e60(plVar1,&local_res8);
    }
    FUN_7ff7518caba0(&local_res8);
  }
  if (((iVar8 == 2) || (cVar3 != '\0')) || (!bVar20 && iVar8 == 3)) {
    FUN_7ff74f7aaee8(lVar12,param_2);
  }
  return;
}
```

## 0x39a86c — LOD / draw distance (same function as 0x39a758)

- containing function: **0x39a39c** (1883 bytes, `FUN_7ff74f7aa39c`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e4a320	DATA	-
0x39b7bc	UNCONDITIONAL_CALL	FUN_7ff74f7ab570@0x39b570
0x26f31c0	UNCONDITIONAL_CALL	FUN_7ff751b02d8c@0x26f2d8c
```

Disassembly around the anchor (`=>` marks it):

```
   0x39a845   84 c0                          TEST AL,AL
   0x39a847   74 0c                          JZ 0x7ff74f7aa855 ; CONDITIONAL_JUMP 0x39a855 LAB_7ff74f7aa855
   0x39a849   40 88 bb 65 20 00 00           MOV byte ptr [RBX + 0x2065],DIL
   0x39a850   40 84 f6                       TEST SIL,SIL
   0x39a853   75 10                          JNZ 0x7ff74f7aa865 ; CONDITIONAL_JUMP 0x39a865 LAB_7ff74f7aa865
   0x39a855   44 89 bb 70 20 00 00           MOV dword ptr [RBX + 0x2070],R15D
   0x39a85c   eb 07                          JMP 0x7ff74f7aa865 ; UNCONDITIONAL_JUMP 0x39a865 LAB_7ff74f7aa865
   0x39a85e   66 89 bb e0 20 00 00           MOV word ptr [RBX + 0x20e0],DI
   0x39a865   e8 6a 8f 20 00                 CALL 0x7ff74f9b37d4 ; UNCONDITIONAL_CALL 0x5a37d4 FUN_7ff74f9b37d4
   0x39a86a   84 c0                          TEST AL,AL
=> 0x39a86c   75 0c                          JNZ 0x7ff74f7aa87a ; CONDITIONAL_JUMP 0x39a87a LAB_7ff74f7aa87a
   0x39a86e   e8 4d bb d3 ff                 CALL 0x7ff74f4e63c0 ; UNCONDITIONAL_CALL 0xd63c0 FUN_7ff74f4e63c0
   0x39a873   41 8a f7                       MOV SIL,R15B
   0x39a876   84 c0                          TEST AL,AL
   0x39a878   74 03                          JZ 0x7ff74f7aa87d ; CONDITIONAL_JUMP 0x39a87d LAB_7ff74f7aa87d
   0x39a87a   40 8a f7                       MOV SIL,DIL
   0x39a87d   49 8b 8d d0 00 00 00           MOV RCX,qword ptr [R13 + 0xd0]
   0x39a884   ff 15 86 80 54 03              CALL qword ptr [0x7ff752cf2910] ; READ 0x38e2910 PTR_IsWindowVisible_7ff752cf2910
   0x39a88a   3b c7                          CMP EAX,EDI
   0x39a88c   41 0f 95 c6                    SETNZ R14B
   0x39a890   44 39 3d 25 07 22 04           CMP dword ptr [0x7ff7539cafbc],R15D ; READ 0x45bafbc DAT_7ff7539cafbc
```

Decompilation: same function as above (0x39a39c).

## 0x133d8a — LOD / draw distance

- containing function: **0x133d18** (1038 bytes, `FUN_7ff74f543d18`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e35380	DATA	-
0x84511	UNCONDITIONAL_CALL	FUN_7ff74f494394@0x84394
```

Disassembly around the anchor (`=>` marks it):

```
   0x133d65   f3 44 0f 10 47 28              MOVSS XMM8,dword ptr [RDI + 0x28]
   0x133d6b   48 8b cb                       MOV RCX,RBX
   0x133d6e   e8 b5 03 00 00                 CALL 0x7ff74f544128 ; UNCONDITIONAL_CALL 0x134128 FUN_7ff74f544128
   0x133d73   85 c0                          TEST EAX,EAX
   0x133d75   74 13                          JZ 0x7ff74f543d8a ; CONDITIONAL_JUMP 0x133d8a LAB_7ff74f543d8a
   0x133d77   48 8b cb                       MOV RCX,RBX
   0x133d7a   e8 a9 03 00 00                 CALL 0x7ff74f544128 ; UNCONDITIONAL_CALL 0x134128 FUN_7ff74f544128
   0x133d7f   8b c0                          MOV EAX,EAX
   0x133d81   45 0f 57 c0                    XORPS XMM8,XMM8
   0x133d85   f3 4c 0f 2a c0                 CVTSI2SS XMM8,RAX
=> 0x133d8a   83 8c 24 80 00 00 00 ff        OR dword ptr [RSP + 0x80],0xffffffff
   0x133d92   48 8b 83 f8 00 00 00           MOV RAX,qword ptr [RBX + 0xf8]
   0x133d99   48 85 c0                       TEST RAX,RAX
   0x133d9c   74 10                          JZ 0x7ff74f543dae ; CONDITIONAL_JUMP 0x133dae LAB_7ff74f543dae
   0x133d9e   f7 80 98 00 00 00 00 00 04 00  TEST dword ptr [RAX + 0x98],0x40000
   0x133da8   0f 85 4e 03 00 00              JNZ 0x7ff74f5440fc ; CONDITIONAL_JUMP 0x1340fc LAB_7ff74f5440fc
   0x133dae   48 8b 0d a3 60 48 04           MOV RCX,qword ptr [0x7ff7539c9e58] ; READ 0x45b9e58 DAT_7ff7539c9e58
   0x133db5   40 38 a9 f1 11 00 00           CMP byte ptr [RCX + 0x11f1],BPL
   0x133dbc   0f 84 86 00 00 00              JZ 0x7ff74f543e48 ; CONDITIONAL_JUMP 0x133e48 LAB_7ff74f543e48
   0x133dc2   48 81 c1 00 05 00 00           ADD RCX,0x500
   0x133dc9   48 8d 93 10 01 00 00           LEA RDX,[RBX + 0x110]
```

Decompilation:

```c
// function FUN_7ff74f543d18 @ 0x133d18  body=1038 bytes  pdataStart=0x133d18
// callers(1): FUN_7ff74f494394@0x84394 
// callees(16): FUN_7ff74fff0378@0xbe0378 FUN_7ff74f5456f4@0x1356f4 FUN_7ff74fd1ea0c@0x90ea0c FUN_7ff74fe01ce0@0x9f1ce0 FUN_7ff751455068@0x2045068 FUN_7ff74f68a8b8@0x27a8b8 FUN_7ff74f545b78@0x135b78 FUN_7ff74f544128@0x134128 FUN_7ff74f8977ac@0x4877ac FUN_7ff74f4e8b24@0xd8b24 FUN_7ff74f6900c8@0x2800c8 FUN_7ff74f7678e0@0x3578e0 FUN_7ff74f4eda58@0xdda58 FUN_7ff74f49a630@0x8a630 FUN_7ff74f88f554@0x47f554 FUN_7ff74f4e6b40@0xd6b40 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff74f543d18(longlong param_1,longlong param_2)

{
  float fVar1;
  char cVar2;
  int iVar3;
  uint uVar4;
  undefined8 uVar5;
  longlong lVar6;
  float *pfVar7;
  bool bVar8;
  float extraout_XMM0_Da;
  float fVar9;
  float fVar10;
  float fVar11;
  float fVar12;
  float fVar13;
  undefined4 local_res8 [2];
  
  if (((*(char *)(param_1 + 0x108) != '\0') || (*(char *)(param_1 + 0xad3f) != '\0')) &&
     (cVar2 = FUN_7ff74fd1ea0c(), cVar2 == '\0')) {
    return;
  }
  fVar13 = *(float *)(param_2 + 0x28);
  iVar3 = FUN_7ff74f544128(param_1);
  if (iVar3 != 0) {
    uVar4 = FUN_7ff74f544128(param_1);
    fVar13 = (float)uVar4;
  }
  local_res8[0] = 0xffffffff;
  if ((*(longlong *)(param_1 + 0xf8) != 0) &&
     ((*(uint *)(*(longlong *)(param_1 + 0xf8) + 0x98) & 0x40000) != 0)) {
    return;
  }
  if (((*(char *)(_DAT_7ff7539c9e58 + 0x11f1) == '\0') ||
      (cVar2 = FUN_7ff74f4eda58(_DAT_7ff7539c9e58 + 0x500,param_1 + 0x110,local_res8), cVar2 != '\0'
      )) || (*(char *)(param_1 + 0x859) != '\0')) {
    uVar5 = FUN_7ff74f4e8b24();
    cVar2 = FUN_7ff74f4e6b40(param_2 + 0x70,uVar5);
    if (cVar2 != '\0') {
      fVar12 = *(float *)(param_2 + 0x70);
      fVar1 = *(float *)(param_2 + 0x74);
      fVar11 = *(float *)(param_2 + 0x78);
      lVar6 = param_1;
      if (*(int *)(param_1 + 0xacd4) != 0) {
        lVar6 = FUN_7ff751455068(param_1);
      }
      pfVar7 = (float *)FUN_7ff74f49a630(lVar6,0);
      fVar10 = fVar12 - *pfVar7;
      fVar9 = fVar11 - pfVar7[2];
      fVar9 = (float)FUN_7ff74fff0378((fVar1 - pfVar7[1]) * (fVar1 - pfVar7[1]) + fVar10 * fVar10 +
                                      fVar9 * fVar9);
      fVar12 = fVar12 - *(float *)(lVar6 + 0x110);
      fVar11 = fVar11 - *(float *)(lVar6 + 0x118);
      fVar10 = fVar1 - *(float *)(lVar6 + 0x114);
      fVar12 = (float)FUN_7ff74fff0378(fVar10 * fVar10 + fVar12 * fVar12 + fVar11 * fVar11);
      if (fVar12 - fVar9 < 0.0) {
        fVar9 = fVar12;
      }
      *(float *)(param_1 + 0xae18) = fVar9;
      fVar12 = *(float *)(lVar6 + 0x114);
      fVar11 = fVar9 - *(float *)(*(longlong *)(param_1 + 0xe8) + 0x2f0);
      *(float *)(param_1 + 0xae1c) = fVar9;
      *(float *)(param_1 + 0xae20) = fVar12 - fVar1;
      cVar2 = FUN_7ff74f6900c8(param_1);
      if (((cVar2 == '\0') || (2 < *(int *)(param_1 + 0x8d4))) ||
         ((1 < *(int *)(param_1 + 0x8d4) &&
          ((*(char *)(param_1 + 0x107) != '\0' && (*(char *)(param_1 + 0x859) == '\0')))))) {
        *(undefined1 *)(param_1 + 0xae14) = 0;
      }
      else {
        cVar2 = FUN_7ff74f5456f4(param_1);
        if (((cVar2 == '\0') && (cVar2 = FUN_7ff74f88f554(), cVar2 == '\0')) &&
           ((fVar13 < fVar11 ||
            ((fVar12 = *(float *)(param_1 + 0xae20), fVar12 < *(float *)(param_2 + 0x30) ||
             (*(float *)(param_2 + 0x2c) <= fVar12 && fVar12 != *(float *)(param_2 + 0x2c))))))) {
          bVar8 = true;
          if (*(longlong *)(param_1 + 0xb0) != 0) {
            bVar8 = *(int *)(*(longlong *)(param_1 + 0xb0) + 0x10) != 0;
          }
          lVar6 = FUN_7ff74f545b78(_DAT_7ff7539c9e40,*(undefined4 *)(param_1 + 4));
          if ((((*(char *)(param_1 + 0x89f) == '\0') || (*(char *)(param_1 + 0xacc8) != '\0')) &&
              ((lVar6 == 0 || (*(char *)(lVar6 + 0xe5) == '\0')))) ||
             (((lVar6 == 0 || (*(char *)(lVar6 + 0xe8) == '\0')) || (!bVar8)))) {
            cVar2 = FUN_7ff74f8977ac(param_1);
            if (cVar2 == '\0') {
              *(undefined1 *)(param_1 + 0xae14) = 0;
            }
          }
          else {
            *(undefined1 *)(param_1 + 0xae24) = 1;
          }
          if (*(char *)(param_1 + 0xae14) == '\0') {
            if (*(char *)(param_1 + 0x884) == '\0') {
              if ((*(float *)(param_2 + 0x30) <= *(float *)(param_1 + 0xae20)) &&
                 (*(float *)(param_1 + 0xae20) <= *(float *)(param_2 + 0x2c))) {
                iVar3 = FUN_7ff74f544128(param_1);
                fVar12 = extraout_XMM0_Da;
                if (iVar3 != 0) {
                  uVar4 = FUN_7ff74f544128(param_1);
                  fVar12 = (float)uVar4;
                }
                if (fVar11 < fVar12) {
                  *(undefined1 *)(param_1 + 0xae29) = 1;
                }
              }
            }
            else {
              *(undefined1 *)(param_1 + 0xae14) = 1;
              *(undefined1 *)(param_1 + 0xae25) = 1;
            }
          }
        }
      }
      *(undefined1 *)(param_1 + 0xae26) = 1;
      cVar2 = FUN_7ff74f5456f4(param_1);
      if (cVar2 == '\0') {
        fVar13 = fVar13 - fVar11;
      }
      *(float *)(param_1 + 0x8c0) = fVar13;
      return;
    }
    *(float *)(param_1 + 0x8c0) = fVar13;
    cVar2 = FUN_7ff74f6900c8(param_1);
    if (cVar2 != '\0') {
      return;
    }
  }
  else if (((*(int *)(param_1 + 0xab84) != -1) || (cVar2 = FUN_7ff74fe01ce0(param_1), cVar2 != '\0')
           ) && (cVar2 = FUN_7ff74f68a8b8(), cVar2 == '\0')) {
    cVar2 = FUN_7ff74f4eda58(_DAT_7ff7539c9e58 + 0x500,param_1 + 0x200,0);
    if (cVar2 == '\0') {
      return;
    }
    FUN_7ff74f7678e0(param_1,param_1 + 0x200);
    return;
  }
  *(undefined1 *)(param_1 + 0xae14) = 0;
  return;
}
```

## 0xde8d0 — LOD / draw distance

- containing function: **0xddc70** (10159 bytes, `FUN_7ff74f4edc70`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e33220	DATA	-
0x4135094	DATA	-
0x41350f0	DATA	-
0x4135130	DATA	-
0x567d1a	UNCONDITIONAL_CALL	FUN_7ff74f977a48@0x567a48
0x10e2219	UNCONDITIONAL_CALL	FUN_7ff7504f2200@0x10e2200
```

Disassembly around the anchor (`=>` marks it):

```
   0xde89a    49 8d 8d fc 00 00 00           LEA RCX,[R13 + 0xfc]
   0xde8a1    32 db                          XOR BL,BL
   0xde8a3    e8 58 a2 a5 00                 CALL 0x7ff74ff48b00 ; UNCONDITIONAL_CALL 0xb38b00 FUN_7ff74ff48b00
   0xde8a8    45 84 e4                       TEST R12B,R12B
   0xde8ab    0f 84 f1 06 00 00              JZ 0x7ff74f4eefa2 ; CONDITIONAL_JUMP 0xdefa2 LAB_7ff74f4eefa2
   0xde8b1    84 db                          TEST BL,BL
   0xde8b3    0f 84 e9 06 00 00              JZ 0x7ff74f4eefa2 ; CONDITIONAL_JUMP 0xdefa2 LAB_7ff74f4eefa2
   0xde8b9    48 8b 05 98 b5 4d 04           MOV RAX,qword ptr [0x7ff7539c9e58] ; READ 0x45b9e58 DAT_7ff7539c9e58
   0xde8c0    f3 45 0f 10 85 c0 00 00 00     MOVSS XMM8,dword ptr [R13 + 0xc0]
   0xde8c9    41 8b 95 e0 00 00 00           MOV EDX,dword ptr [R13 + 0xe0]
=> 0xde8d0    80 b8 f7 11 00 00 00           CMP byte ptr [RAX + 0x11f7],0x0
   0xde8d7    74 43                          JZ 0x7ff74f4ee91c ; CONDITIONAL_JUMP 0xde91c LAB_7ff74f4ee91c
   0xde8d9    f3 41 0f 10 85 90 00 00 00     MOVSS XMM0,dword ptr [R13 + 0x90]
   0xde8e2    f3 0f 10 0d 22 5c b2 03        MOVSS XMM1,dword ptr [0x7ff75301450c] ; READ 0x3c0450c DAT_7ff75301450c
   0xde8ea    0f 2f c1                       COMISS XMM0,XMM1
   0xde8ed    73 1c                          JNC 0x7ff74f4ee90b ; CONDITIONAL_JUMP 0xde90b LAB_7ff74f4ee90b
   0xde8ef    f3 41 0f 10 85 94 00 00 00     MOVSS XMM0,dword ptr [R13 + 0x94]
   0xde8f8    0f 2f c1                       COMISS XMM0,XMM1
   0xde8fb    73 0e                          JNC 0x7ff74f4ee90b ; CONDITIONAL_JUMP 0xde90b LAB_7ff74f4ee90b
   0xde8fd    f3 41 0f 10 85 98 00 00 00     MOVSS XMM0,dword ptr [R13 + 0x98]
   0xde906    0f 2f c1                       COMISS XMM0,XMM1
```

Decompilation:

```c
// function FUN_7ff74f4edc70 @ 0xddc70  body=10159 bytes  pdataStart=0xddc70
// callers(2): FUN_7ff7504f2200@0x10e2200 FUN_7ff74f977a48@0x567a48 
// callees(49): FUN_7ff74f61b1e4@0x20b1e4 FUN_7ff74f7cb6c0@0x3bb6c0 FUN_7ff74f4ed570@0xdd570 FUN_7ff74f4e63f0@0xd63f0 FUN_7ff74f860724@0x450724 FUN_7ff7513b9094@0x1fa9094 FUN_7ff74f4e63d8@0xd63d8 FUN_7ff74f646788@0x236788 FUN_7ff74f4de5f0@0xce5f0 FUN_7ff74f4e6d7c@0xd6d7c FUN_7ff74f457df8@0x47df8 FUN_7ff74faacffc@0x69cffc FUN_7ff74ff489f0@0xb389f0 FUN_7ff7504f4480@0x10e4480 FUN_7ff74f58452c@0x17452c FUN_7ff74f4e6ca0@0xd6ca0 FUN_7ff74f6213d0@0x2113d0 FUN_7ff74f981988@0x571988 FUN_7ff74f8c35d4@0x4b35d4 FUN_7ff74f7cd184@0x3bd184 FUN_7ff74faacdc8@0x69cdc8 FUN_7ff74faad258@0x69d258 FUN_7ff74f7cca00@0x3bca00 FUN_7ff74f4584c0@0x484c0 FUN_7ff74faad740@0x69d740 FUN_7ff74fff0378@0xbe0378 FUN_7ff74f772c90@0x362c90 FUN_7ff74f57407c@0x16407c FUN_7ff74f998ae4@0x588ae4 FUN_7ff74f4ed7ac@0xdd7ac FUN_7ff7504ed0c0@0x10dd0c0 FUN_7ff74fd56204@0x946204 FUN_7ff74f457550@0x47550 FUN_7ff751cb7910@0x28a7910 FUN_7ff74f4ed710@0xdd710 FUN_7ff74fc264c4@0x8164c4 FUN_7ff7504f2990@0x10e2990 __security_check_cookie@0xb912e0 FUN_7ff74f4e8d90@0xd8d90 FUN_7ff74f83f370@0x42f370 FUN_7ff74f621350@0x211350 FUN_7ff74f44f8e4@0x3f8e4 FUN_7ff74f7ccc54@0x3bcc54 FUN_7ff74f4e6cd8@0xd6cd8 FUN_7ff74ff48b00@0xb38b00 FUN_7ff74f587274@0x177274 FUN_7ff74faad330@0x69d330 FUN_7ff74f4e63c0@0xd63c0 FUN_7ff74f575f70@0x165f70 

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff74f4edc70(undefined8 *param_1,ulonglong param_2,ulonglong param_3,float *param_4)

{
  longlong lVar1;
  bool bVar2;
  bool bVar3;
  int *piVar4;
  undefined1 uVar5;
  char cVar6;
  byte bVar7;
  char cVar8;
  char cVar9;
  char cVar10;
  int iVar11;
  int iVar12;
  longlong lVar13;
  int *piVar14;
  uint *puVar15;
  longlong lVar16;
  float *pfVar17;
  longlong lVar18;
  byte bVar19;
  uint uVar20;
  int *piVar21;
  ulonglong uVar22;
  longlong *plVar23;
  uint uVar24;
  undefined8 *puVar25;
  longlong *plVar26;
  uint uVar27;
  uint uVar28;
  float *pfVar29;
  ulonglong uVar30;
  ulonglong uVar31;
  undefined8 extraout_XMM0_Qa;
  undefined8 uVar32;
  undefined8 uVar33;
  undefined8 extraout_XMM0_Qa_00;
  undefined8 extraout_XMM0_Qa_01;
  undefined8 extraout_XMM0_Qb;
  float fVar34;
  undefined1 auVar38 [12];
  undefined1 auVar35 [16];
  undefined1 auVar36 [16];
  undefined1 auVar37 [16];
  float fVar39;
  float fVar40;
  undefined4 uVar41;
  undefined4 uVar42;
  undefined4 uVar43;
  undefined4 uVar44;
  float fVar45;
  float fVar46;
  float fVar47;
  float fVar48;
  float fVar49;
  undefined1 auStack_368 [32];
  longlong *local_348;
  undefined8 *local_340;
  undefined8 *local_338;
  undefined1 local_330;
  uint local_328;
  uint local_324;
  char local_320;
  char local_31f [7];
  float *local_318;
  undefined8 local_310;
  undefined8 *local_308;
  undefined8 local_300;
  longlong local_2f8;
  undefined2 local_2f0 [4];
  longlong *local_2e8;
  undefined4 local_2e0;
  undefined2 local_2dc;
  char local_2da;
  float local_2d8;
  longlong local_2d0;
  float local_2c8;
  float fStack_2c4;
  float fStack_2c0;
  float fStack_2bc;
  longlong *local_2b8;
  longlong local_2b0;
  ulonglong local_2a8;
  longlong *local_2a0;
  longlong local_298;
  longlong local_290;
  ulonglong local_288;
  longlong *local_278;
  undefined8 uStack_270;
  longlong *local_268;
  undefined4 uStack_260;
  undefined4 uStack_25c;
  float local_258 [3];
  undefined4 local_24c;
  undefined1 local_248 [12];
  undefined4 local_23c;
  undefined8 local_238;
  undefined8 uStack_230;
  undefined8 local_228;
  undefined4 uStack_220;
  undefined4 uStack_21c;
  undefined8 local_218;
  undefined4 uStack_210;
  undefined4 uStack_20c;
  undefined8 local_208;
  undefined8 uStack_200;
  undefined8 local_1f8;
  undefined8 uStack_1f0;
  undefined8 local_1e8;
  undefined8 uStack_1e0;
  undefined8 local_1d8;
  undefined8 uStack_1d0;
  undefined1 local_1c8 [16];
  undefined1 local_1b8 [16];
  undefined **local_1a8;
  undefined1 local_1a0;
  undefined ***local_170;
  undefined8 local_168 [2];
  undefined1 local_158 [16];
  undefined8 local_148;
  undefined8 uStack_140;
  undefined4 local_138;
  undefined4 uStack_134;
  undefined4 uStack_130;
  undefined4 uStack_12c;
  undefined1 local_128 [64];
  ulonglong local_e8;
  
  local_e8 = _DAT_7ff7538c7f30 ^ (ulonglong)auStack_368;
  uVar20 = 0;
  local_328 = 0;
  if ((*(byte *)(_DAT_7ff7539c9e58 + 8) & 1) == 0) {
    local_2d8 = *(float *)(_DAT_7ff7539c9e58 + 0x1200);
    uVar33 = *(undefined8 *)(_DAT_7ff753b69910 + 0x530);
    local_318 = param_4;
    local_308 = param_1;
    local_300 = uVar33;
    local_288 = param_3;
    if (param_2 < param_3) {
      do {
        pfVar29 = local_318;
        lVar18 = *(longlong *)(param_1[2] + param_2 * 8);
        local_2a8 = param_2;
        if ((lVar18 != 0) && (*(int *)(lVar18 + 0x118) == 0)) {
          lVar16 = *(longlong *)(lVar18 + 0x108);
          local_2f8 = lVar18;
          lVar13 = FUN_7ff74f4e8d90(param_1,*(undefined4 *)(lVar18 + 0x134));
          local_290 = lVar13;
          if (lVar13 != 0) {
            if (_DAT_7ff7539de800 != (int *)0x0) {
              iVar12 = *_DAT_7ff7539de800;
              if ((iVar12 - 0xbU < 6) || (iVar11 = 0, iVar12 == 8)) {
                iVar11 = _DAT_7ff7539de800[7];
              }
              if ((iVar11 != 0) &&
                 ((((iVar12 - 0xbU < 6 || (iVar12 == 8)) && (_DAT_7ff7539de800[7] != 0)) &&
                  (((char)_DAT_7ff7539de800[0x7ab] == '\0' &&
                   ((*(byte *)(_DAT_7ff7539de800 + 0x7e6) & 0x80) != 0)))))) {
                piVar14 = (int *)**(undefined8 **)(_DAT_7ff7539de800 + 0x888);
                piVar21 = _DAT_7ff7539de800;
                while (*(char *)((longlong)piVar14 + 0x19) == '\0') {
                  if (*(int *)(lVar18 + 0x134) == piVar14[7]) goto LAB_7ff74f4edea6;
                  piVar21 = *(int **)(piVar14 + 4);
                  if (*(char *)((longlong)piVar21 + 0x19) == '\0') {
                    cVar6 = *(char *)((longlong)*(int **)piVar21 + 0x19);
                    piVar14 = piVar21;
                    piVar4 = *(int **)piVar21;
                    while (cVar6 == '\0') {
                      piVar21 = *(int **)piVar4;
                      piVar14 = piVar4;
                      piVar4 = piVar21;
                      cVar6 = *(char *)((longlong)piVar21 + 0x19);
                    }
                  }
                  else {
                    cVar6 = *(char *)((longlong)*(int **)(piVar14 + 2) + 0x19);
                    piVar21 = *(int **)(piVar14 + 2);
                    piVar4 = piVar14;
                    while ((piVar14 = piVar21, piVar21 = piVar14, cVar6 == '\0' &&
                           (piVar4 == *(int **)(piVar14 + 4)))) {
                      cVar6 = *(char *)((longlong)*(int **)(piVar14 + 2) + 0x19);
                      piVar21 = *(int **)(piVar14 + 2);
                      piVar4 = piVar14;
                    }
                  }
                }
                if (*(longlong **)(lVar16 + 0x28) != (longlong *)0x0) {
                  local_348 = *(longlong **)(lVar16 + 0x28);
                  FUN_7ff74f457550(piVar21,pfVar29,lVar13,lVar18);
                }
                goto LAB_7ff74f4f035f;
              }
            }
LAB_7ff74f4edea6:
            local_2e8 = (longlong *)0x0;
            plVar26 = (longlong *)0x0;
            local_2b0 = 0;
            uVar32 = extraout_XMM0_Qa;
            if (*(char *)(lVar18 + 7) == '\0') {
              plVar26 = *(longlong **)(lVar16 + 0x28);
              local_2e8 = plVar26;
              if (plVar26 != (longlong *)0x0) {
                bVar7 = *(byte *)(lVar13 + 0x109);
                if (*(byte *)((longlong)plVar26 + 0x551) != bVar7) {
                  *(byte *)((longlong)plVar26 + 0x551) = bVar7;
                  uVar30 = 0xffffffff;
                  if (bVar7 != 0xff) {
                    uVar30 = (ulonglong)bVar7;
                  }
                  uVar32 = FUN_7ff74f44f8e4(plVar26,uVar33,uVar30);
                }
LAB_7ff74f4edf15:
                local_348 = plVar26;
                FUN_7ff74f457550(uVar32,pfVar29,lVar13,lVar18);
                iVar12 = *(int *)(lVar18 + 0x1a0);
                *(undefined1 *)(lVar18 + 0x19c) = 1;
                *(int *)(lVar18 + 0x1a0) = iVar12 + -1;
                if ((0 < iVar12) && (*(char *)((longlong)pfVar29 + 0x47) == '\0')) {
                  bVar3 = false;
                  if (*(longlong *)(lVar13 + 0xa8) != 0) {
                    if ((((*(char *)(*(longlong *)(lVar13 + 0xa8) + 0xcd) - 1U & 0xfd) == 0) &&
                        (*(char *)(*(longlong *)(lVar13 + 0xa0) + 0x103) == '\0')) &&
                       (*(char *)(*(longlong *)(lVar13 + 0xa0) + 0x104) == '\0')) {
                      bVar3 = true;
                    }
                    else {
                      bVar3 = false;
                    }
                    if (*(char *)(lVar13 + 0x113) == '\x04') {
                      bVar3 = true;
                    }
                  }
                  uVar27 = *(uint *)(lVar13 + 0xb8);
                  local_340 = (undefined8 *)CONCAT71(local_340._1_7_,1);
                  local_348 = plVar26;
                  FUN_7ff74f4ed7ac(local_308,lVar13);
                  if (bVar3) {
LAB_7ff74f4edff8:
                    bVar3 = false;
                  }
                  else {
                    local_328 = uVar20 | 1;
                    if ((((uVar27 & 4) != 0) != ((*(ulonglong *)(lVar13 + 0xb8) & 4) != 0)) &&
                       (local_328 = uVar20 | 3, uVar20 = local_328,
                       (*(ulonglong *)(lVar13 + 0xb8) & 4) == 0)) goto LAB_7ff74f4edff8;
                    bVar3 = true;
                    uVar20 = local_328;
                  }
                  if ((uVar20 & 2) != 0) {
                    local_328 = uVar20 & 0xfffffffd;
                    uVar20 = local_328;
                  }
                  if ((uVar20 & 1) != 0) {
                    local_328 = uVar20 & 0xfffffffe;
                    uVar20 = local_328;
                  }
                  pfVar29 = local_318;
                  if (bVar3) {
                    *(undefined1 *)(lVar18 + 0x19c) = 0;
                  }
                }
                param_1 = local_308;
                if (*(char *)(lVar18 + 0x19c) != '\0') {
                  if ((*(char *)(lVar18 + 0x122) != '\0') && (*(char *)(lVar18 + 300) == '\0')) {
                    local_340 = (undefined8 *)CONCAT44(local_340._4_4_,0xffffffff);
                    local_23c = 0x3f800000;
                    local_24c = 0x3f800000;
                    local_348 = (longlong *)CONCAT44(local_348._4_4_,0xffffffff);
                    (**(code **)(*plVar26 + 0xf0))(plVar26,uVar33,local_248,local_258);
                    FUN_7ff74f998ae4(lVar18 + 0x68,local_248,local_258);
                    *(undefined1 *)(lVar18 + 300) = 1;
                  }
                  uVar33 = 0;
                  if (((((*(ulonglong *)(lVar13 + 0xb8) >> 0x21 & 1) != 0) &&
                       ((*(ulonglong *)(lVar13 + 0xb8) & 0x400000000) == 0)) &&
                      (_DAT_7ff7539d3760 != 0)) && (*(char *)(_DAT_7ff7539d3760 + 0x1f0) != '\0')) {
                    if (*(longlong *)(lVar13 + 0x20) == 0) {
                      if (*(longlong *)(lVar13 + 0xb0) != 0) {
                        uVar33 = *(undefined8 *)(*(longlong *)(lVar13 + 0xb0) + 0x108);
                      }
                    }
                    else {
                      uVar33 = *(undefined8 *)(*(longlong *)(lVar13 + 0x20) + 0x70);
                    }
                    local_1a0 = 1;
                    local_1a8 = &PTR_LAB_7ff752ea9b80;
                    local_170 = &local_1a8;
                    FUN_7ff74f981988(uVar33,&local_1a8,0);
                    if (local_170 != (undefined ***)0x0) {
                      (*(code *)(*local_170)[4])
                                (local_170,
                                 CONCAT71((int7)((ulonglong)&local_1a8 >> 8),local_170 != &local_1a8
                                         ));
                      local_170 = (undefined ***)0x0;
                    }
                    *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x400000000;
                  }
                  fVar47 = *(float *)(lVar13 + 0x80) - pfVar29[4];
                  fVar48 = *(float *)(lVar13 + 0x84) - pfVar29[5];
                  fVar49 = *(float *)(lVar13 + 0x88) - pfVar29[6];
                  fVar45 = *(float *)(lVar13 + 0x70) * *(float *)(lVar13 + 0x90);
                  auVar37._0_4_ = *(float *)(lVar13 + 0x94) * *(float *)(lVar13 + 0x74);
                  fVar46 = *(float *)(lVar13 + 0x98) * *(float *)(lVar13 + 0x78);
                  fVar47 = fVar47 * fVar47;
                  fVar48 = fVar48 * fVar48;
                  fVar49 = fVar49 * fVar49;
                  fVar34 = fVar47 + fVar48 + fVar49;
                  if (fVar34 < 0.0) {
                    fVar34 = (float)FUN_7ff74fff0378();
                  }
                  else {
                    fVar34 = SQRT(fVar34);
                  }
                  auVar37._0_4_ = auVar37._0_4_ * auVar37._0_4_ + fVar45 * fVar45 + fVar46 * fVar46;
                  if (auVar37._0_4_ < 0.0) {
                    auVar37._0_4_ = (float)FUN_7ff74fff0378();
                  }
                  else {
                    auVar37._0_4_ = SQRT(auVar37._0_4_);
                  }
                  fVar34 = fVar34 - auVar37._0_4_ * 0.5;
                  if (fVar34 <= 0.0) {
                    fVar34 = 0.0;
                  }
                  if (plVar26 != (longlong *)0x0) {
                    FUN_7ff74f621350(plVar26,0xffffffff);
                  }
                  lVar16 = _DAT_7ff7539c9e58;
                  lVar18 = *(longlong *)(lVar13 + 0xb0);
                  local_2d0 = 0;
                  local_310 = *(undefined8 *)(_DAT_7ff753b69910 + 0x530);
                  local_278 = (longlong *)0x0;
                  local_2b8 = (longlong *)0x0;
                  local_2a0 = (longlong *)0x0;
                  puVar15 = (uint *)FUN_7ff74f4ed710(*local_308);
                  if (lVar18 != 0) {
                    local_2d0 = *(longlong *)(lVar18 + 0x108);
                    plVar26 = *(longlong **)(local_2d0 + 0x28);
                    if (*(char *)(lVar18 + 7) == '\x01') {
                      local_2a0 = *(longlong **)
                                   (*(longlong *)
                                     (*(longlong *)
                                       (**(longlong **)(*(longlong *)(plVar26[0xc] + 0x28) + 0x10) +
                                       0x28) + 0x10) + 0x28);
                      local_2b8 = plVar26;
                      if (local_2a0 != (longlong *)0x0) {
                        cVar6 = *(char *)(lVar13 + 0x10c);
                        plVar26 = local_2a0;
joined_r0x7ff74f4ee2f4:
                        if (cVar6 == -1) {
                          uVar5 = FUN_7ff74f646788(plVar26,0);
                          *(undefined1 *)(lVar13 + 0x10c) = uVar5;
                        }
                      }
                    }
                    else {
                      local_278 = plVar26;
                      if (plVar26 != (longlong *)0x0) {
                        cVar6 = *(char *)(lVar13 + 0x10c);
                        goto joined_r0x7ff74f4ee2f4;
                      }
                    }
                  }
                  bVar7 = true;
                  *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) & 0xffffff7fffe7df03
                  ;
                  if (((*(int *)(lVar13 + 0xe0) == 0) &&
                      ((*(byte *)(_DAT_7ff7539c9e58 + 0xc) & 0x80) != 0)) &&
                     ((*(float *)(lVar13 + 0x90) < *(float *)(_DAT_7ff7539c9e58 + 0x10) &&
                      ((*(float *)(lVar13 + 0x94) < *(float *)(_DAT_7ff7539c9e58 + 0x14) &&
                       (*(float *)(lVar13 + 0x98) < *(float *)(_DAT_7ff7539c9e58 + 0x18))))))) {
                    *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 8;
                    bVar7 = false;
                    FUN_7ff74ff48b00(lVar13 + 0xfc);
                  }
                  if (((puVar15 == (uint *)0x0) || ((*puVar15 & 0xfffffff0) != 0xc24a0)) ||
                     (cVar6 = FUN_7ff7504f4480(lVar13), cVar6 == '\0')) {
                    if ((((bool)bVar7) && ((*(ulonglong *)(lVar13 + 0xb8) >> 9 & 1) != 0)) &&
                       ((bVar19 = *(byte *)(lVar13 + 0x111), bVar19 != 0 &&
                        ((*(ulonglong *)(lVar16 + 0x19f0 + (ulonglong)(bVar19 >> 6) * 8) >>
                          (bVar19 & 0x3f) & 1) != 0)))) {
                      bVar7 = false;
                      *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x20;
                    }
                  }
                  else {
                    *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 8;
                    bVar7 = false;
                    FUN_7ff74ff48b00(lVar13 + 0xfc);
                  }
                  if (((lVar16 != 0) && (*(char *)(lVar16 + 0x842) != '\0')) &&
                     (*(longlong *)(lVar13 + 0x38) != 0)) {
                    local_2f0[0] = *(undefined2 *)(lVar16 + 0x840);
                    lVar16 = FUN_7ff7504ed0c0(lVar13 + 0x30,local_2f0);
                    if (lVar16 != *(longlong *)(lVar13 + 0x30)) {
                      bVar7 = false;
                      *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x40;
                    }
                  }
                  local_298 = FUN_7ff74f4ed570(*(undefined8 *)(_DAT_7ff7539c9e30 + 0xe0),
                                               *(undefined4 *)(lVar13 + 0xe8));
                  iVar12 = *(int *)(lVar13 + 0xe0);
                  if (iVar12 != 0) {
                    lVar16 = *(longlong *)(lVar13 + 0xa0);
                    bVar3 = false;
                    bVar7 = true;
                    if ((lVar16 != 0) &&
                       (((*(int *)(lVar16 + 0xb4) == 2 && (*(char *)(lVar16 + 0x100) != '\0')) ||
                        ((*(char *)(lVar16 + 0xf7) != '\0' && (*(char *)(lVar13 + 0x10e) != '\0'))))
                       )) {
                      auVar37._0_4_ = *(float *)(lVar13 + 0xc0);
                      fVar45 = (float)FUN_7ff74f587274(_DAT_7ff7539d8458 + 0x80,lVar16,
                                                       *(undefined4 *)(lVar16 + 0x10));
                      if ((auVar37._0_4_ <= fVar45) && (bVar3 = false, 0.0 < auVar37._0_4_)) {
                        bVar3 = true;
                      }
                    }
                    if (((_DAT_7ff7539de800 == (int *)0x0) || ((char)_DAT_7ff7539de800[6] == '\0'))
                       || (*(char *)(_DAT_7ff7539c9e58 + 0x500) == *(char *)(local_298 + 0xd4))) {
                      if (*(char *)(lVar13 + 0x10e) == '\0') {
                        FUN_7ff74ff48b00(lVar13 + 0xfe);
                        *(undefined1 *)(lVar13 + 0x113) = 4;
                      }
                      else {
                        if (bVar3) {
                          FUN_7ff74ff48b00(lVar13 + 0xfe);
                          *(undefined1 *)(lVar13 + 0x113) = 4;
                          bVar7 = false;
                          uVar30 = *(ulonglong *)(lVar13 + 0xb8);
                          if ((((uVar30 >> 0x11 & 1) != 0) &&
                              (lVar16 = *(longlong *)(lVar13 + 0xa0), lVar16 != 0)) &&
                             ((*(int *)(lVar16 + 0xb4) != 2 || (*(char *)(lVar16 + 0x105) != '\x04')
                              ))) {
                            FUN_7ff74ff48b00(lVar13 + 0xfe);
                            *(undefined1 *)(lVar13 + 0x113) = 4;
                            bVar7 = true;
                            uVar30 = *(ulonglong *)(lVar13 + 0xb8);
                          }
                          goto LAB_7ff74f4ee810;
                        }
                        if ((*(uint *)(lVar13 + 0xb8) >> 0x11 & 1) == 0) {
                          cVar6 = FUN_7ff74f4e6d7c(puVar15,iVar12);
                          lVar16 = _DAT_7ff7539d8458 + 8;
                          if (cVar6 != '\0') {
                            *(ulonglong *)(lVar13 + 0xb8) =
                                 *(ulonglong *)(lVar13 + 0xb8) | 0x1000000;
                            bVar7 = false;
                            local_320 = '\0';
                            cVar6 = FUN_7ff74f4e6cd8(lVar16,iVar12,1,&local_320);
                            uVar30 = *(ulonglong *)(lVar13 + 0xb8) & 0xffffffffff7fffff;
                            if (local_320 != '\0') {
                              uVar30 = *(ulonglong *)(lVar13 + 0xb8) | 0x800000;
                            }
                            *(ulonglong *)(lVar13 + 0xb8) = uVar30;
                            if (cVar6 == '\0') {
LAB_7ff74f4ee744:
                              bVar7 = true;
                            }
                            else {
                              cVar6 = FUN_7ff74f4e63c0();
                              if ((cVar6 == '\0') && (cVar6 = FUN_7ff74f4e63d8(), cVar6 == '\0')) {
                                if ((*(ulonglong *)(lVar13 + 0xb8) >> 0x15 & 1) == 0) {
                                  *(ulonglong *)(lVar13 + 0xb8) =
                                       *(ulonglong *)(lVar13 + 0xb8) | 0x400000;
                                  goto LAB_7ff74f4ee744;
                                }
                                if (*(char *)(lVar13 + 0x113) != '\x04') {
                                  if (*(char *)(lVar13 + 0x10d) == '\x05') {
                                    if (*(char *)(lVar13 + 0x113) == '\x03') goto LAB_7ff74f4ee744;
                                    *(undefined1 *)(lVar13 + 0x113) = 3;
                                    auVar37._0_4_ = (float)FUN_7ff74ff489f0(lVar13 + 0xfe);
                                    if (auVar37._0_4_ == 1.0) {
                                      auVar37._0_4_ = 0.0;
                                    }
                                    else {
                                      auVar37._0_4_ = (float)FUN_7ff74ff489f0(lVar13 + 0xfe);
                                      auVar37._0_4_ = (1.0 - auVar37._0_4_) * 30.0 + 30.0;
                                    }
                                    *(float *)(lVar13 + 0xec) = auVar37._0_4_;
                                    FUN_7ff74f7ccc54(local_308,local_310,lVar13,1);
                                    bVar7 = true;
                                    uVar30 = *(ulonglong *)(lVar13 + 0xb8) | 0x200000;
                                  }
                                  else {
                                    *(undefined1 *)(lVar13 + 0x113) = 4;
                                    uVar30 = *(ulonglong *)(lVar13 + 0xb8) | 0x200000;
                                  }
                                  goto LAB_7ff74f4ee810;
                                }
                              }
                            }
                            uVar30 = *(ulonglong *)(lVar13 + 0xb8) | 0x200000;
                            goto LAB_7ff74f4ee810;
                          }
                          local_31f[0] = '\0';
                          cVar6 = FUN_7ff74f772c90(lVar16,iVar12,0,local_31f);
                          uVar30 = *(ulonglong *)(lVar13 + 0xb8) & 0xffffffffff7fffff;
                          if (local_31f[0] != '\0') {
                            uVar30 = *(ulonglong *)(lVar13 + 0xb8) | 0x800000;
                          }
                          *(ulonglong *)(lVar13 + 0xb8) = uVar30;
                          if (cVar6 == '\0') {
                            cVar6 = FUN_7ff74f4e63f0(puVar15,*(undefined4 *)(lVar13 + 0xe0));
                            if (cVar6 == '\0') {
                              *(ulonglong *)(lVar13 + 0xb8) =
                                   *(ulonglong *)(lVar13 + 0xb8) | 0x100000;
                            }
                          }
                          else if (((*(char *)(lVar13 + 0x113) != '\x02') &&
                                   (*(char *)(lVar13 + 0x10d) == '\x05')) &&
                                  (*(char *)(lVar13 + 0x113) != '\x01')) {
                            *(undefined1 *)(lVar13 + 0x113) = 1;
                            auVar37._0_4_ = (float)FUN_7ff74ff489f0(lVar13 + 0xfe);
                            *(float *)(lVar13 + 0xec) = auVar37._0_4_ * 30.0;
                            FUN_7ff74f7ccc54(local_308,local_310,lVar13,1);
                          }
                        }
                        else {
                          FUN_7ff74ff48b00(lVar13 + 0xfe);
                          *(undefined1 *)(lVar13 + 0x113) = 4;
                        }
                      }
                      uVar30 = *(ulonglong *)(lVar13 + 0xb8) & 0xffffffffffdfffff;
                    }
                    else {
                      bVar7 = false;
                      uVar30 = *(ulonglong *)(lVar13 + 0xb8) | 0x1200000;
                    }
LAB_7ff74f4ee810:
                    uVar22 = uVar30 | 0x80000;
                    if ((bool)bVar7) {
                      uVar22 = uVar30 & 0xfffffffffff7ffff;
                    }
                    *(ulonglong *)(lVar13 + 0xb8) = uVar22;
                    if ((uVar22 >> 0x14 & 1) != 0) {
                      bVar7 = false;
                    }
                  }
                  if ((0.0 < *(float *)(lVar13 + 0xc0)) ||
                     (((*(char *)((longlong)local_318 + 0x47) != '\0' &&
                       (_DAT_7ff7539de800 != (int *)0x0)) && (0.0 < (float)_DAT_7ff7539de800[0x7e7])
                      ))) {
                    bVar3 = true;
                  }
                  else {
                    bVar3 = false;
                  }
                  if ((*(ulonglong *)(lVar13 + 0xb8) >> 0x23 & 1) != 0) {
                    bVar3 = false;
                  }
                  if ((*(ulonglong *)(lVar13 + 0xb8) >> 8 & 1) == 0) {
                    bVar3 = false;
                    FUN_7ff74ff48b00(lVar13 + 0xfc);
                  }
                  puVar25 = local_308;
                  if (((bool)bVar7 != false) && (bVar3)) {
                    auVar37._0_4_ = *(float *)(lVar13 + 0xc0);
                    fVar45 = auVar37._0_4_;
                    if ((*(char *)(_DAT_7ff7539c9e58 + 0x11f7) != '\0') &&
                       ((((260.0 <= *(float *)(lVar13 + 0x90) ||
                          (260.0 <= *(float *)(lVar13 + 0x94))) ||
                         (260.0 <= *(float *)(lVar13 + 0x98))) &&
                        (fVar45 = 15000.0, 15000.0 <= auVar37._0_4_)))) {
                      fVar45 = auVar37._0_4_;
                    }
                    if ((*(int *)(lVar13 + 0xe0) != 0) &&
                       ((puVar15 == (uint *)0x0 ||
                        (cVar6 = FUN_7ff74f4e63f0(puVar15), cVar6 == '\0')))) {
                      fVar45 = 3.4028235e+38;
                    }
                    auVar37._0_4_ = 1.0;
                    lVar16 = lVar13 + 0xfc;
                    if (3.4028235e+38 <= fVar45) {
                      fVar45 = 3.4028235e+38;
                    }
                    FUN_7ff74ff48b00(lVar16);
                    pfVar29 = local_318;
                    if (*(char *)((longlong)local_318 + 0x47) != '\0') {
                      if (_DAT_7ff7539de800 != (int *)0x0) {
                        if (0.0 < (float)_DAT_7ff7539de800[0x7e7]) {
                          fVar45 = (float)_DAT_7ff7539de800[0x7e7];
                        }
                        if ((_DAT_7ff7539de800 != (int *)0x0) &&
                           (auVar37._0_4_ = (float)_DAT_7ff7539de800[0x7e8], 0.0 < auVar37._0_4_))
                        goto LAB_7ff74f4ee9a2;
                      }
                      auVar37._0_4_ = 1.0;
                    }
LAB_7ff74f4ee9a2:
                    fVar45 = fVar45 * auVar37._0_4_;
                    if ((*(uint *)(lVar13 + 0xb8) >> 0xc & 1) != 0) {
                      fVar45 = 0.0;
                      FUN_7ff74ff48b00(lVar16);
                    }
                    piVar14 = _DAT_7ff7539de800;
                    bVar3 = false;
                    if (0.0 < fVar45) {
                      auVar37._0_4_ = *(float *)(lVar13 + 0x78) * *(float *)(lVar13 + 0x98);
                      fVar39 = *(float *)(lVar13 + 0x74) * *(float *)(lVar13 + 0x94);
                      fVar46 = *(float *)(lVar13 + 0x90) * *(float *)(lVar13 + 0x70);
                      auVar37._0_4_ =
                           fVar39 * fVar39 + fVar46 * fVar46 + auVar37._0_4_ * auVar37._0_4_;
                      if (auVar37._0_4_ < 0.0) {
                        auVar37._0_4_ = (float)FUN_7ff74fff0378();
                      }
                      else {
                        auVar37._0_4_ = SQRT(auVar37._0_4_);
                      }
                      bVar3 = true;
                      local_2c8 = *(float *)(lVar13 + 0x80) - pfVar29[4];
                      fStack_2bc = *(float *)(lVar13 + 0x8c) - pfVar29[7];
                      fStack_2c4 = *(float *)(lVar13 + 0x84) - pfVar29[5];
                      fStack_2c0 = *(float *)(lVar13 + 0x88) - pfVar29[6];
                      fVar46 = fStack_2c4 * fStack_2c4 + local_2c8 * local_2c8 +
                               fStack_2c0 * fStack_2c0;
                      if (fVar46 < 0.0) {
                        fVar46 = (float)FUN_7ff74fff0378();
                      }
                      else {
                        fVar46 = SQRT(fVar46);
                      }
                      fVar46 = fVar46 - auVar37._0_4_ * 0.5;
                      if (fVar46 <= 0.0) {
                        fVar46 = 0.0;
                      }
                      fVar46 = fVar46 * fVar46;
                      auVar37._0_4_ = fVar45;
                      if (*(char *)((longlong)pfVar29 + 0x47) != '\0') {
                        if (piVar14 == (int *)0x0) {
                          auVar37._0_4_ = 0.0;
                        }
                        else {
                          auVar37._0_4_ = (float)piVar14[0x7e9];
                        }
                        auVar37._0_4_ = auVar37._0_4_ + fVar45;
                      }
                      uVar30 = *(ulonglong *)(lVar13 + 0xb8) & 0xfffffffeffffffff;
                      *(ulonglong *)(lVar13 + 0xb8) = uVar30;
                      if (fVar46 < (auVar37._0_4_ + 200.0) * (auVar37._0_4_ + 200.0)) {
                        if (auVar37._0_4_ * auVar37._0_4_ < fVar46) {
                          if (*(char *)((longlong)pfVar29 + 0x47) != '\0') {
                            FUN_7ff751cb7910(local_2d0);
                            uVar30 = *(ulonglong *)(lVar13 + 0xb8);
                          }
                          *(ulonglong *)(lVar13 + 0xb8) = uVar30 | 0x100000000;
                          if (fVar46 < 0.0) {
                            FUN_7ff74fff0378();
                          }
                          goto LAB_7ff74f4eeb56;
                        }
                        if ((*(char *)((longlong)pfVar29 + 0x47) == '\0') &&
                           (*(float *)(lVar13 + 0xcc) < 30.0)) {
                          *(ulonglong *)(lVar13 + 0xb8) = uVar30 | 0x100000000;
                          auVar37._0_4_ = *(float *)(lVar13 + 0xcc) + *pfVar29;
                          auVar38 = (undefined1  [12])0x0;
                          if (30.0 <= auVar37._0_4_) {
                            auVar38 = ZEXT812(0);
                            auVar37._0_4_ = 30.0;
                          }
                          *(float *)(lVar13 + 0xcc) = auVar37._0_4_;
                          auVar35._0_4_ = auVar37._0_4_ / 30.0;
                          auVar35._4_12_ = auVar38;
                          FUN_7ff74ff48b00(lVar16,auVar35._0_8_);
                        }
                      }
                      else {
                        bVar7 = false;
                        *(ulonglong *)(lVar13 + 0xb8) = uVar30 | 8;
LAB_7ff74f4eeb56:
                        FUN_7ff74ff48b00(lVar16);
                        *(undefined4 *)(lVar13 + 0xcc) = 0x41f00000;
                      }
                      if ((fVar45 * fVar45 < fVar46) &&
                         (((auVar37._0_4_ = *(float *)(lVar13 + 0xc4),
                           auVar37._0_4_ < -1.1920929e-05 || (1.1920929e-05 < auVar37._0_4_)) &&
                          (auVar37._0_4_ * auVar37._0_4_ < fVar46)))) {
                        *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x80;
                      }
                    }
                    if ((*(int *)(lVar13 + 0xe0) != 0) &&
                       ((*(uint *)(lVar13 + 0xb8) >> 0x11 & 1) == 0)) {
                      cVar6 = FUN_7ff74f4e63c0();
                      if ((cVar6 == '\0') &&
                         ((cVar6 = FUN_7ff74f4e63d8(), cVar6 == '\0' &&
                          ((*(uint *)(lVar13 + 0xb8) >> 0x18 & 1) != 0)))) {
                        bVar2 = false;
                      }
                      else {
                        bVar2 = true;
                      }
                      uVar33 = local_310;
                      if (*(char *)(lVar13 + 0x113) == '\x01') {
                        *(float *)(lVar13 + 0xec) = pfVar29[1] + *(float *)(lVar13 + 0xec);
                        if (bVar2) {
                          *(undefined4 *)(lVar13 + 0xec) = 0x41f00000;
                        }
                        FUN_7ff74ff48b00();
                        uVar33 = local_310;
                        if (30.0 <= *(float *)(lVar13 + 0xec)) {
                          *(undefined1 *)(lVar13 + 0x113) = 2;
                          *(undefined4 *)(lVar13 + 0xec) = 0;
                          FUN_7ff74f7ccc54(local_308,local_310,lVar13,0);
                          *(ulonglong *)(lVar13 + 0xb8) =
                               *(ulonglong *)(lVar13 + 0xb8) & 0xfffffffffeffffff;
                        }
                      }
                      if (*(char *)(lVar13 + 0x113) == '\x03') {
                        *(float *)(lVar13 + 0xec) = local_318[1] + *(float *)(lVar13 + 0xec);
                        if (bVar2) {
                          *(undefined4 *)(lVar13 + 0xec) = 0x42700000;
                        }
                        FUN_7ff74ff48b00(lVar13 + 0xfe);
                        if (60.0 <= *(float *)(lVar13 + 0xec)) {
                          *(undefined4 *)(lVar13 + 0xec) = 0;
                          *(undefined1 *)(lVar13 + 0x113) = 4;
                          FUN_7ff74f7ccc54(local_308,uVar33,lVar13,0);
                          *(ulonglong *)(lVar13 + 0xb8) =
                               *(ulonglong *)(lVar13 + 0xb8) & 0xfffffffffeffffff;
                        }
                      }
                      FUN_7ff74ff489f0(lVar16);
                      FUN_7ff74ff489f0(lVar13 + 0xfe);
                      FUN_7ff74ff48b00(lVar16);
                      pfVar29 = local_318;
                    }
                    puVar25 = local_308;
                    local_340 = (undefined8 *)CONCAT71(local_340._1_7_,bVar7);
                    local_348 = local_278;
                    bVar7 = FUN_7ff74f4ed7ac(local_308,lVar13);
                    if (((*(ulonglong *)(lVar13 + 0xb8) >> 10 & 1) != 0) &&
                       (((bVar7 != 0 || ((*(ulonglong *)(lVar13 + 0xb8) & 4) != 0)) &&
                        (auVar37._0_4_ = pfVar29[0x10], *(char *)(pfVar29 + 0x11) != '\0')))) {
                      if (!bVar3) {
                        local_2c8 = *(float *)(lVar13 + 0x80) - pfVar29[4];
                        fStack_2c4 = *(float *)(lVar13 + 0x84) - pfVar29[5];
                        fStack_2c0 = *(float *)(lVar13 + 0x88) - pfVar29[6];
                        fStack_2bc = *(float *)(lVar13 + 0x8c) - pfVar29[7];
                      }
                      pfVar17 = (float *)FUN_7ff74f575f70(local_1b8,&local_2c8);
                      local_2c8 = *pfVar17;
                      fStack_2c4 = pfVar17[1];
                      fStack_2c0 = pfVar17[2];
                      fStack_2bc = pfVar17[3];
                      if (fStack_2c4 * pfVar29[9] + local_2c8 * pfVar29[8] +
                          fStack_2c0 * pfVar29[10] < auVar37._0_4_) {
                        *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x8000000000
                        ;
                      }
                    }
                  }
                  plVar26 = local_278;
                  pfVar29 = local_318;
                  bVar19 = 1;
                  if ((*(char *)((longlong)local_318 + 0x47) != '\0') &&
                     ((_DAT_7ff7539de800 == (int *)0x0 ||
                      ((*(byte *)(_DAT_7ff7539de800 + 0x7e6) & 2) == 0)))) {
                    bVar19 = 0;
                  }
                  local_324 = (uint)bVar7;
                  if ((*(byte *)(lVar13 + 0xb8) & 2) != 0) {
                    local_324 = 0;
                  }
                  uVar20 = local_324;
                  if ((local_278 != (longlong *)0x0) || (local_2b8 != (longlong *)0x0)) {
                    if (((char)local_324 != '\0') && ((bVar19 & *(byte *)(puVar25 + 0x3d)) != 0)) {
                      bVar7 = 0;
                      if (local_278 == (longlong *)0x0) {
                        if (local_2a0 != (longlong *)0x0) {
                          bVar7 = (byte)((uint)*(undefined4 *)((longlong)local_2a0 + 0x4c) >> 0x18);
                          goto LAB_7ff74f4ef034;
                        }
                      }
                      else {
                        bVar7 = (byte)((uint)*(undefined4 *)((longlong)local_278 + 0x4c) >> 0x18);
LAB_7ff74f4ef034:
                        bVar7 = bVar7 >> 1 & 1;
                      }
                      pfVar17 = (float *)(lVar18 + 8);
                      if (((*(char *)((longlong)local_318 + 0x47) != '\0') &&
                          (_DAT_7ff7539de800 != (int *)0x0)) &&
                         ((*(byte *)(_DAT_7ff7539de800 + 0x7e6) & 4) != 0)) {
                        pfVar17 = (float *)(_DAT_7ff7539de800 + 0x810);
                      }
                      uStack_25c = 0;
                      if (local_2b8 == (longlong *)0x0) {
                        if (local_278 != (longlong *)0x0) {
                          lVar16 = *local_278;
                          plVar23 = local_278;
                          goto LAB_7ff74f4ef080;
                        }
                      }
                      else {
                        lVar16 = *local_2b8;
                        plVar23 = local_2b8;
LAB_7ff74f4ef080:
                        (**(code **)(lVar16 + 0x98))(plVar23,&local_268);
                      }
                      uVar20 = local_324;
                      if ((*pfVar17 < -1.1920929e-05) || (1.1920929e-05 < *pfVar17)) {
                        uVar27 = (uint)*(char *)(lVar13 + 0x10c);
                        if ((bVar7 == 0) &&
                           ((*(char *)(lVar18 + 0x128) != '\0' ||
                            (*(char *)(lVar18 + 0x120) != '\0')))) {
                          uVar27 = 10;
                        }
                        else {
                          pfVar29 = local_318;
                          if (uVar27 < 2) goto LAB_7ff74f4ef3c4;
                        }
                        if (_DAT_7ff753b61490 == 0) {
                          lVar16 = 0;
                        }
                        else if (*(int *)(_DAT_7ff753b61490 + 0xb74) == 0) {
                          lVar16 = _DAT_7ff753b61490 + 0x310;
                        }
                        else {
                          lVar16 = _DAT_7ff753b61490 + 0x1a0;
                        }
                        uStack_270 = CONCAT44(uStack_25c,uStack_260);
                        local_278 = local_268;
                        FUN_7ff74f83f370(local_128,lVar16 + 0x80);
                        auVar37 = *(undefined1 (*) [16])(lVar18 + 0x90);
                        uVar33 = *(undefined8 *)(lVar18 + 0xa0);
                        uVar32 = *(undefined8 *)(lVar18 + 0xa8);
                        uVar41 = *(undefined4 *)(lVar18 + 0xb0);
                        uVar42 = *(undefined4 *)(lVar18 + 0xb4);
                        uVar43 = *(undefined4 *)(lVar18 + 0xb8);
                        uVar44 = *(undefined4 *)(lVar18 + 0xbc);
                        local_168[0] = FUN_7ff74f4de5f0();
                        local_158 = auVar37;
                        local_148 = uVar33;
                        uStack_140 = uVar32;
                        local_138 = uVar41;
                        uStack_134 = uVar42;
                        uStack_130 = uVar43;
                        uStack_12c = uVar44;
                        FUN_7ff74f860724(&local_238,local_168,&local_278);
                        FUN_7ff74f58452c(&local_208,&local_238);
                        bVar19 = *(byte *)(lVar13 + 0x108);
                        local_238 = local_208;
                        uStack_230 = uStack_200;
                        if (bVar19 == 0xff) {
                          local_218 = *(undefined8 *)(lVar18 + 0xe0);
                          local_228 = *(undefined8 *)(lVar18 + 0xf0);
                          plVar23 = plVar26;
                          if (local_2a0 != (longlong *)0x0) {
                            plVar23 = local_2a0;
                          }
                          local_1f8 = *(undefined8 *)(lVar18 + 0x80);
                          uStack_1f0 = *(undefined8 *)(lVar18 + 0x88);
                          local_338 = &local_228;
                          local_1e8 = *(undefined8 *)(lVar18 + 0x90);
                          uStack_1e0 = *(undefined8 *)(lVar18 + 0x98);
                          local_330 = 0;
                          local_340 = &local_218;
                          local_1d8 = *(undefined8 *)(lVar18 + 0xa0);
                          uStack_1d0 = *(undefined8 *)(lVar18 + 0xa8);
                          local_1c8 = *(undefined1 (*) [16])(lVar18 + 0xb0);
                          local_348 = (longlong *)CONCAT44(local_348._4_4_,uVar27);
                          _uStack_210 = CONCAT44(0x3f800000,(int)*(undefined8 *)(lVar18 + 0xe8));
                          _uStack_220 = CONCAT44(0x3f800000,(int)*(undefined8 *)(lVar18 + 0xf8));
                          uVar24 = FUN_7ff7504f2990(plVar23,&local_1f8,lVar16,pfVar17);
                        }
                        else if (*(char *)(lVar18 + 0x128) == '\0') {
                          uVar24 = (uint)((char)bVar19 >> 4);
                        }
                        else {
                          uVar24 = (uint)(char)(bVar19 & 0xf);
                        }
                        if ((((plVar26 != (longlong *)0x0) && (*(char *)(lVar18 + 0x128) != '\0'))
                            && (*(longlong *)(plVar26[0x2b] + 0x48) != 0)) &&
                           ((lVar16 = *(longlong *)(*(longlong *)(plVar26[0x2b] + 0x48) + 0x38),
                            lVar16 != 0 && (*(int *)(lVar16 + 4) == -0x46c95b33)))) {
                          uVar24 = 0;
                        }
                        if ((*(byte *)(local_298 + 0xd6) != 0) &&
                           (uVar20 = uVar24 + *(byte *)(local_298 + 0xd6), uVar24 = uVar27 - 1,
                           (int)uVar20 <= (int)(uVar27 - 1))) {
                          uVar24 = uVar20;
                        }
                        pfVar29 = local_318;
                        if ((int)uVar24 < (int)uVar27) {
                          uVar20 = local_324;
                          if (((*(uint *)(lVar18 + 0x30) != uVar24) &&
                              (*(char *)(lVar18 + 0x170) == '\0')) &&
                             ((*(char *)(lVar13 + 0x113) - 1U & 0xfd) != 0)) {
                            if (*(char *)(lVar18 + 7) == '\x01') {
                              if (local_2b8 != (longlong *)0x0) {
                                FUN_7ff7513b9094(local_2b8,
                                                 *(undefined8 *)(_DAT_7ff753b69910 + 0x530),
                                                 (float)uVar24);
                              }
                              if (bVar7 != 0) goto LAB_7ff74f4ef370;
LAB_7ff74f4ef313:
                              if (*(char *)(lVar18 + 0x128) == '\0') {
                                if (*(char *)(lVar18 + 7) != '\x01') {
                                  *(undefined2 *)(lVar18 + 0x170) = 1;
                                  *(undefined4 *)(lVar18 + 0x164) = 0;
                                  *(float *)((longlong)plVar26 + 0x49c) =
                                       (float)*(int *)(lVar18 + 0x30);
                                  *(float *)(plVar26 + 0x93) = (float)(int)uVar24;
                                  *(byte *)((longlong)plVar26 + 0x4a4) =
                                       (byte)((uint)*(undefined4 *)(lVar13 + 0xb8) >> 0x1a) & 1;
                                  goto LAB_7ff74f4ef36a;
                                }
                                goto LAB_7ff74f4ef370;
                              }
LAB_7ff74f4ef379:
                              uVar33 = local_310;
                              iVar12 = *(int *)(lVar18 + 0x3c);
                              if (iVar12 != *(int *)(lVar18 + 0x38)) {
                                FUN_7ff74faacffc(plVar26,local_310,iVar12);
                                if (*(int *)(lVar18 + 0x38) != -1) {
                                  FUN_7ff74faacdc8(plVar26,uVar33);
                                }
                                *(int *)(lVar18 + 0x38) = iVar12;
                              }
                            }
                            else {
                              if (bVar7 == 0) goto LAB_7ff74f4ef313;
                              if (*(char *)(lVar18 + 0x128) != '\0') goto LAB_7ff74f4ef379;
                              *(undefined1 *)(lVar18 + 0x170) = 1;
                              *(undefined4 *)(lVar18 + 0x164) = 0;
                              *(float *)((longlong)plVar26 + 0x49c) = (float)(int)uVar24;
                              *(float *)(plVar26 + 0x93) = (float)*(int *)(lVar18 + 0x30);
LAB_7ff74f4ef36a:
                              *(undefined4 *)(lVar18 + 0x40) = *(undefined4 *)(lVar18 + 0x30);
LAB_7ff74f4ef370:
                              if (*(char *)(lVar18 + 0x128) != '\0') goto LAB_7ff74f4ef379;
                            }
                            *(uint *)(lVar18 + 0x30) = uVar24;
                            *(ulonglong *)(lVar13 + 0xb8) =
                                 *(ulonglong *)(lVar13 + 0xb8) & 0xffffffffffffffef;
                            pfVar29 = local_318;
                            uVar20 = local_324;
                          }
                        }
                        else {
                          *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x10;
                          uVar20 = 0;
                        }
                      }
                    }
LAB_7ff74f4ef3c4:
                    uVar33 = local_310;
                    lVar16 = 0;
                    if ((plVar26 != (longlong *)0x0) && (lVar18 != 0)) {
                      if (*(char *)(lVar18 + 0x128) == '\0') {
                        if ((char)uVar20 != '\0') goto LAB_7ff74f4ef6fc;
                      }
                      else {
                        if ((*(uint *)((longlong)plVar26 + 0x4c) & 0x2000000) != 0) {
                          lVar16 = *(longlong *)(plVar26[0x2b] + 0x28);
                        }
                        if ((char)uVar20 == '\0') {
                          *(undefined1 *)(lVar18 + 0x170) = 0;
                          *(undefined4 *)(lVar18 + 0x164) = 0;
                          *(undefined4 *)((longlong)plVar26 + 0x49c) = 0xbf800000;
                          *(undefined4 *)(plVar26 + 0x93) = 0xbf800000;
                          if (*(int *)(lVar18 + 0x40) != -1) {
                            FUN_7ff74faacdc8(plVar26,local_310);
                          }
                          *(undefined4 *)(lVar18 + 0x40) = 0xffffffff;
                          if ((char)plVar26[0x90] != '\0') {
                            if (lVar16 == 0) {
                              *(undefined4 *)(lVar18 + 0x3c) = 9;
                              bVar3 = false;
                              uVar27 = 0;
                              uVar33 = local_310;
                              if ((int)plVar26[0x24] != 0) {
                                do {
                                  lVar16 = FUN_7ff74f4584c0(plVar26,uVar27);
                                  if (lVar16 != 0) {
                                    lVar1 = *(longlong *)(lVar16 + 0x28);
                                    if (((*(uint *)(lVar1 + 0x4c) & 0x2000000) != 0) &&
                                       ((float)(int)*(float *)(lVar1 + 0x334) !=
                                        (float)(int)*(char *)(*(longlong *)
                                                               (*(longlong *)
                                                                 (*(longlong *)(lVar1 + 0x158) +
                                                                 0x28) + 0x40) + 0x48))) {
                                      FUN_7ff74f7cca00();
                                      bVar3 = true;
                                    }
                                    if (_DAT_7ff753b69910 != 0) {
                                      FUN_7ff74f457df8(lVar16,0,*(undefined8 *)
                                                                 (_DAT_7ff753b69910 + 0x530));
                                    }
                                  }
                                  uVar33 = local_310;
                                  uVar27 = uVar27 + 1;
                                } while (uVar27 < *(uint *)(plVar26 + 0x24));
                                if (bVar3) {
                                  FUN_7ff74f8c35d4(local_310,local_2d0);
                                  if (_DAT_7ff7539c9e58 != 0) {
                                    *(undefined1 *)(_DAT_7ff7539c9e58 + 0x1bb8) = 1;
                                  }
                                  *(ulonglong *)(lVar13 + 0xb8) =
                                       *(ulonglong *)(lVar13 + 0xb8) & 0xfffffffff7ffffff;
                                  *(undefined4 *)(lVar18 + 0x3c) = 9;
                                }
                              }
                              *(undefined4 *)(lVar18 + 0x30) = 9;
                              if (*(int *)(lVar18 + 0x38) == -1) goto LAB_7ff74f4ef5d4;
                              FUN_7ff74faacdc8(plVar26,uVar33);
                              *(undefined4 *)(lVar18 + 0x38) = 0xffffffff;
                            }
                            else {
                              iVar12 = (int)*(char *)(*(longlong *)(lVar16 + 0x40) + 0x48);
                              if ((float)*(int *)(lVar18 + 0x3c) != (float)iVar12) {
                                FUN_7ff74f7cca00(plVar26,iVar12);
                                FUN_7ff74f8c35d4(uVar33,local_2d0);
                                if (_DAT_7ff7539c9e58 != 0) {
                                  *(undefined1 *)(_DAT_7ff7539c9e58 + 0x1bb8) = 1;
                                }
                                *(ulonglong *)(lVar13 + 0xb8) =
                                     *(ulonglong *)(lVar13 + 0xb8) & 0xfffffffff7ffffff;
                                *(int *)(lVar18 + 0x3c) = iVar12;
                                *(int *)(lVar18 + 0x30) = iVar12;
                                if (*(int *)(lVar18 + 0x38) != -1) {
                                  FUN_7ff74faacdc8(plVar26,uVar33);
                                }
LAB_7ff74f4ef5d4:
                                *(undefined4 *)(lVar18 + 0x38) = 0xffffffff;
                              }
                            }
                          }
                        }
                        else {
                          if ((*(int *)(lVar18 + 0x38) != -1) &&
                             (cVar6 = FUN_7ff74faad330(plVar26,local_310), cVar6 != '\0')) {
                            *(undefined2 *)(lVar18 + 0x170) = 1;
                            *(undefined4 *)(lVar18 + 0x164) = 0;
                            *(float *)((longlong)plVar26 + 0x49c) = (float)*(int *)(lVar18 + 0x38);
                            *(float *)(plVar26 + 0x93) = (float)*(int *)(lVar18 + 0x30);
                            if (*(int *)(lVar18 + 0x40) != -1) {
                              FUN_7ff74faacdc8(plVar26,uVar33);
                            }
                            *(undefined4 *)(lVar18 + 0x40) = *(undefined4 *)(lVar18 + 0x38);
                            FUN_7ff74faacffc(plVar26,uVar33);
                            if (*(int *)(lVar18 + 0x38) != -1) {
                              FUN_7ff74faacdc8(plVar26,uVar33);
                            }
                            *(undefined4 *)(lVar18 + 0x38) = 0xffffffff;
                          }
LAB_7ff74f4ef6fc:
                          if (*(char *)(lVar18 + 0x170) != '\0') {
                            auVar37._0_4_ = 30.0;
                            fVar45 = 30.0;
                            *(float *)(lVar18 + 0x164) =
                                 pfVar29[0x12] * pfVar29[1] + *(float *)(lVar18 + 0x164);
                            if (*(uint *)(lVar18 + 0x30) < 10) {
                              auVar37._0_4_ =
                                   *(float *)(lVar18 + 0x174 +
                                             (longlong)(int)*(uint *)(lVar18 + 0x30) * 4);
                            }
                            if (*(uint *)(lVar18 + 0x40) < 10) {
                              fVar45 = *(float *)(lVar18 + 0x174 +
                                                 (longlong)(int)*(uint *)(lVar18 + 0x40) * 4);
                            }
                            cVar6 = FUN_7ff74f4e63c0();
                            if ((cVar6 == '\0') && (cVar6 = FUN_7ff74f4e63d8(), cVar6 == '\0')) {
                              fVar46 = *(float *)(lVar18 + 0x164);
                            }
                            else {
                              fVar46 = fVar45 + auVar37._0_4_;
                              *(float *)(lVar18 + 0x164) = fVar46;
                            }
                            fVar40 = fVar46 / auVar37._0_4_;
                            fVar39 = 1.0;
                            if ((fVar40 < 1.0) && (fVar39 = 0.0, 0.0 <= fVar40)) {
                              fVar39 = fVar40;
                            }
                            *(float *)(lVar18 + 0x168) = fVar39;
                            fVar40 = ((fVar45 - fVar46) + auVar37._0_4_) / fVar45;
                            fVar39 = 1.0;
                            if ((fVar40 < 1.0) && (fVar39 = 0.0, 0.0 <= fVar40)) {
                              fVar39 = fVar40;
                            }
                            *(float *)(lVar18 + 0x16c) = fVar39;
                            if (((*(uint *)((longlong)plVar26 + 0x4c) & 0x2000000) == 0) &&
                               (*(char *)(lVar18 + 0x128) == '\0')) {
                              if ((*(char *)(lVar18 + 0x171) == '\0') && (auVar37._0_4_ <= fVar46))
                              {
                                *(undefined1 *)(lVar18 + 0x171) = 1;
                                local_348 = (longlong *)
                                            (CONCAT71(local_348._1_7_,
                                                      (byte)((uint)*(undefined4 *)(lVar13 + 0xb8) >>
                                                            0x1a)) & 0xffffffffffffff01);
                                FUN_7ff74fd56204(uVar33,plVar26,*(undefined4 *)(lVar18 + 0x30),
                                                 *(undefined4 *)(lVar18 + 0x40));
                                *(float *)((longlong)plVar26 + 0x49c) =
                                     (float)*(int *)(lVar18 + 0x30);
                                *(float *)(plVar26 + 0x93) = (float)*(int *)(lVar18 + 0x40);
                                fVar46 = *(float *)(lVar18 + 0x164);
                              }
                              if (fVar45 + auVar37._0_4_ <= fVar46) {
                                *(undefined1 *)(lVar18 + 0x170) = 0;
                                *(undefined4 *)(lVar18 + 0x164) = 0;
                                *(undefined4 *)((longlong)plVar26 + 0x49c) = 0xbf800000;
                                *(undefined4 *)(plVar26 + 0x93) = 0xbf800000;
                                *(undefined4 *)(lVar18 + 0x40) = 0xffffffff;
                              }
                              *(byte *)((longlong)plVar26 + 0x4a4) =
                                   (byte)((uint)*(undefined4 *)(lVar13 + 0xb8) >> 0x1a) & 1;
                            }
                            else {
                              if ((*(char *)(lVar18 + 0x171) == '\0') && (auVar37._0_4_ <= fVar46))
                              {
                                *(undefined1 *)(lVar18 + 0x171) = 1;
                                FUN_7ff74f7cca00(plVar26,*(undefined4 *)(lVar18 + 0x30));
                                FUN_7ff74f8c35d4(uVar33,local_2d0);
                                if (_DAT_7ff7539c9e58 != 0) {
                                  *(undefined1 *)(_DAT_7ff7539c9e58 + 0x1bb8) = 1;
                                }
                                *(ulonglong *)(lVar13 + 0xb8) =
                                     *(ulonglong *)(lVar13 + 0xb8) & 0xfffffffff7ffffff;
                                *(int *)(lVar18 + 0x3c) = *(int *)(lVar18 + 0x30);
                                *(float *)((longlong)plVar26 + 0x49c) =
                                     (float)*(int *)(lVar18 + 0x30);
                                *(float *)(plVar26 + 0x93) = (float)*(int *)(lVar18 + 0x40);
                                fVar46 = *(float *)(lVar18 + 0x164);
                              }
                              if (fVar45 + auVar37._0_4_ <= fVar46) {
                                *(undefined1 *)(lVar18 + 0x170) = 0;
                                *(undefined4 *)(lVar18 + 0x164) = 0;
                                *(undefined4 *)((longlong)plVar26 + 0x49c) = 0xbf800000;
                                *(undefined4 *)(plVar26 + 0x93) = 0xbf800000;
                                if (*(int *)(lVar18 + 0x40) != -1) {
                                  FUN_7ff74faacdc8(plVar26,uVar33);
                                }
                                *(undefined4 *)(lVar18 + 0x40) = 0xffffffff;
                              }
                            }
                            if (*(char *)(lVar18 + 0x171) == '\0') {
                              auVar37._0_4_ = (float)FUN_7ff74ff489f0();
                              auVar37._0_4_ = auVar37._0_4_ * *(float *)(lVar18 + 0x168);
                            }
                            else {
                              auVar37._0_4_ = (float)FUN_7ff74ff489f0();
                              auVar37._0_4_ = auVar37._0_4_ * *(float *)(lVar18 + 0x16c);
                            }
                            *(float *)(plVar26 + 0x94) = auVar37._0_4_;
                          }
                        }
                      }
                    }
                  }
                  plVar26 = local_2e8;
                  uVar20 = uVar20 & 0xff;
                  if ((*(byte *)(lVar13 + 0xb8) & 1) == 0) {
                    uVar20 = 0;
                  }
                  local_324 = (uint)*(byte *)(local_2f8 + 2);
                  local_310 = CONCAT44(local_310._4_4_,uVar20);
                  if (((((*(byte *)(_DAT_7ff7539c9e58 + 0xc) & 0x40) != 0) &&
                       (*(float *)(lVar13 + 0x90) < *(float *)(_DAT_7ff7539c9e58 + 0x10))) &&
                      (*(float *)(lVar13 + 0x94) < *(float *)(_DAT_7ff7539c9e58 + 0x14))) &&
                     (local_324 = (uint)*(byte *)(local_2f8 + 2),
                     *(float *)(lVar13 + 0x98) < *(float *)(_DAT_7ff7539c9e58 + 0x18))) {
                    local_324 = 0;
                  }
                  uVar27 = local_324;
                  if (*(char *)(local_2f8 + 0x15d) != '\0') {
                    *(undefined1 *)(local_2f8 + 0x15d) = 0;
                    *(undefined4 *)(local_2f8 + 0x160) = 0;
                  }
                  if (local_2e8 != (longlong *)0x0) {
                    cVar6 = '\0';
                    uVar20 = 0;
                    if (((fVar34 <= 500.0) || (*(float *)(local_2e8 + 0x17) - 1.0 < -1.1920929e-05))
                       || (1.1920929e-05 < *(float *)(local_2e8 + 0x17) - 1.0)) {
                      uVar30 = *(ulonglong *)(lVar13 + 0xb8);
                      if ((uVar30 >> 0x20 & 1) == 0) {
                        cVar6 = '\x01';
                      }
                    }
                    else {
                      uVar30 = *(ulonglong *)(lVar13 + 0xb8);
                      uVar20 = 1;
                    }
                    uVar24 = uVar20;
                    if (cVar6 != '\0') {
                      local_328 = local_328 | 4;
                      uVar24 = (uint)*(ushort *)((longlong)local_2e8 + 0x342);
                    }
                    if ((local_328 & 4) != 0) {
                      local_328 = local_328 & 0xfffffffb;
                    }
                    if ((uVar30 & 0x1000000000) == 0) {
                      if (uVar24 == 3) {
                        uVar24 = 0;
                      }
                      else if (uVar24 == 4) {
                        uVar24 = 1;
                      }
                    }
                    else if (uVar24 == 0) {
                      uVar24 = 3;
                    }
                    else if (uVar24 == 1) {
                      uVar24 = 4;
                    }
                    local_348 = (longlong *)&local_2e0;
                    local_2e0 = 0xffffffff;
                    local_2dc = 0;
                    local_2da = cVar6;
                    FUN_7ff74f7cd184(local_2e8,0,local_300,uVar24);
                    plVar23 = local_2e8;
                    uVar24 = *(uint *)(plVar26 + 0x24);
                    uVar28 = 0;
                    if (uVar24 != 0) {
                      do {
                        uVar22 = 0;
                        lVar18 = FUN_7ff74f4584c0(plVar23,uVar28);
                        if (lVar18 != 0) {
                          lVar16 = *(longlong *)(lVar18 + 0x28);
                          if (lVar16 != 0) {
                            if (cVar6 == '\0') {
                              uVar31 = (ulonglong)uVar20;
                            }
                            else {
                              uVar31 = (ulonglong)*(ushort *)(lVar16 + 0x342);
                              local_328 = local_328 | 8;
                            }
                            if ((local_328 & 8) != 0) {
                              local_328 = local_328 & 0xfffffff7;
                            }
                            iVar12 = (int)uVar31;
                            if ((uVar30 & 0x1000000000) == 0) {
                              if (iVar12 == 3) {
                                uVar31 = uVar22 & 0xffffffff;
                              }
                              else if (iVar12 == 4) {
                                uVar31 = 1;
                              }
                            }
                            else if (iVar12 == 0) {
                              uVar31 = 3;
                            }
                            else if (iVar12 == 1) {
                              uVar31 = 4;
                            }
                            local_348 = (longlong *)&local_2e0;
                            local_2da = cVar6;
                            FUN_7ff74f7cd184(lVar16,0,local_300,uVar31);
                          }
                          if (_DAT_7ff753b69910 != 0) {
                            FUN_7ff74f457df8(lVar18,0,*(undefined8 *)(_DAT_7ff753b69910 + 0x530));
                          }
                        }
                        uVar28 = uVar28 + 1;
                        lVar13 = local_290;
                        uVar27 = local_324;
                      } while (uVar28 < uVar24);
                    }
                    uVar20 = (uint)local_310;
                  }
                  plVar26 = local_2e8;
                  lVar18 = local_2f8;
                  cVar6 = (char)uVar20;
                  if ((0.0 < *(float *)(lVar13 + 0xc0)) ||
                     (((*(char *)((longlong)local_318 + 0x47) != '\0' &&
                       (_DAT_7ff7539de800 != (int *)0x0)) && (0.0 < (float)_DAT_7ff7539de800[0x7e7])
                      ))) {
                    bVar3 = true;
                  }
                  else {
                    bVar3 = false;
                  }
                  pfVar29 = local_318;
                  if ((cVar6 != '\0') && (bVar3)) {
                    uVar33 = FUN_7ff74ff489f0();
                    auVar36._8_4_ = (int)extraout_XMM0_Qb;
                    auVar36._0_8_ = uVar33;
                    auVar36._12_4_ = (int)((ulonglong)extraout_XMM0_Qb >> 0x20);
                    auVar37._4_12_ = auVar36._4_12_;
                    auVar37._0_4_ = (float)uVar33 * local_318[0x13] * *(float *)(lVar18 + 0x1ac);
                    pfVar29 = local_318;
                    if (plVar26 != (longlong *)0x0) {
                      local_348 = (longlong *)((ulonglong)local_348 & 0xffffffff00000000);
                      FUN_7ff74f6213d0(plVar26,local_300,local_318,1);
                    }
                    if (local_2b0 != 0) {
                      *(char *)(local_2b0 + 0x81) = (char)(int)(auVar37._0_4_ * 255.0);
                    }
                    if ((-1.1920929e-05 <= auVar37._0_4_) && (auVar37._0_4_ <= 1.1920929e-05)) {
                      cVar6 = '\0';
                    }
                  }
                  lVar16 = local_2b0;
                  auVar37._0_4_ = *(float *)(lVar18 + 0x44);
                  bVar3 = false;
                  if (auVar37._0_4_ <= 0.0) {
                    auVar37._0_4_ = *(float *)(lVar13 + 0xc0);
                  }
                  bVar2 = true;
                  if ((plVar26 != (longlong *)0x0) &&
                     (((*(uint *)((longlong)plVar26 + 0x4c) & 0x200000) == 0 ||
                      (*(char *)((longlong)plVar26 + 0x425) == '\0')))) {
                    bVar2 = false;
                  }
                  if ((char)uVar27 != '\0') {
                    fVar45 = auVar37._0_4_;
                    if (3.4028235e+38 <= auVar37._0_4_) {
                      fVar45 = 3.4028235e+38;
                    }
                    fVar45 = fVar45 * 0.8;
                    fVar46 = fVar45;
                    if (bVar2) {
                      if (fVar45 == 0.0) {
                        fVar45 = 3.4028235e+38;
                      }
                      fVar46 = local_2d8;
                      if (fVar45 <= local_2d8) {
                        fVar46 = fVar45;
                      }
                    }
                    if ((0.0 < fVar46) && (fVar46 * fVar46 < fVar34 * fVar34)) {
                      uVar27 = uVar27 & 0xffffff00;
                      bVar3 = true;
                      local_324 = uVar27;
                    }
                  }
                  uVar20 = uVar27;
                  if (((plVar26 != (longlong *)0x0) && ((char)uVar27 != '\0')) &&
                     (uVar20 = uVar27 & 0xff, local_324 = uVar27 & 0xff,
                     (char)plVar26[0x88] == '\x01')) {
                    uVar20 = 0;
                    local_324 = 0;
                  }
                  cVar8 = (char)uVar20;
                  if (((cVar6 != '\0') && (*(char *)(lVar18 + 5) != '\0')) &&
                     (fVar49 = fVar47 + fVar48 + fVar49, fVar49 < 62500.0)) {
                    fVar49 = fVar49 - 22500.0;
                    if (fVar49 <= 0.0) {
                      fVar49 = 0.0;
                    }
                    fVar49 = fVar49 / 10000.0;
                    fVar45 = 1.0;
                    if ((fVar49 < 1.0) && (fVar45 = fVar49, fVar49 <= 0.0)) {
                      fVar45 = 0.0;
                    }
                    fVar45 = fVar45 * pfVar29[0x13] * *(float *)(lVar18 + 0x1ac);
                    if (plVar26 != (longlong *)0x0) {
                      local_348 = (longlong *)((ulonglong)local_348 & 0xffffffff00000000);
                      FUN_7ff74f6213d0(plVar26,local_300);
                    }
                    if (lVar16 != 0) {
                      fVar46 = (float)FUN_7ff74ff489f0();
                      *(char *)(lVar16 + 0x81) = (char)(int)(fVar46 * 255.0);
                    }
                    if ((-1.1920929e-05 <= fVar45) && (fVar45 <= 1.1920929e-05)) {
                      cVar6 = '\0';
                    }
                  }
                  if (plVar26 != (longlong *)0x0) {
                    if (cVar6 == '\0') {
                      FUN_7ff74f621350(plVar26,0);
                    }
                    uVar33 = local_300;
                    if (*(char *)(lVar18 + 0x128) != '\0') {
                      lVar16 = plVar26[0x2b];
                      if (cVar6 != '\0') {
                        if ((*(int *)(lVar18 + 0x34) == -1) ||
                           (cVar8 = FUN_7ff74faad330(plVar26,local_300), cVar8 != '\0')) {
                          bVar2 = true;
                        }
                        else {
                          bVar2 = false;
                        }
                        if ((*(int *)(lVar18 + 0x30) != *(int *)(lVar18 + 0x34)) && (bVar2)) {
                          cVar8 = (**(code **)(*plVar26 + 0x508))(plVar26,uVar33);
                          if (cVar8 != '\0') {
                            if (lVar16 == 0) {
                              uVar33 = 0;
                            }
                            else {
                              uVar33 = *(undefined8 *)(lVar16 + 0x28);
                            }
                            FUN_7ff74faad258(uVar33,*(undefined4 *)(lVar18 + 0x34),
                                             *(undefined4 *)(lVar18 + 0x30));
                          }
                          uVar33 = local_300;
                          uVar20 = 0;
                          if ((int)plVar26[0x24] != 0) {
                            do {
                              lVar16 = FUN_7ff74f4584c0(plVar26,uVar20);
                              if (lVar16 != 0) {
                                plVar23 = *(longlong **)(lVar16 + 0x28);
                                lVar18 = local_2f8;
                                if (((((*(uint *)((longlong)plVar23 + 0x4c) & 0x2000000) != 0) &&
                                     (cVar8 = (**(code **)(*plVar23 + 0x508))(plVar23,uVar33),
                                     lVar18 = local_2f8, cVar8 != '\0')) && (plVar23[0x2b] != 0)) &&
                                   (lVar13 = *(longlong *)(plVar23[0x2b] + 0x28), lVar13 != 0)) {
                                  FUN_7ff74faad258(lVar13,*(undefined4 *)(local_2f8 + 0x34),
                                                   *(undefined4 *)(local_2f8 + 0x30));
                                }
                                if (_DAT_7ff753b69910 != 0) {
                                  FUN_7ff74f457df8(lVar16,0,*(undefined8 *)
                                                             (_DAT_7ff753b69910 + 0x530));
                                }
                              }
                              uVar20 = uVar20 + 1;
                              lVar13 = local_290;
                            } while (uVar20 < *(uint *)(plVar26 + 0x24));
                          }
                          *(undefined4 *)(lVar18 + 0x34) = *(undefined4 *)(lVar18 + 0x30);
                        }
                      }
                      cVar9 = FUN_7ff74fc264c4(plVar26,local_300);
                      cVar8 = (char)local_324;
                      if (cVar9 == '\0') {
                        *(ulonglong *)(lVar13 + 0xb8) = *(ulonglong *)(lVar13 + 0xb8) | 0x10;
                      }
                    }
                  }
                  if ((*(int *)(lVar13 + 0xd4) == -0x150250bf) &&
                     (*(int *)(_DAT_7ff7539c9e58 + 0x508) - 0x17U < 2)) {
                    cVar8 = '\0';
                  }
                  cVar9 = '\0';
                  if (plVar26 != (longlong *)0x0) {
                    cVar10 = *(char *)(lVar18 + 6);
                    if (cVar10 == '\0') {
                      if ((cVar8 == '\0') || (cVar6 == '\0')) {
                        cVar9 = '\0';
                      }
                      else {
                        cVar9 = '\x01';
                      }
                    }
                    else {
                      FUN_7ff74f7cb6c0(plVar26,(int)plVar26[0x6f],
                                       *(undefined4 *)((longlong)plVar26 + 0x37c),0xffffffff);
                      if (!bVar3) {
                        cVar9 = cVar10;
                      }
                    }
                    uVar20 = *(uint *)((longlong)plVar26 + 0x114) & 0x7fffffff;
                    if (cVar9 == '\0') {
                      uVar20 = *(uint *)((longlong)plVar26 + 0x114) | 0x80000000;
                    }
                    FUN_7ff74f57407c(plVar26,uVar20);
                  }
                  param_1 = local_308;
                  *(undefined1 *)(lVar18 + 0x126) = 0;
                  uVar30 = *(ulonglong *)(lVar13 + 0xb8);
                  if (*(char *)(local_308 + 1) == '\0') {
                    if (((((uVar30 & 1) == 0) || ((uVar30 & 0x10) != 0)) || ((uVar30 & 8) != 0)) ||
                       ((((((uVar30 & 0x20) != 0 || ((uVar30 & 0x40) != 0)) ||
                          (((uVar30 >> 0xd & 1) != 0 ||
                           (((uVar30 >> 0x13 & 1) != 0 || ((char)uVar30 < '\0')))))) ||
                         ((uVar30 & 2) != 0)) || ((uVar30 & 4) != 0)))) {
                      if (*(char *)(lVar18 + 0x123) != '\0') goto LAB_7ff74f4f00c7;
                    }
                    else if (*(char *)(lVar18 + 0x123) == '\0') {
LAB_7ff74f4f00c7:
                      lVar16 = **(longlong **)(local_318 + 0xe);
                      if (local_2a8 <
                          (ulonglong)((*(longlong **)(local_318 + 0xe))[1] - lVar16 >> 3)) {
                        *(longlong *)(lVar16 + local_2a8 * 8) = lVar18;
                      }
                    }
                  }
                  if (*(char *)((longlong)local_308 + 0x1b1) != '\0') {
                    cVar8 = FUN_7ff74f61b1e4(lVar18,lVar13);
                    if (((*(char *)(lVar13 + 0x107) < '\0') ||
                        (*(longlong *)(_DAT_7ff7539c9e58 + 0x1a68) == 0)) ||
                       (*(longlong *)(*(longlong *)(_DAT_7ff7539c9e58 + 0x1a68) + 0x28) == 0)) {
LAB_7ff74f4f0135:
                      cVar9 = '\x01';
                    }
                    else {
                      cVar10 = FUN_7ff74f4e6ca0();
                      cVar9 = '\0';
                      if (cVar10 != '\0') goto LAB_7ff74f4f0135;
                    }
                    if (cVar8 != *(char *)(lVar18 + 0x124)) {
                      *(undefined1 *)(lVar18 + 0x126) = 1;
                    }
                    *(char *)(lVar18 + 0x124) = cVar8;
                    if (cVar9 != *(char *)(lVar18 + 0x125)) {
                      *(undefined1 *)(lVar18 + 0x126) = 1;
                    }
                    *(char *)(lVar18 + 0x125) = cVar9;
                  }
                  uVar33 = local_300;
                  if (*(char *)(lVar13 + 0x110) != *(char *)(lVar13 + 0x10f)) {
                    *(undefined1 *)(lVar18 + 0x126) = 1;
                  }
                  if ((plVar26 != (longlong *)0x0) && (*(char *)(lVar18 + 0x128) != '\0')) {
                    lVar16 = plVar26[0x2b];
                    cVar8 = (**(code **)(*plVar26 + 0x508))(plVar26,local_300);
                    if ((cVar8 != '\0') && (plVar26[0x9a] != 0)) {
                      local_348 = (longlong *)((ulonglong)local_348 & 0xffffffffffffff00);
                      FUN_7ff74faad740(extraout_XMM0_Qa_00,0,uVar33,lVar16);
                    }
                    uVar20 = 0;
                    param_1 = local_308;
                    if ((int)plVar26[0x24] != 0) {
                      do {
                        lVar18 = FUN_7ff74f4584c0(plVar26,uVar20);
                        if (lVar18 != 0) {
                          plVar23 = *(longlong **)(lVar18 + 0x28);
                          if ((((*(uint *)((longlong)plVar23 + 0x4c) & 0x2000000) != 0) &&
                              (cVar8 = (**(code **)(*plVar23 + 0x508))(plVar23,uVar33),
                              cVar8 != '\0')) &&
                             ((plVar23[0x9a] != 0 &&
                              ((plVar23[0x2b] != 0 && (*(longlong *)(plVar23[0x2b] + 0x28) != 0)))))
                             ) {
                            local_348 = (longlong *)((ulonglong)local_348 & 0xffffffffffffff00);
                            FUN_7ff74faad740(extraout_XMM0_Qa_01,0,uVar33);
                          }
                          if (_DAT_7ff753b69910 != 0) {
                            FUN_7ff74f457df8(lVar18,0,*(undefined8 *)(_DAT_7ff753b69910 + 0x530));
                          }
                        }
                        uVar20 = uVar20 + 1;
                        param_1 = local_308;
                        lVar18 = local_2f8;
                      } while (uVar20 < *(uint *)(plVar26 + 0x24));
                    }
                  }
                  *(char *)(lVar18 + 0x15c) = cVar6;
                  uVar33 = local_300;
                  uVar20 = local_328;
                  if (plVar26 == (longlong *)0x0) {
                    *(undefined4 *)(lVar18 + 0x1a0) = 0;
                  }
                  else {
                    if ((*(char *)(lVar18 + 0x170) == '\0') &&
                       ((*(char *)(lVar13 + 0x113) - 1U & 0xfd) != 0)) {
                      iVar12 = *(int *)(lVar18 + 0x30);
                      lVar16 = (longlong)iVar12;
                      if (iVar12 == 0) {
                        iVar12 = 4;
                      }
                      else {
                        if ((*(uint *)((longlong)plVar26 + 0x4c) & 0x2000000) == 0) {
                          if (iVar12 == 9) {
                            fVar45 = *(float *)(lVar18 + 0x10);
                            goto LAB_7ff74f4f02e2;
                          }
                          fVar45 = *(float *)(lVar18 + (lVar16 + 1) * 4);
LAB_7ff74f4f02fb:
                          auVar37._0_4_ = *(float *)(lVar18 + (lVar16 + 2) * 4);
                        }
                        else {
                          fVar45 = *(float *)(lVar18 + (lVar16 + 1) * 4);
                          if (iVar12 != 9) goto LAB_7ff74f4f02fb;
LAB_7ff74f4f02e2:
                          if (*(float *)(lVar13 + 0xc0) <= auVar37._0_4_) {
                            auVar37._0_4_ = *(float *)(lVar13 + 0xc0);
                          }
                        }
                        fVar46 = fVar34 - fVar45;
                        if (auVar37._0_4_ - fVar34 <= fVar34 - fVar45) {
                          fVar46 = auVar37._0_4_ - fVar34;
                        }
                        fVar46 = fVar46 / 23.4;
                        if (fVar46 < 60.0) {
                          fVar34 = 4.0;
                          if (4.0 <= fVar46) {
                            fVar34 = fVar46;
                          }
                          iVar12 = (int)fVar34;
                        }
                        else {
                          iVar12 = 0x3c;
                        }
                      }
                    }
                    else {
                      iVar12 = 0;
                    }
                    *(int *)(lVar18 + 0x1a0) = iVar12;
                  }
                }
              }
            }
            else if ((*(char *)(lVar18 + 7) == '\x01') &&
                    (local_2b0 = *(longlong *)(lVar16 + 0x28), param_1 = local_308, local_2b0 != 0))
            goto LAB_7ff74f4edf15;
          }
        }
LAB_7ff74f4f035f:
        param_2 = local_2a8 + 1;
      } while (param_2 < local_288);
    }
  }
  return;
}
```

## 0x209edc — LOD / draw distance

- containing function: **0x209eb0** (206 bytes, `FUN_7ff74f619eb0`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e3c3b8	DATA	-
0x4146978	DATA	-
0x1fce974	UNCONDITIONAL_CALL	FUN_7ff7513de868@0x1fce868
0x1fcea16	UNCONDITIONAL_CALL	FUN_7ff7513de868@0x1fce868
0x208c0c	UNCONDITIONAL_CALL	FUN_7ff74f6186e8@0x2086e8
0x208c38	UNCONDITIONAL_CALL	FUN_7ff74f6186e8@0x2086e8
0x209f67	UNCONDITIONAL_CALL	FUN_7ff74f619f30@0x209f30
0x210dba	UNCONDITIONAL_CALL	FUN_7ff74f620c54@0x210c54
0x211abd	UNCONDITIONAL_CALL	FUN_7ff74f621a4c@0x211a4c
0x2128a2	UNCONDITIONAL_CALL	FUN_7ff74f6224a4@0x2124a4
0x6692a1	UNCONDITIONAL_CALL	FUN_7ff74fa79110@0x669110
0x363652	UNCONDITIONAL_CALL	FUN_7ff74f7734b0@0x3634b0
```

Disassembly around the anchor (`=>` marks it):

```
   0x209eb0   40 53                          PUSH RBX
   0x209eb2   48 83 ec 20                    SUB RSP,0x20
   0x209eb6   80 b9 08 03 00 00 00           CMP byte ptr [RCX + 0x308],0x0
   0x209ebd   48 8b d9                       MOV RBX,RCX
   0x209ec0   0f 85 c6 b8 75 01              JNZ 0x7ff750d7578c ; CONDITIONAL_JUMP 0x196578c LAB_7ff750d7578c
   0x209ec6   48 8b 8b 10 03 00 00           MOV RCX,qword ptr [RBX + 0x310]
   0x209ecd   48 85 c9                       TEST RCX,RCX
   0x209ed0   75 26                          JNZ 0x7ff74f619ef8 ; CONDITIONAL_JUMP 0x209ef8 LAB_7ff74f619ef8
   0x209ed2   48 8b 05 7f ff 3a 04           MOV RAX,qword ptr [0x7ff7539c9e58] ; READ 0x45b9e58 DAT_7ff7539c9e58
   0x209ed9   48 85 c0                       TEST RAX,RAX
=> 0x209edc   74 0c                          JZ 0x7ff74f619eea ; CONDITIONAL_JUMP 0x209eea LAB_7ff74f619eea
   0x209ede   38 88 74 17 00 00              CMP byte ptr [RAX + 0x1774],CL
   0x209ee4   0f 85 f6 b8 75 01              JNZ 0x7ff750d757e0 ; CONDITIONAL_JUMP 0x19657e0 LAB_7ff750d757e0
   0x209eea   f3 0f 10 83 9c 02 00 00        MOVSS XMM0,dword ptr [RBX + 0x29c]
   0x209ef2   48 83 c4 20                    ADD RSP,0x20
   0x209ef6   5b                             POP RBX
   0x209ef7   c3                             RET
   0x209ef8   e8 2b a2 f2 ff                 CALL 0x7ff74f544128 ; UNCONDITIONAL_CALL 0x134128 FUN_7ff74f544128
   0x209efd   f3 0f 10 8b 9c 02 00 00        MOVSS XMM1,dword ptr [RBX + 0x29c]
   0x209f05   0f 57 c0                       XORPS XMM0,XMM0
   0x209f08   8b c0                          MOV EAX,EAX
```

Decompilation:

```c
// function FUN_7ff74f619eb0 @ 0x209eb0  body=206 bytes  pdataStart=0x209eb0
// callers(8): FUN_7ff74f619f30@0x209f30 FUN_7ff74f7734b0@0x3634b0 FUN_7ff74f621a4c@0x211a4c FUN_7ff74f620c54@0x210c54 FUN_7ff7513de868@0x1fce868 FUN_7ff74f6186e8@0x2086e8 FUN_7ff74f6224a4@0x2124a4 FUN_7ff74fa79110@0x669110 
// callees(1): FUN_7ff74f544128@0x134128 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

float FUN_7ff74f619eb0(longlong param_1)

{
  uint uVar1;
  undefined8 uVar2;
  
  if ((*(char *)(param_1 + 0x308) == '\0') || (*(longlong *)(param_1 + 0x348) == 0)) {
    if (*(longlong *)(param_1 + 0x310) == 0) {
      if ((_DAT_7ff7539c9e58 != 0) && (*(char *)(_DAT_7ff7539c9e58 + 0x1774) != '\0')) {
        return *(float *)(_DAT_7ff7539c9e58 + 6000);
      }
      return *(float *)(param_1 + 0x29c);
    }
    uVar1 = FUN_7ff74f544128();
    if ((float)uVar1 <= *(float *)(param_1 + 0x29c)) {
      return *(float *)(param_1 + 0x29c);
    }
    uVar2 = *(undefined8 *)(param_1 + 0x310);
  }
  else {
    uVar1 = FUN_7ff74f544128();
    if ((float)uVar1 <= *(float *)(param_1 + 0x29c)) {
      return *(float *)(param_1 + 0x29c);
    }
    uVar2 = *(undefined8 *)(param_1 + 0x348);
  }
  uVar1 = FUN_7ff74f544128(uVar2);
  return (float)uVar1;
}
```

## 0x1774ad — LOD / draw distance

- containing function: **0x1772b4** (693 bytes, `FUN_7ff74f5872b4`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e377b0	DATA	-
0x8614ea	UNCONDITIONAL_CALL	FUN_7ff74fc714bc@0x8614bc
0xfae1e5	UNCONDITIONAL_CALL	FUN_7ff7503be0f0@0xfae0f0
```

Disassembly around the anchor (`=>` marks it):

```
   0x177484   48 8b 47 08                    MOV RAX,qword ptr [RDI + 0x8]
   0x177488   f3 41 0f 10 47 58              MOVSS XMM0,dword ptr [R15 + 0x58]
   0x17748e   f3 0f 11 45 78                 MOVSS dword ptr [RBP + 0x78],XMM0
   0x177493   48 85 c0                       TEST RAX,RAX
   0x177496   74 0a                          JZ 0x7ff74f5874a2 ; CONDITIONAL_JUMP 0x1774a2 LAB_7ff74f5874a2
   0x177498   f3 0f 10 40 04                 MOVSS XMM0,dword ptr [RAX + 0x4]
   0x17749d   f3 0f 11 45 78                 MOVSS dword ptr [RBP + 0x78],XMM0
   0x1774a2   8b 0f                          MOV ECX,dword ptr [RDI]
   0x1774a4   48 8d 55 78                    LEA RDX,[RBP + 0x78]
   0x1774a8   e8 03 fd ff ff                 CALL 0x7ff74f5871b0 ; UNCONDITIONAL_CALL 0x1771b0 FUN_7ff74f5871b0
=> 0x1774ad   8b 0f                          MOV ECX,dword ptr [RDI]
   0x1774af   48 8d 55 78                    LEA RDX,[RBP + 0x78]
   0x1774b3   e8 10 fc ff ff                 CALL 0x7ff74f5870c8 ; UNCONDITIONAL_CALL 0x1770c8 FUN_7ff74f5870c8
   0x1774b8   0f 2f 75 78                    COMISS XMM6,dword ptr [RBP + 0x78]
   0x1774bc   72 0f                          JC 0x7ff74f5874cd ; CONDITIONAL_JUMP 0x1774cd LAB_7ff74f5874cd
   0x1774be   4c 8b c7                       MOV R8,RDI
   0x1774c1   48 8d 55 d0                    LEA RDX,[RBP + -0x30]
   0x1774c5   49 8b ce                       MOV RCX,R14
   0x1774c8   e8 4f 02 00 00                 CALL 0x7ff74f58771c ; UNCONDITIONAL_CALL 0x17771c FUN_7ff74f58771c
   0x1774cd   80 3e 01                       CMP byte ptr [RSI],0x1
   0x1774d0   75 13                          JNZ 0x7ff74f5874e5 ; CONDITIONAL_JUMP 0x1774e5 LAB_7ff74f5874e5
```

Decompilation:

```c
// function FUN_7ff74f5872b4 @ 0x1772b4  body=693 bytes  pdataStart=0x1772b4
// callers(2): FUN_7ff74fc714bc@0x8614bc FUN_7ff7503be0f0@0xfae0f0 
// callees(10): FUN_7ff74f5871b0@0x1771b0 FUN_7ff7503c83a0@0xfb83a0 FUN_7ff7503c86b0@0xfb86b0 FUN_7ff74f5871f8@0x1771f8 FUN_7ff74fb2bb6c@0x71bb6c FUN_7ff74f587274@0x177274 FUN_7ff74f5870c8@0x1770c8 FUN_7ff74f58771c@0x17771c FUN_7ff74f585270@0x175270 FUN_7ff74fa398b0@0x6298b0 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff74f5872b4(longlong param_1,undefined4 param_2,undefined8 param_3,undefined8 *param_4,
                     undefined8 param_5)

{
  char *pcVar1;
  char cVar2;
  undefined4 *puVar3;
  longlong *plVar4;
  undefined8 uVar5;
  undefined8 uVar6;
  undefined8 uVar7;
  undefined8 uVar8;
  longlong lVar9;
  undefined8 uVar10;
  longlong *plVar11;
  longlong *plVar12;
  float fVar13;
  float fVar14;
  float local_res20 [2];
  undefined8 local_108;
  undefined8 uStack_100;
  undefined8 local_f8;
  undefined8 uStack_f0;
  longlong local_e8 [5];
  undefined8 uStack_c0;
  undefined8 local_b8;
  undefined8 uStack_b0;
  undefined4 local_a8;
  undefined4 uStack_a4;
  undefined4 uStack_a0;
  undefined4 uStack_9c;
  undefined4 local_98;
  undefined4 uStack_94;
  undefined4 uStack_90;
  undefined4 uStack_8c;
  undefined1 local_88 [16];
  undefined1 local_78 [16];
  undefined1 local_68 [64];
  
  FUN_7ff74fb2bb6c(param_3);
  FUN_7ff74fb2bb6c(param_5);
  if ((param_4[1] != 0) && ((*(byte *)(_DAT_7ff7539c9e58 + 8) & 1) == 0)) {
    local_e8[2] = 0;
    local_e8[3] = 0;
    local_e8[0] = 0;
    local_e8[1] = 0;
    FUN_7ff7503c86b0(param_2,local_e8);
    plVar11 = *(longlong **)*param_4;
    while (*(char *)((longlong)plVar11 + 0x19) == '\0') {
      puVar3 = (undefined4 *)plVar11[4];
      uVar5 = *(undefined8 *)(puVar3 + 0x1c);
      uVar6 = *(undefined8 *)(puVar3 + 0x1e);
      uVar7 = *(undefined8 *)(puVar3 + 0x20);
      uVar8 = *(undefined8 *)(puVar3 + 0x22);
      local_108 = uVar5;
      uStack_100 = uVar6;
      local_f8 = uVar7;
      uStack_f0 = uVar8;
      FUN_7ff74f585270(local_68,&local_f8,&local_108);
      uVar10 = 0;
      if (*(longlong *)(puVar3 + 0x2c) != 0) {
        uVar10 = *(undefined8 *)(*(longlong *)(puVar3 + 0x2c) + 0xc0);
      }
      pcVar1 = (char *)(puVar3 + 0x33);
      FUN_7ff7503c83a0(puVar3[4],1,local_68,puVar3[0x28],pcVar1,local_e8,0,uVar10,
                       (longlong)puVar3 + 0xdd,(longlong)puVar3 + 0xda);
      if (*pcVar1 != '\x01') {
        if (*pcVar1 == '\0') {
          *(undefined1 *)((longlong)puVar3 + 0xc5) = 0;
LAB_7ff74f5874be:
          FUN_7ff74f58771c(param_3,local_88,puVar3);
        }
        else {
          local_e8[4] = *(longlong *)(puVar3 + 8);
          uStack_c0 = *(undefined8 *)(puVar3 + 10);
          local_b8 = *(undefined8 *)(puVar3 + 0xc);
          uStack_b0 = *(undefined8 *)(puVar3 + 0xe);
          local_a8 = puVar3[0x10];
          uStack_a4 = puVar3[0x11];
          uStack_a0 = puVar3[0x12];
          uStack_9c = puVar3[0x13];
          local_98 = puVar3[0x14];
          uStack_94 = puVar3[0x15];
          uStack_90 = puVar3[0x16];
          uStack_8c = puVar3[0x17];
          local_108 = uVar5;
          uStack_100 = uVar6;
          local_f8 = uVar7;
          uStack_f0 = uVar8;
          fVar13 = (float)FUN_7ff74f5871f8(local_a8,local_e8 + 4,&local_108,&local_f8);
          lVar9 = _DAT_7ff7539d8458 + 0x80;
          *(undefined1 *)((longlong)puVar3 + 0xc5) = 0;
          *(undefined1 *)(puVar3 + 0x36) = 0;
          fVar14 = (float)FUN_7ff74f587274(lVar9,0);
          if (fVar13 <= fVar14) {
            if (*(char *)((longlong)puVar3 + 199) != '\0') {
              if (*(char *)((longlong)puVar3 + 0xcd) == '\x04') {
                *(undefined1 *)((longlong)puVar3 + 0xc5) = 1;
              }
              else {
                *(undefined1 *)(puVar3 + 0x36) = 1;
              }
            }
            local_res20[0] = *(float *)(param_1 + 0x58);
            if (*(longlong *)(puVar3 + 2) != 0) {
              local_res20[0] = *(float *)(*(longlong *)(puVar3 + 2) + 4);
            }
            FUN_7ff74f5871b0(*puVar3,local_res20);
            FUN_7ff74f5870c8(*puVar3,local_res20);
            if (local_res20[0] <= fVar13) goto LAB_7ff74f5874be;
          }
          else {
            *(undefined1 *)((longlong)puVar3 + 0xc5) = 1;
          }
        }
      }
      if (*pcVar1 == '\x01') {
        FUN_7ff74f58771c(param_5,local_78,puVar3);
      }
      plVar4 = (longlong *)plVar11[2];
      if (*(char *)((longlong)plVar4 + 0x19) == '\0') {
        cVar2 = *(char *)(*plVar4 + 0x19);
        plVar11 = plVar4;
        plVar4 = (longlong *)*plVar4;
        while (cVar2 == '\0') {
          cVar2 = *(char *)(*plVar4 + 0x19);
          plVar11 = plVar4;
          plVar4 = (longlong *)*plVar4;
        }
      }
      else {
        plVar4 = (longlong *)plVar11[1];
        plVar12 = plVar11;
        while ((plVar11 = plVar4, *(char *)((longlong)plVar11 + 0x19) == '\0' &&
               (plVar12 == (longlong *)plVar11[2]))) {
          plVar12 = plVar11;
          plVar4 = (longlong *)plVar11[1];
        }
      }
    }
    if (local_e8[0] != 0) {
      FUN_7ff74fa398b0();
    }
  }
  return;
}
```

## 0x39722b — LOD / draw distance

- containing function: **0x397218** (73 bytes, `FUN_7ff74f7a7218`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e4a0b0	DATA	-
0x396fc2	UNCONDITIONAL_CALL	FUN_7ff74f7a6f54@0x396f54
```

Disassembly around the anchor (`=>` marks it):

```
   0x3971f3   41 8b c0                       MOV EAX,R8D
   0x3971f6   83 f8 02                       CMP EAX,0x2
   0x3971f9   41 0f 94 c0                    SETZ R8B
   0x3971fd   41 8d 40 01                    LEA EAX,[R8 + 0x1]
   0x397201   48 83 c4 28                    ADD RSP,0x28
   0x397205   c3                             RET
   0x397218   48 83 ec 28                    SUB RSP,0x28
   0x39721c   48 83 3d 24 e1 3a 04 00        CMP qword ptr [0x7ff753b55348],0x0 ; READ 0x4745348 DAT_7ff753b55348
   0x397224   74 20                          JZ 0x7ff74f7a7246 ; CONDITIONAL_JUMP 0x397246 LAB_7ff74f7a7246
   0x397226   e8 91 ff ff ff                 CALL 0x7ff74f7a71bc ; UNCONDITIONAL_CALL 0x3971bc FUN_7ff74f7a71bc
=> 0x39722b   ba 08 00 00 00                 MOV EDX,0x8
   0x397230   83 e8 01                       SUB EAX,0x1
   0x397233   74 0a                          JZ 0x7ff74f7a723f ; CONDITIONAL_JUMP 0x39723f LAB_7ff74f7a723f
   0x397235   83 f8 01                       CMP EAX,0x1
   0x397238   75 11                          JNZ 0x7ff74f7a724b ; CONDITIONAL_JUMP 0x39724b LAB_7ff74f7a724b
   0x39723a   8d 50 03                       LEA EDX,[RAX + 0x3]
   0x39723d   eb 0c                          JMP 0x7ff74f7a724b ; UNCONDITIONAL_JUMP 0x39724b LAB_7ff74f7a724b
   0x39723f   ba 06 00 00 00                 MOV EDX,0x6
   0x397244   eb 05                          JMP 0x7ff74f7a724b ; UNCONDITIONAL_JUMP 0x39724b LAB_7ff74f7a724b
   0x397246   ba 08 00 00 00                 MOV EDX,0x8
   0x39724b   e8 14 00 00 00                 CALL 0x7ff74f7a7264 ; UNCONDITIONAL_CALL 0x397264 FUN_7ff74f7a7264
```

Decompilation:

```c
// function FUN_7ff74f7a7218 @ 0x397218  body=73 bytes  pdataStart=0x397218
// callers(1): FUN_7ff74f7a6f54@0x396f54 
// callees(2): FUN_7ff74f7a7264@0x397264 FUN_7ff74f7a71bc@0x3971bc 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

ulonglong FUN_7ff74f7a7218(void)

{
  char cVar1;
  int iVar2;
  ulonglong uVar3;
  
  if (_DAT_7ff753b55348 == 0) {
    uVar3 = 8;
  }
  else {
    iVar2 = FUN_7ff74f7a71bc();
    uVar3 = 8;
    if (iVar2 == 1) {
      uVar3 = 6;
    }
    else if (iVar2 == 2) {
      uVar3 = 4;
    }
  }
  cVar1 = FUN_7ff74f7a7264();
  uVar3 = uVar3 & 0xffffffff;
  if (cVar1 != '\0') {
    uVar3 = 1;
  }
  return uVar3;
}
```

## 0x5af50a — Framerate: menu framegen (reads DYNAMIC_FPS setting @0x45baf58)

- containing function: **0x5af4e4** (249 bytes, `FUN_7ff74f9bf4e4`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e5d64c	DATA	-
0x5af4d8	UNCONDITIONAL_CALL	FUN_7ff74f9bf410@0x5af410
```

Disassembly around the anchor (`=>` marks it):

```
   0x5af4e8   4c 8b c1                       MOV R8,RCX
   0x5af4eb   e8 14 01 00 00                 CALL 0x7ff74f9bf604 ; UNCONDITIONAL_CALL 0x5af604 FUN_7ff74f9bf604
   0x5af4f0   45 8b 48 70                    MOV R9D,dword ptr [R8 + 0x70]
   0x5af4f4   44 8b d0                       MOV R10D,EAX
   0x5af4f7   41 89 40 6c                    MOV dword ptr [R8 + 0x6c],EAX
   0x5af4fb   44 3b c8                       CMP R9D,EAX
   0x5af4fe   76 07                          JBE 0x7ff74f9bf507 ; CONDITIONAL_JUMP 0x5af507 LAB_7ff74f9bf507
   0x5af500   41 89 40 70                    MOV dword ptr [R8 + 0x70],EAX
   0x5af504   44 8b c8                       MOV R9D,EAX
   0x5af507   45 33 db                       XOR R11D,R11D
=> 0x5af50a   44 39 1d 47 ba 00 04           CMP dword ptr [0x7ff7539caf58],R11D ; READ 0x45baf58 DAT_7ff7539caf58
   0x5af511   75 09                          JNZ 0x7ff74f9bf51c ; CONDITIONAL_JUMP 0x5af51c LAB_7ff74f9bf51c
   0x5af513   45 89 50 70                    MOV dword ptr [R8 + 0x70],R10D
   0x5af517   e9 bc 00 00 00                 JMP 0x7ff74f9bf5d8 ; UNCONDITIONAL_JUMP 0x5af5d8 LAB_7ff74f9bf5d8
   0x5af51c   45 38 58 69                    CMP byte ptr [R8 + 0x69],R11B
   0x5af520   0f 85 b2 00 00 00              JNZ 0x7ff74f9bf5d8 ; CONDITIONAL_JUMP 0x5af5d8 LAB_7ff74f9bf5d8
   0x5af526   e8 69 70 a9 ff                 CALL 0x7ff74f456594 ; UNCONDITIONAL_CALL 0x46594 FUN_7ff74f456594
   0x5af52b   69 c8 e8 03 00 00              IMUL ECX,EAX,0x3e8
   0x5af531   b8 89 88 88 88                 MOV EAX,0x88888889
   0x5af536   f7 e1                          MUL ECX
   0x5af538   41 8a cb                       MOV CL,R11B
```

Decompilation:

```c
// function FUN_7ff74f9bf4e4 @ 0x5af4e4  body=249 bytes  pdataStart=0x5af4e4
// callers(1): FUN_7ff74f9bf410@0x5af410 
// callees(2): FUN_7ff74f9bf604@0x5af604 FUN_7ff74f456594@0x46594 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff74f9bf4e4(longlong param_1)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  char cVar4;
  int iVar5;
  
  uVar1 = FUN_7ff74f9bf604();
  *(uint *)(param_1 + 0x6c) = uVar1;
  uVar3 = *(uint *)(param_1 + 0x70);
  if (uVar1 < *(uint *)(param_1 + 0x70)) {
    *(uint *)(param_1 + 0x70) = uVar1;
    uVar3 = uVar1;
  }
  iVar5 = 0;
  if (_DAT_7ff7539caf58 == 0) {
    *(uint *)(param_1 + 0x70) = uVar1;
    return;
  }
  if (*(char *)(param_1 + 0x69) != '\0') {
    return;
  }
  iVar2 = FUN_7ff74f456594();
  cVar4 = (char)iVar5;
  if (*(int *)(param_1 + 0x5c) == 4) {
    if (uVar3 == 0x78) {
      *(undefined4 *)(param_1 + 0x70) = 0x3c;
      uVar3 = 0x3c;
LAB_7ff74f9bf56f:
      cVar4 = '\x01';
      *(undefined4 *)(param_1 + 0x5c) = 3;
    }
    else if (uVar3 == 0x3c) {
      *(undefined4 *)(param_1 + 0x70) = 0x1e;
      uVar3 = 0x1e;
      goto LAB_7ff74f9bf56f;
    }
    if (cVar4 == '\0') goto LAB_7ff74f9bf5c6;
  }
  else {
    if (((int)-((uint)(iVar2 * 1000) / 0x3c) <= *(int *)(param_1 + 0x60)) ||
       (*(int *)(param_1 + 0x5c) != iVar5)) goto LAB_7ff74f9bf5c6;
    if ((uVar3 == 0x1e) && (uVar1 != 0x1e)) {
      *(undefined4 *)(param_1 + 0x70) = 0x3c;
      uVar3 = 0x3c;
    }
    else {
      if ((uVar3 != 0x3c) || (uVar1 != 0x78)) goto LAB_7ff74f9bf5c6;
      *(undefined4 *)(param_1 + 0x70) = 0x78;
      uVar3 = 0x78;
    }
  }
  *(int *)(param_1 + 0x60) = iVar5;
LAB_7ff74f9bf5c6:
  if (_DAT_7ff753b69910 != 0) {
    *(uint *)(_DAT_7ff753b69910 + 0x618) = uVar3;
  }
  return;
}
```

## 0x5af5c2 — Framerate: dynamic-FPS setting (same function as 0x5af50a)

- containing function: **0x5af4e4** (249 bytes, `FUN_7ff74f9bf4e4`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e5d64c	DATA	-
0x5af4d8	UNCONDITIONAL_CALL	FUN_7ff74f9bf410@0x5af410
```

Disassembly around the anchor (`=>` marks it):

```
   0x5af59e   41 c7 40 70 3c 00 00 00        MOV dword ptr [R8 + 0x70],0x3c
   0x5af5a6   41 8d 41 1e                    LEA EAX,[R9 + 0x1e]
   0x5af5aa   eb 16                          JMP 0x7ff74f9bf5c2 ; UNCONDITIONAL_JUMP 0x5af5c2 LAB_7ff74f9bf5c2
   0x5af5ac   41 83 f9 3c                    CMP R9D,0x3c
   0x5af5b0   75 14                          JNZ 0x7ff74f9bf5c6 ; CONDITIONAL_JUMP 0x5af5c6 LAB_7ff74f9bf5c6
   0x5af5b2   41 8b c1                       MOV EAX,R9D
   0x5af5b5   41 83 fa 78                    CMP R10D,0x78
   0x5af5b9   75 0b                          JNZ 0x7ff74f9bf5c6 ; CONDITIONAL_JUMP 0x5af5c6 LAB_7ff74f9bf5c6
   0x5af5bb   45 89 50 70                    MOV dword ptr [R8 + 0x70],R10D
   0x5af5bf   41 8b c2                       MOV EAX,R10D
=> 0x5af5c2   45 89 58 60                    MOV dword ptr [R8 + 0x60],R11D
   0x5af5c6   48 8b 0d 43 a3 1a 04           MOV RCX,qword ptr [0x7ff753b69910] ; READ 0x4759910 DAT_7ff753b69910
   0x5af5cd   48 85 c9                       TEST RCX,RCX
   0x5af5d0   74 06                          JZ 0x7ff74f9bf5d8 ; CONDITIONAL_JUMP 0x5af5d8 LAB_7ff74f9bf5d8
   0x5af5d2   89 81 18 06 00 00              MOV dword ptr [RCX + 0x618],EAX
   0x5af5d8   48 83 c4 28                    ADD RSP,0x28
   0x5af5dc   c3                             RET
   0x5af604   48 83 ec 28                    SUB RSP,0x28
   0x5af608   48 8b 05 21 89 f0 03           MOV RAX,qword ptr [0x7ff7538c7f30] ; READ 0x44b7f30 DAT_7ff7538c7f30
   0x5af60f   48 33 c4                       XOR RAX,RSP
   0x5af612   48 89 44 24 10                 MOV qword ptr [RSP + 0x10],RAX
```

Decompilation: same function as above (0x5af4e4).

## 0x433130 — Framerate: dynamic-FPS target

- containing function: **0x433114** (271 bytes, `FUN_7ff74f843114`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e50cd4	DATA	-
0x434d8d	UNCONDITIONAL_CALL	FUN_7ff74f844d24@0x434d24
```

Disassembly around the anchor (`=>` marks it):

```
   0x43310e   72 e1                          JC 0x7ff74f8430f1 ; CONDITIONAL_JUMP 0x4330f1 LAB_7ff74f8430f1
   0x433110   48 8b c1                       MOV RAX,RCX
   0x433113   c3                             RET
   0x433114   48 89 5c 24 10                 MOV qword ptr [RSP + 0x10],RBX
   0x433119   48 89 74 24 18                 MOV qword ptr [RSP + 0x18],RSI
   0x43311e   57                             PUSH RDI
   0x43311f   48 83 ec 30                    SUB RSP,0x30
   0x433123   0f 29 74 24 20                 MOVAPS xmmword ptr [RSP + 0x20],XMM6
   0x433128   48 8b f9                       MOV RDI,RCX
   0x43312b   e8 54 33 c1 ff                 CALL 0x7ff74f456484 ; UNCONDITIONAL_CALL 0x46484 FUN_7ff74f456484
=> 0x433130   48 8b 71 50                    MOV RSI,qword ptr [RCX + 0x50]
   0x433134   0f 28 f0                       MOVAPS XMM6,XMM0
   0x433137   48 8b 1e                       MOV RBX,qword ptr [RSI]
   0x43313a   48 3b de                       CMP RBX,RSI
   0x43313d   74 3b                          JZ 0x7ff74f84317a ; CONDITIONAL_JUMP 0x43317a LAB_7ff74f84317a
   0x43313f   48 8b 43 10                    MOV RAX,qword ptr [RBX + 0x10]
   0x433143   48 8d 8f 08 01 00 00           LEA RCX,[RDI + 0x108]
   0x43314a   89 44 24 40                    MOV dword ptr [RSP + 0x40],EAX
   0x43314e   48 8d 54 24 40                 LEA RDX,[RSP + 0x40]
   0x433153   48 c1 e8 20                    SHR RAX,0x20
   0x433157   84 c0                          TEST AL,AL
```

Decompilation:

```c
// function FUN_7ff74f843114 @ 0x433114  body=271 bytes  pdataStart=0x433114
// callers(1): FUN_7ff74f844d24@0x434d24 
// callees(4): FUN_7ff74f456484@0x46484 FUN_7ff74f8435cc@0x4335cc FUN_7ff74f84364c@0x43364c FUN_7ff74f842d10@0x432d10 

void FUN_7ff74f843114(longlong param_1)

{
  undefined8 *puVar1;
  float *pfVar2;
  undefined4 *puVar3;
  longlong lVar4;
  undefined8 *puVar5;
  float fVar6;
  undefined4 local_res8 [2];
  
  lVar4 = param_1;
  fVar6 = (float)FUN_7ff74f456484();
  puVar1 = *(undefined8 **)(lVar4 + 0x50);
  for (puVar5 = (undefined8 *)*puVar1; puVar5 != puVar1; puVar5 = (undefined8 *)*puVar5) {
    local_res8[0] = (undefined4)puVar5[2];
    if ((char)((ulonglong)puVar5[2] >> 0x20) == '\0') {
      puVar3 = (undefined4 *)FUN_7ff74f8435cc(param_1 + 0x108,local_res8);
      *puVar3 = 0;
    }
    else {
      pfVar2 = (float *)FUN_7ff74f8435cc(param_1 + 0x108,local_res8);
      *pfVar2 = fVar6 + *pfVar2;
    }
  }
  puVar1 = *(undefined8 **)(param_1 + 400);
  for (puVar5 = (undefined8 *)*puVar1; puVar5 != puVar1; puVar5 = (undefined8 *)*puVar5) {
    local_res8[0] = (undefined4)puVar5[2];
    if ((char)((ulonglong)puVar5[2] >> 0x20) == '\0') {
      puVar3 = (undefined4 *)FUN_7ff74f84364c(param_1 + 0x248,local_res8);
      *puVar3 = 0;
    }
    else {
      pfVar2 = (float *)FUN_7ff74f84364c(param_1 + 0x248,local_res8);
      *pfVar2 = fVar6 + *pfVar2;
    }
  }
  puVar1 = *(undefined8 **)(param_1 + 0x2d0);
  for (puVar5 = (undefined8 *)*puVar1; puVar5 != puVar1; puVar5 = (undefined8 *)*puVar5) {
    local_res8[0] = (undefined4)puVar5[2];
    if ((char)((ulonglong)puVar5[2] >> 0x20) == '\0') {
      puVar3 = (undefined4 *)FUN_7ff74f842d10(param_1 + 0x388,local_res8);
      *puVar3 = 0;
    }
    else {
      pfVar2 = (float *)FUN_7ff74f842d10(param_1 + 0x388,local_res8);
      *pfVar2 = fVar6 + *pfVar2;
    }
  }
  return;
}
```

## 0x1fde50e — Resolution

- containing function: **0x1fde504** (98 bytes, `FUN_7ff7513ee504`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4f9ba18	DATA	-
0x1fe6356	UNCONDITIONAL_CALL	FUN_7ff7513f6064@0x1fe6064
0x1350af1	UNCONDITIONAL_CALL	FUN_7ff750760210@0x1350210
0x1fd6d59	UNCONDITIONAL_CALL	FUN_7ff7513e6d44@0x1fd6d44
```

Disassembly around the anchor (`=>` marks it):

```
   0x1fde4dc  8b 0d 0e cb 5d 02              MOV ECX,dword ptr [0x7ff7539caff0] ; READ 0x45baff0 DAT_7ff7539caff0
   0x1fde4e2  e8 41 fa ff ff                 CALL 0x7ff7513edf28 ; UNCONDITIONAL_CALL 0x1fddf28 FUN_7ff7513edf28
   0x1fde4e7  8b 0d 07 cb 5d 02              MOV ECX,dword ptr [0x7ff7539caff4] ; READ 0x45baff4 DAT_7ff7539caff4
   0x1fde4ed  e8 82 fa ff ff                 CALL 0x7ff7513edf74 ; UNCONDITIONAL_CALL 0x1fddf74 FUN_7ff7513edf74
   0x1fde4f2  8b 0d 58 ca 5d 02              MOV ECX,dword ptr [0x7ff7539caf50] ; READ 0x45baf50 DAT_7ff7539caf50
   0x1fde4f8  48 83 c4 20                    ADD RSP,0x20
   0x1fde4fc  5b                             POP RBX
   0x1fde4fd  e9 e6 fe ff ff                 JMP 0x7ff7513ee3e8 ; UNCONDITIONAL_CALL 0x1fde3e8 FUN_7ff7513ee3e8
   0x1fde504  48 83 ec 28                    SUB RSP,0x28
   0x1fde508  8b 05 12 cb 5d 02              MOV EAX,dword ptr [0x7ff7539cb020] ; READ 0x45bb020 DAT_7ff7539cb020
=> 0x1fde50e  4c 8d 05 db a5 99 01           LEA R8,[0x7ff752d88af0] ; DATA 0x3978af0 DAT_7ff752d88af0
   0x1fde515  48 63 d1                       MOVSXD RDX,ECX
   0x1fde518  41 39 04 d0                    CMP dword ptr [R8 + RDX*0x8],EAX
   0x1fde51c  75 0d                          JNZ 0x7ff7513ee52b ; CONDITIONAL_JUMP 0x1fde52b LAB_7ff7513ee52b
   0x1fde51e  8b 05 00 cb 5d 02              MOV EAX,dword ptr [0x7ff7539cb024] ; READ 0x45bb024 DAT_7ff7539cb024
   0x1fde524  41 39 44 d0 04                 CMP dword ptr [R8 + RDX*0x8 + 0x4],EAX
   0x1fde529  74 36                          JZ 0x7ff7513ee561 ; CONDITIONAL_JUMP 0x1fde561 LAB_7ff7513ee561
   0x1fde52b  49 8b 04 d0                    MOV RAX,qword ptr [R8 + RDX*0x8]
   0x1fde52f  83 3d 1a ca 5d 02 ff           CMP dword ptr [0x7ff7539caf50],-0x1 ; READ 0x45baf50 DAT_7ff7539caf50
   0x1fde536  89 0d 0c ca 5d 02              MOV dword ptr [0x7ff7539caf48],ECX ; WRITE 0x45baf48 DAT_7ff7539caf48
   0x1fde53c  89 0d a2 d7 5d 02              MOV dword ptr [0x7ff7539cbce4],ECX ; WRITE 0x45bbce4 DAT_7ff7539cbce4
```

Decompilation:

```c
// function FUN_7ff7513ee504 @ 0x1fde504  body=98 bytes  pdataStart=0x1fde504
// callers(3): FUN_7ff7513f6064@0x1fe6064 FUN_7ff7513e6d44@0x1fd6d44 FUN_7ff750760210@0x1350210 
// callees(1): FUN_7ff7513ee3e8@0x1fde3e8 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff7513ee504(int param_1)

{
  int iVar1;
  longlong lVar2;
  
  lVar2 = (longlong)param_1;
  if ((*(int *)(&DAT_7ff752d88af0 + lVar2 * 8) != _DAT_7ff7539cb020) ||
     (*(int *)(&UNK_7ff752d88af4 + lVar2 * 8) != _DAT_7ff7539cb024)) {
    _DAT_7ff7539cb020 = *(undefined8 *)(&DAT_7ff752d88af0 + lVar2 * 8);
    iVar1 = 8;
    if (_DAT_7ff7539caf50 != -1) {
      iVar1 = _DAT_7ff7539caf50;
    }
    _DAT_7ff7539caf48 = param_1;
    _DAT_7ff7539cb030 = _DAT_7ff7539cb020;
    _DAT_7ff7539cbce4 = param_1;
    FUN_7ff7513ee3e8(iVar1);
  }
  return;
}
```

## 0x1fde43e — Resolution

- containing function: **0x1fde3e8** (149 bytes, `FUN_7ff7513ee3e8`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4f9b9f4	DATA	-
0x1fe6307	UNCONDITIONAL_CALL	FUN_7ff7513f6064@0x1fe6064
0x1fddf69	UNCONDITIONAL_CALL	FUN_7ff7513edf28@0x1fddf28
0x1fde55c	UNCONDITIONAL_CALL	FUN_7ff7513ee504@0x1fde504
0xe6c595	UNCONDITIONAL_CALL	FUN_7ff75027c380@0xe6c380
0x12a3530	UNCONDITIONAL_CALL	FUN_7ff7506b2368@0x12a2368
0x1fde0af	UNCONDITIONAL_CALL	FUN_7ff7513ee034@0x1fde034
0xfbb8c7	UNCONDITIONAL_CALL	FUN_7ff7503cb890@0xfbb890
0x1fde4fd	UNCONDITIONAL_CALL	FUN_7ff7513ee4b8@0x1fde4b8
```

Disassembly around the anchor (`=>` marks it):

```
   0x1fde41d  75 1f                          JNZ 0x7ff7513ee43e ; CONDITIONAL_JUMP 0x1fde43e LAB_7ff7513ee43e
   0x1fde41f  41 83 fb 02                    CMP R11D,0x2
   0x1fde423  7f 0d                          JG 0x7ff7513ee432 ; CONDITIONAL_JUMP 0x1fde432 LAB_7ff7513ee432
   0x1fde425  83 f8 09                       CMP EAX,0x9
   0x1fde428  7f 0e                          JG 0x7ff7513ee438 ; CONDITIONAL_JUMP 0x1fde438 LAB_7ff7513ee438
   0x1fde42a  41 bb 02 00 00 00              MOV R11D,0x2
   0x1fde430  eb 0c                          JMP 0x7ff7513ee43e ; UNCONDITIONAL_JUMP 0x1fde43e LAB_7ff7513ee43e
   0x1fde432  41 83 fb 05                    CMP R11D,0x5
   0x1fde436  7f 06                          JG 0x7ff7513ee43e ; CONDITIONAL_JUMP 0x1fde43e LAB_7ff7513ee43e
   0x1fde438  41 bb 05 00 00 00              MOV R11D,0x5
=> 0x1fde43e  48 8b c8                       MOV RCX,RAX
   0x1fde441  49 63 c3                       MOVSXD RAX,R11D
   0x1fde444  48 c1 e1 04                    SHL RCX,0x4
   0x1fde448  48 03 c8                       ADD RCX,RAX
   0x1fde44b  48 63 84 8a b0 87 97 03        MOVSXD RAX,dword ptr [RDX + RCX*0x4 + 0x39787b0]
   0x1fde453  48 8b 84 c2 d0 83 97 03        MOV RAX,qword ptr [RDX + RAX*0x8 + 0x39783d0]
   0x1fde45b  48 89 05 c6 cb 5d 02           MOV qword ptr [0x7ff7539cb028],RAX ; WRITE 0x45bb028 DAT_7ff7539cb028
   0x1fde462  e8 39 06 00 00                 CALL 0x7ff7513eeaa0 ; UNCONDITIONAL_CALL 0x1fdeaa0 FUN_7ff7513eeaa0
   0x1fde467  44 39 1d 82 cb 5d 02           CMP dword ptr [0x7ff7539caff0],R11D ; READ 0x45baff0 DAT_7ff7539caff0
   0x1fde46e  74 08                          JZ 0x7ff7513ee478 ; CONDITIONAL_JUMP 0x1fde478 LAB_7ff7513ee478
   0x1fde470  41 8b cb                       MOV ECX,R11D
```

Decompilation:

```c
// function FUN_7ff7513ee3e8 @ 0x1fde3e8  body=149 bytes  pdataStart=0x1fde3e8
// callers(8): FUN_7ff7513f6064@0x1fe6064 FUN_7ff7506b2368@0x12a2368 FUN_7ff7513ee504@0x1fde504 FUN_7ff75027c380@0xe6c380 FUN_7ff7513ee034@0x1fde034 FUN_7ff7513ee4b8@0x1fde4b8 FUN_7ff7513edf28@0x1fddf28 FUN_7ff7503cb890@0xfbb890 
// callees(2): FUN_7ff7513eeaa0@0x1fdeaa0 FUN_7ff7513edf28@0x1fddf28 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_7ff7513ee3e8(int param_1)

{
  int iVar1;
  
  _DAT_7ff7539cb038 = *(undefined4 *)(&UNK_7ff7530490c0 + (longlong)param_1 * 4);
  iVar1 = param_1;
  if (_DAT_7ff7539caff8 == 1) {
    if (param_1 < 3) {
      if (_DAT_7ff7539caf48 < 10) {
        iVar1 = 2;
        goto LAB_7ff7513ee43e;
      }
    }
    else if (5 < param_1) goto LAB_7ff7513ee43e;
    iVar1 = 5;
  }
LAB_7ff7513ee43e:
  _DAT_7ff7539cb028 =
       *(undefined8 *)
        (&DAT_7ff752d883d0 +
        (longlong)
        *(int *)(&DAT_7ff752d887b0 + ((longlong)_DAT_7ff7539caf48 * 0x10 + (longlong)iVar1) * 4) * 8
        );
  _DAT_7ff7539caf50 = param_1;
  FUN_7ff7513eeaa0();
  if (_DAT_7ff7539caff0 != iVar1) {
    FUN_7ff7513edf28(iVar1);
  }
  return;
}
```

## 0x8781f8 — FOV

- containing function: **0x878054** (2415 bytes, `FUN_7ff74fc88054`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x4e782bc	DATA	-
0x298dc0f	DATA	FUN_7ff751d9d6c8@0x298d6c8
```

Disassembly around the anchor (`=>` marks it):

```
   0x8781c4   81 fb 01 04 00 00              CMP EBX,0x401
   0x8781ca   0f 85 ac 00 00 00              JNZ 0x7ff74fc8827c ; CONDITIONAL_JUMP 0x87827c LAB_7ff74fc8827c
   0x8781d0   ba eb ff ff ff                 MOV EDX,0xffffffeb
   0x8781d5   49 8b cf                       MOV RCX,R15
   0x8781d8   ff 15 92 a7 06 03              CALL qword ptr [0x7ff752cf2970] ; READ 0x38e2970 PTR_GetWindowLongPtrA_7ff752cf2970
   0x8781de   4c 8b e8                       MOV R13,RAX
   0x8781e1   48 85 c0                       TEST RAX,RAX
   0x8781e4   0f 84 6d 07 00 00              JZ 0x7ff74fc88957 ; CONDITIONAL_JUMP 0x878957 LAB_7ff74fc88957
   0x8781ea   44 39 35 3f 2d d4 03           CMP dword ptr [0x7ff7539caf30],R14D ; READ 0x45baf30 DAT_7ff7539caf30
   0x8781f1   48 8b 0d 28 2e d4 03           MOV RCX,qword ptr [0x7ff7539cb020] ; READ 0x45bb020 DAT_7ff7539cb020
=> 0x8781f8   48 89 4d 97                    MOV qword ptr [RBP + -0x69],RCX
   0x8781fc   75 67                          JNZ 0x7ff74fc88265 ; CONDITIONAL_JUMP 0x878265 LAB_7ff74fc88265
   0x8781fe   8b 0d 30 2d d4 03              MOV ECX,dword ptr [0x7ff7539caf34] ; READ 0x45baf34 DAT_7ff7539caf34
   0x878204   4c 8d 4d 97                    LEA R9,[RBP + -0x69]
   0x878208   89 4c 24 20                    MOV dword ptr [RSP + 0x20],ECX
   0x87820c   45 33 c0                       XOR R8D,R8D
   0x87820f   48 8d 4c 24 40                 LEA RCX,[RSP + 0x40]
   0x878214   49 8b d7                       MOV RDX,R15
   0x878217   e8 54 91 ad 00                 CALL 0x7ff750761370 ; UNCONDITIONAL_CALL 0x1351370 FUN_7ff750761370
   0x87821c   45 33 c9                       XOR R9D,R9D
   0x87821f   c7 44 24 30 42 00 00 00        MOV dword ptr [RSP + 0x30],0x42
```

Decompilation:

```c
// function FUN_7ff74fc88054 @ 0x878054  body=2415 bytes  pdataStart=0x878054
// callers(0): 
// callees(21): FUN_7ff7513eed2c@0x1fded2c FUN_7ff74fc889c4@0x8789c4 FUN_7ff7513f3b8c@0x1fe3b8c FUN_7ff7513f548c@0x1fe548c FUN_7ff7513f2424@0x1fe2424 __security_check_cookie@0xb912e0 FUN_7ff7513f56a8@0x1fe56a8 FUN_7ff7513f0e74@0x1fe0e74 FUN_7ff74fc88b4c@0x878b4c FUN_7ff7513ebb1c@0x1fdbb1c FUN_7ff7513f6684@0x1fe6684 FUN_7ff74fac8a2c@0x6b8a2c FUN_7ff750761370@0x1351370 FUN_7ff7513f66c8@0x1fe66c8 FUN_7ff7513ee348@0x1fde348 FUN_7ff750ce9a84@0x18d9a84 FUN_7ff7513f3984@0x1fe3984 FUN_7ff7503cb718@0xfbb718 FUN_7ff7503ca838@0xfba838 FUN_7ff7513ee578@0x1fde578 FUN_7ff751da9378@0x2999378 

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* WARNING: Type propagation algorithm not settling */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

LRESULT FUN_7ff74fc88054(HWND param_1,UINT param_2,WPARAM param_3,longlong *param_4)

{
  undefined1 auVar1 [16];
  LPCSTR lpCursorName;
  char cVar2;
  short sVar3;
  UINT UVar4;
  BOOL BVar5;
  int iVar6;
  LONG_PTR LVar7;
  undefined1 (*pauVar8) [16];
  ulonglong *puVar9;
  HINSTANCE pHVar10;
  HCURSOR hCursor;
  LRESULT LVar11;
  undefined8 *puVar12;
  uint uVar13;
  longlong lVar14;
  HINSTANCE hInstance;
  int iVar15;
  undefined1 auStackY_128 [32];
  int iVar16;
  undefined1 local_e8 [16];
  ulonglong local_d8;
  ulonglong uStack_d0;
  tagRECT local_c8;
  undefined8 local_b8;
  undefined8 uStack_b0;
  undefined1 local_a8 [4];
  int local_a4;
  int local_a0;
  int local_9c;
  int local_98;
  ulonglong local_58;
  
  local_58 = _DAT_7ff7538c7f30 ^ (ulonglong)auStackY_128;
  UVar4 = FUN_7ff74fac8a2c();
  pHVar10 = (HINSTANCE)0x0;
  if ((UVar4 != 0) && (UVar4 == param_2)) {
    FUN_7ff750ce9a84();
  }
  if (param_2 == 0x1c) {
    LVar7 = GetWindowLongPtrA(param_1,-0x15);
    if (LVar7 != 0) {
      *(bool *)(LVar7 + 0x1ab) = (int)param_3 != 0;
      return 0;
    }
    goto LAB_7ff74fc88957;
  }
  if (param_2 == 0x7e) {
    GetWindowLongPtrA(param_1,-0x15);
    FUN_7ff7513f548c();
  }
  else {
    if (param_2 == 0x24) {
      if (param_4 != (longlong *)0x0) {
        *(undefined4 *)((longlong)param_4 + 0x1c) = 0x80000000;
        *(undefined4 *)(param_4 + 3) = 0x80000000;
        *(undefined4 *)((longlong)param_4 + 0x24) = 0x7fffffff;
        *(undefined4 *)(param_4 + 4) = 0x7fffffff;
        return 0;
      }
      goto LAB_7ff74fc88957;
    }
    if (param_2 == 1) {
      if ((param_4 == (longlong *)0x0) || (*param_4 == 0)) {
        return -1;
      }
      lVar14 = *param_4;
      iVar6 = -0x15;
LAB_7ff74fc88127:
      SetWindowLongPtrA(param_1,iVar6,lVar14);
      return 0;
    }
    if (param_2 == 5) {
      if (DAT_7ff7539d6f9e == '\x01') {
        DAT_7ff7539d6f9e = '\0';
      }
      else {
        LVar7 = GetWindowLongPtrA(param_1,-0x15);
        if (LVar7 != 0) {
          if (_DAT_7ff7539caf30 != 0) {
            return 0;
          }
          uVar13 = (uint)((ulonglong)param_4 >> 0x10);
          if ((((uint)param_4 & 0xffff) == *(uint *)(LVar7 + 0x128)) &&
             ((uVar13 & 0xffff) == *(uint *)(LVar7 + 300))) {
            return 0;
          }
          local_c8._0_8_ = CONCAT44(uVar13,(uint)param_4) & 0xffff0000ffff;
          *(undefined8 *)(LVar7 + 0x128) = local_c8._0_8_;
          *(undefined1 *)(LVar7 + 0x138) = 1;
          *(undefined4 *)(LVar7 + 0x13c) = 5;
          return 0;
        }
      }
      goto LAB_7ff74fc88957;
    }
    if (param_2 == 0x401) {
      LVar7 = GetWindowLongPtrA(param_1,-0x15);
      if (LVar7 != 0) {
        local_c8.left = _DAT_7ff7539cb020;
        local_c8.top = _DAT_7ff7539cb024;
        if (_DAT_7ff7539caf30 == 0) {
          pauVar8 = (undefined1 (*) [16])FUN_7ff750761370(local_e8,param_1,0,&local_c8);
          local_b8 = *(undefined8 *)pauVar8[1];
          uStack_b0 = *(undefined8 *)(pauVar8[1] + 8);
          auVar1 = *pauVar8;
          *(undefined8 *)(LVar7 + 0x128) = local_b8;
          SetWindowPos(param_1,(HWND)0xfffffffffffffffe,0,0,auVar1._8_4_,auVar1._12_4_,0x42);
        }
LAB_7ff74fc88265:
        *(undefined4 *)(LVar7 + 0x13c) = 5;
        *(undefined1 *)(LVar7 + 0x138) = 1;
        return 0;
      }
      goto LAB_7ff74fc88957;
    }
    if (param_2 != 3) {
      if (param_2 == 0x402) {
        LVar7 = GetWindowLongPtrA(param_1,-0x15);
        if (LVar7 != 0) {
          lVar14 = (longlong)_DAT_7ff7539caf34;
          if ((_DAT_7ff7539caf5c == 1) &&
             (cVar2 = FUN_7ff7513eed2c(_DAT_7ff7539caf34), cVar2 == '\0')) {
            FUN_7ff7513ee348(0);
            *(undefined4 *)(LVar7 + 0x1bc) = 0;
            *(undefined4 *)(LVar7 + 0x13c) = 0x14;
            *(undefined1 *)(LVar7 + 0x138) = 1;
          }
          lVar14 = *(longlong *)(&DAT_7ff7539cb040 + lVar14 * 8);
          if (lVar14 == 0) {
            iVar6 = FUN_7ff7513f2424();
            lVar14 = *(longlong *)(&DAT_7ff7539cb040 + (longlong)iVar6 * 8);
          }
          FUN_7ff7513f0e74(local_a8,lVar14);
          iVar6 = local_9c - local_a4;
          iVar15 = local_98 - local_a0;
          SetWindowLongPtrA(param_1,-0x10,0xcf0000);
          SetWindowLongPtrA(param_1,-0x14,0x200);
          SetWindowPos(param_1,(HWND)0xfffffffffffffffe,local_a4,local_a0,iVar6,iVar15,0x203d);
          Sleep(100);
          SetWindowLongPtrA(param_1,-0x10,0x80080000);
          SetWindowLongPtrA(param_1,-0x14,0);
          SetWindowPos(param_1,(HWND)0xfffffffffffffffe,local_a4,local_a0,iVar6,iVar15,0x2568);
          *(undefined1 *)(LVar7 + 0x1a9) = 0;
          local_c8.top = iVar15;
          local_c8.left = iVar6;
          if ((*(int *)(LVar7 + 0x128) == iVar6) && (*(int *)(LVar7 + 300) == iVar15)) {
            return 0;
          }
          *(undefined8 *)(LVar7 + 0x128) = local_c8._0_8_;
          *(undefined4 *)(LVar7 + 0x13c) = 5;
          *(undefined1 *)(LVar7 + 0x138) = 1;
          return 0;
        }
      }
      else if (param_2 == 0x403) {
        LVar7 = GetWindowLongPtrA(param_1,-0x15);
        iVar15 = _DAT_7ff7539cbccc;
        iVar6 = _DAT_7ff7539caf30;
        if (LVar7 != 0) {
          if (_DAT_7ff7539caf30 == _DAT_7ff7539cbccc) {
            return 0;
          }
          cVar2 = FUN_7ff7513f3984(_DAT_7ff7539caf48);
          iVar16 = _DAT_7ff7539cbce4;
          if (cVar2 == '\0') {
            do {
              cVar2 = FUN_7ff7513f3984(pHVar10);
              iVar16 = (int)pHVar10;
              if (cVar2 != '\0') break;
              uVar13 = (int)pHVar10 + 1;
              pHVar10 = (HINSTANCE)(ulonglong)uVar13;
              iVar16 = _DAT_7ff7539cbce4;
            } while ((int)uVar13 < 0xd);
          }
          _DAT_7ff7539cbce4 = iVar16;
          if (iVar6 == 0) {
            puVar9 = (ulonglong *)FUN_7ff750761370(local_e8,param_1,0,LVar7 + 0x130);
            local_c8._0_8_ = *puVar9;
            local_c8._8_8_ = puVar9[1];
            local_d8 = puVar9[2];
            uStack_d0 = puVar9[3];
            *(ulonglong *)(LVar7 + 0x128) = local_d8;
            SetWindowLongPtrA(param_1,-0x10,0xc00000);
            SetWindowLongPtrA(param_1,-0x14,0x200);
            UVar4 = 0x42;
            local_a0 = 0;
            local_a4 = 0;
            iVar15 = local_c8.bottom;
            iVar16 = local_c8.right;
          }
          else {
            if (iVar6 != 1) {
              _DAT_7ff7539caf30 = iVar15;
              return 0;
            }
            lVar14 = *(longlong *)(&DAT_7ff7539cb040 + (longlong)_DAT_7ff7539caf34 * 8);
            if (lVar14 == 0) {
              iVar15 = FUN_7ff7513f2424();
              lVar14 = *(longlong *)(&DAT_7ff7539cb040 + (longlong)iVar15 * 8);
            }
            FUN_7ff7513f0e74(local_a8,lVar14);
            SetWindowLongPtrA(param_1,-0x10,0x80080000);
            SetWindowLongPtrA(param_1,-0x14,0);
            UVar4 = 0x40;
            iVar15 = local_98 - local_a0;
            iVar16 = local_9c - local_a4;
          }
          SetWindowPos(param_1,(HWND)0xfffffffffffffffe,local_a4,local_a0,iVar16,iVar15,UVar4);
          *(undefined4 *)(LVar7 + 0x13c) = 5;
          *(undefined1 *)(LVar7 + 0x138) = 1;
          _DAT_7ff7539cbccc = iVar6;
          if (_DAT_7ff7539caf5c != 1) {
            return 0;
          }
          cVar2 = FUN_7ff7513f3b8c();
          if (cVar2 != '\0') {
            return 0;
          }
          FUN_7ff7513ee348(0);
          goto LAB_7ff74fc88265;
        }
      }
      else {
        if (param_2 == 0x404) {
          LVar7 = GetWindowLongPtrA(param_1,-0x15);
          if (LVar7 == 0) {
            return 0;
          }
          *(undefined4 *)(LVar7 + 0x13c) = 5;
          *(undefined1 *)(LVar7 + 0x138) = 1;
          return 0;
        }
        if (param_2 == 0x405) {
          LVar7 = GetWindowLongPtrA(param_1,-0x15);
          if (LVar7 == 0) {
            return 0;
          }
          *(undefined4 *)(LVar7 + 0x13c) = 5;
          *(undefined2 *)(LVar7 + 0x138) = 0x101;
          return 0;
        }
        if (param_2 == 0x10) {
          LVar7 = GetWindowLongPtrA(param_1,-0x15);
          if (LVar7 != 0) {
            *(undefined1 *)(LVar7 + 0x138) = 0;
          }
        }
        else {
          if (param_2 == 2) {
            PostQuitMessage(0);
            return 0;
          }
          if (((param_2 == 0x281 || param_2 == 0x282) || (param_2 - 0x10d < 3)) ||
             (param_2 == 0x286)) {
            if (_DAT_7ff7539dc560 != 0) {
              LVar11 = FUN_7ff74fc889c4(_DAT_7ff7539dc560,param_1,param_2,param_3);
              return LVar11;
            }
            return 0;
          }
          if (param_2 == 0x104) {
            LVar7 = GetWindowLongPtrA(param_1,-0x15);
            if (LVar7 != 0) {
              if (param_3 == 0x5b || param_3 == 0x5c) {
                *(undefined1 *)(LVar7 + 0x13a) = 1;
              }
              if (param_3 == 0xa0 || param_3 == 0xa1) {
                *(undefined1 *)(LVar7 + 0x13b) = 1;
              }
            }
          }
          else {
            if (param_2 != 0x105) {
              if (param_2 == 0x100) {
                if ((((param_3 - 0x25 & 0xfffffffffffffffd) == 0) &&
                    (LVar7 = GetWindowLongPtrA(param_1,-0x15), LVar7 != 0)) &&
                   ((*(char *)(LVar7 + 0x13a) != '\0' && (*(char *)(LVar7 + 0x13b) != '\0')))) {
                  iVar6 = -0x10;
                  lVar14 = 0xcf0000;
                  goto LAB_7ff74fc88127;
                }
              }
              else if (param_2 != 0x101 && param_2 != 0x102) {
                if (param_2 == 0x51) {
                  BVar5 = GetKeyboardLayoutNameA((LPSTR)&local_c8);
                  if (BVar5 != 0) {
                    FUN_7ff7513ebb1c(&local_c8);
                  }
                }
                else if (param_2 == 0x112) {
                  if (param_3 == 0xf100) {
                    return 1;
                  }
                }
                else {
                  sVar3 = (short)(param_3 >> 0x10);
                  if (param_2 == 0x20e) {
                    FUN_7ff7513f6684((int)sVar3);
                  }
                  else if (param_2 == 0x20a) {
                    FUN_7ff7513f66c8((int)sVar3);
                  }
                  else {
                    if (param_2 == 0x406) {
                      if (param_3 != 0) {
                        do {
                          iVar6 = ShowCursor(1);
                        } while (iVar6 < 0);
                        return 0;
                      }
                      do {
                        iVar6 = ShowCursor(0);
                      } while (-1 < iVar6);
                      return 0;
                    }
                    if (param_2 == 0x20) {
                      if (((ulonglong)param_4 & 0xffff) == 1) {
                        LVar7 = GetWindowLongPtrA(param_1,-0x15);
                        if (LVar7 == 0) {
                          return 1;
                        }
                        lpCursorName = *(LPCSTR *)(_DAT_7ff753fe08d8 + 0x458);
                        hInstance = pHVar10;
                        if (lpCursorName + -0x66 < (LPCSTR)0x6) {
                          hInstance = *(HINSTANCE *)(LVar7 + 0x80);
                        }
                        puVar12 = (undefined8 *)(LVar7 + 0x140);
                        do {
                          if ((LPCSTR)*puVar12 == lpCursorName) {
                            hCursor = *(HCURSOR *)(LVar7 + 0x148 + (longlong)(int)pHVar10 * 0x10);
                            if (hCursor != (HCURSOR)0x0) goto LAB_7ff74fc88939;
                            break;
                          }
                          uVar13 = (int)pHVar10 + 1;
                          pHVar10 = (HINSTANCE)(ulonglong)uVar13;
                          puVar12 = puVar12 + 2;
                        } while (uVar13 < 5);
                        hCursor = LoadCursorA(hInstance,lpCursorName);
                        if (hCursor == (HCURSOR)0x0) {
                          return 1;
                        }
LAB_7ff74fc88939:
                        SetCursor(hCursor);
                        return 1;
                      }
                    }
                    else if (param_2 == 0x219) {
                      FUN_7ff74fc88b4c(param_3,param_4);
                    }
                  }
                }
                goto LAB_7ff74fc88957;
              }
              if (_DAT_7ff7539dc560 != 0) {
                LVar11 = FUN_7ff751da9378();
                return LVar11;
              }
              _DAT_7ff7539cb020 = (LONG)_DAT_7ff7539cb020;
              _DAT_7ff7539cb024 = SUB84(_DAT_7ff7539cb020,4);
              return 0;
            }
            LVar7 = GetWindowLongPtrA(param_1,-0x15);
            if (LVar7 != 0) {
              if (param_3 == 0x5b || param_3 == 0x5c) {
                *(undefined1 *)(LVar7 + 0x13a) = 0;
              }
              if (param_3 == 0xa0 || param_3 == 0xa1) {
                *(undefined1 *)(LVar7 + 0x13b) = 0;
              }
            }
          }
        }
      }
      goto LAB_7ff74fc88957;
    }
    if ((_DAT_7ff7539caf30 == 0) && (BVar5 = GetWindowRect(param_1,&local_c8), BVar5 != 0)) {
      FUN_7ff7513ee578(local_c8._0_8_ & 0xffffffff,local_c8.top);
      FUN_7ff7513f56a8();
    }
    _DAT_7ff7539cbcc4 = FUN_7ff7503cb718();
  }
  FUN_7ff7503ca838();
LAB_7ff74fc88957:
  LVar11 = DefWindowProcA(param_1,param_2,param_3,(LPARAM)param_4);
  return LVar11;
}
```

## 0x28a6198 — FOV

- containing function: **0x28a6144** (92 bytes, `FUN_7ff751cb6144`)
- callers (1 level; DATA = vtable/function-pointer table slot):

```
0x5024e6c	DATA	-
0x28a7081	UNCONDITIONAL_CALL	FUN_7ff751cb7070@0x28a7070
0x28a6074	UNCONDITIONAL_CALL	FUN_7ff751cb6048@0x28a6048
```

Disassembly around the anchor (`=>` marks it):

```
   0x28a616a  75 08                          JNZ 0x7ff751cb6174 ; CONDITIONAL_JUMP 0x28a6174 LAB_7ff751cb6174
   0x28a616c  f3 0f 10 25 4c e2 35 01        MOVSS XMM4,dword ptr [0x7ff7530143c0] ; READ 0x3c043c0 DAT_7ff7530143c0
   0x28a6174  8b 05 a6 4e d1 01              MOV EAX,dword ptr [0x7ff7539cb020] ; READ 0x45bb020 DAT_7ff7539cb020
   0x28a617a  0f 57 d2                       XORPS XMM2,XMM2
   0x28a617d  0f 57 c9                       XORPS XMM1,XMM1
   0x28a6180  f3 48 0f 2a d0                 CVTSI2SS XMM2,RAX
   0x28a6185  8b 05 99 4e d1 01              MOV EAX,dword ptr [0x7ff7539cb024] ; READ 0x45bb024 DAT_7ff7539cb024
   0x28a618b  f3 48 0f 2a c8                 CVTSI2SS XMM1,RAX
   0x28a6190  f3 0f 5e d1                    DIVSS XMM2,XMM1
   0x28a6194  f3 0f 5e e2                    DIVSS XMM4,XMM2
=> 0x28a6198  0f 28 c4                       MOVAPS XMM0,XMM4
   0x28a619b  48 83 c4 28                    ADD RSP,0x28
   0x28a619f  c3                             RET
   0x28a61a0  48 83 ec 28                    SUB RSP,0x28
   0x28a61a4  f3 0f 10 25 b8 e3 32 01        MOVSS XMM4,dword ptr [0x7ff752fe4564] ; READ 0x3bd4564 DAT_7ff752fe4564
   0x28a61ac  e8 57 00 00 00                 CALL 0x7ff751cb6208 ; UNCONDITIONAL_CALL 0x28a6208 FUN_7ff751cb6208
   0x28a61b1  f3 0f 10 2d c3 e1 35 01        MOVSS XMM5,dword ptr [0x7ff75301437c] ; READ 0x3c0437c DAT_7ff75301437c
   0x28a61b9  83 f8 02                       CMP EAX,0x2
   0x28a61bc  75 03                          JNZ 0x7ff751cb61c1 ; CONDITIONAL_JUMP 0x28a61c1 LAB_7ff751cb61c1
   0x28a61be  0f 28 e5                       MOVAPS XMM4,XMM5
   0x28a61c1  e8 42 00 00 00                 CALL 0x7ff751cb6208 ; UNCONDITIONAL_CALL 0x28a6208 FUN_7ff751cb6208
```

Decompilation:

```c
// function FUN_7ff751cb6144 @ 0x28a6144  body=92 bytes  pdataStart=0x28a6144
// callers(2): FUN_7ff751cb6048@0x28a6048 FUN_7ff751cb7070@0x28a7070 
// callees(1): FUN_7ff751cb6208@0x28a6208 

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined8 FUN_7ff751cb6144(void)

{
  int iVar1;
  float fVar2;
  undefined4 uVar3;
  
  fVar2 = 1.7777778;
  uVar3 = 0;
  iVar1 = FUN_7ff751cb6208();
  if (iVar1 == 2) {
    fVar2 = 2.3703704;
    uVar3 = 0;
  }
  iVar1 = FUN_7ff751cb6208();
  if (iVar1 == 3) {
    fVar2 = 3.5555556;
    uVar3 = 0;
  }
  return CONCAT44(uVar3,fVar2 / ((float)_DAT_7ff7539cb020 / (float)_DAT_7ff7539cb024));
}
```
