# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_14014e644 @ 14014e644

### Immediates (>=0x10000)

- 0x1465a3f
- 0x1465b4b
- 0x14669e9
- 0x1887d73
- 0x1887d74
- 0xc0000001
- 0xc00000bb
- 0xffffffff
- 0x14021e290
- 0x1402387a0
- 0x140246148
- 0x1402507e5

### Disassembly

```asm
14014e644: MOV RAX,RSP
14014e647: MOV qword ptr [RAX + 0x8],RBX
14014e64b: MOV qword ptr [RAX + 0x10],RSI
14014e64f: MOV qword ptr [RAX + 0x18],RDI
14014e653: PUSH RBP
14014e654: PUSH R12
14014e656: PUSH R13
14014e658: PUSH R14
14014e65a: PUSH R15
14014e65c: LEA RBP,[RAX + -0x2f]
14014e660: SUB RSP,0xb0
14014e667: MOVZX EBX,DL
14014e66a: MOV RDI,RCX
14014e66d: XOR EDX,EDX
14014e66f: MOVZX R15D,R8B
14014e673: LEA RCX,[RBP + -0x49]
14014e677: MOVZX R14D,R9B
14014e67b: LEA R8D,[RDX + 0x48]
14014e67f: CALL 0x14020d8c0
14014e684: XOR ESI,ESI
14014e686: MOV RCX,qword ptr [0x140246148]
14014e68d: LEA R13,[0x140246148]
14014e694: LEA R11,[0x1402387a0]
14014e69b: LEA R12D,[RSI + 0x1]
14014e69f: CMP RCX,R13
14014e6a2: JZ 0x14014e6cf
14014e6a4: TEST byte ptr [RCX + 0x158],R12B
14014e6ab: JZ 0x14014e6cf
14014e6ad: CMP byte ptr [RCX + 0x155],0x5
14014e6b4: JC 0x14014e6cf
14014e6b6: MOV RCX,qword ptr [RCX + 0x144]
14014e6bd: LEA EDX,[RSI + 0xc]
14014e6c0: MOV R8,R11
14014e6c3: CALL 0x1400015d8
14014e6c8: LEA R11,[0x1402387a0]
14014e6cf: TEST RDI,RDI
14014e6d2: JNZ 0x14014e70a
14014e6d4: MOV RCX,qword ptr [0x140246148]
14014e6db: CMP RCX,R13
14014e6de: JZ 0x14014eae6
14014e6e4: TEST byte ptr [RCX + 0x158],R12B
14014e6eb: JZ 0x14014eae6
14014e6f1: CMP byte ptr [RCX + 0x155],0x2
14014e6f8: JC 0x14014eae6
14014e6fe: MOV RCX,qword ptr [RCX + 0x144]
14014e705: LEA EDX,[RDI + 0xd]
14014e708: JMP 0x14014e747
14014e70a: MOV RCX,qword ptr [RDI]
14014e70d: TEST RCX,RCX
14014e710: JZ 0x14014e75b
14014e712: CMP dword ptr [RCX + 0x1465b4b],ESI
14014e718: JLE 0x14014e75b
14014e71a: MOV RCX,qword ptr [0x140246148]
14014e721: CMP RCX,R13
14014e724: JZ 0x14014eae6
14014e72a: TEST byte ptr [RCX + 0x2c],R12B
14014e72e: JZ 0x14014eae6
14014e734: CMP byte ptr [RCX + 0x29],0x2
14014e738: JC 0x14014eae6
14014e73e: MOV RCX,qword ptr [RCX + 0x18]
14014e742: MOV EDX,0xe
14014e747: MOV R8,R11
14014e74a: LEA R9,[0x14021e290]
14014e751: CALL 0x140001600
14014e756: JMP 0x14014eae6
14014e75b: MOVZX EDX,byte ptr [RCX + 0x1887d73]
14014e762: TEST DL,DL
14014e764: JNZ 0x14014e778
14014e766: CMP byte ptr [RCX + 0x1887d74],SIL
14014e76d: JNZ 0x14014e778
14014e76f: CMP dword ptr [RCX + 0x1465a3f],R12D
14014e776: JNZ 0x14014e7e3
14014e778: CMP byte ptr [RCX + 0x14669e9],SIL
14014e77f: JZ 0x14014ea91
14014e785: CMP BL,0xc0
14014e788: JNZ 0x14014ea91
14014e78e: MOV R10,qword ptr [0x140246148]
14014e795: CMP R10,R13
14014e798: JZ 0x14014e7e3
14014e79a: TEST byte ptr [R10 + 0xa4],R12B
14014e7a1: JZ 0x14014e7e3
14014e7a3: CMP byte ptr [R10 + 0xa1],0x2
14014e7ab: JC 0x14014e7e3
14014e7ad: MOVZX R8D,byte ptr [RCX + 0x1887d74]
14014e7b5: LEA R9,[0x14021e290]
14014e7bc: MOV RCX,qword ptr [R10 + 0x90]
14014e7c3: MOV EAX,EDX
14014e7c5: MOV dword ptr [RSP + 0x30],EAX
14014e7c9: MOV EDX,0xf
14014e7ce: MOV dword ptr [RSP + 0x28],R8D
14014e7d3: MOV R8,R11
14014e7d6: MOV dword ptr [RSP + 0x20],0x1ba
14014e7de: CALL 0x140007d48
14014e7e3: MOV DL,R14B
14014e7e6: MOV CL,BL
14014e7e8: CALL 0x14014f720
14014e7ed: MOV R8D,EAX
14014e7f0: CMP EAX,-0x1
14014e7f3: JZ 0x14014ea45
14014e7f9: MOV DL,byte ptr [RBP + 0x5f]
14014e7fc: MOV AL,byte ptr [RBP + 0x57]
14014e7ff: MOV byte ptr [RBP + -0x47],AL
14014e802: MOV RAX,qword ptr [RBP + 0x77]
14014e806: MOV qword ptr [RBP + -0x41],RAX
14014e80a: LEA ECX,[RDX + -0x5]
14014e80d: MOVZX EAX,word ptr [RBP + 0x6f]
14014e811: TEST CL,0xf7
14014e814: MOV dword ptr [RBP + -0x39],EAX
14014e817: MOV RAX,qword ptr [RBP + 0x67]
14014e81b: MOV qword ptr [RBP + -0x31],RAX
14014e81f: MOV RAX,qword ptr [RBP + 0x7f]
14014e823: MOV qword ptr [RBP + -0x29],RAX
14014e827: MOV EAX,dword ptr [RBP + 0x87]
14014e82d: MOV dword ptr [RBP + -0x21],EAX
14014e830: LEA RAX,[RBP + -0x19]
14014e834: MOV qword ptr [RBP + -0x19],RAX
14014e838: LEA RAX,[RBP + -0x19]
14014e83c: MOV qword ptr [RBP + -0x11],RAX
14014e840: LEA RAX,[0x1402507e5]
14014e847: MOVZX ECX,SIL
14014e84b: CMOVZ ECX,R12D
14014e84f: MOV byte ptr [RBP + -0x45],DL
14014e852: MOV byte ptr [RBP + -0x46],CL
14014e855: LEA RDX,[RBP + -0x49]
14014e859: IMUL RCX,R8,0xd
14014e85d: MOV byte ptr [RBP + -0x49],BL
14014e860: MOV byte ptr [RBP + -0x48],R14B
14014e864: MOV dword ptr [RBP + -0x9],ESI
14014e867: MOV RAX,qword ptr [RCX + RAX*0x1]
14014e86b: MOV RCX,RDI
14014e86e: CALL qword ptr [0x14022a3f8]
14014e874: MOV ESI,EAX
14014e876: TEST EAX,EAX
14014e878: JZ 0x14014e8df
14014e87a: MOV R10,qword ptr [0x140246148]
14014e881: CMP R10,R13
14014e884: JZ 0x14014ea3e
14014e88a: TEST byte ptr [R10 + 0x158],R12B
14014e891: JZ 0x14014ea05
14014e897: CMP byte ptr [R10 + 0x155],0x2
14014e89f: JC 0x14014ea05
14014e8a5: MOV RCX,qword ptr [R10 + 0x144]
14014e8ac: LEA R9,[0x14021e290]
14014e8b3: MOV dword ptr [RSP + 0x38],R15D
14014e8b8: LEA R8,[0x1402387a0]
14014e8bf: MOV dword ptr [RSP + 0x30],R14D
14014e8c4: MOV EDX,0x11
14014e8c9: MOV dword ptr [RSP + 0x28],EBX
14014e8cd: MOV dword ptr [RSP + 0x20],0x1e3
14014e8d5: CALL 0x140007bb4
14014e8da: JMP 0x14014ea05
14014e8df: MOV RCX,qword ptr [RBP + -0x19]
14014e8e3: LEA RAX,[RBP + -0x19]
14014e8e7: MOV RBX,RCX
14014e8ea: MOV R14,qword ptr [RCX]
14014e8ed: CMP RCX,RAX
14014e8f0: JZ 0x14014e9bd
14014e8f6: MOV RCX,qword ptr [0x140246148]
14014e8fd: CMP RCX,R13
14014e900: JZ 0x14014e958
14014e902: TEST byte ptr [RCX + 0x158],R12B
14014e909: JZ 0x14014e958
14014e90b: CMP byte ptr [RCX + 0x155],0x3
14014e912: JC 0x14014e958
14014e914: MOVZX R8D,byte ptr [RBP + -0x46]
14014e919: MOV EDX,0x12
14014e91e: MOVZX R9D,byte ptr [RBP + -0x47]
14014e923: MOV EAX,dword ptr [RBX + 0x14]
14014e926: MOVZX R10D,byte ptr [RBX + 0x10]
14014e92b: MOV RCX,qword ptr [RCX + 0x144]
14014e932: MOV dword ptr [RSP + 0x38],EAX
14014e936: MOV dword ptr [RSP + 0x30],R8D
14014e93b: LEA R8,[0x1402387a0]
14014e942: MOV dword ptr [RSP + 0x28],R9D
14014e947: LEA R9,[0x14021e290]
14014e94e: MOV dword ptr [RSP + 0x20],R10D
14014e953: CALL 0x140007bb4
14014e958: MOVZX EAX,word ptr [RBX + 0x20]
14014e95c: MOV RCX,RDI
14014e95f: MOV R9B,byte ptr [RBP + -0x46]
14014e963: MOV R8B,byte ptr [RBP + -0x47]
14014e967: MOV DL,byte ptr [RBX + 0x10]
14014e96a: MOV word ptr [RSP + 0x50],AX
14014e96f: MOV EAX,dword ptr [RBP + -0x21]
14014e972: MOV dword ptr [RSP + 0x48],EAX
14014e976: MOV RAX,qword ptr [RBP + -0x29]
14014e97a: MOV qword ptr [RSP + 0x40],RAX
14014e97f: MOV RAX,qword ptr [RBX + 0x18]
14014e983: MOV qword ptr [RSP + 0x38],RAX
14014e988: MOV EAX,dword ptr [RBX + 0x14]
14014e98b: MOV dword ptr [RSP + 0x30],EAX
14014e98f: MOV RAX,qword ptr [RBP + -0x41]
14014e993: MOV qword ptr [RSP + 0x28],RAX
14014e998: MOV AL,byte ptr [RBP + -0x45]
14014e99b: MOV byte ptr [RSP + 0x20],AL
14014e99f: CALL 0x14014eb0c
14014e9a4: MOV RBX,R14
14014e9a7: MOV ESI,EAX
14014e9a9: MOV R14,qword ptr [R14]
14014e9ac: LEA RAX,[RBP + -0x19]
14014e9b0: CMP RBX,RAX
14014e9b3: JNZ 0x14014e8f6
14014e9b9: MOV RCX,qword ptr [RBP + -0x19]
14014e9bd: TEST RBX,RBX
14014e9c0: JZ 0x14014ea05
14014e9c2: MOV RBX,qword ptr [RCX]
14014e9c5: LEA RAX,[RBP + -0x19]
14014e9c9: CMP RCX,RAX
14014e9cc: JZ 0x14014ea05
14014e9ce: MOV RDX,RBX
14014e9d1: MOV RAX,qword ptr [RCX + 0x8]
14014e9d5: MOV qword ptr [RDX + 0x8],RAX
14014e9d9: MOV qword ptr [RAX],RDX
14014e9dc: MOV RDX,RCX
14014e9df: AND qword ptr [RCX],0x0
14014e9e3: AND qword ptr [RCX + 0x8],0x0
14014e9e8: MOV RCX,RDI
14014e9eb: DEC dword ptr [RBP + -0x9]
14014e9ee: CALL 0x1401502ac
14014e9f3: MOV RDX,qword ptr [RBX]
14014e9f6: LEA RAX,[RBP + -0x19]
14014e9fa: MOV RCX,RBX
14014e9fd: MOV RBX,RDX
14014ea00: CMP RCX,RAX
14014ea03: JNZ 0x14014e9d1
14014ea05: MOV RCX,qword ptr [0x140246148]
14014ea0c: CMP RCX,R13
14014ea0f: JZ 0x14014ea3e
14014ea11: TEST byte ptr [RCX + 0x158],R12B
14014ea18: JZ 0x14014ea3e
14014ea1a: CMP byte ptr [RCX + 0x155],0x5
14014ea21: JC 0x14014ea3e
14014ea23: MOV RCX,qword ptr [RCX + 0x144]
14014ea2a: LEA R8,[0x1402387a0]
14014ea31: MOV EDX,0x14
14014ea36: MOV R9D,ESI
14014ea39: CALL 0x14000764c
14014ea3e: MOV EAX,ESI
14014ea40: JMP 0x14014eaeb
14014ea45: MOV RCX,qword ptr [0x140246148]
14014ea4c: CMP RCX,R13
14014ea4f: JZ 0x14014ea8a
14014ea51: TEST byte ptr [RCX + 0x158],R12B
14014ea58: JZ 0x14014ea8a
14014ea5a: CMP byte ptr [RCX + 0x155],R12B
14014ea61: JC 0x14014ea8a
14014ea63: MOV RCX,qword ptr [RCX + 0x144]
14014ea6a: LEA R9,[0x14021e290]
14014ea71: MOV EDX,0x13
14014ea76: MOV dword ptr [RSP + 0x20],0x1f8
14014ea7e: LEA R8,[0x1402387a0]
14014ea85: CALL 0x140001664
14014ea8a: MOV EAX,0xc00000bb
14014ea8f: JMP 0x14014eaeb
14014ea91: MOV R10,qword ptr [0x140246148]
14014ea98: CMP R10,R13
14014ea9b: JZ 0x14014eae6
14014ea9d: TEST byte ptr [R10 + 0xa4],R12B
14014eaa4: JZ 0x14014eae6
14014eaa6: CMP byte ptr [R10 + 0xa1],0x2
14014eaae: JC 0x14014eae6
14014eab0: MOVZX R8D,byte ptr [RCX + 0x1887d74]
14014eab8: LEA R9,[0x14021e290]
14014eabf: MOV RCX,qword ptr [R10 + 0x90]
14014eac6: MOV EAX,EDX
14014eac8: MOV dword ptr [RSP + 0x30],EAX
14014eacc: MOV EDX,0x10
14014ead1: MOV dword ptr [RSP + 0x28],R8D
14014ead6: MOV R8,R11
14014ead9: MOV dword ptr [RSP + 0x20],0x1be
14014eae1: CALL 0x140007d48
14014eae6: MOV EAX,0xc0000001
14014eaeb: LEA R11,[RSP + 0xb0]
14014eaf3: MOV RBX,qword ptr [R11 + 0x30]
14014eaf7: MOV RSI,qword ptr [R11 + 0x38]
14014eafb: MOV RDI,qword ptr [R11 + 0x40]
14014eaff: MOV RSP,R11
14014eb02: POP R15
14014eb04: POP R14
14014eb06: POP R13
14014eb08: POP R12
14014eb0a: POP RBP
14014eb0b: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */
/* WARNING: Type propagation algorithm not settling */

int FUN_14014e644(longlong *param_1,byte param_2,undefined1 param_3,undefined1 param_4,
                 undefined1 param_5,char param_6,undefined8 param_7,ushort param_8,
                 undefined8 param_9,undefined8 param_10,undefined4 param_11)

{
  char cVar1;
  longlong lVar2;
  undefined8 ******ppppppuVar3;
  undefined8 *******pppppppuVar4;
  uint uVar5;
  int iVar6;
  undefined8 uVar7;
  undefined8 *******pppppppuVar8;
  undefined4 uVar9;
  undefined8 *******pppppppuVar10;
  bool bVar11;
  byte local_78;
  undefined1 local_77;
  undefined1 local_76;
  undefined1 local_75;
  char local_74;
  undefined8 local_70;
  uint local_68;
  undefined8 local_60;
  undefined8 local_58;
  undefined4 local_50;
  undefined8 *******local_48;
  undefined8 *******local_40;
  int local_38;
  
  FUN_14020d8c0(&local_78,0,0x48);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0x155])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0xc,&DAT_1402387a0);
  }
  if (param_1 == (longlong *)0x0) {
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return -0x3fffffff;
    }
    if ((PTR_LOOP_140246148[0x158] & 1) == 0) {
      return -0x3fffffff;
    }
    if ((byte)PTR_LOOP_140246148[0x155] < 2) {
      return -0x3fffffff;
    }
    uVar7 = *(undefined8 *)(PTR_LOOP_140246148 + 0x144);
    uVar9 = 0xd;
  }
  else {
    lVar2 = *param_1;
    if ((lVar2 == 0) || (*(int *)(lVar2 + 0x1465b4b) < 1)) {
      cVar1 = *(char *)(lVar2 + 0x1887d73);
      if ((cVar1 != '\0') ||
         ((*(char *)(lVar2 + 0x1887d74) != '\0' || (*(int *)(lVar2 + 0x1465a3f) == 1)))) {
        if ((*(char *)(lVar2 + 0x14669e9) == '\0') || (param_2 != 0xc0)) {
          if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
            return -0x3fffffff;
          }
          if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
            return -0x3fffffff;
          }
          if ((byte)PTR_LOOP_140246148[0xa1] < 2) {
            return -0x3fffffff;
          }
          FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x10,&DAT_1402387a0,
                        "MtCmdSendSetQueryCmdHelper",0x1be,*(undefined1 *)(lVar2 + 0x1887d74),cVar1)
          ;
          return -0x3fffffff;
        }
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
          FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xf,&DAT_1402387a0,
                        "MtCmdSendSetQueryCmdHelper",0x1ba,*(undefined1 *)(lVar2 + 0x1887d74),cVar1)
          ;
        }
      }
      uVar5 = FUN_14014f720(param_2,param_4);
      if (uVar5 == 0xffffffff) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (PTR_LOOP_140246148[0x155] != '\0')) {
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x13,&DAT_1402387a0,
                        "MtCmdSendSetQueryCmdHelper",0x1f8);
        }
        return -0x3fffff45;
      }
      local_76 = param_5;
      local_70 = param_9;
      local_75 = (param_6 - 5U & 0xf7) == 0;
      local_68 = (uint)param_8;
      local_60 = param_7;
      local_58 = param_10;
      local_50 = param_11;
      local_48 = &local_48;
      local_40 = &local_48;
      local_74 = param_6;
      local_38 = 0;
      local_78 = param_2;
      local_77 = param_4;
      iVar6 = (**(code **)((longlong)&PTR_FUN_1402507e5 + (ulonglong)uVar5 * 0xd))
                        (param_1,&local_78);
      if (iVar6 == 0) {
        pppppppuVar10 = local_48;
        pppppppuVar8 = local_48;
        pppppppuVar4 = (undefined8 *******)*local_48;
        if ((undefined8 ********)local_48 != &local_48) {
          do {
            pppppppuVar8 = pppppppuVar4;
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x155])) {
              FUN_140007bb4(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x12,&DAT_1402387a0,
                            "MtCmdSendSetQueryCmdHelper",*(undefined1 *)(pppppppuVar10 + 2),local_76
                            ,local_75,*(undefined4 *)((longlong)pppppppuVar10 + 0x14));
            }
            iVar6 = FUN_14014eb0c(param_1,*(undefined1 *)(pppppppuVar10 + 2),local_76,local_75,
                                  local_74,local_70,*(undefined4 *)((longlong)pppppppuVar10 + 0x14),
                                  pppppppuVar10[3],local_58,local_50,
                                  *(undefined2 *)(pppppppuVar10 + 4));
            pppppppuVar10 = pppppppuVar8;
            pppppppuVar4 = (undefined8 *******)*pppppppuVar8;
          } while ((undefined8 ********)pppppppuVar8 != &local_48);
        }
        if ((pppppppuVar8 != (undefined8 *******)0x0) &&
           (pppppppuVar10 = (undefined8 *******)*local_48, pppppppuVar8 = local_48,
           (undefined8 ********)local_48 != &local_48)) {
          do {
            ppppppuVar3 = pppppppuVar8[1];
            pppppppuVar10[1] = ppppppuVar3;
            *ppppppuVar3 = pppppppuVar10;
            *pppppppuVar8 = (undefined8 ******)0x0;
            pppppppuVar8[1] = (undefined8 ******)0x0;
            local_38 = local_38 + -1;
            FUN_1401502ac(param_1,pppppppuVar8);
            bVar11 = (undefined8 ********)pppppppuVar10 != &local_48;
            pppppppuVar8 = pppppppuVar10;
            pppppppuVar10 = (undefined8 *******)*pppppppuVar10;
          } while (bVar11);
        }
      }
      else {
        if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
          return iVar6;
        }
        if (((PTR_LOOP_140246148[0x158] & 1) != 0) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
          FUN_140007bb4(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x11,&DAT_1402387a0,
                        "MtCmdSendSetQueryCmdHelper",0x1e3,param_2,param_4,param_3);
        }
      }
      if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
        if ((PTR_LOOP_140246148[0x158] & 1) == 0) {
          return iVar6;
        }
        if (4 < (byte)PTR_LOOP_140246148[0x155]) {
          FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x14,&DAT_1402387a0,iVar6);
          return iVar6;
        }
        return iVar6;
      }
      return iVar6;
    }
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return -0x3fffffff;
    }
    if ((PTR_LOOP_140246148[0x2c] & 1) == 0) {
      return -0x3fffffff;
    }
    if ((byte)PTR_LOOP_140246148[0x29] < 2) {
      return -0x3fffffff;
    }
    uVar7 = *(undefined8 *)(PTR_LOOP_140246148 + 0x18);
    uVar9 = 0xe;
  }
  WPP_SF_s(uVar7,uVar9,&DAT_1402387a0,"MtCmdSendSetQueryCmdHelper");
  return -0x3fffffff;
}


```

## FUN_1400cd2a8 @ 1400cd2a8

### Immediates (>=0x10000)

- 0x1464f07
- 0x14650c9
- 0x1465a24
- 0x1465a3f
- 0x1465ad8
- 0x1465b4b
- 0x14669e9
- 0x146cde9
- 0x1800000
- 0x18865d4
- 0x1887d73
- 0x1887d74
- 0x1ffffff
- 0x5030201
- 0xc0000001
- 0xc000009a
- 0xee111007
- 0xfe7fffff
- 0xfffcffff
- 0xffff3fff
- 0x140219b00
- 0x140219b90
- 0x140238608
- 0x140246148

### Disassembly

```asm
1400cd2a8: MOV R11,RSP
1400cd2ab: MOV qword ptr [R11 + 0x18],RBX
1400cd2af: MOV byte ptr [R11 + 0x20],R9B
1400cd2b3: MOV byte ptr [RSP + 0x10],DL
1400cd2b7: MOV qword ptr [R11 + 0x8],RCX
1400cd2bb: PUSH RBP
1400cd2bc: PUSH RSI
1400cd2bd: PUSH RDI
1400cd2be: PUSH R12
1400cd2c0: PUSH R13
1400cd2c2: PUSH R14
1400cd2c4: PUSH R15
1400cd2c6: SUB RSP,0x90
1400cd2cd: MOV dword ptr [RSP + 0x78],0x5030201
1400cd2d5: MOV DIL,R9B
1400cd2d8: MOV dword ptr [RSP + 0x7c],0xee111007
1400cd2e0: MOV AL,DL
1400cd2e2: MOV byte ptr [R11 + -0x48],0xef
1400cd2e7: MOV RBX,RCX
1400cd2ea: MOVZX R13D,R8B
1400cd2ee: MOV R15D,0x1
1400cd2f4: MOV RCX,qword ptr [0x140246148]
1400cd2fb: LEA R12,[0x140246148]
1400cd302: LEA R10,[0x140238608]
1400cd309: CMP RCX,R12
1400cd30c: JZ 0x1400cd341
1400cd30e: TEST byte ptr [RCX + 0xa4],R15B
1400cd315: JZ 0x1400cd33a
1400cd317: CMP byte ptr [RCX + 0xa1],0x5
1400cd31e: JC 0x1400cd33a
1400cd320: MOV RCX,qword ptr [RCX + 0x90]
1400cd327: LEA EDX,[R15 + 0x32]
1400cd32b: MOV R8,R10
1400cd32e: CALL 0x1400015d8
1400cd333: LEA R10,[0x140238608]
1400cd33a: MOV AL,byte ptr [RSP + 0xd8]
1400cd341: XOR R14D,R14D
1400cd344: TEST RBX,RBX
1400cd347: JNZ 0x1400cd399
1400cd349: MOV RCX,qword ptr [0x140246148]
1400cd350: CMP RCX,R12
1400cd353: JZ 0x1400cdc2b
1400cd359: TEST byte ptr [RCX + 0xa4],R15B
1400cd360: JZ 0x1400cdc2b
1400cd366: CMP byte ptr [RCX + 0xa1],0x2
1400cd36d: JC 0x1400cdc2b
1400cd373: MOV RCX,qword ptr [RCX + 0x90]
1400cd37a: LEA EDX,[RBX + 0x34]
1400cd37d: LEA R9,[0x140219b90]
1400cd384: MOV dword ptr [RSP + 0x20],0x49a
1400cd38c: MOV R8,R10
1400cd38f: CALL 0x140001664
1400cd394: JMP 0x1400cdc2b
1400cd399: MOV RSI,qword ptr [RBX]
1400cd39c: TEST RSI,RSI
1400cd39f: JZ 0x1400cd3eb
1400cd3a1: CMP dword ptr [RSI + 0x1465b4b],R14D
1400cd3a8: JLE 0x1400cd3eb
1400cd3aa: MOV RCX,qword ptr [0x140246148]
1400cd3b1: CMP RCX,R12
1400cd3b4: JZ 0x1400cdc2b
1400cd3ba: TEST byte ptr [RCX + 0x2c],R15B
1400cd3be: JZ 0x1400cdc2b
1400cd3c4: CMP byte ptr [RCX + 0x29],0x2
1400cd3c8: JC 0x1400cdc2b
1400cd3ce: MOV RCX,qword ptr [RCX + 0x18]
1400cd3d2: LEA R9,[0x140219b90]
1400cd3d9: MOV EDX,0x35
1400cd3de: MOV R8,R10
1400cd3e1: CALL 0x140001600
1400cd3e6: JMP 0x1400cdc2b
1400cd3eb: MOVZX EDX,byte ptr [RSI + 0x1887d73]
1400cd3f2: LEA RBP,[0x140219b90]
1400cd3f9: TEST DL,DL
1400cd3fb: JNZ 0x1400cd40f
1400cd3fd: CMP byte ptr [RSI + 0x1887d74],R14B
1400cd404: JNZ 0x1400cd40f
1400cd406: CMP dword ptr [RSI + 0x1465a3f],R15D
1400cd40d: JNZ 0x1400cd474
1400cd40f: CMP byte ptr [RSI + 0x14669e9],R14B
1400cd416: JZ 0x1400cdbd7
1400cd41c: CMP AL,0xc0
1400cd41e: JNZ 0x1400cdbd7
1400cd424: MOV RCX,qword ptr [0x140246148]
1400cd42b: CMP RCX,R12
1400cd42e: JZ 0x1400cd474
1400cd430: TEST byte ptr [RCX + 0xa4],R15B
1400cd437: JZ 0x1400cd474
1400cd439: CMP byte ptr [RCX + 0xa1],0x2
1400cd440: JC 0x1400cd474
1400cd442: MOVZX R8D,byte ptr [RSI + 0x1887d74]
1400cd44a: MOV EAX,EDX
1400cd44c: MOV RCX,qword ptr [RCX + 0x90]
1400cd453: MOV EDX,0x36
1400cd458: MOV dword ptr [RSP + 0x30],EAX
1400cd45c: MOV R9,RBP
1400cd45f: MOV dword ptr [RSP + 0x28],R8D
1400cd464: MOV R8,R10
1400cd467: MOV dword ptr [RSP + 0x20],0x4ab
1400cd46f: CALL 0x140007d48
1400cd474: CMP byte ptr [RSI + 0x146cde9],R14B
1400cd47b: JNZ 0x1400cd54d
1400cd481: CMP dword ptr [RSI + 0x1465ad8],0x2
1400cd488: JZ 0x1400cd497
1400cd48a: CMP byte ptr [0x14025c6b1],R14B
1400cd491: JNZ 0x1400cd54d
1400cd497: MOV CL,R14B
1400cd49a: MOVZX EAX,CL
1400cd49d: CMP byte ptr [RSP + RAX*0x1 + 0x78],R13B
1400cd4a2: JZ 0x1400cd514
1400cd4a4: ADD CL,R15B
1400cd4a7: CMP CL,0x9
1400cd4aa: JC 0x1400cd49a
1400cd4ac: MOV R8D,dword ptr [RSI + 0x18865d4]
1400cd4b3: ADD R8D,R15D
1400cd4b6: MOV dword ptr [RSI + 0x18865d4],R8D
1400cd4bd: MOV RCX,qword ptr [0x140246148]
1400cd4c4: CMP RCX,R12
1400cd4c7: JZ 0x1400cdc2b
1400cd4cd: TEST byte ptr [RCX + 0xa4],R15B
1400cd4d4: JZ 0x1400cdc2b
1400cd4da: CMP byte ptr [RCX + 0xa1],0x2
1400cd4e1: JC 0x1400cdc2b
1400cd4e7: MOV RCX,qword ptr [RCX + 0x90]
1400cd4ee: MOV EDX,0x39
1400cd4f3: MOV dword ptr [RSP + 0x28],R8D
1400cd4f8: MOV R9,RBP
1400cd4fb: LEA R8,[0x140238608]
1400cd502: MOV dword ptr [RSP + 0x20],0x4cf
1400cd50a: CALL 0x140007b24
1400cd50f: JMP 0x1400cdc2b
1400cd514: MOV RCX,qword ptr [0x140246148]
1400cd51b: CMP RCX,R12
1400cd51e: JZ 0x1400cd54d
1400cd520: TEST byte ptr [RCX + 0xa4],R15B
1400cd527: JZ 0x1400cd54d
1400cd529: CMP byte ptr [RCX + 0xa1],0x3
1400cd530: JC 0x1400cd54d
1400cd532: MOV RCX,qword ptr [RCX + 0x90]
1400cd539: LEA R8,[0x140238608]
1400cd540: MOV EDX,0x38
1400cd545: MOV R9,RBP
1400cd548: CALL 0x140001600
1400cd54d: MOV RAX,qword ptr [RSI + 0x1464f07]
1400cd554: MOV EDX,0x738
1400cd559: MOV R8D,0x7927
1400cd55f: MOV ECX,R14D
1400cd562: MOV dword ptr [RSP + 0x70],ECX
1400cd566: MOV R10D,0x6639
1400cd56c: LEA R11D,[RDX + -0x21]
1400cd570: LEA R9D,[R8 + -0x2]
1400cd574: TEST RAX,RAX
1400cd577: JZ 0x1400cd58b
1400cd579: MOV RCX,RSI
1400cd57c: CALL qword ptr [0x14022a3f8]
1400cd582: LEA RBX,[RSI + 0x1f72]
1400cd589: JMP 0x1400cd5ba
1400cd58b: LEA RBX,[RSI + 0x1f72]
1400cd592: MOVZX EAX,word ptr [RBX]
1400cd595: CMP AX,R10W
1400cd599: JZ 0x1400cd5b2
1400cd59b: CMP AX,DX
1400cd59e: JZ 0x1400cd5b2
1400cd5a0: CMP AX,R8W
1400cd5a4: JZ 0x1400cd5b2
1400cd5a6: CMP AX,R9W
1400cd5aa: JZ 0x1400cd5b2
1400cd5ac: CMP AX,R11W
1400cd5b0: JNZ 0x1400cd5c0
1400cd5b2: MOV RCX,RSI
1400cd5b5: CALL 0x1401cc290
1400cd5ba: MOV dword ptr [RSP + 0x70],EAX
1400cd5be: MOV ECX,EAX
1400cd5c0: MOVZX R15D,word ptr [RSP + 0x108]
1400cd5c9: LEA EDX,[R15 + RCX*0x1]
1400cd5cd: MOV RCX,RSI
1400cd5d0: CALL 0x1400c5ad8
1400cd5d5: XOR EDX,EDX
1400cd5d7: MOV R14,RAX
1400cd5da: TEST RAX,RAX
1400cd5dd: JNZ 0x1400cd62f
1400cd5df: MOV RCX,qword ptr [0x140246148]
1400cd5e6: LEA R12,[0x140246148]
1400cd5ed: CMP RCX,R12
1400cd5f0: JZ 0x1400cd625
1400cd5f2: TEST byte ptr [RCX + 0xa4],0x1
1400cd5f9: JZ 0x1400cd625
1400cd5fb: CMP byte ptr [RCX + 0xa1],0x1
1400cd602: JC 0x1400cd625
1400cd604: MOV RCX,qword ptr [RCX + 0x90]
1400cd60b: LEA EDX,[RAX + 0x3a]
1400cd60e: MOV R9,RBP
1400cd611: MOV dword ptr [RSP + 0x20],0x4de
1400cd619: LEA R8,[0x140238608]
1400cd620: CALL 0x140001664
1400cd625: MOV EDI,0xc000009a
1400cd62a: JMP 0x1400cdb9a
1400cd62f: CMP R13B,0xee
1400cd633: MOV word ptr [R14 + 0x8],DX
1400cd638: MOV EAX,0xc000
1400cd63d: MOV byte ptr [R14 + 0xa],R13B
1400cd641: MOV ECX,0x8000
1400cd646: MOV byte ptr [R14 + 0xb],DIL
1400cd64a: CMOVNZ AX,CX
1400cd64e: MOV dword ptr [R14 + 0xe],EDX
1400cd652: MOV word ptr [R14 + 0x3a],AX
1400cd657: MOV ECX,0x6639
1400cd65c: MOV AL,byte ptr [RSP + 0xf8]
1400cd663: MOV byte ptr [R14 + 0xc],AL
1400cd667: MOV RAX,qword ptr [RSP + 0x118]
1400cd66f: MOV qword ptr [R14 + 0x18],RAX
1400cd673: MOV RAX,qword ptr [RSP + 0x110]
1400cd67b: MOV qword ptr [R14 + 0x28],RAX
1400cd67f: MOV EAX,dword ptr [RSP + 0x120]
1400cd686: MOV dword ptr [R14 + 0x20],EAX
1400cd68a: MOV RAX,qword ptr [RSP + 0xd0]
1400cd692: MOV qword ptr [R14 + 0x80],RAX
1400cd699: MOV byte ptr [R14 + 0x38],DL
1400cd69d: MOVZX EAX,word ptr [RBX]
1400cd6a0: CMP AX,CX
1400cd6a3: JZ 0x1400cd6d1
1400cd6a5: MOV ECX,0x738
1400cd6aa: CMP AX,CX
1400cd6ad: JZ 0x1400cd6d1
1400cd6af: MOV ECX,0x7927
1400cd6b4: CMP AX,CX
1400cd6b7: JZ 0x1400cd6d1
1400cd6b9: MOV ECX,0x7925
1400cd6be: CMP AX,CX
1400cd6c1: JZ 0x1400cd6d1
1400cd6c3: MOV ECX,0x717
1400cd6c8: CMP AX,CX
1400cd6cb: JNZ 0x1400cdabe
1400cd6d1: MOV RDI,qword ptr [R14 + 0x68]
1400cd6d5: MOV dword ptr [R14 + 0x7c],0x1
1400cd6dd: CMP byte ptr [RSI + 0x146cde9],DL
1400cd6e3: JNZ 0x1400cd8fa
1400cd6e9: MOV RCX,qword ptr [0x140246148]
1400cd6f0: LEA RAX,[0x140246148]
1400cd6f7: CMP RCX,RAX
1400cd6fa: JZ 0x1400cd730
1400cd6fc: TEST byte ptr [RCX + 0xa4],0x1
1400cd703: JZ 0x1400cd730
1400cd705: CMP byte ptr [RCX + 0xa1],0x3
1400cd70c: JC 0x1400cd730
1400cd70e: MOV RCX,qword ptr [RCX + 0x90]
1400cd715: LEA R8,[0x140238608]
1400cd71c: MOV EDX,0x3b
1400cd721: MOV dword ptr [RSP + 0x20],R13D
1400cd726: MOV R9,RBP
1400cd729: CALL 0x140001664
1400cd72e: XOR EDX,EDX
1400cd730: MOV RBX,qword ptr [R14 + 0x68]
1400cd734: LEA EAX,[R15 + 0x40]
1400cd738: MOVZX ECX,AX
1400cd73b: MOV EAX,dword ptr [RDI + 0x4]
1400cd73e: AND EAX,0xffff3fff
1400cd743: MOV word ptr [RDI],DX
1400cd746: OR ECX,dword ptr [RDI]
1400cd748: BTS EAX,0xe
1400cd74c: AND ECX,0xfe7fffff
1400cd752: MOV dword ptr [RDI + 0x4],EAX
1400cd755: CMP byte ptr [RSP + 0xd8],DL
1400cd75c: JNZ 0x1400cd766
1400cd75e: OR ECX,0x1800000
1400cd764: JMP 0x1400cd76a
1400cd766: BTS ECX,0x18
1400cd76a: AND ECX,0x1ffffff
1400cd770: MOV R8B,0x1
1400cd773: BTS ECX,0x1e
1400cd777: XOR EDX,EDX
1400cd779: MOV dword ptr [RDI],ECX
1400cd77b: MOV RCX,RSI
1400cd77e: MOVZX EDI,byte ptr [RSP + 0xd8]
1400cd786: MOV byte ptr [RBX + 0x24],DIL
1400cd78a: MOV byte ptr [RBX + 0x25],0xa0
1400cd78e: CALL 0x14009a46c
1400cd793: MOV byte ptr [RBX + 0x27],AL
1400cd796: LEA EAX,[R15 + 0x20]
1400cd79a: MOV word ptr [RBX + 0x20],AX
1400cd79e: LEA EAX,[R13 + -0x10]
1400cd7a2: CMP AL,0x1
1400cd7a4: JA 0x1400cd7ee
1400cd7a6: MOV RCX,qword ptr [0x140246148]
1400cd7ad: LEA RAX,[0x140246148]
1400cd7b4: CMP RCX,RAX
1400cd7b7: JZ 0x1400cd7ea
1400cd7b9: TEST byte ptr [RCX + 0xa4],0x1
1400cd7c0: JZ 0x1400cd7ea
1400cd7c2: CMP byte ptr [RCX + 0xa1],0x3
1400cd7c9: JC 0x1400cd7ea
1400cd7cb: MOV RCX,qword ptr [RCX + 0x90]
1400cd7d2: LEA R8,[0x140238608]
1400cd7d9: MOV EDX,0x3c
1400cd7de: MOV dword ptr [RSP + 0x20],EDI
1400cd7e2: MOV R9,RBP
1400cd7e5: CALL 0x140001664
1400cd7ea: MOV byte ptr [RBX + 0x25],0xa0
1400cd7ee: CMP R13B,0xee
1400cd7f2: JNZ 0x1400cd847
1400cd7f4: MOV RCX,qword ptr [0x140246148]
1400cd7fb: LEA RAX,[0x140246148]
1400cd802: CMP RCX,RAX
1400cd805: JZ 0x1400cd838
1400cd807: TEST byte ptr [RCX + 0xa4],0x1
1400cd80e: JZ 0x1400cd838
1400cd810: CMP byte ptr [RCX + 0xa1],0x3
1400cd817: JC 0x1400cd838
1400cd819: MOV RCX,qword ptr [RCX + 0x90]
1400cd820: LEA R8,[0x140238608]
1400cd827: MOV EDX,0x3d
1400cd82c: MOV dword ptr [RSP + 0x20],EDI
1400cd830: MOV R9,RBP
1400cd833: CALL 0x140001664
1400cd838: XOR EAX,EAX
1400cd83a: MOV word ptr [RBX + 0x24],0xa000
1400cd840: MOV byte ptr [RBX + 0x27],AL
1400cd843: MOV dword ptr [R14 + 0x7c],EAX
1400cd847: MOV RDX,qword ptr [RSP + 0x100]
1400cd84f: TEST RDX,RDX
1400cd852: JZ 0x1400cd860
1400cd854: LEA RCX,[RBX + 0x40]
1400cd858: MOV R8D,R15D
1400cd85b: CALL 0x140010118
1400cd860: MOV RCX,qword ptr [0x140246148]
1400cd867: LEA R12,[0x140246148]
1400cd86e: CMP RCX,R12
1400cd871: JZ 0x1400cdac5
1400cd877: TEST byte ptr [RCX + 0xa4],0x1
1400cd87e: JZ 0x1400cdac5
1400cd884: CMP byte ptr [RCX + 0xa1],0x3
1400cd88b: JC 0x1400cdac5
1400cd891: MOV EAX,dword ptr [RSI + 0x1465a24]
1400cd897: LEA R8,[0x140238608]
1400cd89e: MOVZX R9D,byte ptr [RBX + 0x27]
1400cd8a3: MOV EDX,0x3e
1400cd8a8: MOVZX R10D,byte ptr [RBX + 0x2b]
1400cd8ad: MOVZX EBX,byte ptr [RSP + 0xf0]
1400cd8b5: MOV RCX,qword ptr [RCX + 0x90]
1400cd8bc: MOV dword ptr [RSP + 0x60],EAX
1400cd8c0: MOV EAX,dword ptr [R14 + 0x7c]
1400cd8c4: MOV dword ptr [RSP + 0x58],EAX
1400cd8c8: MOV RAX,qword ptr [R14 + 0x68]
1400cd8cc: MOV qword ptr [RSP + 0x50],RAX
1400cd8d1: MOV dword ptr [RSP + 0x48],R15D
1400cd8d6: MOV dword ptr [RSP + 0x40],R9D
1400cd8db: MOV R9,RBP
1400cd8de: MOV dword ptr [RSP + 0x38],R10D
1400cd8e3: MOV dword ptr [RSP + 0x30],EDI
1400cd8e7: MOV dword ptr [RSP + 0x28],EBX
1400cd8eb: MOV dword ptr [RSP + 0x20],R13D
1400cd8f0: CALL 0x1400d43d4
1400cd8f5: JMP 0x1400cdac5
1400cd8fa: MOV RCX,qword ptr [0x140246148]
1400cd901: LEA RAX,[0x140246148]
1400cd908: CMP RCX,RAX
1400cd90b: JZ 0x1400cd941
1400cd90d: TEST byte ptr [RCX + 0xa4],0x1
1400cd914: JZ 0x1400cd941
1400cd916: CMP byte ptr [RCX + 0xa1],0x3
1400cd91d: JC 0x1400cd941
1400cd91f: MOV RCX,qword ptr [RCX + 0x90]
1400cd926: LEA R8,[0x140238608]
1400cd92d: MOV EDX,0x3f
1400cd932: MOV dword ptr [RSP + 0x20],R13D
1400cd937: MOV R9,RBP
1400cd93a: CALL 0x140001664
1400cd93f: XOR EDX,EDX
1400cd941: MOV RBX,qword ptr [R14 + 0x68]
1400cd945: LEA EAX,[R15 + 0x30]
1400cd949: MOV R12B,byte ptr [RSP + 0xd8]
1400cd951: MOV R8B,0x1
1400cd954: MOV word ptr [RDI],DX
1400cd957: XOR EDX,EDX
1400cd959: MOVZX ECX,AX
1400cd95c: OR ECX,dword ptr [RDI]
1400cd95e: MOV EAX,dword ptr [RDI + 0x4]
1400cd961: AND ECX,0xfe7fffff
1400cd967: BTS ECX,0x18
1400cd96b: AND EAX,0xfffcffff
1400cd970: MOV dword ptr [RDI],ECX
1400cd972: BTS EAX,0x10
1400cd976: MOV dword ptr [RDI + 0x4],EAX
1400cd979: MOV RCX,RSI
1400cd97c: MOVZX EDI,byte ptr [RSP + 0xf0]
1400cd984: MOV byte ptr [RBX + 0x26],DIL
1400cd988: MOV byte ptr [RBX + 0x24],R12B
1400cd98c: MOV byte ptr [RBX + 0x25],0xa0
1400cd990: CALL 0x14009a46c
1400cd995: AND word ptr [RBX + 0x2a],0x0
1400cd99a: MOV byte ptr [RBX + 0x27],AL
1400cd99d: LEA EAX,[R15 + 0x10]
1400cd9a1: MOV word ptr [RBX + 0x20],AX
1400cd9a5: CMP R13B,0xed
1400cd9a9: JNZ 0x1400cda1f
1400cd9ab: MOV R8D,0x8000
1400cd9b1: MOV word ptr [RBX + 0x22],R8W
1400cd9b6: MOV RCX,qword ptr [0x140246148]
1400cd9bd: LEA RAX,[0x140246148]
1400cd9c4: CMP RCX,RAX
1400cd9c7: JZ 0x1400cda0b
1400cd9c9: TEST byte ptr [RCX + 0xa4],0x1
1400cd9d0: JZ 0x1400cda0b
1400cd9d2: CMP byte ptr [RCX + 0xa1],0x4
1400cd9d9: JC 0x1400cda0b
1400cd9db: MOVZX R12D,byte ptr [RSP + 0xe8]
1400cd9e4: MOV EDX,0x40
1400cd9e9: MOV RCX,qword ptr [RCX + 0x90]
1400cd9f0: MOV R9,RBP
1400cd9f3: MOV dword ptr [RSP + 0x28],R8D
1400cd9f8: LEA R8,[0x140238608]
1400cd9ff: MOV dword ptr [RSP + 0x20],R12D
1400cda04: CALL 0x140007b24
1400cda09: JMP 0x1400cda13
1400cda0b: MOV R12B,byte ptr [RSP + 0xe8]
1400cda13: MOV byte ptr [RBX + 0x29],R12B
1400cda17: MOV R12B,byte ptr [RSP + 0xd8]
1400cda1f: MOV RDX,qword ptr [RSP + 0x100]
1400cda27: TEST RDX,RDX
1400cda2a: JZ 0x1400cda38
1400cda2c: MOV R8D,R15D
1400cda2f: LEA RCX,[RBX + 0x40]
1400cda33: CALL 0x140010118
1400cda38: MOV RCX,qword ptr [0x140246148]
1400cda3f: LEA RAX,[0x140246148]
1400cda46: CMP RCX,RAX
1400cda49: JZ 0x1400cdabe
1400cda4b: TEST byte ptr [RCX + 0xa4],0x1
1400cda52: JZ 0x1400cdabe
1400cda54: CMP byte ptr [RCX + 0xa1],0x3
1400cda5b: JC 0x1400cdabe
1400cda5d: MOV EAX,dword ptr [RSI + 0x1465a24]
1400cda63: LEA R8,[0x140238608]
1400cda6a: MOVZX R9D,byte ptr [RBX + 0x27]
1400cda6f: MOV EDX,0x41
1400cda74: MOVZX R10D,byte ptr [RBX + 0x2b]
1400cda79: MOV RCX,qword ptr [RCX + 0x90]
1400cda80: MOV dword ptr [RSP + 0x60],EAX
1400cda84: MOV EAX,dword ptr [R14 + 0x7c]
1400cda88: MOV dword ptr [RSP + 0x58],EAX
1400cda8c: MOV RAX,qword ptr [R14 + 0x68]
1400cda90: MOV qword ptr [RSP + 0x50],RAX
1400cda95: MOV dword ptr [RSP + 0x48],R15D
1400cda9a: MOV dword ptr [RSP + 0x40],R9D
1400cda9f: MOV R9,RBP
1400cdaa2: MOV dword ptr [RSP + 0x38],R10D
1400cdaa7: MOVZX R11D,R12B
1400cdaab: MOV dword ptr [RSP + 0x30],R11D
1400cdab0: MOV dword ptr [RSP + 0x28],EDI
1400cdab4: MOV dword ptr [RSP + 0x20],R13D
1400cdab9: CALL 0x1400d43d4
1400cdabe: LEA R12,[0x140246148]
1400cdac5: MOV RDX,qword ptr [RSP + 0xd0]
1400cdacd: MOV R8,R14
1400cdad0: MOV RCX,RSI
1400cdad3: CALL 0x1400c8340
1400cdad8: MOV EDI,EAX
1400cdada: CMP EAX,0x103
1400cdadf: JNZ 0x1400cdae8
1400cdae1: XOR EDI,EDI
1400cdae3: JMP 0x1400cdb9a
1400cdae8: MOVZX EBX,byte ptr [RSI + 0x14650c9]
1400cdaef: XOR EBP,EBP
1400cdaf1: ADD BX,word ptr [RSP + 0x70]
1400cdaf6: ADD BX,R15W
1400cdafa: CMP byte ptr [R14],BPL
1400cdafd: JZ 0x1400cdb9a
1400cdb03: MOV byte ptr [R14],BPL
1400cdb06: MOV RCX,qword ptr [0x140246148]
1400cdb0d: CMP RCX,R12
1400cdb10: JZ 0x1400cdb57
1400cdb12: TEST byte ptr [RCX + 0xa4],0x1
1400cdb19: JZ 0x1400cdb57
1400cdb1b: CMP byte ptr [RCX + 0xa1],0x3
1400cdb22: JC 0x1400cdb57
1400cdb24: MOV RAX,qword ptr [R14 + 0x68]
1400cdb28: LEA EDX,[RBP + 0xe]
1400cdb2b: MOV RCX,qword ptr [RCX + 0x90]
1400cdb32: LEA R9,[0x140219b00]
1400cdb39: MOV qword ptr [RSP + 0x30],RAX
1400cdb3e: LEA R8,[0x140238608]
1400cdb45: MOV qword ptr [RSP + 0x28],R14
1400cdb4a: MOV dword ptr [RSP + 0x20],0x1a0
1400cdb52: CALL 0x140012c6c
1400cdb57: MOV EAX,0x100
1400cdb5c: CMP BX,AX
1400cdb5f: JNC 0x1400cdb71
1400cdb61: LFENCE
1400cdb64: MOV RDX,R14
1400cdb67: MOV RCX,RSI
1400cdb6a: CALL 0x14009b52c
1400cdb6f: JMP 0x1400cdb9a
1400cdb71: CMP qword ptr [R14 + 0x68],RBP
1400cdb75: JZ 0x1400cdb8a
1400cdb77: LFENCE
1400cdb7a: MOV RCX,qword ptr [R14 + 0x68]
1400cdb7e: MOVZX EDX,BX
1400cdb81: CALL 0x1400100e8
1400cdb86: MOV qword ptr [R14 + 0x68],RBP
1400cdb8a: LFENCE
1400cdb8d: MOV EDX,0x90
1400cdb92: MOV RCX,R14
1400cdb95: CALL 0x1400100e8
1400cdb9a: MOV RCX,qword ptr [0x140246148]
1400cdba1: CMP RCX,R12
1400cdba4: JZ 0x1400cdbd3
1400cdba6: TEST byte ptr [RCX + 0xa4],0x1
1400cdbad: JZ 0x1400cdbd3
1400cdbaf: CMP byte ptr [RCX + 0xa1],0x5
1400cdbb6: JC 0x1400cdbd3
1400cdbb8: MOV RCX,qword ptr [RCX + 0x90]
1400cdbbf: LEA R8,[0x140238608]
1400cdbc6: MOV EDX,0x42
1400cdbcb: MOV R9D,EDI
1400cdbce: CALL 0x14000764c
1400cdbd3: MOV EAX,EDI
1400cdbd5: JMP 0x1400cdc30
1400cdbd7: MOV RCX,qword ptr [0x140246148]
1400cdbde: CMP RCX,R12
1400cdbe1: JZ 0x1400cdc2b
1400cdbe3: TEST byte ptr [RCX + 0xa4],R15B
1400cdbea: JZ 0x1400cdc2b
1400cdbec: CMP byte ptr [RCX + 0xa1],0x2
1400cdbf3: JC 0x1400cdc2b
1400cdbf5: MOVZX R8D,byte ptr [RSI + 0x1887d74]
1400cdbfd: LEA R9,[0x140219b90]
1400cdc04: MOV RCX,qword ptr [RCX + 0x90]
1400cdc0b: MOV EAX,EDX
1400cdc0d: MOV dword ptr [RSP + 0x30],EAX
1400cdc11: MOV EDX,0x37
1400cdc16: MOV dword ptr [RSP + 0x28],R8D
1400cdc1b: MOV R8,R10
1400cdc1e: MOV dword ptr [RSP + 0x20],0x4af
1400cdc26: CALL 0x140007d48
1400cdc2b: MOV EAX,0xc0000001
1400cdc30: MOV RBX,qword ptr [RSP + 0xe0]
1400cdc38: ADD RSP,0x90
1400cdc3f: POP R15
1400cdc41: POP R14
1400cdc43: POP R13
1400cdc45: POP R12
1400cdc47: POP RDI
1400cdc48: POP RSI
1400cdc49: POP RBP
1400cdc4a: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

int FUN_1400cd2a8(longlong *param_1,byte param_2,char param_3,char param_4,byte param_5,char param_6
                 ,longlong param_7,ushort param_8,undefined8 param_9,undefined8 param_10,
                 undefined4 param_11)

{
  byte bVar1;
  short sVar2;
  longlong lVar3;
  uint *puVar4;
  longlong lVar5;
  undefined1 uVar6;
  undefined2 uVar7;
  char *pcVar8;
  byte bVar9;
  uint uVar10;
  int iVar11;
  undefined8 in_stack_ffffffffffffff60;
  undefined8 uVar12;
  undefined4 uVar13;
  undefined8 in_stack_ffffffffffffff68;
  undefined8 uVar14;
  undefined4 uVar15;
  short local_58;
  char local_50 [24];
  
  uVar15 = (undefined4)((ulonglong)in_stack_ffffffffffffff68 >> 0x20);
  uVar13 = (undefined4)((ulonglong)in_stack_ffffffffffffff60 >> 0x20);
  local_50[0] = '\x01';
  local_50[1] = '\x02';
  local_50[2] = '\x03';
  local_50[3] = '\x05';
  local_50[4] = '\a';
  local_50[5] = '\x10';
  local_50[6] = '\x11';
  local_50[7] = -0x12;
  local_50[8] = 0xef;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x33,&DAT_140238608);
  }
  if (param_1 == (longlong *)0x0) {
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x34,&DAT_140238608,
                    "MtCmdSendSetQueryCmdAdv",0x49a);
    }
  }
  else {
    lVar3 = *param_1;
    if ((lVar3 == 0) || (*(int *)(lVar3 + 0x1465b4b) < 1)) {
      bVar1 = *(byte *)(lVar3 + 0x1887d73);
      bVar9 = 0;
      if ((bVar1 != 0) ||
         ((*(char *)(lVar3 + 0x1887d74) != '\0' || (*(int *)(lVar3 + 0x1465a3f) == 1)))) {
        if ((*(char *)(lVar3 + 0x14669e9) == '\0') || (param_2 != 0xc0)) {
          if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
            return -0x3fffffff;
          }
          if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
            return -0x3fffffff;
          }
          if ((byte)PTR_LOOP_140246148[0xa1] < 2) {
            return -0x3fffffff;
          }
          FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x37,&DAT_140238608,
                        "MtCmdSendSetQueryCmdAdv",0x4af,
                        CONCAT44(uVar13,(uint)*(byte *)(lVar3 + 0x1887d74)),
                        CONCAT44(uVar15,(uint)bVar1));
          return -0x3fffffff;
        }
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
          uVar14 = CONCAT44(uVar15,(uint)bVar1);
          uVar12 = CONCAT44(uVar13,(uint)*(byte *)(lVar3 + 0x1887d74));
          FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x36,&DAT_140238608,
                        "MtCmdSendSetQueryCmdAdv",0x4ab,uVar12,uVar14);
          uVar15 = (undefined4)((ulonglong)uVar14 >> 0x20);
          uVar13 = (undefined4)((ulonglong)uVar12 >> 0x20);
        }
      }
      if ((*(char *)(lVar3 + 0x146cde9) != '\0') ||
         ((*(int *)(lVar3 + 0x1465ad8) != 2 && (DAT_14025c6b1 != '\0')))) {
LAB_1400cd54d:
        local_58 = 0;
        if (*(code **)(lVar3 + 0x1464f07) == (code *)0x0) {
          sVar2 = *(short *)(lVar3 + 0x1f72);
          if ((((sVar2 == 0x6639) || (sVar2 == 0x738)) || (sVar2 == 0x7927)) ||
             ((sVar2 == 0x7925 || (sVar2 == 0x717)))) {
            local_58 = FUN_1401cc290(lVar3);
          }
        }
        else {
          local_58 = (**(code **)(lVar3 + 0x1464f07))(lVar3);
        }
        pcVar8 = (char *)FUN_1400c5ad8(lVar3);
        if (pcVar8 == (char *)0x0) {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
            FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3a,&DAT_140238608,
                          "MtCmdSendSetQueryCmdAdv",0x4de);
          }
          iVar11 = -0x3fffff66;
        }
        else {
          pcVar8[8] = '\0';
          pcVar8[9] = '\0';
          uVar7 = 0xc000;
          pcVar8[10] = param_3;
          pcVar8[0xb] = param_4;
          if (param_3 != -0x12) {
            uVar7 = 0x8000;
          }
          pcVar8[0xe] = '\0';
          pcVar8[0xf] = '\0';
          pcVar8[0x10] = '\0';
          pcVar8[0x11] = '\0';
          *(undefined2 *)(pcVar8 + 0x3a) = uVar7;
          pcVar8[0xc] = param_6;
          *(undefined8 *)(pcVar8 + 0x18) = param_10;
          *(undefined8 *)(pcVar8 + 0x28) = param_9;
          *(undefined4 *)(pcVar8 + 0x20) = param_11;
          *(longlong **)(pcVar8 + 0x80) = param_1;
          pcVar8[0x38] = '\0';
          sVar2 = *(short *)(lVar3 + 0x1f72);
          if (((sVar2 == 0x6639) || (sVar2 == 0x738)) ||
             ((sVar2 == 0x7927 || ((sVar2 == 0x7925 || (sVar2 == 0x717)))))) {
            puVar4 = *(uint **)(pcVar8 + 0x68);
            pcVar8[0x7c] = '\x01';
            pcVar8[0x7d] = '\0';
            pcVar8[0x7e] = '\0';
            pcVar8[0x7f] = '\0';
            if (*(char *)(lVar3 + 0x146cde9) == '\0') {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3b,&DAT_140238608,
                              "MtCmdSendSetQueryCmdAdv",param_3);
              }
              lVar5 = *(longlong *)(pcVar8 + 0x68);
              *(undefined2 *)puVar4 = 0;
              uVar10 = param_8 + 0x40 & 0xffff | *puVar4 & 0xfe7fffff;
              puVar4[1] = puVar4[1] & 0xffff3fff | 0x4000;
              if (param_2 == 0) {
                uVar10 = uVar10 | 0x1800000;
              }
              else {
                uVar10 = uVar10 | 0x1000000;
              }
              *puVar4 = uVar10 & 0x1ffffff | 0x40000000;
              *(byte *)(lVar5 + 0x24) = param_2;
              *(undefined1 *)(lVar5 + 0x25) = 0xa0;
              uVar6 = FUN_14009a46c(lVar3,0,1);
              *(undefined1 *)(lVar5 + 0x27) = uVar6;
              *(ushort *)(lVar5 + 0x20) = param_8 + 0x20;
              uVar10 = (uint)param_2;
              if ((byte)(param_3 - 0x10U) < 2) {
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1]))
                {
                  FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3c,&DAT_140238608,
                                "MtCmdSendSetQueryCmdAdv",uVar10);
                }
                *(undefined1 *)(lVar5 + 0x25) = 0xa0;
              }
              if (param_3 == -0x12) {
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1]))
                {
                  FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3d,&DAT_140238608,
                                "MtCmdSendSetQueryCmdAdv",uVar10);
                }
                *(undefined2 *)(lVar5 + 0x24) = 0xa000;
                *(undefined1 *)(lVar5 + 0x27) = 0;
                pcVar8[0x7c] = '\0';
                pcVar8[0x7d] = '\0';
                pcVar8[0x7e] = '\0';
                pcVar8[0x7f] = '\0';
              }
              if (param_7 != 0) {
                FUN_140010118(lVar5 + 0x40,param_7,param_8);
              }
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_1400d43d4(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3e,&DAT_140238608,
                              "MtCmdSendSetQueryCmdAdv",param_3,CONCAT44(uVar13,(uint)param_5),
                              CONCAT44(uVar15,uVar10),*(undefined1 *)(lVar5 + 0x2b),
                              *(undefined1 *)(lVar5 + 0x27),param_8,*(undefined8 *)(pcVar8 + 0x68),
                              *(undefined4 *)(pcVar8 + 0x7c),*(undefined4 *)(lVar3 + 0x1465a24));
              }
            }
            else {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3f,&DAT_140238608,
                              "MtCmdSendSetQueryCmdAdv",param_3);
              }
              lVar5 = *(longlong *)(pcVar8 + 0x68);
              *(undefined2 *)puVar4 = 0;
              *puVar4 = param_8 + 0x30 & 0xffff | *puVar4 & 0xfe7fffff | 0x1000000;
              puVar4[1] = puVar4[1] & 0xfffcffff | 0x10000;
              *(byte *)(lVar5 + 0x26) = param_5;
              *(byte *)(lVar5 + 0x24) = param_2;
              *(undefined1 *)(lVar5 + 0x25) = 0xa0;
              uVar6 = FUN_14009a46c(lVar3,0,1);
              *(undefined2 *)(lVar5 + 0x2a) = 0;
              *(undefined1 *)(lVar5 + 0x27) = uVar6;
              *(ushort *)(lVar5 + 0x20) = param_8 + 0x10;
              if (param_3 == -0x13) {
                *(undefined2 *)(lVar5 + 0x22) = 0x8000;
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1]))
                {
                  uVar12 = CONCAT44(uVar13,0x8000);
                  FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x40,&DAT_140238608,
                                "MtCmdSendSetQueryCmdAdv",param_4,uVar12);
                  uVar13 = (undefined4)((ulonglong)uVar12 >> 0x20);
                }
                *(char *)(lVar5 + 0x29) = param_4;
              }
              if (param_7 != 0) {
                FUN_140010118(lVar5 + 0x40,param_7,param_8);
              }
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_1400d43d4(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x41,&DAT_140238608,
                              "MtCmdSendSetQueryCmdAdv",param_3,CONCAT44(uVar13,(uint)param_5),
                              CONCAT44(uVar15,(uint)param_2),*(undefined1 *)(lVar5 + 0x2b),
                              *(undefined1 *)(lVar5 + 0x27),param_8,*(undefined8 *)(pcVar8 + 0x68),
                              *(undefined4 *)(pcVar8 + 0x7c),*(undefined4 *)(lVar3 + 0x1465a24));
              }
            }
          }
          iVar11 = FUN_1400c8340(lVar3,param_1,pcVar8);
          if (iVar11 == 0x103) {
            iVar11 = 0;
          }
          else {
            param_8 = (ushort)*(byte *)(lVar3 + 0x14650c9) + local_58 + param_8;
            if (*pcVar8 != '\0') {
              *pcVar8 = '\0';
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_140012c6c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe,&DAT_140238608,
                              "MtCmdFreeCmdMsg",0x1a0,pcVar8,*(undefined8 *)(pcVar8 + 0x68));
              }
              if (param_8 < 0x100) {
                FUN_14009b52c(lVar3,pcVar8);
              }
              else {
                if (*(longlong *)(pcVar8 + 0x68) != 0) {
                  FUN_1400100e8(*(undefined8 *)(pcVar8 + 0x68),param_8);
                  pcVar8[0x68] = '\0';
                  pcVar8[0x69] = '\0';
                  pcVar8[0x6a] = '\0';
                  pcVar8[0x6b] = '\0';
                  pcVar8[0x6c] = '\0';
                  pcVar8[0x6d] = '\0';
                  pcVar8[0x6e] = '\0';
                  pcVar8[0x6f] = '\0';
                }
                FUN_1400100e8(pcVar8,0x90);
              }
            }
          }
        }
        if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
          return iVar11;
        }
        if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
          return iVar11;
        }
        if (4 < (byte)PTR_LOOP_140246148[0xa1]) {
          FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x42,&DAT_140238608,iVar11);
          return iVar11;
        }
        return iVar11;
      }
      do {
        if (local_50[bVar9] == param_3) {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x38,&DAT_140238608,
                     "MtCmdSendSetQueryCmdAdv");
          }
          goto LAB_1400cd54d;
        }
        bVar9 = bVar9 + 1;
      } while (bVar9 < 9);
      iVar11 = *(int *)(lVar3 + 0x18865d4) + 1;
      *(int *)(lVar3 + 0x18865d4) = iVar11;
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
        FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x39,&DAT_140238608,
                      "MtCmdSendSetQueryCmdAdv",0x4cf,iVar11);
      }
    }
    else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
             ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x29])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x35,&DAT_140238608,
               "MtCmdSendSetQueryCmdAdv");
    }
  }
  return -0x3fffffff;
}


```

