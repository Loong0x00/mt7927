# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_14014f720 @ 14014f720

### Immediates (>=0x10000)

- 0xffffffff
- 0x1402507e0

### Disassembly

```asm
14014f720: MOVZX R10D,CL
14014f724: XOR R8B,R8B
14014f727: MOV R9B,DL
14014f72a: MOV R11D,0x1
14014f730: CMP R10B,0xed
14014f734: JNZ 0x14014f740
14014f736: TEST DL,DL
14014f738: MOVZX R8D,R8B
14014f73c: CMOVNZ R8D,R11D
14014f740: XOR ECX,ECX
14014f742: LEA RDX,[0x1402507e0]
14014f749: MOV EAX,0xa5
14014f74e: CMP word ptr [RDX + 0x2],AX
14014f752: JZ 0x14014f76a
14014f754: TEST R8B,R8B
14014f757: JNZ 0x14014f75f
14014f759: CMP word ptr [RDX],R10W
14014f75d: JZ 0x14014f776
14014f75f: CMP R8B,R11B
14014f762: JNZ 0x14014f76a
14014f764: CMP byte ptr [RDX + 0x4],R9B
14014f768: JZ 0x14014f776
14014f76a: ADD ECX,R11D
14014f76d: ADD RDX,0xd
14014f771: CMP ECX,0x3a
14014f774: JC 0x14014f749
14014f776: CMP ECX,0x3a
14014f779: JNC 0x14014f781
14014f77b: LFENCE
14014f77e: MOV EAX,ECX
14014f780: RET
14014f781: OR EAX,0xffffffff
14014f784: RET
```

### Decompiled C

```c

uint FUN_14014f720(byte param_1,char param_2)

{
  bool bVar1;
  uint uVar2;
  ushort *puVar3;
  
  bVar1 = false;
  if ((param_1 == 0xed) && (bVar1 = false, param_2 != '\0')) {
    bVar1 = true;
  }
  uVar2 = 0;
  puVar3 = &DAT_1402507e0;
  do {
    if ((puVar3[1] != 0xa5) &&
       (((!bVar1 && (*puVar3 == (ushort)param_1)) || ((bVar1 && ((char)puVar3[2] == param_2))))))
    break;
    uVar2 = uVar2 + 1;
    puVar3 = (ushort *)((longlong)puVar3 + 0xd);
  } while (uVar2 < 0x3a);
  if (uVar2 < 0x3a) {
    return uVar2;
  }
  return 0xffffffff;
}


```

## FUN_14014eb0c @ 14014eb0c

### Immediates (>=0x10000)

- 0x7fffff
- 0x1464f07
- 0x1465a24
- 0x1465a3f
- 0x1465ad8
- 0x1465b4b
- 0x14669e9
- 0x146ad48
- 0x146cde9
- 0x18865d4
- 0x1887d73
- 0x1887d74
- 0x41000000
- 0xc0000001
- 0xc000009a
- 0x14021e2b0
- 0x1402387a0
- 0x140246148

### Disassembly

```asm
14014eb0c: MOV RAX,RSP
14014eb0f: MOV byte ptr [RAX + 0x20],R9B
14014eb13: MOV byte ptr [RAX + 0x18],R8B
14014eb17: MOV byte ptr [RAX + 0x10],DL
14014eb1a: MOV qword ptr [RAX + 0x8],RCX
14014eb1e: PUSH RBX
14014eb1f: PUSH RBP
14014eb20: PUSH RSI
14014eb21: PUSH RDI
14014eb22: PUSH R12
14014eb24: PUSH R14
14014eb26: PUSH R15
14014eb28: SUB RSP,0x80
14014eb2f: MOV RBX,qword ptr [RCX]
14014eb32: XOR ESI,ESI
14014eb34: MOV R15D,ESI
14014eb37: MOV R14B,DL
14014eb3a: MOV RCX,qword ptr [0x140246148]
14014eb41: LEA RAX,[0x140246148]
14014eb48: LEA R12,[0x1402387a0]
14014eb4f: CMP RCX,RAX
14014eb52: JZ 0x14014eb7f
14014eb54: TEST byte ptr [RCX + 0x158],0x1
14014eb5b: JZ 0x14014eb78
14014eb5d: CMP byte ptr [RCX + 0x155],0x5
14014eb64: JC 0x14014eb78
14014eb66: MOV RCX,qword ptr [RCX + 0x144]
14014eb6d: LEA EDX,[RSI + 0x15]
14014eb70: MOV R8,R12
14014eb73: CALL 0x1400015d8
14014eb78: LEA RAX,[0x140246148]
14014eb7f: TEST RBX,RBX
14014eb82: JZ 0x14014ebd0
14014eb84: CMP dword ptr [RBX + 0x1465b4b],ESI
14014eb8a: JLE 0x14014ebd0
14014eb8c: MOV RCX,qword ptr [0x140246148]
14014eb93: CMP RCX,RAX
14014eb96: JZ 0x14014f0a9
14014eb9c: TEST byte ptr [RCX + 0x2c],0x1
14014eba0: JZ 0x14014f0a9
14014eba6: MOV EDI,0x2
14014ebab: CMP byte ptr [RCX + 0x29],DIL
14014ebaf: JC 0x14014f0a9
14014ebb5: MOV RCX,qword ptr [RCX + 0x18]
14014ebb9: LEA EDX,[RDI + 0x14]
14014ebbc: LEA R9,[0x14021e2b0]
14014ebc3: MOV R8,R12
14014ebc6: CALL 0x140001600
14014ebcb: JMP 0x14014f0a9
14014ebd0: MOV EDI,0x2
14014ebd5: CMP byte ptr [RBX + 0x146cde9],SIL
14014ebdc: JNZ 0x14014ec44
14014ebde: CMP dword ptr [RBX + 0x1465ad8],EDI
14014ebe4: JNZ 0x14014ec44
14014ebe6: MOV R8D,dword ptr [RBX + 0x18865d4]
14014ebed: INC R8D
14014ebf0: MOV dword ptr [RBX + 0x18865d4],R8D
14014ebf7: MOV RCX,qword ptr [0x140246148]
14014ebfe: CMP RCX,RAX
14014ec01: JZ 0x14014f0a9
14014ec07: TEST byte ptr [RCX + 0x158],0x1
14014ec0e: JZ 0x14014f0a9
14014ec14: CMP byte ptr [RCX + 0x155],DIL
14014ec1b: JC 0x14014f0a9
14014ec21: MOV RCX,qword ptr [RCX + 0x144]
14014ec28: LEA EDX,[RDI + 0x15]
14014ec2b: MOV dword ptr [RSP + 0x20],R8D
14014ec30: LEA R9,[0x14021e2b0]
14014ec37: MOV R8,R12
14014ec3a: CALL 0x140001664
14014ec3f: JMP 0x14014f0a9
14014ec44: MOVZX EDX,byte ptr [RBX + 0x1887d73]
14014ec4b: TEST DL,DL
14014ec4d: JNZ 0x14014ec61
14014ec4f: CMP byte ptr [RBX + 0x1887d74],SIL
14014ec56: JNZ 0x14014ec61
14014ec58: CMP dword ptr [RBX + 0x1465a3f],0x1
14014ec5f: JNZ 0x14014eccc
14014ec61: CMP byte ptr [RBX + 0x14669e9],SIL
14014ec68: JZ 0x14014f055
14014ec6e: CMP R14B,0xd
14014ec72: JNZ 0x14014f055
14014ec78: MOV RCX,qword ptr [0x140246148]
14014ec7f: CMP RCX,RAX
14014ec82: JZ 0x14014eccc
14014ec84: TEST byte ptr [RCX + 0xa4],0x1
14014ec8b: JZ 0x14014eccc
14014ec8d: CMP byte ptr [RCX + 0xa1],DIL
14014ec94: JC 0x14014eccc
14014ec96: MOVZX R8D,byte ptr [RBX + 0x1887d74]
14014ec9e: LEA R9,[0x14021e2b0]
14014eca5: MOV RCX,qword ptr [RCX + 0x90]
14014ecac: MOV EAX,EDX
14014ecae: MOV dword ptr [RSP + 0x30],EAX
14014ecb2: MOV EDX,0x18
14014ecb7: MOV dword ptr [RSP + 0x28],R8D
14014ecbc: MOV R8,R12
14014ecbf: MOV dword ptr [RSP + 0x20],0x254
14014ecc7: CALL 0x140007d48
14014eccc: MOV RAX,qword ptr [RBX + 0x1464f07]
14014ecd3: TEST RAX,RAX
14014ecd6: JZ 0x14014ece3
14014ecd8: MOV RCX,RBX
14014ecdb: CALL qword ptr [0x14022a3f8]
14014ece1: JMP 0x14014ed24
14014ece3: MOVZX EAX,word ptr [RBX + 0x1f72]
14014ecea: MOV ECX,0x6639
14014ecef: CMP AX,CX
14014ecf2: JZ 0x14014ed1c
14014ecf4: MOV ECX,0x738
14014ecf9: CMP AX,CX
14014ecfc: JZ 0x14014ed1c
14014ecfe: MOV ECX,0x7927
14014ed03: CMP AX,CX
14014ed06: JZ 0x14014ed1c
14014ed08: MOV ECX,0x7925
14014ed0d: CMP AX,CX
14014ed10: JZ 0x14014ed1c
14014ed12: MOV ECX,0x717
14014ed17: CMP AX,CX
14014ed1a: JNZ 0x14014ed27
14014ed1c: MOV RCX,RBX
14014ed1f: CALL 0x1401cc290
14014ed24: MOV R15D,EAX
14014ed27: MOV EBP,dword ptr [RSP + 0xf0]
14014ed2e: TEST EBP,EBP
14014ed30: JZ 0x14014efc4
14014ed36: CMP qword ptr [RSP + 0xf8],RSI
14014ed3e: JZ 0x14014efc4
14014ed44: LEA R12D,[R15 + RBP*0x1]
14014ed48: MOV RCX,RBX
14014ed4b: MOV EDX,R12D
14014ed4e: CALL 0x1400c5ad8
14014ed53: MOV RSI,RAX
14014ed56: TEST RAX,RAX
14014ed59: JZ 0x14014efbd
14014ed5f: MOV CL,0xed
14014ed61: CALL 0x1400ca864
14014ed66: MOV CL,byte ptr [RSP + 0xe0]
14014ed6d: XOR R9D,R9D
14014ed70: MOV R8,qword ptr [RSP + 0xc0]
14014ed78: MOV word ptr [RSI + 0x3a],AX
14014ed7c: MOV RAX,qword ptr [RSP + 0x100]
14014ed84: MOV qword ptr [RSI + 0x18],RAX
14014ed88: MOV RAX,qword ptr [RSP + 0xe8]
14014ed90: MOV qword ptr [RSI + 0x28],RAX
14014ed94: MOV EAX,dword ptr [RSP + 0x108]
14014ed9b: MOV dword ptr [RSI + 0x20],EAX
14014ed9e: MOVZX EAX,word ptr [RSP + 0x110]
14014eda6: MOV word ptr [RSI + 0x8c],AX
14014edad: MOV word ptr [RSI + 0x8],R9W
14014edb2: MOV byte ptr [RSI + 0xa],0xed
14014edb6: MOV byte ptr [RSI + 0xc],CL
14014edb9: MOV byte ptr [RSI + 0xb],R14B
14014edbd: MOV dword ptr [RSI + 0xe],R9D
14014edc1: MOV byte ptr [RSI + 0x38],R9B
14014edc5: MOV qword ptr [RSI + 0x80],R8
14014edcc: MOV dword ptr [RSI + 0x7c],0x1
14014edd3: MOV RCX,qword ptr [0x140246148]
14014edda: LEA RAX,[0x140246148]
14014ede1: CMP RCX,RAX
14014ede4: JZ 0x14014ee21
14014ede6: TEST byte ptr [RCX + 0x158],0x1
14014eded: JZ 0x14014ee21
14014edef: CMP byte ptr [RCX + 0x155],0x4
14014edf6: JC 0x14014ee21
14014edf8: MOV RCX,qword ptr [RCX + 0x144]
14014edff: LEA EDX,[R9 + 0x1b]
14014ee03: LEA R9,[0x14021e2b0]
14014ee0a: LEA R8,[0x1402387a0]
14014ee11: CALL 0x140001600
14014ee16: MOV R8,qword ptr [RSP + 0xc0]
14014ee1e: XOR R9D,R9D
14014ee21: MOV R14,qword ptr [RSI + 0x68]
14014ee25: MOV dword ptr [RSP + 0x70],R15D
14014ee2a: MOV EAX,dword ptr [R14 + 0x4]
14014ee2e: BTR EAX,0xf
14014ee32: MOV word ptr [R14],R9W
14014ee36: BTS EAX,0xe
14014ee3a: MOV dword ptr [R14 + 0x4],EAX
14014ee3e: LEA EAX,[R15 + RBP*0x1]
14014ee42: MOVZX R15D,byte ptr [RSP + 0xc8]
14014ee4b: MOVZX ECX,AX
14014ee4e: MOV EAX,dword ptr [R14]
14014ee51: AND EAX,0x7fffff
14014ee56: OR ECX,EAX
14014ee58: MOV AL,byte ptr [RSP + 0xd0]
14014ee5f: OR ECX,0x41000000
14014ee65: MOV dword ptr [R14],ECX
14014ee68: MOV word ptr [R14 + 0x22],R15W
14014ee6d: MOV byte ptr [R14 + 0x25],0xa0
14014ee72: MOV byte ptr [R14 + 0x2b],DIL
14014ee76: TEST AL,AL
14014ee78: JZ 0x14014ee96
14014ee7a: CMP byte ptr [RSP + 0xd8],R9B
14014ee82: SETNZ AL
14014ee85: ADD AL,DIL
14014ee88: MOV byte ptr [R14 + 0x2b],AL
14014ee8c: MOV DIL,AL
14014ee8f: MOV AL,byte ptr [RSP + 0xd0]
14014ee96: CMP dword ptr [R8 + 0x10],0x5
14014ee9b: MOV DL,DIL
14014ee9e: JNZ 0x14014eeb2
14014eea0: CMP R15B,0x3
14014eea4: JNZ 0x14014eeb2
14014eea6: CMP byte ptr [RBX + 0x146ad48],R9B
14014eead: JZ 0x14014eeb2
14014eeaf: OR DL,0x1
14014eeb2: NEG AL
14014eeb4: MOV R8B,0x1
14014eeb7: SBB CL,CL
14014eeb9: AND CL,0x4
14014eebc: OR CL,DL
14014eebe: XOR EDX,EDX
14014eec0: MOV byte ptr [R14 + 0x2b],CL
14014eec4: MOV RCX,RBX
14014eec7: CALL 0x14009a46c
14014eecc: MOV RDX,qword ptr [RSP + 0xf8]
14014eed4: LEA RCX,[R14 + 0x30]
14014eed8: MOV byte ptr [R14 + 0x27],AL
14014eedc: MOV R8D,EBP
14014eedf: XOR EAX,EAX
14014eee1: MOV byte ptr [R14 + 0x2a],AL
14014eee5: MOV EAX,dword ptr [RSP + 0x70]
14014eee9: SUB AX,0x20
14014eeed: ADD AX,BP
14014eef0: MOV word ptr [R14 + 0x20],AX
14014eef5: CALL 0x140010118
14014eefa: MOV RCX,qword ptr [0x140246148]
14014ef01: LEA RAX,[0x140246148]
14014ef08: CMP RCX,RAX
14014ef0b: JZ 0x14014ef8b
14014ef0d: TEST byte ptr [RCX + 0x158],0x1
14014ef14: JZ 0x14014ef8b
14014ef16: CMP byte ptr [RCX + 0x155],0x3
14014ef1d: JC 0x14014ef8b
14014ef1f: MOV EAX,dword ptr [RBX + 0x1465a24]
14014ef25: MOV EDX,0x1c
14014ef2a: MOVZX R8D,byte ptr [R14 + 0x27]
14014ef2f: MOVZX R9D,byte ptr [R14 + 0x2b]
14014ef34: MOVZX R11D,byte ptr [RSP + 0xd0]
14014ef3d: MOV RCX,qword ptr [RCX + 0x144]
14014ef44: MOV dword ptr [RSP + 0x60],EAX
14014ef48: MOV EAX,dword ptr [RSI + 0x7c]
14014ef4b: MOV dword ptr [RSP + 0x58],EAX
14014ef4f: MOV RAX,qword ptr [RSI + 0x68]
14014ef53: MOV qword ptr [RSP + 0x50],RAX
14014ef58: MOV dword ptr [RSP + 0x48],EBP
14014ef5c: MOV dword ptr [RSP + 0x40],R8D
14014ef61: LEA R8,[0x1402387a0]
14014ef68: MOV dword ptr [RSP + 0x38],R9D
14014ef6d: LEA R9,[0x14021e2b0]
14014ef74: MOV dword ptr [RSP + 0x30],R15D
14014ef79: MOV dword ptr [RSP + 0x28],R11D
14014ef7e: MOV dword ptr [RSP + 0x20],0xed
14014ef86: CALL 0x1400d43d4
14014ef8b: MOV RDX,qword ptr [RSP + 0xc0]
14014ef93: MOV R8,RSI
14014ef96: MOV RCX,RBX
14014ef99: CALL 0x1400c8340
14014ef9e: MOV EDI,EAX
14014efa0: CMP EAX,0x103
14014efa5: JNZ 0x14014efad
14014efa7: XOR EAX,EAX
14014efa9: MOV EDI,EAX
14014efab: JMP 0x14014f011
14014efad: MOV R8D,R12D
14014efb0: MOV RDX,RSI
14014efb3: MOV RCX,RBX
14014efb6: CALL 0x1400c9f4c
14014efbb: JMP 0x14014f011
14014efbd: LEA R12,[0x1402387a0]
14014efc4: MOV RCX,qword ptr [0x140246148]
14014efcb: LEA RAX,[0x140246148]
14014efd2: CMP RCX,RAX
14014efd5: JZ 0x14014f00c
14014efd7: TEST byte ptr [RCX + 0x158],0x1
14014efde: JZ 0x14014f00c
14014efe0: CMP byte ptr [RCX + 0x155],0x1
14014efe7: JC 0x14014f00c
14014efe9: MOV RCX,qword ptr [RCX + 0x144]
14014eff0: LEA R9,[0x14021e2b0]
14014eff7: MOV EDX,0x1a
14014effc: MOV dword ptr [RSP + 0x20],0x269
14014f004: MOV R8,R12
14014f007: CALL 0x140001664
14014f00c: MOV EDI,0xc000009a
14014f011: MOV RCX,qword ptr [0x140246148]
14014f018: LEA RAX,[0x140246148]
14014f01f: CMP RCX,RAX
14014f022: JZ 0x14014f051
14014f024: TEST byte ptr [RCX + 0x158],0x1
14014f02b: JZ 0x14014f051
14014f02d: CMP byte ptr [RCX + 0x155],0x5
14014f034: JC 0x14014f051
14014f036: MOV RCX,qword ptr [RCX + 0x144]
14014f03d: LEA R8,[0x1402387a0]
14014f044: MOV EDX,0x1d
14014f049: MOV R9D,EDI
14014f04c: CALL 0x14000764c
14014f051: MOV EAX,EDI
14014f053: JMP 0x14014f0ae
14014f055: MOV RCX,qword ptr [0x140246148]
14014f05c: CMP RCX,RAX
14014f05f: JZ 0x14014f0a9
14014f061: TEST byte ptr [RCX + 0xa4],0x1
14014f068: JZ 0x14014f0a9
14014f06a: CMP byte ptr [RCX + 0xa1],DIL
14014f071: JC 0x14014f0a9
14014f073: MOVZX R8D,byte ptr [RBX + 0x1887d74]
14014f07b: LEA R9,[0x14021e2b0]
14014f082: MOV RCX,qword ptr [RCX + 0x90]
14014f089: MOV EAX,EDX
14014f08b: MOV dword ptr [RSP + 0x30],EAX
14014f08f: MOV EDX,0x19
14014f094: MOV dword ptr [RSP + 0x28],R8D
14014f099: MOV R8,R12
14014f09c: MOV dword ptr [RSP + 0x20],0x258
14014f0a4: CALL 0x140007d48
14014f0a9: MOV EAX,0xc0000001
14014f0ae: ADD RSP,0x80
14014f0b5: POP R15
14014f0b7: POP R14
14014f0b9: POP R12
14014f0bb: POP RDI
14014f0bc: POP RSI
14014f0bd: POP RBP
14014f0be: POP RBX
14014f0bf: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

int FUN_14014eb0c(longlong *param_1,byte param_2,char param_3,char param_4,undefined1 param_5,
                 undefined8 param_6,int param_7,longlong param_8,undefined8 param_9,
                 undefined4 param_10,undefined2 param_11)

{
  char cVar1;
  short sVar2;
  longlong lVar3;
  uint *puVar4;
  byte bVar5;
  undefined1 uVar6;
  undefined2 uVar7;
  int iVar8;
  longlong lVar9;
  int iVar10;
  
  lVar3 = *param_1;
  iVar10 = 0;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0x155])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x15,&DAT_1402387a0);
  }
  if ((lVar3 != 0) && (0 < *(int *)(lVar3 + 0x1465b4b))) {
    if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
       (((PTR_LOOP_140246148[0x2c] & 1) != 0 && (1 < (byte)PTR_LOOP_140246148[0x29])))) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x16,&DAT_1402387a0,
               "MtCmdSendSetQueryUniCmdAdv");
    }
    return -0x3fffffff;
  }
  bVar5 = 2;
  if ((*(char *)(lVar3 + 0x146cde9) == '\0') && (*(int *)(lVar3 + 0x1465ad8) == 2)) {
    iVar10 = *(int *)(lVar3 + 0x18865d4) + 1;
    *(int *)(lVar3 + 0x18865d4) = iVar10;
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return -0x3fffffff;
    }
    if ((PTR_LOOP_140246148[0x158] & 1) == 0) {
      return -0x3fffffff;
    }
    if ((byte)PTR_LOOP_140246148[0x155] < 2) {
      return -0x3fffffff;
    }
    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x17,&DAT_1402387a0,
                  "MtCmdSendSetQueryUniCmdAdv",iVar10);
    return -0x3fffffff;
  }
  cVar1 = *(char *)(lVar3 + 0x1887d73);
  if (((cVar1 != '\0') || (*(char *)(lVar3 + 0x1887d74) != '\0')) ||
     (*(int *)(lVar3 + 0x1465a3f) == 1)) {
    if ((*(char *)(lVar3 + 0x14669e9) == '\0') || (param_2 != 0xd)) {
      if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
        return -0x3fffffff;
      }
      if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
        return -0x3fffffff;
      }
      if ((byte)PTR_LOOP_140246148[0xa1] < 2) {
        return -0x3fffffff;
      }
      FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x19,&DAT_1402387a0,
                    "MtCmdSendSetQueryUniCmdAdv",600,*(undefined1 *)(lVar3 + 0x1887d74),cVar1);
      return -0x3fffffff;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
      FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x18,&DAT_1402387a0,
                    "MtCmdSendSetQueryUniCmdAdv",0x254,*(undefined1 *)(lVar3 + 0x1887d74),cVar1);
    }
  }
  if (*(code **)(lVar3 + 0x1464f07) == (code *)0x0) {
    sVar2 = *(short *)(lVar3 + 0x1f72);
    if ((((sVar2 == 0x6639) || (sVar2 == 0x738)) || (sVar2 == 0x7927)) ||
       ((sVar2 == 0x7925 || (sVar2 == 0x717)))) {
      iVar10 = FUN_1401cc290(lVar3);
    }
  }
  else {
    iVar10 = (**(code **)(lVar3 + 0x1464f07))(lVar3);
  }
  if ((param_7 != 0) && (param_8 != 0)) {
    lVar9 = FUN_1400c5ad8(lVar3,iVar10 + param_7);
    if (lVar9 != 0) {
      uVar7 = FUN_1400ca864(0xed);
      *(undefined2 *)(lVar9 + 0x3a) = uVar7;
      *(undefined8 *)(lVar9 + 0x18) = param_9;
      *(undefined8 *)(lVar9 + 0x28) = param_6;
      *(undefined4 *)(lVar9 + 0x20) = param_10;
      *(undefined2 *)(lVar9 + 0x8c) = param_11;
      *(undefined2 *)(lVar9 + 8) = 0;
      *(undefined1 *)(lVar9 + 10) = 0xed;
      *(undefined1 *)(lVar9 + 0xc) = param_5;
      *(byte *)(lVar9 + 0xb) = param_2;
      *(undefined4 *)(lVar9 + 0xe) = 0;
      *(undefined1 *)(lVar9 + 0x38) = 0;
      *(longlong **)(lVar9 + 0x80) = param_1;
      *(undefined4 *)(lVar9 + 0x7c) = 1;
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0x155])) {
        WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x1b,&DAT_1402387a0);
      }
      puVar4 = *(uint **)(lVar9 + 0x68);
      *(undefined2 *)puVar4 = 0;
      puVar4[1] = puVar4[1] & 0xffff7fff | 0x4000;
      *puVar4 = iVar10 + param_7 & 0xffffU | *puVar4 & 0x7fffff | 0x41000000;
      *(ushort *)((longlong)puVar4 + 0x22) = (ushort)param_2;
      *(undefined1 *)((longlong)puVar4 + 0x25) = 0xa0;
      *(undefined1 *)((longlong)puVar4 + 0x2b) = 2;
      if (param_3 != '\0') {
        bVar5 = (param_4 != '\0') + 2;
        *(byte *)((longlong)puVar4 + 0x2b) = bVar5;
      }
      if ((((int)param_1[2] == 5) && (param_2 == 3)) && (*(char *)(lVar3 + 0x146ad48) != '\0')) {
        bVar5 = bVar5 | 1;
      }
      *(byte *)((longlong)puVar4 + 0x2b) = -(param_3 != '\0') & 4U | bVar5;
      uVar6 = FUN_14009a46c(lVar3,0,1);
      *(undefined1 *)((longlong)puVar4 + 0x27) = uVar6;
      *(undefined1 *)((longlong)puVar4 + 0x2a) = 0;
      *(short *)(puVar4 + 8) = (short)iVar10 + -0x20 + (short)param_7;
      FUN_140010118(puVar4 + 0xc,param_8,param_7);
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x155])) {
        FUN_1400d43d4(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x1c,&DAT_1402387a0,
                      "MtCmdSendSetQueryUniCmdAdv",0xed,param_3,param_2,
                      *(undefined1 *)((longlong)puVar4 + 0x2b),
                      *(undefined1 *)((longlong)puVar4 + 0x27),param_7,*(undefined8 *)(lVar9 + 0x68)
                      ,*(undefined4 *)(lVar9 + 0x7c),*(undefined4 *)(lVar3 + 0x1465a24));
      }
      iVar8 = FUN_1400c8340(lVar3,param_1,lVar9);
      if (iVar8 == 0x103) {
        iVar8 = 0;
      }
      else {
        FUN_1400c9f4c(lVar3,lVar9,iVar10 + param_7);
      }
      goto LAB_14014f011;
    }
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (PTR_LOOP_140246148[0x155] != '\0')) {
    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x1a,&DAT_1402387a0,
                  "MtCmdSendSetQueryUniCmdAdv",0x269);
  }
  iVar8 = -0x3fffff66;
LAB_14014f011:
  if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
    return iVar8;
  }
  if ((PTR_LOOP_140246148[0x158] & 1) != 0) {
    if (4 < (byte)PTR_LOOP_140246148[0x155]) {
      FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x1d,&DAT_1402387a0,iVar8);
      return iVar8;
    }
    return iVar8;
  }
  return iVar8;
}


```

