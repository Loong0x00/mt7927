# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_1401e5430 @ 1401e5430

### Immediates (>=0x10000)

- 0x1464d29
- 0x1465077
- 0x14669e0
- 0xc0000001
- 0x1402259c0
- 0x14023aa70
- 0x140246148
- 0x14025dd40

### Disassembly

```asm
1401e5430: MOV qword ptr [RSP + 0x8],RBX
1401e5435: MOV qword ptr [RSP + 0x10],RBP
1401e543a: MOV qword ptr [RSP + 0x18],RSI
1401e543f: PUSH RDI
1401e5440: PUSH R12
1401e5442: PUSH R13
1401e5444: PUSH R14
1401e5446: PUSH R15
1401e5448: SUB RSP,0x30
1401e544c: MOV RSI,RCX
1401e544f: MOV RCX,qword ptr [0x140246148]
1401e5456: LEA R15,[0x140246148]
1401e545d: LEA R12,[0x14023aa70]
1401e5464: MOV R14D,0x1
1401e546a: CMP RCX,R15
1401e546d: JZ 0x1401e5494
1401e546f: TEST byte ptr [RCX + 0xa4],R14B
1401e5476: JZ 0x1401e5494
1401e5478: CMP byte ptr [RCX + 0xa1],0x5
1401e547f: JC 0x1401e5494
1401e5481: MOV RCX,qword ptr [RCX + 0x90]
1401e5488: LEA EDX,[R14 + 0x33]
1401e548c: MOV R8,R12
1401e548f: CALL 0x1400015d8
1401e5494: XOR EDI,EDI
1401e5496: LEA R13,[0x1402259c0]
1401e549d: MOV BPL,0x3
1401e54a0: CMP dword ptr [RSI + 0x14669e0],EDI
1401e54a6: JNZ 0x1401e54f4
1401e54a8: LEA RCX,[RSI + 0x1465077]
1401e54af: MOV R8D,0x15e
1401e54b5: LEA RDX,[0x14025dd40]
1401e54bc: CALL 0x140010118
1401e54c1: MOV RCX,qword ptr [0x140246148]
1401e54c8: CMP RCX,R15
1401e54cb: JZ 0x1401e54f4
1401e54cd: TEST byte ptr [RCX + 0xa4],R14B
1401e54d4: JZ 0x1401e54f4
1401e54d6: CMP byte ptr [RCX + 0xa1],BPL
1401e54dd: JC 0x1401e54f4
1401e54df: MOV RCX,qword ptr [RCX + 0x90]
1401e54e6: LEA EDX,[RDI + 0x35]
1401e54e9: MOV R9,R13
1401e54ec: MOV R8,R12
1401e54ef: CALL 0x140001600
1401e54f4: MOV RCX,RSI
1401e54f7: CALL 0x1401ce900
1401e54fc: MOV EBX,EAX
1401e54fe: MOV RCX,qword ptr [0x140246148]
1401e5505: CMP RCX,R15
1401e5508: JZ 0x1401e553f
1401e550a: TEST byte ptr [RCX + 0xa4],R14B
1401e5511: JZ 0x1401e553f
1401e5513: CMP byte ptr [RCX + 0xa1],BPL
1401e551a: JC 0x1401e553f
1401e551c: MOV RCX,qword ptr [RCX + 0x90]
1401e5523: MOV EDX,0x36
1401e5528: MOV dword ptr [RSP + 0x28],EAX
1401e552c: MOV R9,R13
1401e552f: MOV R8,R12
1401e5532: MOV dword ptr [RSP + 0x20],0x34b
1401e553a: CALL 0x140007b24
1401e553f: TEST EBX,EBX
1401e5541: JNZ 0x1401e564c
1401e5547: MOV dword ptr [RSI + 0x1464d29],EDI
1401e554d: MOV RCX,qword ptr [0x140246148]
1401e5554: CMP RCX,R15
1401e5557: JZ 0x1401e5580
1401e5559: TEST byte ptr [RCX + 0xa4],R14B
1401e5560: JZ 0x1401e5580
1401e5562: CMP byte ptr [RCX + 0xa1],BPL
1401e5569: JC 0x1401e5580
1401e556b: MOV RCX,qword ptr [RCX + 0x90]
1401e5572: LEA EDX,[RBX + 0x37]
1401e5575: MOV R9,R13
1401e5578: MOV R8,R12
1401e557b: CALL 0x140001600
1401e5580: MOV RCX,RSI
1401e5583: CALL 0x14000d410
1401e5588: MOV EBX,EAX
1401e558a: TEST EAX,EAX
1401e558c: JZ 0x1401e55dc
1401e558e: MOV RCX,qword ptr [0x140246148]
1401e5595: CMP RCX,R15
1401e5598: JZ 0x1401e56cb
1401e559e: TEST byte ptr [RCX + 0xa4],R14B
1401e55a5: JZ 0x1401e5696
1401e55ab: CMP byte ptr [RCX + 0xa1],R14B
1401e55b2: JC 0x1401e5696
1401e55b8: MOV RCX,qword ptr [RCX + 0x90]
1401e55bf: MOV EDX,0x38
1401e55c4: MOV R9,R13
1401e55c7: MOV dword ptr [RSP + 0x20],0x356
1401e55cf: MOV R8,R12
1401e55d2: CALL 0x140001664
1401e55d7: JMP 0x1401e5696
1401e55dc: MOV RCX,RSI
1401e55df: CALL 0x1401ce900
1401e55e4: MOV EBP,EAX
1401e55e6: CMP EAX,R14D
1401e55e9: JZ 0x1401e5696
1401e55ef: MOV ECX,0x3e8
1401e55f4: CALL 0x14000ee4c
1401e55f9: ADD EDI,R14D
1401e55fc: CMP EDI,0x1f4
1401e5602: JBE 0x1401e55dc
1401e5604: MOV RCX,qword ptr [0x140246148]
1401e560b: CMP RCX,R15
1401e560e: JZ 0x1401e5645
1401e5610: TEST byte ptr [RCX + 0xa4],R14B
1401e5617: JZ 0x1401e5645
1401e5619: CMP byte ptr [RCX + 0xa1],R14B
1401e5620: JC 0x1401e5645
1401e5622: MOV RCX,qword ptr [RCX + 0x90]
1401e5629: MOV EDX,0x39
1401e562e: MOV dword ptr [RSP + 0x28],EBP
1401e5632: MOV R9,R13
1401e5635: MOV R8,R12
1401e5638: MOV dword ptr [RSP + 0x20],0x36c
1401e5640: CALL 0x140007b24
1401e5645: MOV EBX,0xc0000001
1401e564a: JMP 0x1401e5696
1401e564c: MOV dword ptr [RSI + 0x1464d29],R14D
1401e5653: MOV RCX,qword ptr [0x140246148]
1401e565a: CMP RCX,R15
1401e565d: JZ 0x1401e568c
1401e565f: TEST byte ptr [RCX + 0xa4],R14B
1401e5666: JZ 0x1401e568c
1401e5668: CMP byte ptr [RCX + 0xa1],0x4
1401e566f: JC 0x1401e568c
1401e5671: MOV RCX,qword ptr [RCX + 0x90]
1401e5678: MOV EDX,0x3a
1401e567d: MOV R9,R13
1401e5680: MOV dword ptr [RSP + 0x20],EBX
1401e5684: MOV R8,R12
1401e5687: CALL 0x140001664
1401e568c: MOV RCX,RSI
1401e568f: CALL 0x14000d410
1401e5694: MOV EBX,EAX
1401e5696: MOV RCX,qword ptr [0x140246148]
1401e569d: CMP RCX,R15
1401e56a0: JZ 0x1401e56cb
1401e56a2: TEST byte ptr [RCX + 0xa4],R14B
1401e56a9: JZ 0x1401e56cb
1401e56ab: CMP byte ptr [RCX + 0xa1],0x5
1401e56b2: JC 0x1401e56cb
1401e56b4: MOV RCX,qword ptr [RCX + 0x90]
1401e56bb: MOV EDX,0x3b
1401e56c0: MOV R9D,EBX
1401e56c3: MOV R8,R12
1401e56c6: CALL 0x14000764c
1401e56cb: MOV RBP,qword ptr [RSP + 0x68]
1401e56d0: MOV EAX,EBX
1401e56d2: MOV RBX,qword ptr [RSP + 0x60]
1401e56d7: MOV RSI,qword ptr [RSP + 0x70]
1401e56dc: ADD RSP,0x30
1401e56e0: POP R15
1401e56e2: POP R14
1401e56e4: POP R13
1401e56e6: POP R12
1401e56e8: POP RDI
1401e56e9: RET
```

### Decompiled C

```c

int FUN_1401e5430(longlong param_1)

{
  int iVar1;
  int iVar2;
  uint uVar3;
  
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x34,&DAT_14023aa70);
  }
  uVar3 = 0;
  if (((*(int *)(param_1 + 0x14669e0) == 0) &&
      (FUN_140010118(param_1 + 0x1465077,&DAT_14025dd40,0x15e),
      (undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148)) &&
     (((PTR_LOOP_140246148[0xa4] & 1) != 0 && (2 < (byte)PTR_LOOP_140246148[0xa1])))) {
    WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x35,&DAT_14023aa70,
             "MT6639PreFirmwareDownloadInit");
  }
  iVar1 = FUN_1401ce900(param_1);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x36,&DAT_14023aa70,
                  "MT6639PreFirmwareDownloadInit",0x34b,iVar1);
  }
  if (iVar1 == 0) {
    *(undefined4 *)(param_1 + 0x1464d29) = 0;
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x37,&DAT_14023aa70,
               "MT6639PreFirmwareDownloadInit");
    }
    iVar1 = FUN_14000d410(param_1);
    if (iVar1 == 0) {
      do {
        iVar2 = FUN_1401ce900(param_1);
        if (iVar2 == 1) goto LAB_1401e5696;
        FUN_14000ee4c(1000);
        uVar3 = uVar3 + 1;
      } while (uVar3 < 0x1f5);
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
        FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x39,&DAT_14023aa70,
                      "MT6639PreFirmwareDownloadInit",0x36c,iVar2);
      }
      iVar1 = -0x3fffffff;
    }
    else {
      if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
        return iVar1;
      }
      if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (PTR_LOOP_140246148[0xa1] != '\0')) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x38,&DAT_14023aa70,
                      "MT6639PreFirmwareDownloadInit",0x356);
      }
    }
  }
  else {
    *(undefined4 *)(param_1 + 0x1464d29) = 1;
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3a,&DAT_14023aa70,
                    "MT6639PreFirmwareDownloadInit",iVar1);
    }
    iVar1 = FUN_14000d410(param_1);
  }
LAB_1401e5696:
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x3b,&DAT_14023aa70,iVar1);
  }
  return iVar1;
}


```

## FUN_1401e5be0 @ 1401e5be0

### Immediates (>=0x10000)

- 0x30004f
- 0x542200
- 0x660077
- 0x1466a49
- 0x7c024208
- 0x7c0242b4
- 0x7c027030
- 0x7c0270f0
- 0x7c0270f4
- 0x7c0270f8
- 0x7c0270fc
- 0x140225a40
- 0x14023aa70
- 0x140246148

### Disassembly

```asm
1401e5be0: PUSH RBP
1401e5be2: PUSH RBX
1401e5be3: PUSH RDI
1401e5be4: MOV RBP,RSP
1401e5be7: SUB RSP,0x40
1401e5beb: AND dword ptr [RBP + 0x38],0x0
1401e5bef: MOV DIL,DL
1401e5bf2: MOV R8B,DL
1401e5bf5: MOV RBX,RCX
1401e5bf8: XOR EDX,EDX
1401e5bfa: CALL 0x1401d8724
1401e5bff: LEA R8,[RBP + 0x30]
1401e5c03: MOV EDX,0x7c024208
1401e5c08: MOV RCX,RBX
1401e5c0b: CALL 0x1400099ac
1401e5c10: MOV RCX,qword ptr [0x140246148]
1401e5c17: LEA RAX,[0x140246148]
1401e5c1e: CMP RCX,RAX
1401e5c21: JZ 0x1401e5c67
1401e5c23: TEST byte ptr [RCX + 0x2c],0x1
1401e5c27: JZ 0x1401e5c67
1401e5c29: CMP byte ptr [RCX + 0x29],0x3
1401e5c2d: JC 0x1401e5c67
1401e5c2f: AND dword ptr [RSP + 0x38],0x0
1401e5c34: LEA R9,[0x140225a40]
1401e5c3b: MOV EAX,dword ptr [RBP + 0x30]
1401e5c3e: LEA R8,[0x14023aa70]
1401e5c45: MOV RCX,qword ptr [RCX + 0x18]
1401e5c49: MOV EDX,0x61
1401e5c4e: MOV dword ptr [RSP + 0x30],EAX
1401e5c52: MOV dword ptr [RSP + 0x28],0x7c024208
1401e5c5a: MOV dword ptr [RSP + 0x20],0x689
1401e5c62: CALL 0x140007bb4
1401e5c67: TEST DIL,DIL
1401e5c6a: JZ 0x1401e5d17
1401e5c70: CMP byte ptr [RBX + 0x1466a49],0x0
1401e5c77: JZ 0x1401e5cfe
1401e5c7d: AND dword ptr [RBP + 0x28],0x0
1401e5c81: LEA R8,[RBP + 0x28]
1401e5c85: MOV EDI,0x7c027030
1401e5c8a: MOV RCX,RBX
1401e5c8d: MOV EDX,EDI
1401e5c8f: CALL 0x1400099ac
1401e5c94: MOV R8D,dword ptr [RBP + 0x28]
1401e5c98: MOV EDX,EDI
1401e5c9a: MOV RCX,RBX
1401e5c9d: CALL 0x140009a18
1401e5ca2: MOV R8D,0x660077
1401e5ca8: MOV EDX,0x7c0270f0
1401e5cad: MOV RCX,RBX
1401e5cb0: MOV dword ptr [RBP + 0x28],R8D
1401e5cb4: CALL 0x140009a18
1401e5cb9: MOV R8D,0x1100
1401e5cbf: MOV EDX,0x7c0270f4
1401e5cc4: MOV RCX,RBX
1401e5cc7: MOV dword ptr [RBP + 0x28],R8D
1401e5ccb: CALL 0x140009a18
1401e5cd0: MOV R8D,0x30004f
1401e5cd6: MOV EDX,0x7c0270f8
1401e5cdb: MOV RCX,RBX
1401e5cde: MOV dword ptr [RBP + 0x28],R8D
1401e5ce2: CALL 0x140009a18
1401e5ce7: MOV R8D,0x542200
1401e5ced: MOV EDX,0x7c0270fc
1401e5cf2: MOV RCX,RBX
1401e5cf5: MOV dword ptr [RBP + 0x28],R8D
1401e5cf9: CALL 0x140009a18
1401e5cfe: MOV R8D,dword ptr [RBP + 0x30]
1401e5d02: MOV EDX,0x7c024208
1401e5d07: OR R8D,0x5
1401e5d0b: MOV RCX,RBX
1401e5d0e: MOV dword ptr [RBP + 0x30],R8D
1401e5d12: CALL 0x140009a18
1401e5d17: MOV EDI,0x7c0242b4
1401e5d1c: LEA R8,[RBP + 0x38]
1401e5d20: MOV EDX,EDI
1401e5d22: MOV RCX,RBX
1401e5d25: CALL 0x1400099ac
1401e5d2a: MOV R8D,dword ptr [RBP + 0x38]
1401e5d2e: MOV EDX,EDI
1401e5d30: BTS R8D,0x1c
1401e5d35: MOV RCX,RBX
1401e5d38: MOV dword ptr [RBP + 0x38],R8D
1401e5d3c: CALL 0x140009a18
1401e5d41: ADD RSP,0x40
1401e5d45: POP RDI
1401e5d46: POP RBX
1401e5d47: POP RBP
1401e5d48: RET
```

### Decompiled C

```c

void FUN_1401e5be0(longlong param_1,char param_2)

{
  undefined4 local_res10 [2];
  uint local_res18 [2];
  uint local_res20 [2];
  
  local_res20[0] = 0;
  FUN_1401d8724(param_1,0,param_2);
  FUN_1400099ac(param_1,0x7c024208,local_res18);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140007bb4(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x61,&DAT_14023aa70,"MT6639WpdmaConfig"
                  ,0x689,0x7c024208,local_res18[0],0);
  }
  if (param_2 != '\0') {
    if (*(char *)(param_1 + 0x1466a49) != '\0') {
      local_res10[0] = 0;
      FUN_1400099ac(param_1,0x7c027030,local_res10);
      FUN_140009a18(param_1,0x7c027030,local_res10[0]);
      local_res10[0] = 0x660077;
      FUN_140009a18(param_1,0x7c0270f0);
      local_res10[0] = 0x1100;
      FUN_140009a18(param_1,0x7c0270f4);
      local_res10[0] = 0x30004f;
      FUN_140009a18(param_1,0x7c0270f8);
      local_res10[0] = 0x542200;
      FUN_140009a18(param_1,0x7c0270fc);
    }
    local_res18[0] = local_res18[0] | 5;
    FUN_140009a18(param_1,0x7c024208);
  }
  FUN_1400099ac(param_1,0x7c0242b4,local_res20);
  local_res20[0] = local_res20[0] | 0x10000000;
  FUN_140009a18(param_1,0x7c0242b4);
  return;
}


```

## FUN_1401e4580 @ 1401e4580

### Immediates (>=0x10000)

- 0x5ff98
- 0x600a0
- 0xe0238
- 0x200190
- 0x200198
- 0x260128
- 0x26012c
- 0x260130
- 0x260134
- 0x260138
- 0x26013c
- 0x260140
- 0x260144
- 0x260148
- 0x26014c
- 0x260180
- 0x260184
- 0x3fff0000
- 0x7c024300
- 0x7c024304
- 0x7c024308
- 0x7c02430c
- 0x7c024500
- 0x7c024504
- 0x7c024508
- 0x7c02450c
- 0x140225a20
- 0x14023aa70
- 0x140246148

### Disassembly

```asm
1401e4580: MOV qword ptr [RSP + 0x20],RBX
1401e4585: PUSH RBP
1401e4586: PUSH RSI
1401e4587: PUSH RDI
1401e4588: PUSH R12
1401e458a: PUSH R13
1401e458c: PUSH R14
1401e458e: PUSH R15
1401e4590: SUB RSP,0x40
1401e4594: MOV R13,qword ptr [RCX]
1401e4597: LEA RSI,[RCX + 0xe0238]
1401e459e: XOR EBP,EBP
1401e45a0: MOV qword ptr [RSP + 0x90],R13
1401e45a8: LEA R15,[0x140246148]
1401e45af: LEA R12D,[RBP + 0x1]
1401e45b3: MOV EDI,EBP
1401e45b5: SHL EDI,0x4
1401e45b8: CMP EBP,0x2
1401e45bb: JNZ 0x1401e45f3
1401e45bd: MOV RAX,qword ptr [R13 + 0x1f80]
1401e45c4: MOVZX R9D,byte ptr [RAX + 0xd1]
1401e45cc: MOV EDI,R9D
1401e45cf: SHL EDI,0x4
1401e45d2: MOV RCX,qword ptr [0x140246148]
1401e45d9: CMP RCX,R15
1401e45dc: JZ 0x1401e4670
1401e45e2: TEST byte ptr [RCX + 0x2c],R12B
1401e45e6: JZ 0x1401e463c
1401e45e8: CMP byte ptr [RCX + 0x29],0x3
1401e45ec: JC 0x1401e463c
1401e45ee: LEA EDX,[RBP + 0x55]
1401e45f1: JMP 0x1401e4628
1401e45f3: CMP EBP,0x3
1401e45f6: JNZ 0x1401e463c
1401e45f8: MOV RAX,qword ptr [R13 + 0x1f80]
1401e45ff: MOVZX R9D,byte ptr [RAX + 0xd0]
1401e4607: MOV EDI,R9D
1401e460a: SHL EDI,0x4
1401e460d: MOV RCX,qword ptr [0x140246148]
1401e4614: CMP RCX,R15
1401e4617: JZ 0x1401e4670
1401e4619: TEST byte ptr [RCX + 0x2c],R12B
1401e461d: JZ 0x1401e463c
1401e461f: CMP byte ptr [RCX + 0x29],BPL
1401e4623: JC 0x1401e463c
1401e4625: LEA EDX,[RBP + 0x55]
1401e4628: MOV RCX,qword ptr [RCX + 0x18]
1401e462c: LEA R8,[0x14023aa70]
1401e4633: MOV dword ptr [RSP + 0x20],EDI
1401e4637: CALL 0x140007688
1401e463c: MOV RCX,qword ptr [0x140246148]
1401e4643: CMP RCX,R15
1401e4646: JZ 0x1401e4670
1401e4648: TEST byte ptr [RCX + 0x2c],R12B
1401e464c: JZ 0x1401e4670
1401e464e: CMP byte ptr [RCX + 0x29],0x3
1401e4652: JC 0x1401e4670
1401e4654: MOV RCX,qword ptr [RCX + 0x18]
1401e4658: LEA R8,[0x14023aa70]
1401e465f: MOV EDX,0x59
1401e4664: MOV dword ptr [RSP + 0x20],EDI
1401e4668: MOV R9D,EBP
1401e466b: CALL 0x140007688
1401e4670: MOV R14D,dword ptr [RSI + -0x5ff40]
1401e4677: LEA EAX,[RDI + 0x7c024300]
1401e467d: AND dword ptr [RSI + -0x8],0x0
1401e4681: LEA EDX,[RDI + 0x7c02430c]
1401e4687: AND dword ptr [RSI],0x0
1401e468a: LEA R8,[RSI + -0xc]
1401e468e: AND dword ptr [RSI + -0x10],0x0
1401e4692: MOV RCX,R13
1401e4695: MOV dword ptr [RSI + 0x4],EAX
1401e4698: LEA EAX,[RDI + 0x7c024308]
1401e469e: MOV dword ptr [RSI + 0x8],EAX
1401e46a1: LEA EAX,[RDI + 0x7c024304]
1401e46a7: MOV dword ptr [RSI + 0x10],EAX
1401e46aa: MOV dword ptr [RSI + 0xc],EDX
1401e46ad: CALL 0x1400099ac
1401e46b2: MOV R9D,dword ptr [RSI + -0xc]
1401e46b6: AND R9D,0xfff
1401e46bd: MOV dword ptr [RSI + -0x8],R9D
1401e46c1: MOV RCX,qword ptr [0x140246148]
1401e46c8: CMP RCX,R15
1401e46cb: JZ 0x1401e4702
1401e46cd: TEST byte ptr [RCX + 0x2c],R12B
1401e46d1: JZ 0x1401e4702
1401e46d3: CMP byte ptr [RCX + 0x29],0x3
1401e46d7: JC 0x1401e4702
1401e46d9: MOV EAX,dword ptr [RSI + -0x10]
1401e46dc: LEA R8,[0x14023aa70]
1401e46e3: MOV RCX,qword ptr [RCX + 0x18]
1401e46e7: MOV EDX,0x5a
1401e46ec: MOV dword ptr [RSP + 0x30],R9D
1401e46f1: MOV R9D,EBP
1401e46f4: MOV dword ptr [RSP + 0x28],EAX
1401e46f8: MOV dword ptr [RSP + 0x20],R14D
1401e46fd: CALL 0x140007968
1401e4702: MOV EDX,dword ptr [RSI + 0x4]
1401e4705: MOV R8D,R14D
1401e4708: MOV RCX,R13
1401e470b: CALL 0x140009a18
1401e4710: MOV R8D,dword ptr [RSI + -0x10]
1401e4714: MOV RCX,R13
1401e4717: MOV EDX,dword ptr [RSI + 0x8]
1401e471a: CALL 0x140009a18
1401e471f: MOV R8D,dword ptr [RSI + -0x4]
1401e4723: MOV RCX,R13
1401e4726: MOV EDX,dword ptr [RSI + 0x10]
1401e4729: CALL 0x140009a18
1401e472e: ADD EBP,R12D
1401e4731: ADD RSI,0x5ff98
1401e4738: CMP EBP,0x4
1401e473b: JC 0x1401e45b3
1401e4741: XOR EBP,EBP
1401e4743: XOR ESI,ESI
1401e4745: MOV R14,qword ptr [R13 + 0x1f80]
1401e474c: MOV dword ptr [RSP + 0x80],EDI
1401e4753: CMP EBP,R12D
1401e4756: JNZ 0x1401e4781
1401e4758: MOV EDI,0x60
1401e475d: MOV dword ptr [RSP + 0x80],EDI
1401e4764: MOV RCX,qword ptr [0x140246148]
1401e476b: CMP RCX,R15
1401e476e: JZ 0x1401e47e9
1401e4770: TEST byte ptr [RCX + 0x2c],R12B
1401e4774: JZ 0x1401e47e9
1401e4776: CMP byte ptr [RCX + 0x29],0x3
1401e477a: JC 0x1401e47e9
1401e477c: LEA EDX,[RDI + -0x5]
1401e477f: JMP 0x1401e47d6
1401e4781: TEST EBP,EBP
1401e4783: JNZ 0x1401e47ac
1401e4785: LEA EDI,[RBP + 0x40]
1401e4788: MOV dword ptr [RSP + 0x80],EDI
1401e478f: MOV RCX,qword ptr [0x140246148]
1401e4796: CMP RCX,R15
1401e4799: JZ 0x1401e47e9
1401e479b: TEST byte ptr [RCX + 0x2c],R12B
1401e479f: JZ 0x1401e47e9
1401e47a1: CMP byte ptr [RCX + 0x29],0x3
1401e47a5: JC 0x1401e47e9
1401e47a7: LEA EDX,[RBP + 0x5c]
1401e47aa: JMP 0x1401e47d6
1401e47ac: CMP EBP,0x2
1401e47af: JNZ 0x1401e47e9
1401e47b1: LEA EDI,[RBP + 0x6e]
1401e47b4: MOV dword ptr [RSP + 0x80],EDI
1401e47bb: MOV RCX,qword ptr [0x140246148]
1401e47c2: CMP RCX,R15
1401e47c5: JZ 0x1401e47e9
1401e47c7: TEST byte ptr [RCX + 0x2c],R12B
1401e47cb: JZ 0x1401e47e9
1401e47cd: CMP byte ptr [RCX + 0x29],0x3
1401e47d1: JC 0x1401e47e9
1401e47d3: LEA EDX,[RBP + 0x5b]
1401e47d6: MOV RCX,qword ptr [RCX + 0x18]
1401e47da: LEA R8,[0x14023aa70]
1401e47e1: MOV R9D,EDI
1401e47e4: CALL 0x14000764c
1401e47e9: MOV EAX,dword ptr [RSI + R14*0x1 + 0x260138]
1401e47f1: LEA EDX,[RDI + 0x7c02450c]
1401e47f7: MOV R15D,dword ptr [RSI + R14*0x1 + 0x200198]
1401e47ff: LEA RBX,[RSI + R14*0x1]
1401e4803: AND dword ptr [RSI + R14*0x1 + 0x260130],0x0
1401e480c: LEA R8,[RBX + 0x26012c]
1401e4813: SUB EAX,R12D
1401e4816: MOV dword ptr [RSI + R14*0x1 + 0x260144],EDX
1401e481e: MOV dword ptr [RSI + R14*0x1 + 0x260128],EAX
1401e4826: MOV RCX,R13
1401e4829: LEA EAX,[RDI + 0x7c024500]
1401e482f: MOV byte ptr [RSI + R14*0x1 + 0x26014c],0x0
1401e4838: MOV dword ptr [RSI + R14*0x1 + 0x26013c],EAX
1401e4840: LEA EAX,[RDI + 0x7c024508]
1401e4846: MOV dword ptr [RSI + R14*0x1 + 0x260140],EAX
1401e484e: LEA EAX,[RDI + 0x7c024504]
1401e4854: MOV dword ptr [RSI + R14*0x1 + 0x260148],EAX
1401e485c: CALL 0x1400099ac
1401e4861: MOV R9D,dword ptr [RBX + 0x26012c]
1401e4868: AND R9D,0xfff
1401e486f: MOV dword ptr [RSI + R14*0x1 + 0x260130],R9D
1401e4877: MOV RCX,qword ptr [0x140246148]
1401e487e: LEA RAX,[0x140246148]
1401e4885: CMP RCX,RAX
1401e4888: JZ 0x1401e48c4
1401e488a: TEST byte ptr [RCX + 0x2c],R12B
1401e488e: JZ 0x1401e48c4
1401e4890: CMP byte ptr [RCX + 0x29],0x3
1401e4894: JC 0x1401e48c4
1401e4896: MOV EAX,dword ptr [RSI + R14*0x1 + 0x260128]
1401e489e: LEA R8,[0x14023aa70]
1401e48a5: MOV RCX,qword ptr [RCX + 0x18]
1401e48a9: MOV EDX,0x5e
1401e48ae: MOV dword ptr [RSP + 0x30],R9D
1401e48b3: MOV R9D,EBP
1401e48b6: MOV dword ptr [RSP + 0x28],EAX
1401e48ba: MOV dword ptr [RSP + 0x20],R15D
1401e48bf: CALL 0x140007968
1401e48c4: MOV EDX,dword ptr [RSI + R14*0x1 + 0x26013c]
1401e48cc: MOV R8D,R15D
1401e48cf: MOV RCX,R13
1401e48d2: CALL 0x140009a18
1401e48d7: MOV R8D,dword ptr [RSI + R14*0x1 + 0x260128]
1401e48df: MOV RCX,R13
1401e48e2: MOV EDX,dword ptr [RSI + R14*0x1 + 0x260140]
1401e48ea: CALL 0x140009a18
1401e48ef: MOV R8D,dword ptr [RSI + R14*0x1 + 0x260138]
1401e48f7: MOV RCX,R13
1401e48fa: MOV EDX,dword ptr [RSI + R14*0x1 + 0x260148]
1401e4902: CALL 0x140009a18
1401e4907: AND dword ptr [RSI + R14*0x1 + 0x260180],0x0
1401e4910: XOR R15D,R15D
1401e4913: AND dword ptr [RSI + R14*0x1 + 0x260184],0x0
1401e491c: MOV EAX,dword ptr [RSI + R14*0x1 + 0x260138]
1401e4924: TEST EAX,EAX
1401e4926: JZ 0x1401e4a3a
1401e492c: LEA R12,[R14 + 0x200190]
1401e4933: MOV R13D,EAX
1401e4936: ADD R12,RSI
1401e4939: LEA RDI,[0x140246148]
1401e4940: MOV RBX,qword ptr [R12]
1401e4944: MOV EAX,dword ptr [RBX + 0x4]
1401e4947: TEST EAX,EAX
1401e4949: JNS 0x1401e499a
1401e494b: MOV RCX,qword ptr [0x140246148]
1401e4952: CMP RCX,RDI
1401e4955: JZ 0x1401e4990
1401e4957: TEST byte ptr [RCX + 0x2c],0x1
1401e495b: JZ 0x1401e4990
1401e495d: CMP byte ptr [RCX + 0x29],0x2
1401e4961: JC 0x1401e4990
1401e4963: MOV RCX,qword ptr [RCX + 0x18]
1401e4967: LEA R9,[0x140225a20]
1401e496e: MOV dword ptr [RSP + 0x30],R15D
1401e4973: LEA R8,[0x14023aa70]
1401e497a: MOV dword ptr [RSP + 0x28],EBP
1401e497e: MOV EDX,0x5f
1401e4983: MOV dword ptr [RSP + 0x20],0x665
1401e498b: CALL 0x140007d48
1401e4990: MOV EAX,dword ptr [RBX + 0x4]
1401e4993: BTR EAX,0x1f
1401e4997: MOV dword ptr [RBX + 0x4],EAX
1401e499a: MOVZX R8D,word ptr [RBX + 0x6]
1401e499f: AND R8D,0x3fff
1401e49a6: CMP R8D,dword ptr [RSI + R14*0x1 + 0x260134]
1401e49ae: JZ 0x1401e4a15
1401e49b0: MOV RCX,qword ptr [0x140246148]
1401e49b7: CMP RCX,RDI
1401e49ba: JZ 0x1401e49fa
1401e49bc: TEST byte ptr [RCX + 0x2c],0x1
1401e49c0: JZ 0x1401e49fa
1401e49c2: CMP byte ptr [RCX + 0x29],0x2
1401e49c6: JC 0x1401e49fa
1401e49c8: MOV RCX,qword ptr [RCX + 0x18]
1401e49cc: LEA R9,[0x140225a20]
1401e49d3: MOV dword ptr [RSP + 0x38],R8D
1401e49d8: MOV EDX,0x60
1401e49dd: MOV dword ptr [RSP + 0x30],R15D
1401e49e2: LEA R8,[0x14023aa70]
1401e49e9: MOV dword ptr [RSP + 0x28],EBP
1401e49ed: MOV dword ptr [RSP + 0x20],0x66e
1401e49f5: CALL 0x140007bb4
1401e49fa: MOV EAX,dword ptr [RBX + 0x4]
1401e49fd: MOV ECX,dword ptr [RSI + R14*0x1 + 0x260134]
1401e4a05: SHL ECX,0x10
1401e4a08: XOR ECX,EAX
1401e4a0a: AND ECX,0x3fff0000
1401e4a10: XOR ECX,EAX
1401e4a12: MOV dword ptr [RBX + 0x4],ECX
1401e4a15: INC R15D
1401e4a18: ADD R12,0x60
1401e4a1c: CMP R15D,R13D
1401e4a1f: JC 0x1401e4940
1401e4a25: MOV EDI,dword ptr [RSP + 0x80]
1401e4a2c: MOV R12D,0x1
1401e4a32: MOV R13,qword ptr [RSP + 0x90]
1401e4a3a: ADD EBP,R12D
1401e4a3d: LEA R15,[0x140246148]
1401e4a44: ADD RSI,0x600a0
1401e4a4b: CMP EBP,0x3
1401e4a4e: JC 0x1401e4745
1401e4a54: MOV RBX,qword ptr [RSP + 0x98]
1401e4a5c: ADD RSP,0x40
1401e4a60: POP R15
1401e4a62: POP R14
1401e4a64: POP R13
1401e4a66: POP R12
1401e4a68: POP RDI
1401e4a69: POP RSI
1401e4a6a: POP RBP
1401e4a6b: RET
```

### Decompiled C

```c

void FUN_1401e4580(longlong *param_1)

{
  byte bVar1;
  byte bVar2;
  longlong lVar3;
  longlong lVar4;
  longlong lVar5;
  undefined4 uVar6;
  uint uVar7;
  longlong lVar8;
  int iVar9;
  uint uVar10;
  uint uVar11;
  longlong *plVar12;
  uint uVar13;
  int local_res8;
  
  lVar3 = *param_1;
  param_1 = param_1 + 0x1c047;
  uVar7 = 0;
  do {
    iVar9 = uVar7 << 4;
    if (uVar7 == 2) {
      bVar1 = *(byte *)(*(longlong *)(lVar3 + 0x1f80) + 0xd1);
      iVar9 = (uint)bVar1 << 4;
      if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
        if ((PTR_LOOP_140246148[0x2c] & 1) != 0) {
          bVar2 = PTR_LOOP_140246148[0x29];
joined_r0x0001401e4623:
          if (2 < bVar2) {
            WPP_SF_DD(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar7 + 0x55,&DAT_14023aa70,bVar1,
                      iVar9);
          }
        }
        goto LAB_1401e463c;
      }
    }
    else {
      if (uVar7 == 3) {
        bVar1 = *(byte *)(*(longlong *)(lVar3 + 0x1f80) + 0xd0);
        iVar9 = (uint)bVar1 << 4;
        if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) goto LAB_1401e4670;
        if ((PTR_LOOP_140246148[0x2c] & 1) != 0) {
          bVar2 = PTR_LOOP_140246148[0x29];
          goto joined_r0x0001401e4623;
        }
      }
LAB_1401e463c:
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        WPP_SF_DD(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x59,&DAT_14023aa70,uVar7,iVar9);
      }
    }
LAB_1401e4670:
    lVar8 = param_1[-0xbfe8];
    *(undefined4 *)(param_1 + -1) = 0;
    *(undefined4 *)param_1 = 0;
    *(undefined4 *)(param_1 + -2) = 0;
    *(int *)((longlong)param_1 + 4) = iVar9 + 0x7c024300;
    *(int *)(param_1 + 1) = iVar9 + 0x7c024308;
    *(int *)(param_1 + 2) = iVar9 + 0x7c024304;
    *(int *)((longlong)param_1 + 0xc) = iVar9 + 0x7c02430c;
    FUN_1400099ac(lVar3,iVar9 + 0x7c02430c,(undefined4 *)((longlong)param_1 + -0xc));
    uVar11 = *(uint *)((longlong)param_1 + -0xc) & 0xfff;
    *(uint *)(param_1 + -1) = uVar11;
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
      FID_conflict_WPP_SF_LLDD
                (*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x5a,&DAT_14023aa70,uVar7,(int)lVar8,
                 (int)param_1[-2],uVar11);
    }
    FUN_140009a18(lVar3,*(undefined4 *)((longlong)param_1 + 4),(int)lVar8);
    FUN_140009a18(lVar3,(int)param_1[1],(int)param_1[-2]);
    FUN_140009a18(lVar3,(int)param_1[2],*(undefined4 *)((longlong)param_1 + -4));
    uVar7 = uVar7 + 1;
    param_1 = param_1 + 0xbff3;
  } while (uVar7 < 4);
  uVar7 = 0;
  lVar8 = 0;
  do {
    lVar4 = *(longlong *)(lVar3 + 0x1f80);
    if (uVar7 == 1) {
      iVar9 = 0x60;
      local_res8 = 0x60;
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        uVar6 = 0x5b;
LAB_1401e47d6:
        FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar6,&DAT_14023aa70,iVar9);
      }
    }
    else if (uVar7 == 0) {
      iVar9 = 0x40;
      local_res8 = 0x40;
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        uVar6 = 0x5c;
        goto LAB_1401e47d6;
      }
    }
    else {
      local_res8 = iVar9;
      if (uVar7 == 2) {
        iVar9 = 0x70;
        local_res8 = 0x70;
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
          uVar6 = 0x5d;
          goto LAB_1401e47d6;
        }
      }
    }
    uVar6 = *(undefined4 *)(lVar8 + 0x200198 + lVar4);
    *(undefined4 *)(lVar8 + 0x260130 + lVar4) = 0;
    *(int *)(lVar8 + 0x260144 + lVar4) = iVar9 + 0x7c02450c;
    *(int *)(lVar8 + 0x260128 + lVar4) = *(int *)(lVar8 + 0x260138 + lVar4) + -1;
    *(undefined1 *)(lVar8 + 0x26014c + lVar4) = 0;
    *(int *)(lVar8 + 0x26013c + lVar4) = iVar9 + 0x7c024500;
    *(int *)(lVar8 + 0x260140 + lVar4) = iVar9 + 0x7c024508;
    *(int *)(lVar8 + 0x260148 + lVar4) = iVar9 + 0x7c024504;
    FUN_1400099ac(lVar3,iVar9 + 0x7c02450c,lVar8 + lVar4 + 0x26012c);
    uVar11 = *(uint *)(lVar8 + lVar4 + 0x26012c) & 0xfff;
    *(uint *)(lVar8 + 0x260130 + lVar4) = uVar11;
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
      FID_conflict_WPP_SF_LLDD
                (*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x5e,&DAT_14023aa70,uVar7,uVar6,
                 *(undefined4 *)(lVar8 + 0x260128 + lVar4),uVar11);
    }
    FUN_140009a18(lVar3,*(undefined4 *)(lVar8 + 0x26013c + lVar4),uVar6);
    FUN_140009a18(lVar3,*(undefined4 *)(lVar8 + 0x260140 + lVar4),
                  *(undefined4 *)(lVar8 + 0x260128 + lVar4));
    FUN_140009a18(lVar3,*(undefined4 *)(lVar8 + 0x260148 + lVar4),
                  *(undefined4 *)(lVar8 + 0x260138 + lVar4));
    *(undefined4 *)(lVar8 + 0x260180 + lVar4) = 0;
    uVar13 = 0;
    *(undefined4 *)(lVar8 + 0x260184 + lVar4) = 0;
    uVar11 = *(uint *)(lVar8 + 0x260138 + lVar4);
    if (uVar11 != 0) {
      plVar12 = (longlong *)(lVar4 + 0x200190 + lVar8);
      do {
        lVar5 = *plVar12;
        if (*(int *)(lVar5 + 4) < 0) {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x29])) {
            FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x5f,&DAT_14023aa70,
                          "MT6639InitTxRxRing",0x665,uVar7,uVar13);
          }
          *(uint *)(lVar5 + 4) = *(uint *)(lVar5 + 4) & 0x7fffffff;
        }
        uVar10 = *(ushort *)(lVar5 + 6) & 0x3fff;
        if (uVar10 != *(uint *)(lVar8 + 0x260134 + lVar4)) {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x29])) {
            FUN_140007bb4(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x60,&DAT_14023aa70,
                          "MT6639InitTxRxRing",0x66e,uVar7,uVar13,uVar10);
          }
          *(uint *)(lVar5 + 4) =
               (*(int *)(lVar8 + 0x260134 + lVar4) << 0x10 ^ *(uint *)(lVar5 + 4)) & 0x3fff0000 ^
               *(uint *)(lVar5 + 4);
        }
        uVar13 = uVar13 + 1;
        plVar12 = plVar12 + 0xc;
        iVar9 = local_res8;
      } while (uVar13 < uVar11);
    }
    uVar7 = uVar7 + 1;
    lVar8 = lVar8 + 0x600a0;
    if (2 < uVar7) {
      return;
    }
  } while( true );
}


```

## FUN_1401e43e0 @ 1401e43e0

### Immediates (>=0x10000)

- 0x2600f000
- 0x74030188
- 0x7c024204
- 0x7c02422c
- 0xfffffffc
- 0x1402259e0
- 0x14023aa70
- 0x140246148

### Disassembly

```asm
1401e43e0: MOV qword ptr [RSP + 0x18],RSI
1401e43e5: PUSH RDI
1401e43e6: SUB RSP,0x50
1401e43ea: AND dword ptr [RSP + 0x60],0x0
1401e43ef: MOV R8D,0x2600f000
1401e43f5: MOVZX ESI,DL
1401e43f8: MOV RDI,RCX
1401e43fb: MOV AL,SIL
1401e43fe: NEG AL
1401e4400: SBB EDX,EDX
1401e4402: AND EDX,0xfffffffc
1401e4405: ADD EDX,0x7c02422c
1401e440b: CALL 0x140009a18
1401e4410: LEA R8,[RSP + 0x60]
1401e4415: MOV EDX,0x7c024204
1401e441a: MOV RCX,RDI
1401e441d: CALL 0x1400099ac
1401e4422: MOVZX EAX,word ptr [RDI + 0x1f72]
1401e4429: MOV ECX,0x6639
1401e442e: CMP AX,CX
1401e4431: JZ 0x1401e445b
1401e4433: MOV ECX,0x738
1401e4438: CMP AX,CX
1401e443b: JZ 0x1401e445b
1401e443d: MOV ECX,0x7927
1401e4442: CMP AX,CX
1401e4445: JZ 0x1401e445b
1401e4447: MOV ECX,0x7925
1401e444c: CMP AX,CX
1401e444f: JZ 0x1401e445b
1401e4451: MOV ECX,0x717
1401e4456: CMP AX,CX
1401e4459: JNZ 0x1401e449a
1401e445b: AND dword ptr [RSP + 0x68],0x0
1401e4460: LEA R8,[RSP + 0x68]
1401e4465: MOV EDX,0x74030188
1401e446a: MOV RCX,RDI
1401e446d: CALL 0x1400099ac
1401e4472: MOV R8D,dword ptr [RSP + 0x68]
1401e4477: MOV EDX,0x74030188
1401e447c: MOV RCX,RDI
1401e447f: TEST SIL,SIL
1401e4482: JZ 0x1401e448b
1401e4484: BTS R8D,0x10
1401e4489: JMP 0x1401e4490
1401e448b: BTR R8D,0x10
1401e4490: MOV dword ptr [RSP + 0x68],R8D
1401e4495: CALL 0x140009a18
1401e449a: MOV RCX,qword ptr [0x140246148]
1401e44a1: LEA RAX,[0x140246148]
1401e44a8: CMP RCX,RAX
1401e44ab: JZ 0x1401e44f9
1401e44ad: TEST byte ptr [RCX + 0x2c],0x1
1401e44b1: JZ 0x1401e44f9
1401e44b3: CMP byte ptr [RCX + 0x29],0x3
1401e44b7: JC 0x1401e44f9
1401e44b9: MOV EAX,dword ptr [RSP + 0x60]
1401e44bd: LEA R9,[0x1402259e0]
1401e44c4: MOV RCX,qword ptr [RCX + 0x18]
1401e44c8: LEA R8,[0x14023aa70]
1401e44cf: MOV dword ptr [RSP + 0x40],0x2600f000
1401e44d7: MOV EDX,0x40
1401e44dc: MOV dword ptr [RSP + 0x38],ESI
1401e44e0: MOV dword ptr [RSP + 0x30],EAX
1401e44e4: MOV dword ptr [RSP + 0x28],0x7c024204
1401e44ec: MOV dword ptr [RSP + 0x20],0x511
1401e44f4: CALL 0x140015c00
1401e44f9: MOV RSI,qword ptr [RSP + 0x70]
1401e44fe: ADD RSP,0x50
1401e4502: POP RDI
1401e4503: RET
```

### Decompiled C

```c

void FUN_1401e43e0(longlong param_1,char param_2)

{
  short sVar1;
  undefined4 local_res8 [2];
  uint local_res10 [2];
  
  local_res8[0] = 0;
  FUN_140009a18(param_1,(-(uint)(param_2 != '\0') & 0xfffffffc) + 0x7c02422c,0x2600f000);
  FUN_1400099ac(param_1,0x7c024204,local_res8);
  sVar1 = *(short *)(param_1 + 0x1f72);
  if ((((sVar1 == 0x6639) || (sVar1 == 0x738)) || (sVar1 == 0x7927)) ||
     ((sVar1 == 0x7925 || (sVar1 == 0x717)))) {
    local_res10[0] = 0;
    FUN_1400099ac(param_1,0x74030188,local_res10);
    if (param_2 == '\0') {
      local_res10[0] = local_res10[0] & 0xfffeffff;
    }
    else {
      local_res10[0] = local_res10[0] | 0x10000;
    }
    FUN_140009a18(param_1,0x74030188);
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140015c00(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x40,&DAT_14023aa70,
                  "MT6639ConfigIntMask",0x511,0x7c024204,local_res8[0],param_2,0x2600f000);
  }
  return;
}


```

## FUN_1401fe1a4 @ 1401fe1a4

### Immediates (>=0x10000)

- 0x140012
- 0x639ce6
- 0x1464d7f
- 0x1464d87
- 0x1464d8f
- 0x1464d9f
- 0x1464da7
- 0x1464dc7
- 0x1464de7
- 0x146518f
- 0x1465b63
- 0x1465eb4
- 0x1466a49
- 0x1466ad5
- 0x146cde9
- 0x1471b2f
- 0xc0000001
- 0xc00000bb
- 0x140228a90
- 0x140228ab0
- 0x140228ad0
- 0x14023b980
- 0x140246148

### Disassembly

```asm
1401fe1a4: MOV qword ptr [RSP + 0x20],RBX
1401fe1a9: PUSH RBP
1401fe1aa: PUSH RSI
1401fe1ab: PUSH RDI
1401fe1ac: PUSH R12
1401fe1ae: PUSH R13
1401fe1b0: PUSH R14
1401fe1b2: PUSH R15
1401fe1b4: MOV RBP,RSP
1401fe1b7: SUB RSP,0x40
1401fe1bb: MOV R15D,0x1
1401fe1c1: MOV dword ptr [RBP + -0x10],0x140012
1401fe1c8: XOR R12D,R12D
1401fe1cb: MOV dword ptr [RBP + 0x48],R15D
1401fe1cf: LEA RAX,[0x140228a90]
1401fe1d6: MOV dword ptr [RBP + 0x40],R12D
1401fe1da: MOV qword ptr [RBP + -0x8],RAX
1401fe1de: MOV RBX,RCX
1401fe1e1: MOV R14D,0xc0000001
1401fe1e7: MOV RCX,qword ptr [0x140246148]
1401fe1ee: LEA R13,[0x140246148]
1401fe1f5: CMP RCX,R13
1401fe1f8: JZ 0x1401fe223
1401fe1fa: TEST byte ptr [RCX + 0xa4],R15B
1401fe201: JZ 0x1401fe223
1401fe203: CMP byte ptr [RCX + 0xa1],0x5
1401fe20a: JC 0x1401fe223
1401fe20c: MOV RCX,qword ptr [RCX + 0x90]
1401fe213: LEA EDX,[R15 + 0xc]
1401fe217: LEA R8,[0x14023b980]
1401fe21e: CALL 0x1400015d8
1401fe223: MOV RAX,qword ptr [RBX + 0x1464de7]
1401fe22a: MOV ESI,0xc00000bb
1401fe22f: TEST RAX,RAX
1401fe232: JZ 0x1401fe245
1401fe234: MOV RCX,RBX
1401fe237: CALL qword ptr [0x14022a3f8]
1401fe23d: MOV DIL,AL
1401fe240: MOV R8D,R12D
1401fe243: JMP 0x1401fe24b
1401fe245: MOV R8D,ESI
1401fe248: MOV DIL,R12B
1401fe24b: MOV RCX,qword ptr [0x140246148]
1401fe252: CMP RCX,R13
1401fe255: JZ 0x1401fe295
1401fe257: TEST byte ptr [RCX + 0xa4],R15B
1401fe25e: JZ 0x1401fe295
1401fe260: CMP byte ptr [RCX + 0xa1],0x3
1401fe267: JC 0x1401fe295
1401fe269: MOV RCX,qword ptr [RCX + 0x90]
1401fe270: LEA R9,[0x140228ab0]
1401fe277: MOV dword ptr [RSP + 0x28],R8D
1401fe27c: MOV EDX,0xe
1401fe281: MOVZX EAX,DIL
1401fe285: LEA R8,[0x14023b980]
1401fe28c: MOV dword ptr [RSP + 0x20],EAX
1401fe290: CALL 0x140007b24
1401fe295: CMP DIL,R15B
1401fe298: JNZ 0x1401fe37d
1401fe29e: CMP byte ptr [RBX + 0x1471b2f],R15B
1401fe2a5: JNZ 0x1401fe2e2
1401fe2a7: MOV RCX,qword ptr [0x140246148]
1401fe2ae: CMP RCX,R13
1401fe2b1: JZ 0x1401fe2db
1401fe2b3: TEST byte ptr [RCX + 0x2c],R15B
1401fe2b7: JZ 0x1401fe2db
1401fe2b9: CMP byte ptr [RCX + 0x29],0x3
1401fe2bd: JC 0x1401fe2db
1401fe2bf: MOV RCX,qword ptr [RCX + 0x18]
1401fe2c3: LEA R9,[0x140228ab0]
1401fe2ca: MOV EDX,0xf
1401fe2cf: LEA R8,[0x14023b980]
1401fe2d6: CALL 0x140001600
1401fe2db: XOR EAX,EAX
1401fe2dd: JMP 0x1401fe860
1401fe2e2: MOV RCX,qword ptr [0x140246148]
1401fe2e9: CMP RCX,R13
1401fe2ec: JZ 0x1401fe316
1401fe2ee: TEST byte ptr [RCX + 0x2c],R15B
1401fe2f2: JZ 0x1401fe316
1401fe2f4: CMP byte ptr [RCX + 0x29],0x3
1401fe2f8: JC 0x1401fe316
1401fe2fa: MOV RCX,qword ptr [RCX + 0x18]
1401fe2fe: LEA R9,[0x140228ab0]
1401fe305: MOV EDX,0x10
1401fe30a: LEA R8,[0x14023b980]
1401fe311: CALL 0x140001600
1401fe316: MOV RAX,qword ptr [RBX + 0x1464da7]
1401fe31d: MOV byte ptr [RBX + 0x146cde9],R15B
1401fe324: TEST RAX,RAX
1401fe327: JZ 0x1401fe37d
1401fe329: MOV RCX,RBX
1401fe32c: CALL qword ptr [0x14022a3f8]
1401fe332: TEST EAX,EAX
1401fe334: JZ 0x1401fe37a
1401fe336: CMP EAX,ESI
1401fe338: JZ 0x1401fe37d
1401fe33a: MOV RCX,qword ptr [0x140246148]
1401fe341: CMP RCX,R13
1401fe344: JZ 0x1401fe58e
1401fe34a: TEST byte ptr [RCX + 0xa4],R15B
1401fe351: JZ 0x1401fe58e
1401fe357: CMP byte ptr [RCX + 0xa1],R15B
1401fe35e: JC 0x1401fe58e
1401fe364: MOV dword ptr [RSP + 0x28],EAX
1401fe368: MOV EDX,0x11
1401fe36d: MOV dword ptr [RSP + 0x20],0xdd
1401fe375: JMP 0x1401fe574
1401fe37a: MOV DIL,R12B
1401fe37d: MOV RAX,qword ptr [RBX + 0x1464d7f]
1401fe384: TEST RAX,RAX
1401fe387: JZ 0x1401fe3da
1401fe389: MOV RCX,RBX
1401fe38c: CALL qword ptr [0x14022a3f8]
1401fe392: TEST EAX,EAX
1401fe394: JZ 0x1401fe3da
1401fe396: CMP EAX,ESI
1401fe398: JZ 0x1401fe3da
1401fe39a: MOV RCX,qword ptr [0x140246148]
1401fe3a1: CMP RCX,R13
1401fe3a4: JZ 0x1401fe58e
1401fe3aa: TEST byte ptr [RCX + 0xa4],R15B
1401fe3b1: JZ 0x1401fe58e
1401fe3b7: CMP byte ptr [RCX + 0xa1],R15B
1401fe3be: JC 0x1401fe58e
1401fe3c4: MOV dword ptr [RSP + 0x28],EAX
1401fe3c8: MOV EDX,0x12
1401fe3cd: MOV dword ptr [RSP + 0x20],0xeb
1401fe3d5: JMP 0x1401fe574
1401fe3da: TEST DIL,DIL
1401fe3dd: JNZ 0x1401fe59d
1401fe3e3: MOV RAX,qword ptr [RBX + 0x1464d87]
1401fe3ea: MOV byte ptr [RBX + 0x146cde9],R12B
1401fe3f1: MOV byte ptr [RBX + 0x1471b2f],R12B
1401fe3f8: MOV dword ptr [RBX + 0x639ce6],0x2a
1401fe402: TEST RAX,RAX
1401fe405: JZ 0x1401fe458
1401fe407: MOV RCX,RBX
1401fe40a: CALL qword ptr [0x14022a3f8]
1401fe410: TEST EAX,EAX
1401fe412: JZ 0x1401fe458
1401fe414: CMP EAX,ESI
1401fe416: JZ 0x1401fe458
1401fe418: MOV RCX,qword ptr [0x140246148]
1401fe41f: CMP RCX,R13
1401fe422: JZ 0x1401fe58e
1401fe428: TEST byte ptr [RCX + 0xa4],R15B
1401fe42f: JZ 0x1401fe58e
1401fe435: CMP byte ptr [RCX + 0xa1],R15B
1401fe43c: JC 0x1401fe58e
1401fe442: MOV dword ptr [RSP + 0x28],EAX
1401fe446: MOV EDX,0x13
1401fe44b: MOV dword ptr [RSP + 0x20],0xff
1401fe453: JMP 0x1401fe574
1401fe458: MOV RAX,qword ptr [RBX + 0x1464d8f]
1401fe45f: TEST RAX,RAX
1401fe462: JZ 0x1401fe4be
1401fe464: MOV RCX,RBX
1401fe467: CALL qword ptr [0x14022a3f8]
1401fe46d: TEST EAX,EAX
1401fe46f: JZ 0x1401fe4be
1401fe471: CMP EAX,ESI
1401fe473: JZ 0x1401fe4be
1401fe475: MOV RCX,qword ptr [0x140246148]
1401fe47c: CMP RCX,R13
1401fe47f: JZ 0x1401fe4fb
1401fe481: TEST byte ptr [RCX + 0xa4],R15B
1401fe488: JZ 0x1401fe4be
1401fe48a: CMP byte ptr [RCX + 0xa1],R15B
1401fe491: JC 0x1401fe4be
1401fe493: MOV RCX,qword ptr [RCX + 0x90]
1401fe49a: LEA R9,[0x140228ab0]
1401fe4a1: MOV dword ptr [RSP + 0x28],EAX
1401fe4a5: LEA R8,[0x14023b980]
1401fe4ac: MOV EDX,0x14
1401fe4b1: MOV dword ptr [RSP + 0x20],0x107
1401fe4b9: CALL 0x140007b24
1401fe4be: MOV RCX,qword ptr [0x140246148]
1401fe4c5: CMP RCX,R13
1401fe4c8: JZ 0x1401fe4fb
1401fe4ca: TEST byte ptr [RCX + 0x2c0],R15B
1401fe4d1: JZ 0x1401fe4fb
1401fe4d3: CMP byte ptr [RCX + 0x2bd],0x4
1401fe4da: JC 0x1401fe4fb
1401fe4dc: MOV RCX,qword ptr [RCX + 0x2ac]
1401fe4e3: LEA R9,[0x140228ab0]
1401fe4ea: MOV EDX,0x15
1401fe4ef: LEA R8,[0x14023b980]
1401fe4f6: CALL 0x140001600
1401fe4fb: MOVZX EAX,word ptr [RBX + 0x1f72]
1401fe502: MOV ECX,0x6639
1401fe507: CMP AX,CX
1401fe50a: JZ 0x1401fe520
1401fe50c: MOV ECX,0x738
1401fe511: CMP AX,CX
1401fe514: JZ 0x1401fe520
1401fe516: MOV ECX,0x7927
1401fe51b: CMP AX,CX
1401fe51e: JNZ 0x1401fe528
1401fe520: MOV RCX,RBX
1401fe523: CALL 0x1401e01f8
1401fe528: MOV RAX,qword ptr [RBX + 0x1464d9f]
1401fe52f: TEST RAX,RAX
1401fe532: JZ 0x1401fe596
1401fe534: MOV RCX,RBX
1401fe537: CALL qword ptr [0x14022a3f8]
1401fe53d: TEST EAX,EAX
1401fe53f: JZ 0x1401fe596
1401fe541: CMP EAX,ESI
1401fe543: JZ 0x1401fe596
1401fe545: MOV RCX,qword ptr [0x140246148]
1401fe54c: CMP RCX,R13
1401fe54f: JZ 0x1401fe58e
1401fe551: TEST byte ptr [RCX + 0xa4],R15B
1401fe558: JZ 0x1401fe58e
1401fe55a: CMP byte ptr [RCX + 0xa1],R15B
1401fe561: JC 0x1401fe58e
1401fe563: MOV dword ptr [RSP + 0x28],EAX
1401fe567: MOV EDX,0x16
1401fe56c: MOV dword ptr [RSP + 0x20],0x117
1401fe574: MOV RCX,qword ptr [RCX + 0x90]
1401fe57b: LEA R9,[0x140228ab0]
1401fe582: LEA R8,[0x14023b980]
1401fe589: CALL 0x140007b24
1401fe58e: MOV EAX,R14D
1401fe591: JMP 0x1401fe860
1401fe596: MOV byte ptr [RBX + 0x146cde9],R15B
1401fe59d: MOV RAX,qword ptr [0x14025ff78]
1401fe5a4: LEA R9,[RBP + 0x50]
1401fe5a8: MOV RDX,qword ptr [RBX + 0x10]
1401fe5ac: XOR R8D,R8D
1401fe5af: MOV RCX,qword ptr [0x14025c960]
1401fe5b6: CALL qword ptr [0x14022a3f8]
1401fe5bc: MOV RAX,qword ptr [0x14025ff28]
1401fe5c3: LEA RCX,[RBP + 0x40]
1401fe5c7: MOV RDX,qword ptr [RBP + 0x50]
1401fe5cb: LEA R9,[RBP + -0x10]
1401fe5cf: MOV qword ptr [RSP + 0x20],RCX
1401fe5d4: XOR R8D,R8D
1401fe5d7: MOV RCX,qword ptr [0x14025c960]
1401fe5de: CALL qword ptr [0x14022a3f8]
1401fe5e4: MOVZX EAX,word ptr [RBX + 0x1f72]
1401fe5eb: MOV ECX,0x7925
1401fe5f0: CMP AX,CX
1401fe5f3: JZ 0x1401fe603
1401fe5f5: MOV ECX,0x717
1401fe5fa: CMP AX,CX
1401fe5fd: JNZ 0x1401fe6a2
1401fe603: CMP dword ptr [RBP + 0x40],R15D
1401fe607: JNZ 0x1401fe653
1401fe609: MOV RCX,qword ptr [0x140246148]
1401fe610: CMP RCX,R13
1401fe613: JZ 0x1401fe64e
1401fe615: TEST byte ptr [RCX + 0xa4],R15B
1401fe61c: JZ 0x1401fe64e
1401fe61e: CMP byte ptr [RCX + 0xa1],0x3
1401fe625: JC 0x1401fe64e
1401fe627: MOV RCX,qword ptr [RCX + 0x90]
1401fe62e: LEA R9,[0x140228ab0]
1401fe635: MOV EDX,0x17
1401fe63a: MOV dword ptr [RSP + 0x20],0x12f
1401fe642: LEA R8,[0x14023b980]
1401fe649: CALL 0x140001664
1401fe64e: MOV DL,R15B
1401fe651: JMP 0x1401fe69a
1401fe653: MOV RCX,qword ptr [0x140246148]
1401fe65a: CMP RCX,R13
1401fe65d: JZ 0x1401fe698
1401fe65f: TEST byte ptr [RCX + 0xa4],R15B
1401fe666: JZ 0x1401fe698
1401fe668: CMP byte ptr [RCX + 0xa1],0x3
1401fe66f: JC 0x1401fe698
1401fe671: MOV RCX,qword ptr [RCX + 0x90]
1401fe678: LEA R9,[0x140228ab0]
1401fe67f: MOV EDX,0x18
1401fe684: MOV dword ptr [RSP + 0x20],0x134
1401fe68c: LEA R8,[0x14023b980]
1401fe693: CALL 0x140001664
1401fe698: XOR EDX,EDX
1401fe69a: MOV RCX,RBX
1401fe69d: CALL 0x1400d3040
1401fe6a2: MOV RCX,RBX
1401fe6a5: CALL 0x1401e01f8
1401fe6aa: MOV RAX,qword ptr [RBX + 0x1464dc7]
1401fe6b1: TEST RAX,RAX
1401fe6b4: JZ 0x1401fe6c1
1401fe6b6: MOV RCX,RBX
1401fe6b9: CALL qword ptr [0x14022a3f8]
1401fe6bf: MOV ESI,EAX
1401fe6c1: CMP byte ptr [RBX + 0x1466a49],R12B
1401fe6c8: JZ 0x1401fe6da
1401fe6ca: CALL 0x1401dfab8
1401fe6cf: MOV RCX,RBX
1401fe6d2: CALL 0x1401dfe20
1401fe6d7: MOV R14D,EAX
1401fe6da: MOV RCX,RBX
1401fe6dd: CALL 0x1400d1484
1401fe6e2: CMP byte ptr [RBX + 0x1466a49],R12B
1401fe6e9: JZ 0x1401fe7c2
1401fe6ef: TEST R14D,R14D
1401fe6f2: JNZ 0x1401fe7c2
1401fe6f8: MOV RDX,qword ptr [RBX + 0x14c0]
1401fe6ff: MOV RCX,RBX
1401fe702: CALL 0x1400b7324
1401fe707: MOV RDX,qword ptr [RBX + 0x14c0]
1401fe70e: MOV RCX,RBX
1401fe711: CALL 0x1400b7424
1401fe716: CALL 0x1401e0024
1401fe71b: LEA RDX,[RBP + 0x48]
1401fe71f: LEA RCX,[0x140228ad0]
1401fe726: CALL 0x140001790
1401fe72b: TEST EAX,EAX
1401fe72d: JZ 0x1401fe76a
1401fe72f: MOV RCX,qword ptr [0x140246148]
1401fe736: CMP RCX,R13
1401fe739: JZ 0x1401fe76a
1401fe73b: TEST byte ptr [RCX + 0x2c],R15B
1401fe73f: JZ 0x1401fe76a
1401fe741: CMP byte ptr [RCX + 0x29],R15B
1401fe745: JC 0x1401fe76a
1401fe747: MOV RCX,qword ptr [RCX + 0x18]
1401fe74b: LEA EDX,[R14 + 0x19]
1401fe74f: LEA R9,[0x140228ab0]
1401fe756: MOV dword ptr [RSP + 0x20],0x14b
1401fe75e: LEA R8,[0x14023b980]
1401fe765: CALL 0x140001664
1401fe76a: CMP byte ptr [RBP + 0x48],R15B
1401fe76e: SETZ R8B
1401fe772: MOV RDX,qword ptr [RBX + 0x14c0]
1401fe779: MOV RCX,RBX
1401fe77c: CALL 0x1400b751c
1401fe781: MOV RCX,qword ptr [0x140246148]
1401fe788: CMP RCX,R13
1401fe78b: JZ 0x1401fe7bd
1401fe78d: TEST byte ptr [RCX + 0x2c],R15B
1401fe791: JZ 0x1401fe7bd
1401fe793: CMP byte ptr [RCX + 0x29],0x3
1401fe797: JC 0x1401fe7bd
1401fe799: MOV RCX,qword ptr [RCX + 0x18]
1401fe79d: LEA R9,[0x140228ab0]
1401fe7a4: MOV EDX,0x1b
1401fe7a9: MOV dword ptr [RSP + 0x20],0x157
1401fe7b1: LEA R8,[0x14023b980]
1401fe7b8: CALL 0x140001664
1401fe7bd: CALL 0x1401dfbc0
1401fe7c2: CMP byte ptr [RBX + 0x1465eb4],R12B
1401fe7c9: JZ 0x1401fe7f1
1401fe7cb: CMP byte ptr [RBX + 0x1465b63],R12B
1401fe7d2: JZ 0x1401fe7f1
1401fe7d4: MOVZX EAX,byte ptr [RBX + 0x146518f]
1401fe7db: TEST AL,AL
1401fe7dd: JZ 0x1401fe7f1
1401fe7df: CMP dword ptr [0x140260e04],EAX
1401fe7e5: JC 0x1401fe7f1
1401fe7e7: XOR EDX,EDX
1401fe7e9: MOV RCX,RBX
1401fe7ec: CALL 0x1400c9dbc
1401fe7f1: MOV RCX,RBX
1401fe7f4: MOV RDX,qword ptr [RBX + 0x14c0]
1401fe7fb: CMP byte ptr [RBX + 0x1466ad5],R12B
1401fe802: JZ 0x1401fe81d
1401fe804: MOV R8B,R15B
1401fe807: CALL 0x1400b676c
1401fe80c: MOV RDX,qword ptr [RBX + 0x14c0]
1401fe813: MOV RCX,RBX
1401fe816: CALL 0x1400b6be0
1401fe81b: JMP 0x1401fe825
1401fe81d: XOR R8D,R8D
1401fe820: CALL 0x1400b676c
1401fe825: MOV RCX,qword ptr [0x140246148]
1401fe82c: CMP RCX,R13
1401fe82f: JZ 0x1401fe85e
1401fe831: TEST byte ptr [RCX + 0xa4],R15B
1401fe838: JZ 0x1401fe85e
1401fe83a: CMP byte ptr [RCX + 0xa1],0x5
1401fe841: JC 0x1401fe85e
1401fe843: MOV RCX,qword ptr [RCX + 0x90]
1401fe84a: LEA R8,[0x14023b980]
1401fe851: MOV EDX,0x1c
1401fe856: MOV R9D,ESI
1401fe859: CALL 0x14000764c
1401fe85e: MOV EAX,ESI
1401fe860: MOV RBX,qword ptr [RSP + 0x98]
1401fe868: ADD RSP,0x40
1401fe86c: POP R15
1401fe86e: POP R14
1401fe870: POP R13
1401fe872: POP R12
1401fe874: POP RDI
1401fe875: POP RSI
1401fe876: POP RBP
1401fe877: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

undefined4 FUN_1401fe1a4(longlong param_1)

{
  short sVar1;
  uint uVar2;
  int iVar3;
  undefined4 uVar4;
  int iVar5;
  undefined8 uVar6;
  char cVar7;
  char cVar8;
  undefined4 uVar9;
  int local_res8 [2];
  undefined4 local_res10 [2];
  undefined8 local_res18;
  undefined8 in_stack_ffffffffffffffa8;
  undefined8 uVar10;
  undefined4 uVar12;
  int *piVar11;
  undefined4 local_48 [2];
  wchar_t *local_40;
  
  uVar12 = (undefined4)((ulonglong)in_stack_ffffffffffffffa8 >> 0x20);
  local_48[0] = 0x140012;
  local_res10[0] = 1;
  local_res8[0] = 0;
  local_40 = L"WPPEnable";
  iVar5 = -0x3fffffff;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xd,&DAT_14023b980);
  }
  uVar4 = 0xc00000bb;
  if (*(code **)(param_1 + 0x1464de7) == (code *)0x0) {
    uVar9 = 0xc00000bb;
    uVar2 = 0;
  }
  else {
    uVar2 = (**(code **)(param_1 + 0x1464de7))(param_1);
    uVar2 = uVar2 & 0xff;
    uVar9 = 0;
  }
  cVar7 = (char)uVar2;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
    uVar6 = CONCAT44(uVar12,uVar2);
    FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe,&DAT_14023b980,
                  "HalDownloadPatchFirmware",uVar6,uVar9);
    uVar12 = (undefined4)((ulonglong)uVar6 >> 0x20);
  }
  cVar8 = cVar7;
  if (cVar7 == '\x01') {
    if (*(char *)(param_1 + 0x1471b2f) == '\x01') {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xf,&DAT_14023b980,
                 "HalDownloadPatchFirmware");
      }
      return 0;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x10,&DAT_14023b980,
               "HalDownloadPatchFirmware");
    }
    *(undefined1 *)(param_1 + 0x146cde9) = 1;
    if (((*(code **)(param_1 + 0x1464da7) != (code *)0x0) &&
        (iVar3 = (**(code **)(param_1 + 0x1464da7))(param_1), cVar8 = '\0', iVar3 != 0)) &&
       (cVar8 = cVar7, iVar3 != -0x3fffff45)) {
      if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
        return 0xc0000001;
      }
      if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
        return 0xc0000001;
      }
      if (PTR_LOOP_140246148[0xa1] == '\0') {
        return 0xc0000001;
      }
      uVar6 = 0x11;
      uVar10 = CONCAT44(uVar12,0xdd);
      goto LAB_1401fe574;
    }
  }
  if (((*(code **)(param_1 + 0x1464d7f) != (code *)0x0) &&
      (iVar3 = (**(code **)(param_1 + 0x1464d7f))(param_1), iVar3 != 0)) && (iVar3 != -0x3fffff45))
  {
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return 0xc0000001;
    }
    if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
      return 0xc0000001;
    }
    if (PTR_LOOP_140246148[0xa1] == '\0') {
      return 0xc0000001;
    }
    uVar6 = 0x12;
    uVar10 = CONCAT44(uVar12,0xeb);
    goto LAB_1401fe574;
  }
  if (cVar8 != '\0') goto LAB_1401fe59d;
  *(undefined1 *)(param_1 + 0x146cde9) = 0;
  *(undefined1 *)(param_1 + 0x1471b2f) = 0;
  *(undefined4 *)(param_1 + 0x639ce6) = 0x2a;
  if (((*(code **)(param_1 + 0x1464d87) != (code *)0x0) &&
      (iVar3 = (**(code **)(param_1 + 0x1464d87))(param_1), iVar3 != 0)) && (iVar3 != -0x3fffff45))
  {
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return 0xc0000001;
    }
    if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
      return 0xc0000001;
    }
    if (PTR_LOOP_140246148[0xa1] == '\0') {
      return 0xc0000001;
    }
    uVar6 = 0x13;
    uVar10 = CONCAT44(uVar12,0xff);
    goto LAB_1401fe574;
  }
  if (((*(code **)(param_1 + 0x1464d8f) == (code *)0x0) ||
      (iVar3 = (**(code **)(param_1 + 0x1464d8f))(param_1), iVar3 == 0)) || (iVar3 == -0x3fffff45))
  {
LAB_1401fe4be:
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c0] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0x2bd])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x2ac),0x15,&DAT_14023b980,
               "HalDownloadPatchFirmware");
    }
  }
  else if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
    if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (PTR_LOOP_140246148[0xa1] != '\0')) {
      uVar6 = CONCAT44(uVar12,0x107);
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x14,&DAT_14023b980,
                    "HalDownloadPatchFirmware",uVar6,iVar3);
      uVar12 = (undefined4)((ulonglong)uVar6 >> 0x20);
    }
    goto LAB_1401fe4be;
  }
  sVar1 = *(short *)(param_1 + 0x1f72);
  if (((sVar1 == 0x6639) || (sVar1 == 0x738)) || (sVar1 == 0x7927)) {
    FUN_1401e01f8(param_1);
  }
  if (((*(code **)(param_1 + 0x1464d9f) == (code *)0x0) ||
      (iVar3 = (**(code **)(param_1 + 0x1464d9f))(param_1), iVar3 == 0)) || (iVar3 == -0x3fffff45))
  {
    *(undefined1 *)(param_1 + 0x146cde9) = 1;
LAB_1401fe59d:
    (*DAT_14025ff78)(DAT_14025c960,*(undefined8 *)(param_1 + 0x10),0,&local_res18);
    piVar11 = local_res8;
    (*DAT_14025ff28)(DAT_14025c960,local_res18,0,local_48,piVar11);
    uVar12 = (undefined4)((ulonglong)piVar11 >> 0x20);
    if ((*(short *)(param_1 + 0x1f72) == 0x7925) || (*(short *)(param_1 + 0x1f72) == 0x717)) {
      if (local_res8[0] == 1) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
          uVar6 = CONCAT44(uVar12,0x12f);
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x17,&DAT_14023b980,
                        "HalDownloadPatchFirmware",uVar6);
          uVar12 = (undefined4)((ulonglong)uVar6 >> 0x20);
        }
        uVar6 = 1;
      }
      else {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
          uVar6 = CONCAT44(uVar12,0x134);
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x18,&DAT_14023b980,
                        "HalDownloadPatchFirmware",uVar6);
          uVar12 = (undefined4)((ulonglong)uVar6 >> 0x20);
        }
        uVar6 = 0;
      }
      FUN_1400d3040(param_1,uVar6);
    }
    FUN_1401e01f8(param_1);
    if (*(code **)(param_1 + 0x1464dc7) != (code *)0x0) {
      uVar4 = (**(code **)(param_1 + 0x1464dc7))(param_1);
    }
    if (*(char *)(param_1 + 0x1466a49) != '\0') {
      FUN_1401dfab8();
      iVar5 = FUN_1401dfe20(param_1);
    }
    FUN_1400d1484(param_1);
    if ((*(char *)(param_1 + 0x1466a49) != '\0') && (iVar5 == 0)) {
      FUN_1400b7324(param_1,*(undefined8 *)(param_1 + 0x14c0));
      FUN_1400b7424(param_1,*(undefined8 *)(param_1 + 0x14c0));
      FUN_1401e0024();
      iVar5 = FUN_140001790(L"FwSramEvtBlock",local_res10);
      if ((iVar5 != 0) &&
         ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148 &&
           ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')))) {
        uVar6 = CONCAT44(uVar12,0x14b);
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x19,&DAT_14023b980,
                      "HalDownloadPatchFirmware",uVar6);
        uVar12 = (undefined4)((ulonglong)uVar6 >> 0x20);
      }
      FUN_1400b751c(param_1,*(undefined8 *)(param_1 + 0x14c0),(char)local_res10[0] == '\x01');
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x1b,&DAT_14023b980,
                      "HalDownloadPatchFirmware",CONCAT44(uVar12,0x157));
      }
      FUN_1401dfbc0();
    }
    if (((*(char *)(param_1 + 0x1465eb4) != '\0') && (*(char *)(param_1 + 0x1465b63) != '\0')) &&
       ((*(byte *)(param_1 + 0x146518f) != 0 && (*(byte *)(param_1 + 0x146518f) <= DAT_140260e04))))
    {
      FUN_1400c9dbc(param_1,0);
    }
    if (*(char *)(param_1 + 0x1466ad5) == '\0') {
      FUN_1400b676c(param_1,*(undefined8 *)(param_1 + 0x14c0),0);
    }
    else {
      FUN_1400b676c(param_1,*(undefined8 *)(param_1 + 0x14c0),1);
      FUN_1400b6be0(param_1,*(undefined8 *)(param_1 + 0x14c0));
    }
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return uVar4;
    }
    if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
      return uVar4;
    }
    if (4 < (byte)PTR_LOOP_140246148[0xa1]) {
      FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1c,&DAT_14023b980,uVar4);
      return uVar4;
    }
    return uVar4;
  }
  if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
    return 0xc0000001;
  }
  if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
    return 0xc0000001;
  }
  if (PTR_LOOP_140246148[0xa1] == '\0') {
    return 0xc0000001;
  }
  uVar6 = 0x16;
  uVar10 = CONCAT44(uVar12,0x117);
LAB_1401fe574:
  FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),uVar6,&DAT_14023b980,
                "HalDownloadPatchFirmware",uVar10,iVar3);
  return 0xc0000001;
}


```

## FUN_1401d13e0 @ 1401d13e0

### Immediates (>=0x10000)

- 0x35511c
- 0x35513c
- 0x355154
- 0x35515c
- 0x1464d29
- 0x1464d2d
- 0x1465087
- 0x146694c
- 0x35414b4d
- 0xc0000001
- 0xffffffff
- 0x1402248a0
- 0x1402249f0
- 0x14023a980
- 0x140246148
- 0x14025e5f9

### Disassembly

```asm
1401d13e0: PUSH RBP
1401d13e2: PUSH RBX
1401d13e3: PUSH RSI
1401d13e4: PUSH RDI
1401d13e5: PUSH R12
1401d13e7: PUSH R13
1401d13e9: PUSH R14
1401d13eb: PUSH R15
1401d13ed: MOV RBP,RSP
1401d13f0: SUB RSP,0x68
1401d13f4: OR dword ptr [RBP + -0x30],0xffffffff
1401d13f8: XOR ESI,ESI
1401d13fa: OR dword ptr [RBP + -0x2c],0xffffffff
1401d13fe: XORPS XMM0,XMM0
1401d1401: XORPS XMM1,XMM1
1401d1404: MOV dword ptr [RBP + 0x58],ESI
1401d1407: MOV dword ptr [RBP + 0x50],ESI
1401d140a: MOV R14B,SIL
1401d140d: MOV qword ptr [RBP + 0x60],RSI
1401d1411: MOV RDI,RCX
1401d1414: MOVUPS xmmword ptr [RBP + -0x18],XMM0
1401d1418: MOV qword ptr [RBP + -0x38],RSI
1401d141c: MOVUPS xmmword ptr [RBP + -0x28],XMM1
1401d1420: MOV dword ptr [RBP + 0x48],0xc0000001
1401d1427: MOV RCX,qword ptr [0x140246148]
1401d142e: LEA R12,[0x140246148]
1401d1435: LEA R13,[0x14023a980]
1401d143c: LEA R15D,[RSI + 0x1]
1401d1440: CMP RCX,R12
1401d1443: JZ 0x1401d146b
1401d1445: TEST byte ptr [RCX + 0xa4],R15B
1401d144c: JZ 0x1401d146b
1401d144e: CMP byte ptr [RCX + 0xa1],0x5
1401d1455: JC 0x1401d146b
1401d1457: MOV RCX,qword ptr [RCX + 0x90]
1401d145e: MOV EDX,0xc4
1401d1463: MOV R8,R13
1401d1466: CALL 0x1400015d8
1401d146b: TEST dword ptr [RDI + 0x1310],0x100
1401d1475: JZ 0x1401d151b
1401d147b: MOV RCX,qword ptr [0x140246148]
1401d1482: CMP RCX,R12
1401d1485: JZ 0x1401d14ba
1401d1487: TEST byte ptr [RCX + 0x2c],R15B
1401d148b: JZ 0x1401d14ba
1401d148d: CMP byte ptr [RCX + 0x29],R15B
1401d1491: JC 0x1401d14ba
1401d1493: MOV EAX,dword ptr [RBP + 0x48]
1401d1496: MOV EDX,0xc5
1401d149b: MOV dword ptr [RSP + 0x28],EAX
1401d149f: MOV dword ptr [RSP + 0x20],0x8b9
1401d14a7: MOV RCX,qword ptr [RCX + 0x18]
1401d14ab: LEA R9,[0x1402249f0]
1401d14b2: MOV R8,R13
1401d14b5: CALL 0x140007b24
1401d14ba: MOV RCX,qword ptr [RBP + -0x38]
1401d14be: TEST RCX,RCX
1401d14c1: JZ 0x1401d14d1
1401d14c3: CALL 0x14020d50e
1401d14c8: MOV RCX,qword ptr [RBP + -0x38]
1401d14cc: CALL 0x14020d502
1401d14d1: MOV RCX,qword ptr [0x140246148]
1401d14d8: CMP RCX,R12
1401d14db: JZ 0x1401d1507
1401d14dd: TEST byte ptr [RCX + 0xa4],R15B
1401d14e4: JZ 0x1401d1507
1401d14e6: CMP byte ptr [RCX + 0xa1],0x5
1401d14ed: JC 0x1401d1507
1401d14ef: MOV R9D,dword ptr [RBP + 0x48]
1401d14f3: MOV EDX,0xe9
1401d14f8: MOV RCX,qword ptr [RCX + 0x90]
1401d14ff: MOV R8,R13
1401d1502: CALL 0x14000764c
1401d1507: MOV EAX,dword ptr [RBP + 0x48]
1401d150a: ADD RSP,0x68
1401d150e: POP R15
1401d1510: POP R14
1401d1512: POP R13
1401d1514: POP R12
1401d1516: POP RDI
1401d1517: POP RSI
1401d1518: POP RBX
1401d1519: POP RBP
1401d151a: RET
1401d151b: MOV R9,qword ptr [RDI + 0x1465087]
1401d1522: TEST R9,R9
1401d1525: JNZ 0x1401d155c
1401d1527: MOV RCX,qword ptr [0x140246148]
1401d152e: CMP RCX,R12
1401d1531: JZ 0x1401d14ba
1401d1533: TEST byte ptr [RCX + 0x2c],R15B
1401d1537: JZ 0x1401d14ba
1401d1539: CMP byte ptr [RCX + 0x29],R15B
1401d153d: JC 0x1401d14ba
1401d1543: MOV EAX,dword ptr [RBP + 0x48]
1401d1546: MOV EDX,0xc6
1401d154b: MOV dword ptr [RSP + 0x28],EAX
1401d154f: MOV dword ptr [RSP + 0x20],0x8bf
1401d1557: JMP 0x1401d14a7
1401d155c: MOV RCX,qword ptr [0x140246148]
1401d1563: CMP RCX,R12
1401d1566: JZ 0x1401d15a5
1401d1568: TEST byte ptr [RCX + 0xa4],R15B
1401d156f: JZ 0x1401d15a5
1401d1571: CMP byte ptr [RCX + 0xa1],0x3
1401d1578: JC 0x1401d15a5
1401d157a: MOVZX R8D,byte ptr [0x14025e5f8]
1401d1582: MOV EDX,0xc7
1401d1587: MOV EAX,dword ptr [0x14025e601]
1401d158d: MOV RCX,qword ptr [RCX + 0x90]
1401d1594: MOV dword ptr [RSP + 0x28],EAX
1401d1598: MOV dword ptr [RSP + 0x20],R8D
1401d159d: MOV R8,R13
1401d15a0: CALL 0x140007b24
1401d15a5: MOV AL,byte ptr [0x14025e5f8]
1401d15ab: LEA RBX,[0x1402249f0]
1401d15b2: CMP AL,R15B
1401d15b5: JNZ 0x1401d15cc
1401d15b7: CMP dword ptr [0x14025e601],ESI
1401d15bd: JNZ 0x1401d1b8d
1401d15c3: MOV AL,SIL
1401d15c6: MOV byte ptr [0x14025e5f8],AL
1401d15cc: TEST AL,AL
1401d15ce: JNZ 0x1401d1b8d
1401d15d4: CMP dword ptr [RDI + 0x146694c],R15D
1401d15db: JNZ 0x1401d17c7
1401d15e1: MOV RCX,qword ptr [0x140246148]
1401d15e8: CMP RCX,R12
1401d15eb: JZ 0x1401d161a
1401d15ed: TEST byte ptr [RCX + 0xa4],R15B
1401d15f4: JZ 0x1401d161a
1401d15f6: CMP byte ptr [RCX + 0xa1],0x3
1401d15fd: JC 0x1401d161a
1401d15ff: MOV R9,qword ptr [RDI + 0x1465087]
1401d1606: MOV EDX,0xc8
1401d160b: MOV RCX,qword ptr [RCX + 0x90]
1401d1612: MOV R8,R13
1401d1615: CALL 0x140001600
1401d161a: MOV RDX,qword ptr [RDI + 0x1465087]
1401d1621: LEA R8,[RBP + 0x50]
1401d1625: MOV RCX,RDI
1401d1628: CALL 0x1401ff1e0
1401d162d: MOV qword ptr [RBP + 0x60],RAX
1401d1631: TEST RAX,RAX
1401d1634: JZ 0x1401d177e
1401d163a: MOV RCX,qword ptr [0x140246148]
1401d1641: CMP RCX,R12
1401d1644: JZ 0x1401d1666
1401d1646: TEST byte ptr [RCX + 0x2c],R15B
1401d164a: JZ 0x1401d1666
1401d164c: CMP byte ptr [RCX + 0x29],0x3
1401d1650: JC 0x1401d1666
1401d1652: MOV RCX,qword ptr [RCX + 0x18]
1401d1656: MOV EDX,0xc9
1401d165b: MOV R9,RBX
1401d165e: MOV R8,R13
1401d1661: CALL 0x140001600
1401d1666: MOV EDX,dword ptr [RBP + 0x50]
1401d1669: MOV R14B,R15B
1401d166c: TEST EDX,EDX
1401d166e: JNZ 0x1401d16b5
1401d1670: MOV RCX,qword ptr [0x140246148]
1401d1677: CMP RCX,R12
1401d167a: JZ 0x1401d1f3c
1401d1680: TEST byte ptr [RCX + 0x2c],R15B
1401d1684: JZ 0x1401d1f3c
1401d168a: CMP byte ptr [RCX + 0x29],R15B
1401d168e: JC 0x1401d1f3c
1401d1694: MOV EDX,0xcb
1401d1699: MOV dword ptr [RSP + 0x20],0x8e0
1401d16a1: MOV RCX,qword ptr [RCX + 0x18]
1401d16a5: MOV R9,RBX
1401d16a8: MOV R8,R13
1401d16ab: CALL 0x140001664
1401d16b0: JMP 0x1401d1f3c
1401d16b5: LEA R9,[0x1402248a0]
1401d16bc: MOV dword ptr [0x14025e601],EDX
1401d16c2: MOV R8D,0x35414b4d
1401d16c8: MOV dword ptr [RSP + 0x20],0x8e6
1401d16d0: LEA RCX,[0x14025e5f9]
1401d16d7: CALL 0x14000feac
1401d16dc: MOV RCX,qword ptr [0x14025e5f9]
1401d16e3: TEST RCX,RCX
1401d16e6: JNZ 0x1401d1740
1401d16e8: MOV RCX,qword ptr [0x140246148]
1401d16ef: CMP RCX,R12
1401d16f2: JZ 0x1401d1f3c
1401d16f8: TEST byte ptr [RCX + 0x4a0],R15B
1401d16ff: JZ 0x1401d1f3c
1401d1705: CMP byte ptr [RCX + 0x49d],R15B
1401d170c: JC 0x1401d1f3c
1401d1712: MOV EAX,dword ptr [0x14025e601]
1401d1718: MOV EDX,0xcc
1401d171d: MOV dword ptr [RSP + 0x28],EAX
1401d1721: MOV dword ptr [RSP + 0x20],0x8ea
1401d1729: MOV RCX,qword ptr [RCX + 0x48c]
1401d1730: MOV R9,RBX
1401d1733: MOV R8,R13
1401d1736: CALL 0x140007b24
1401d173b: JMP 0x1401d1f3c
1401d1740: MOV R8D,dword ptr [0x14025e601]
1401d1747: MOV RDX,qword ptr [RBP + 0x60]
1401d174b: CALL 0x14020d600
1401d1750: MOV RCX,qword ptr [0x140246148]
1401d1757: CMP RCX,R12
1401d175a: JZ 0x1401d1b8d
1401d1760: TEST byte ptr [RCX + 0x2c],R15B
1401d1764: JZ 0x1401d1b8d
1401d176a: CMP byte ptr [RCX + 0x29],0x3
1401d176e: JC 0x1401d1b8d
1401d1774: MOV EDX,0xcd
1401d1779: JMP 0x1401d1b6d
1401d177e: MOV RCX,qword ptr [0x140246148]
1401d1785: CMP RCX,R12
1401d1788: JZ 0x1401d14ba
1401d178e: TEST byte ptr [RCX + 0x2c],R15B
1401d1792: JZ 0x1401d14ba
1401d1798: CMP byte ptr [RCX + 0x29],R15B
1401d179c: JC 0x1401d14ba
1401d17a2: MOV RCX,qword ptr [RCX + 0x18]
1401d17a6: LEA R9,[0x1402249f0]
1401d17ad: MOV EDX,0xca
1401d17b2: MOV dword ptr [RSP + 0x20],0x8da
1401d17ba: MOV R8,R13
1401d17bd: CALL 0x140001664
1401d17c2: JMP 0x1401d14ba
1401d17c7: MOV RCX,qword ptr [0x140246148]
1401d17ce: CMP RCX,R12
1401d17d1: JZ 0x1401d1800
1401d17d3: TEST byte ptr [RCX + 0xa4],R15B
1401d17da: JZ 0x1401d1800
1401d17dc: CMP byte ptr [RCX + 0xa1],0x3
1401d17e3: JC 0x1401d1800
1401d17e5: MOV R9,qword ptr [RDI + 0x1465087]
1401d17ec: MOV EDX,0xce
1401d17f1: MOV RCX,qword ptr [RCX + 0x90]
1401d17f8: MOV R8,R13
1401d17fb: CALL 0x140001600
1401d1800: MOV RDX,qword ptr [RDI + 0x1465087]
1401d1807: LEA R8,[RBP + 0x50]
1401d180b: MOV RCX,RDI
1401d180e: CALL 0x1401ff1e0
1401d1813: MOV qword ptr [RBP + 0x60],RAX
1401d1817: TEST RAX,RAX
1401d181a: JZ 0x1401d1943
1401d1820: MOV RCX,qword ptr [0x140246148]
1401d1827: CMP RCX,R12
1401d182a: JZ 0x1401d184c
1401d182c: TEST byte ptr [RCX + 0x2c],R15B
1401d1830: JZ 0x1401d184c
1401d1832: CMP byte ptr [RCX + 0x29],0x3
1401d1836: JC 0x1401d184c
1401d1838: MOV RCX,qword ptr [RCX + 0x18]
1401d183c: MOV EDX,0xcf
1401d1841: MOV R9,RBX
1401d1844: MOV R8,R13
1401d1847: CALL 0x140001600
1401d184c: MOV EDX,dword ptr [RBP + 0x50]
1401d184f: MOV R14B,R15B
1401d1852: TEST EDX,EDX
1401d1854: JNZ 0x1401d188c
1401d1856: MOV RCX,qword ptr [0x140246148]
1401d185d: CMP RCX,R12
1401d1860: JZ 0x1401d1f3c
1401d1866: TEST byte ptr [RCX + 0x2c],R15B
1401d186a: JZ 0x1401d1f3c
1401d1870: CMP byte ptr [RCX + 0x29],R15B
1401d1874: JC 0x1401d1f3c
1401d187a: MOV EDX,0xd0
1401d187f: MOV dword ptr [RSP + 0x20],0x8ff
1401d1887: JMP 0x1401d16a1
1401d188c: LEA R9,[0x1402248a0]
1401d1893: MOV dword ptr [0x14025e601],EDX
1401d1899: MOV R8D,0x35414b4d
1401d189f: MOV dword ptr [RSP + 0x20],0x904
1401d18a7: LEA RCX,[0x14025e5f9]
1401d18ae: CALL 0x14000feac
1401d18b3: MOV RCX,qword ptr [0x14025e5f9]
1401d18ba: TEST RCX,RCX
1401d18bd: JNZ 0x1401d1905
1401d18bf: MOV RCX,qword ptr [0x140246148]
1401d18c6: CMP RCX,R12
1401d18c9: JZ 0x1401d1f3c
1401d18cf: TEST byte ptr [RCX + 0x4a0],R15B
1401d18d6: JZ 0x1401d1f3c
1401d18dc: CMP byte ptr [RCX + 0x49d],R15B
1401d18e3: JC 0x1401d1f3c
1401d18e9: MOV EAX,dword ptr [0x14025e601]
1401d18ef: MOV EDX,0xd1
1401d18f4: MOV dword ptr [RSP + 0x28],EAX
1401d18f8: MOV dword ptr [RSP + 0x20],0x908
1401d1900: JMP 0x1401d1729
1401d1905: MOV R8D,dword ptr [0x14025e601]
1401d190c: MOV RDX,qword ptr [RBP + 0x60]
1401d1910: CALL 0x14020d600
1401d1915: MOV RCX,qword ptr [0x140246148]
1401d191c: CMP RCX,R12
1401d191f: JZ 0x1401d1b8d
1401d1925: TEST byte ptr [RCX + 0x2c],R15B
1401d1929: JZ 0x1401d1b8d
1401d192f: CMP byte ptr [RCX + 0x29],0x3
1401d1933: JC 0x1401d1b8d
1401d1939: MOV EDX,0xd2
1401d193e: JMP 0x1401d1b6d
1401d1943: MOV RDX,qword ptr [RDI + 0x1465087]
1401d194a: LEA RCX,[RBP + -0x28]
1401d194e: CALL 0x14020d514
1401d1953: MOV RAX,qword ptr [RBP + -0x30]
1401d1957: LEA R9,[RBP + -0x28]
1401d195b: LEA R8,[RBP + 0x50]
1401d195f: MOV qword ptr [RSP + 0x20],RAX
1401d1964: LEA RDX,[RBP + -0x38]
1401d1968: LEA RCX,[RBP + 0x48]
1401d196c: CALL 0x14020d4fc
1401d1971: MOV RCX,qword ptr [0x140246148]
1401d1978: CMP RCX,R12
1401d197b: JZ 0x1401d19ac
1401d197d: TEST byte ptr [RCX + 0x2c],R15B
1401d1981: JZ 0x1401d19ac
1401d1983: CMP byte ptr [RCX + 0x29],0x3
1401d1987: JC 0x1401d19ac
1401d1989: MOV EAX,dword ptr [RBP + 0x50]
1401d198c: MOV EDX,0xd3
1401d1991: MOV RCX,qword ptr [RCX + 0x18]
1401d1995: MOV R9,RBX
1401d1998: MOV dword ptr [RSP + 0x28],EAX
1401d199c: MOV R8,R13
1401d199f: MOV dword ptr [RSP + 0x20],0x917
1401d19a7: CALL 0x140007b24
1401d19ac: MOVZX EDX,word ptr [RBP + -0x26]
1401d19b0: XOR R8D,R8D
1401d19b3: MOV RCX,qword ptr [RBP + -0x20]
1401d19b7: CALL 0x14008d67e
1401d19bc: CMP dword ptr [RBP + 0x48],ESI
1401d19bf: JNZ 0x1401d1be4
1401d19c5: CMP dword ptr [RBP + 0x50],ESI
1401d19c8: JZ 0x1401d1be4
1401d19ce: MOV R8,qword ptr [RBP + -0x38]
1401d19d2: LEA RDX,[RBP + 0x60]
1401d19d6: LEA RCX,[RBP + 0x48]
1401d19da: CALL 0x14020d508
1401d19df: CMP dword ptr [RBP + 0x48],ESI
1401d19e2: JZ 0x1401d1a2f
1401d19e4: MOV RCX,qword ptr [0x140246148]
1401d19eb: CMP RCX,R12
1401d19ee: JZ 0x1401d1a18
1401d19f0: TEST byte ptr [RCX + 0x2c],R15B
1401d19f4: JZ 0x1401d1a18
1401d19f6: CMP byte ptr [RCX + 0x29],R15B
1401d19fa: JC 0x1401d1a18
1401d19fc: MOV EDX,0xd8
1401d1a01: MOV dword ptr [RSP + 0x20],0x948
1401d1a09: MOV RCX,qword ptr [RCX + 0x18]
1401d1a0d: MOV R9,RBX
1401d1a10: MOV R8,R13
1401d1a13: CALL 0x140001664
1401d1a18: MOV RCX,qword ptr [RBP + -0x38]
1401d1a1c: TEST RCX,RCX
1401d1a1f: JZ 0x1401d14d1
1401d1a25: CALL 0x14020d502
1401d1a2a: JMP 0x1401d14ba
1401d1a2f: MOV RCX,qword ptr [0x140246148]
1401d1a36: CMP RCX,R12
1401d1a39: JZ 0x1401d1a62
1401d1a3b: TEST byte ptr [RCX + 0x2c],R15B
1401d1a3f: JZ 0x1401d1a62
1401d1a41: CMP byte ptr [RCX + 0x29],0x3
1401d1a45: JC 0x1401d1a62
1401d1a47: MOV EAX,dword ptr [RBP + 0x50]
1401d1a4a: MOV EDX,0xd9
1401d1a4f: MOV RCX,qword ptr [RCX + 0x18]
1401d1a53: MOV R9,RBX
1401d1a56: MOV R8,R13
1401d1a59: MOV dword ptr [RSP + 0x20],EAX
1401d1a5d: CALL 0x140001664
1401d1a62: MOV EDX,dword ptr [RBP + 0x50]
1401d1a65: TEST EDX,EDX
1401d1a67: JNZ 0x1401d1aba
1401d1a69: MOV RCX,qword ptr [0x140246148]
1401d1a70: CMP RCX,R12
1401d1a73: JZ 0x1401d1a9d
1401d1a75: TEST byte ptr [RCX + 0x2c],R15B
1401d1a79: JZ 0x1401d1a9d
1401d1a7b: CMP byte ptr [RCX + 0x29],R15B
1401d1a7f: JC 0x1401d1a9d
1401d1a81: MOV RCX,qword ptr [RCX + 0x18]
1401d1a85: MOV EDX,0xda
1401d1a8a: MOV R9,RBX
1401d1a8d: MOV dword ptr [RSP + 0x20],0x95a
1401d1a95: MOV R8,R13
1401d1a98: CALL 0x140001664
1401d1a9d: MOV RCX,qword ptr [RBP + -0x38]
1401d1aa1: TEST RCX,RCX
1401d1aa4: JZ 0x1401d1b34
1401d1aaa: CALL 0x14020d50e
1401d1aaf: MOV RCX,qword ptr [RBP + -0x38]
1401d1ab3: CALL 0x14020d502
1401d1ab8: JMP 0x1401d1b34
1401d1aba: LEA R9,[0x1402248a0]
1401d1ac1: MOV dword ptr [0x14025e601],EDX
1401d1ac7: MOV R8D,0x35414b4d
1401d1acd: MOV dword ptr [RSP + 0x20],0x974
1401d1ad5: LEA RCX,[0x14025e5f9]
1401d1adc: CALL 0x14000feac
1401d1ae1: MOV RCX,qword ptr [0x14025e5f9]
1401d1ae8: TEST RCX,RCX
1401d1aeb: JNZ 0x1401d1b40
1401d1aed: MOV RCX,qword ptr [0x140246148]
1401d1af4: CMP RCX,R12
1401d1af7: JZ 0x1401d1b34
1401d1af9: TEST byte ptr [RCX + 0x4a0],R15B
1401d1b00: JZ 0x1401d1b34
1401d1b02: CMP byte ptr [RCX + 0x49d],R15B
1401d1b09: JC 0x1401d1b34
1401d1b0b: MOV EAX,dword ptr [0x14025e601]
1401d1b11: MOV EDX,0xdc
1401d1b16: MOV RCX,qword ptr [RCX + 0x48c]
1401d1b1d: MOV R9,RBX
1401d1b20: MOV dword ptr [RSP + 0x28],EAX
1401d1b24: MOV R8,R13
1401d1b27: MOV dword ptr [RSP + 0x20],0x978
1401d1b2f: CALL 0x140007b24
1401d1b34: MOV dword ptr [RBP + 0x48],0xc0000001
1401d1b3b: JMP 0x1401d14ba
1401d1b40: MOV R8D,dword ptr [0x14025e601]
1401d1b47: MOV RDX,qword ptr [RBP + 0x60]
1401d1b4b: CALL 0x14020d600
1401d1b50: MOV RCX,qword ptr [0x140246148]
1401d1b57: CMP RCX,R12
1401d1b5a: JZ 0x1401d1b8d
1401d1b5c: TEST byte ptr [RCX + 0x2c],R15B
1401d1b60: JZ 0x1401d1b8d
1401d1b62: CMP byte ptr [RCX + 0x29],0x3
1401d1b66: JC 0x1401d1b8d
1401d1b68: MOV EDX,0xdd
1401d1b6d: MOV EAX,dword ptr [0x14025e601]
1401d1b73: MOV R9,RBX
1401d1b76: MOV RCX,qword ptr [RCX + 0x18]
1401d1b7a: MOV R8,R13
1401d1b7d: MOV dword ptr [RSP + 0x28],EAX
1401d1b81: MOV EAX,dword ptr [RBP + 0x50]
1401d1b84: MOV dword ptr [RSP + 0x20],EAX
1401d1b88: CALL 0x140007b24
1401d1b8d: MOV DL,R15B
1401d1b90: MOV RCX,RDI
1401d1b93: CALL 0x1401d2388
1401d1b98: MOVZX ECX,byte ptr [RDI + 0x1464d2d]
1401d1b9f: MOV dword ptr [RBP + 0x48],EAX
1401d1ba2: TEST CL,CL
1401d1ba4: JNZ 0x1401d1c1a
1401d1ba6: MOV RCX,qword ptr [0x140246148]
1401d1bad: CMP RCX,R12
1401d1bb0: JZ 0x1401d1ca6
1401d1bb6: TEST byte ptr [RCX + 0xa4],R15B
1401d1bbd: JZ 0x1401d1ca6
1401d1bc3: CMP byte ptr [RCX + 0xa1],0x4
1401d1bca: JC 0x1401d1ca6
1401d1bd0: MOV RCX,qword ptr [RCX + 0x90]
1401d1bd7: MOV EDX,0xde
1401d1bdc: XOR R9D,R9D
1401d1bdf: JMP 0x1401d1c9e
1401d1be4: MOV RCX,qword ptr [0x140246148]
1401d1beb: CMP RCX,R12
1401d1bee: JZ 0x1401d1a18
1401d1bf4: TEST byte ptr [RCX + 0x2c],R15B
1401d1bf8: JZ 0x1401d1a18
1401d1bfe: CMP byte ptr [RCX + 0x29],R15B
1401d1c02: JC 0x1401d1a18
1401d1c08: MOV EDX,0xd6
1401d1c0d: MOV dword ptr [RSP + 0x20],0x929
1401d1c15: JMP 0x1401d1a09
1401d1c1a: CMP CL,R15B
1401d1c1d: JNZ 0x1401d1c5d
1401d1c1f: MOV R10,qword ptr [0x140246148]
1401d1c26: CMP R10,R12
1401d1c29: JZ 0x1401d1c55
1401d1c2b: TEST byte ptr [R10 + 0xa4],R15B
1401d1c32: JZ 0x1401d1c55
1401d1c34: CMP byte ptr [R10 + 0xa1],0x4
1401d1c3c: JC 0x1401d1c55
1401d1c3e: MOV R9D,ECX
1401d1c41: MOV EDX,0xdf
1401d1c46: MOV RCX,qword ptr [R10 + 0x90]
1401d1c4d: MOV R8,R13
1401d1c50: CALL 0x14000764c
1401d1c55: MOV dword ptr [RBP + 0x48],ESI
1401d1c58: JMP 0x1401d1f33
1401d1c5d: CMP CL,0x2
1401d1c60: JNZ 0x1401d1ecb
1401d1c66: MOV dword ptr [RDI + 0x1464d29],0x2
1401d1c70: MOV R10,qword ptr [0x140246148]
1401d1c77: CMP R10,R12
1401d1c7a: JZ 0x1401d1ca6
1401d1c7c: TEST byte ptr [R10 + 0xa4],R15B
1401d1c83: JZ 0x1401d1ca6
1401d1c85: CMP byte ptr [R10 + 0xa1],0x4
1401d1c8d: JC 0x1401d1ca6
1401d1c8f: MOV R9D,ECX
1401d1c92: MOV EDX,0xe0
1401d1c97: MOV RCX,qword ptr [R10 + 0x90]
1401d1c9e: MOV R8,R13
1401d1ca1: CALL 0x14000764c
1401d1ca6: MOV dword ptr [RDI + 0x35515c],R15D
1401d1cad: CMP byte ptr [0x14025e5f8],SIL
1401d1cb4: MOV dword ptr [RBP + 0x48],ESI
1401d1cb7: JNZ 0x1401d1ceb
1401d1cb9: MOV RSI,qword ptr [RBP + 0x60]
1401d1cbd: MOV RCX,qword ptr [0x140246148]
1401d1cc4: CMP RCX,R12
1401d1cc7: JZ 0x1401d1d2e
1401d1cc9: TEST byte ptr [RCX + 0x2c],R15B
1401d1ccd: JZ 0x1401d1d2e
1401d1ccf: CMP byte ptr [RCX + 0x29],0x3
1401d1cd3: JC 0x1401d1d2e
1401d1cd5: MOV RCX,qword ptr [RCX + 0x18]
1401d1cd9: MOV EDX,0xe3
1401d1cde: MOV R9,RBX
1401d1ce1: MOV R8,R13
1401d1ce4: CALL 0x140001600
1401d1ce9: JMP 0x1401d1d2e
1401d1ceb: MOV R8D,dword ptr [0x14025e601]
1401d1cf2: MOV RSI,qword ptr [0x14025e5f9]
1401d1cf9: MOV dword ptr [RBP + 0x50],R8D
1401d1cfd: MOV RCX,qword ptr [0x140246148]
1401d1d04: CMP RCX,R12
1401d1d07: JZ 0x1401d1d2e
1401d1d09: TEST byte ptr [RCX + 0x2c],R15B
1401d1d0d: JZ 0x1401d1d2e
1401d1d0f: CMP byte ptr [RCX + 0x29],0x3
1401d1d13: JC 0x1401d1d2e
1401d1d15: MOV RCX,qword ptr [RCX + 0x18]
1401d1d19: MOV EDX,0xe4
1401d1d1e: MOV dword ptr [RSP + 0x20],R8D
1401d1d23: MOV R9,RBX
1401d1d26: MOV R8,R13
1401d1d29: CALL 0x140001664
1401d1d2e: LEA RCX,[RDI + 0x35513c]
1401d1d35: MOV R8D,0x20
1401d1d3b: MOV RDX,RSI
1401d1d3e: CALL 0x140010118
1401d1d43: MOV RCX,qword ptr [0x140246148]
1401d1d4a: CMP RCX,R12
1401d1d4d: JZ 0x1401d1d82
1401d1d4f: TEST byte ptr [RCX + 0xa4],R15B
1401d1d56: JZ 0x1401d1d82
1401d1d58: CMP byte ptr [RCX + 0xa1],0x3
1401d1d5f: JC 0x1401d1d82
1401d1d61: MOV EAX,dword ptr [RDI + 0x355154]
1401d1d67: MOV EDX,0xe5
1401d1d6c: MOV RCX,qword ptr [RCX + 0x90]
1401d1d73: MOV R9,RBX
1401d1d76: MOV R8,R13
1401d1d79: MOV dword ptr [RSP + 0x20],EAX
1401d1d7d: CALL 0x140001664
1401d1d82: LEA RCX,[RDI + 0x35511c]
1401d1d89: MOV R8D,0x20
1401d1d8f: MOV RDX,RSI
1401d1d92: CALL 0x140010118
1401d1d97: MOV R8D,dword ptr [RBP + 0x50]
1401d1d9b: LEA RAX,[RBP + -0x18]
1401d1d9f: LEA R9,[RBP + 0x58]
1401d1da3: MOV qword ptr [RSP + 0x20],RAX
1401d1da8: MOV RDX,RSI
1401d1dab: MOV RCX,RDI
1401d1dae: CALL 0x1401d9f70
1401d1db3: MOV RCX,qword ptr [0x140246148]
1401d1dba: MOV ESI,dword ptr [RBP + 0x58]
1401d1dbd: CMP RCX,R12
1401d1dc0: JZ 0x1401d1df1
1401d1dc2: TEST byte ptr [RCX + 0xa4],R15B
1401d1dc9: JZ 0x1401d1df1
1401d1dcb: CMP byte ptr [RCX + 0xa1],0x3
1401d1dd2: JC 0x1401d1df1
1401d1dd4: MOVZX R9D,byte ptr [RBP + -0x10]
1401d1dd9: MOV EDX,0xe6
1401d1dde: MOV RCX,qword ptr [RCX + 0x90]
1401d1de5: MOV R8,R13
1401d1de8: MOV dword ptr [RSP + 0x20],ESI
1401d1dec: CALL 0x140007688
1401d1df1: LEA R9,[RBP + -0x18]
1401d1df5: MOV R8D,0x2
1401d1dfb: MOV EDX,ESI
1401d1dfd: MOV RCX,RDI
1401d1e00: CALL 0x1401d991c
1401d1e05: MOV dword ptr [RBP + 0x48],EAX
1401d1e08: TEST EAX,EAX
1401d1e0a: JNZ 0x1401d1f33
1401d1e10: MOV RCX,RDI
1401d1e13: MOV dword ptr [RDI + 0x1464d29],0x4
1401d1e1d: CALL 0x1401d2298
1401d1e22: MOV dword ptr [RBP + 0x48],EAX
1401d1e25: TEST EAX,EAX
1401d1e27: JNZ 0x1401d1e80
1401d1e29: MOV dword ptr [RDI + 0x1464d29],0x5
1401d1e33: MOV byte ptr [0x14025e5f8],R15B
1401d1e3a: MOV RCX,qword ptr [0x140246148]
1401d1e41: CMP RCX,R12
1401d1e44: JZ 0x1401d1f33
1401d1e4a: TEST byte ptr [RCX + 0xa4],R15B
1401d1e51: JZ 0x1401d1f33
1401d1e57: CMP byte ptr [RCX + 0xa1],0x3
1401d1e5e: JC 0x1401d1f33
1401d1e64: MOV RCX,qword ptr [RCX + 0x90]
1401d1e6b: MOV EDX,0xe7
1401d1e70: MOV R9,RBX
1401d1e73: MOV R8,R13
1401d1e76: CALL 0x140001600
1401d1e7b: JMP 0x1401d1f33
1401d1e80: MOV RCX,qword ptr [0x140246148]
1401d1e87: CMP RCX,R12
1401d1e8a: JZ 0x1401d1f33
1401d1e90: TEST byte ptr [RCX + 0xa4],R15B
1401d1e97: JZ 0x1401d1f33
1401d1e9d: CMP byte ptr [RCX + 0xa1],R15B
1401d1ea4: JC 0x1401d1f33
1401d1eaa: MOV RCX,qword ptr [RCX + 0x90]
1401d1eb1: MOV EDX,0xe8
1401d1eb6: MOV R9,RBX
1401d1eb9: MOV dword ptr [RSP + 0x20],0x9d0
1401d1ec1: MOV R8,R13
1401d1ec4: CALL 0x140001664
1401d1ec9: JMP 0x1401d1f33
1401d1ecb: CMP CL,0x3
1401d1ece: JNZ 0x1401d1ef6
1401d1ed0: MOV R10,qword ptr [0x140246148]
1401d1ed7: CMP R10,R12
1401d1eda: JZ 0x1401d1f2c
1401d1edc: TEST byte ptr [R10 + 0xa4],R15B
1401d1ee3: JZ 0x1401d1f2c
1401d1ee5: CMP byte ptr [R10 + 0xa1],0x4
1401d1eed: JC 0x1401d1f2c
1401d1eef: MOV EDX,0xe1
1401d1ef4: JMP 0x1401d1f1a
1401d1ef6: MOV R10,qword ptr [0x140246148]
1401d1efd: CMP R10,R12
1401d1f00: JZ 0x1401d1f2c
1401d1f02: TEST byte ptr [R10 + 0xa4],R15B
1401d1f09: JZ 0x1401d1f2c
1401d1f0b: CMP byte ptr [R10 + 0xa1],0x4
1401d1f13: JC 0x1401d1f2c
1401d1f15: MOV EDX,0xe2
1401d1f1a: MOV R9D,ECX
1401d1f1d: MOV R8,R13
1401d1f20: MOV RCX,qword ptr [R10 + 0x90]
1401d1f27: CALL 0x14000764c
1401d1f2c: MOV dword ptr [RBP + 0x48],0xc0000001
1401d1f33: CMP R14B,R15B
1401d1f36: JNZ 0x1401d14ba
1401d1f3c: MOV RCX,qword ptr [RBP + 0x60]
1401d1f40: TEST RCX,RCX
1401d1f43: JZ 0x1401d14ba
1401d1f49: MOV EDX,dword ptr [0x14025e601]
1401d1f4f: CALL 0x1400100e8
1401d1f54: JMP 0x1401d14d1
```

### Decompiled C

```c

int FUN_1401d13e0(longlong param_1)

{
  char cVar1;
  bool bVar2;
  bool bVar3;
  bool bVar4;
  undefined8 uVar5;
  undefined8 uVar6;
  longlong lVar7;
  undefined8 uVar8;
  int local_res8 [2];
  int local_res10 [2];
  undefined4 local_res18 [2];
  longlong local_res20;
  undefined8 in_stack_ffffffffffffff78;
  undefined4 uVar10;
  undefined8 *puVar9;
  undefined4 uVar11;
  longlong local_78;
  undefined4 local_70;
  undefined4 uStack_6c;
  undefined8 local_68;
  undefined8 uStack_60;
  undefined8 local_58;
  ulonglong uStack_50;
  
  uVar10 = (undefined4)((ulonglong)in_stack_ffffffffffffff78 >> 0x20);
  local_70 = 0xffffffff;
  uStack_6c = 0xffffffff;
  local_res18[0] = 0;
  local_res10[0] = 0;
  bVar2 = false;
  bVar3 = false;
  local_res20 = 0;
  local_58 = 0;
  uStack_50 = 0;
  local_78 = 0;
  local_68 = 0;
  uStack_60 = 0;
  local_res8[0] = -0x3fffffff;
  bVar4 = true;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xc4,&DAT_14023a980);
  }
  if ((*(uint *)(param_1 + 0x1310) & 0x100) == 0) {
    if (*(longlong *)(param_1 + 0x1465087) == 0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
        uVar5 = 0xc6;
        uVar6 = CONCAT44(uVar10,0x8bf);
        goto LAB_1401d14a7;
      }
    }
    else {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
        uVar5 = CONCAT44(uVar10,(uint)DAT_14025e5f8);
        FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),199,&DAT_14023a980,
                      *(longlong *)(param_1 + 0x1465087),uVar5,DAT_14025e601);
        uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
      }
      if (DAT_14025e5f8 == 1) {
        if (DAT_14025e601 == 0) {
          DAT_14025e5f8 = 0;
          goto LAB_1401d15cc;
        }
LAB_1401d1b8d:
        local_res8[0] = FUN_1401d2388(param_1,1);
        cVar1 = *(char *)(param_1 + 0x1464d2d);
        if (cVar1 == '\0') {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
            uVar5 = *(undefined8 *)(PTR_LOOP_140246148 + 0x90);
            uVar6 = 0xde;
            uVar8 = 0;
LAB_1401d1c9e:
            FUN_14000764c(uVar5,uVar6,&DAT_14023a980,uVar8);
          }
LAB_1401d1ca6:
          *(undefined4 *)(param_1 + 0x35515c) = 1;
          lVar7 = DAT_14025e5f9;
          local_res8[0] = 0;
          if (DAT_14025e5f8 == 0) {
            lVar7 = local_res20;
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
              WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xe3,&DAT_14023a980,
                       "AsicConnac3xLoadRomPatch");
            }
          }
          else {
            local_res10[0] = DAT_14025e601;
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
              uVar5 = CONCAT44(uVar10,DAT_14025e601);
              FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xe4,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",uVar5);
              uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
            }
          }
          FUN_140010118(param_1 + 0x35513c,lVar7,0x20);
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe5,&DAT_14023a980,
                          "AsicConnac3xLoadRomPatch",
                          CONCAT44(uVar10,*(undefined4 *)(param_1 + 0x355154)));
          }
          FUN_140010118(param_1 + 0x35511c,lVar7,0x20);
          puVar9 = &local_58;
          FUN_1401d9f70(param_1,lVar7,local_res10[0],local_res18,puVar9);
          uVar10 = local_res18[0];
          uVar11 = (undefined4)((ulonglong)puVar9 >> 0x20);
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            uVar5 = CONCAT44(uVar11,local_res18[0]);
            WPP_SF_DD(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe6,&DAT_14023a980,
                      uStack_50 & 0xff,uVar5);
            uVar11 = (undefined4)((ulonglong)uVar5 >> 0x20);
          }
          local_res8[0] = FUN_1401d991c(param_1,uVar10,2,&local_58);
          if (local_res8[0] == 0) {
            *(undefined4 *)(param_1 + 0x1464d29) = 4;
            local_res8[0] = FUN_1401d2298(param_1);
            if (local_res8[0] == 0) {
              *(undefined4 *)(param_1 + 0x1464d29) = 5;
              DAT_14025e5f8 = 1;
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe7,&DAT_14023a980,
                         "AsicConnac3xLoadRomPatch");
              }
            }
            else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                     ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
              FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe8,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",CONCAT44(uVar11,0x9d0));
            }
          }
        }
        else if (cVar1 == '\x01') {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
            FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xdf,&DAT_14023a980,1);
          }
          local_res8[0] = 0;
        }
        else {
          if (cVar1 == '\x02') {
            *(undefined4 *)(param_1 + 0x1464d29) = 2;
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
              uVar8 = 2;
              uVar6 = 0xe0;
              uVar5 = *(undefined8 *)(PTR_LOOP_140246148 + 0x90);
              goto LAB_1401d1c9e;
            }
            goto LAB_1401d1ca6;
          }
          if (cVar1 == '\x03') {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
              uVar5 = 0xe1;
LAB_1401d1f1a:
              FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),uVar5,&DAT_14023a980,cVar1);
            }
          }
          else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                   ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
            uVar5 = 0xe2;
            goto LAB_1401d1f1a;
          }
          local_res8[0] = -0x3fffffff;
        }
        if (!bVar2) goto LAB_1401d14ba;
      }
      else {
LAB_1401d15cc:
        if (DAT_14025e5f8 != 0) goto LAB_1401d1b8d;
        if (*(int *)(param_1 + 0x146694c) == 1) {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),200,&DAT_14023a980,
                     *(undefined8 *)(param_1 + 0x1465087));
          }
          local_res20 = FUN_1401ff1e0(param_1,*(undefined8 *)(param_1 + 0x1465087),local_res10);
          if (local_res20 == 0) {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
              FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xca,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",CONCAT44(uVar10,0x8da));
            }
            goto LAB_1401d14ba;
          }
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
            WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xc9,&DAT_14023a980,
                     "AsicConnac3xLoadRomPatch");
          }
          if (local_res10[0] == 0) {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
              uVar5 = 0xcb;
              uVar6 = CONCAT44(uVar10,0x8e0);
LAB_1401d16a1:
              FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar5,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",uVar6);
            }
          }
          else {
            DAT_14025e601 = local_res10[0];
            uVar5 = CONCAT44(uVar10,0x8e6);
            FUN_14000feac(&DAT_14025e5f9,local_res10[0],0x35414b4d,
                          "e:\\worktmp\\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\\7295\\wlan_driver\\seattle\\wifi_driver\\windows\\hal\\chips\\mtconnac3x.c"
                          ,uVar5);
            uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
            if (DAT_14025e5f9 != 0) {
              FUN_14020d600(DAT_14025e5f9,local_res20,DAT_14025e601);
              bVar2 = bVar4;
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
                uVar5 = 0xcd;
                bVar3 = bVar4;
LAB_1401d1b6d:
                uVar6 = CONCAT44(uVar10,local_res10[0]);
                FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar5,&DAT_14023a980,
                              "AsicConnac3xLoadRomPatch",uVar6,DAT_14025e601);
                uVar10 = (undefined4)((ulonglong)uVar6 >> 0x20);
                bVar2 = bVar3;
              }
              goto LAB_1401d1b8d;
            }
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x4a0] & 1) != 0)) && (PTR_LOOP_140246148[0x49d] != '\0')) {
              uVar5 = 0xcc;
              uVar6 = CONCAT44(uVar10,0x8ea);
LAB_1401d1729:
              FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x48c),uVar5,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",uVar6,DAT_14025e601);
            }
          }
        }
        else {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xce,&DAT_14023a980,
                     *(undefined8 *)(param_1 + 0x1465087));
          }
          local_res20 = FUN_1401ff1e0(param_1,*(undefined8 *)(param_1 + 0x1465087),local_res10);
          if (local_res20 == 0) {
            NdisInitializeString(&local_68,*(undefined8 *)(param_1 + 0x1465087));
            uVar5 = CONCAT44(uStack_6c,local_70);
            NdisOpenFile(local_res8,&local_78,local_res10,&local_68,uVar5);
            uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
              uVar5 = CONCAT44(uVar10,0x917);
              FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xd3,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",uVar5,local_res10[0]);
              uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
            }
            NdisFreeMemory(uStack_60,local_68._2_2_,0);
            if ((local_res8[0] == 0) && (local_res10[0] != 0)) {
              NdisMapFile(local_res8,&local_res20,local_78);
              if (local_res8[0] == 0) {
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29]))
                {
                  uVar5 = CONCAT44(uVar10,local_res10[0]);
                  FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xd9,&DAT_14023a980,
                                "AsicConnac3xLoadRomPatch",uVar5);
                  uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
                }
                if (local_res10[0] == 0) {
                  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0'))
                  {
                    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xda,&DAT_14023a980,
                                  "AsicConnac3xLoadRomPatch",CONCAT44(uVar10,0x95a));
                  }
                  if (local_78 != 0) {
                    NdisUnmapFile();
                    NdisCloseFile(local_78);
                  }
                }
                else {
                  DAT_14025e601 = local_res10[0];
                  uVar5 = CONCAT44(uVar10,0x974);
                  FUN_14000feac(&DAT_14025e5f9,local_res10[0],0x35414b4d,
                                "e:\\worktmp\\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\\7295\\wlan_driver\\seattle\\wifi_driver\\windows\\hal\\chips\\mtconnac3x.c"
                                ,uVar5);
                  uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
                  if (DAT_14025e5f9 != 0) {
                    FUN_14020d600(DAT_14025e5f9,local_res20,DAT_14025e601);
                    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) &&
                       (2 < (byte)PTR_LOOP_140246148[0x29])) {
                      uVar5 = 0xdd;
                      goto LAB_1401d1b6d;
                    }
                    goto LAB_1401d1b8d;
                  }
                  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                      ((PTR_LOOP_140246148[0x4a0] & 1) != 0)) && (PTR_LOOP_140246148[0x49d] != '\0')
                     ) {
                    FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x48c),0xdc,&DAT_14023a980,
                                  "AsicConnac3xLoadRomPatch",CONCAT44(uVar10,0x978),DAT_14025e601);
                  }
                }
                local_res8[0] = -0x3fffffff;
                goto LAB_1401d14ba;
              }
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
                uVar5 = 0xd8;
                uVar6 = CONCAT44(uVar10,0x948);
                goto LAB_1401d1a09;
              }
            }
            else if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    (((PTR_LOOP_140246148[0x2c] & 1) != 0 && (PTR_LOOP_140246148[0x29] != '\0')))) {
              uVar5 = 0xd6;
              uVar6 = CONCAT44(uVar10,0x929);
LAB_1401d1a09:
              FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar5,&DAT_14023a980,
                            "AsicConnac3xLoadRomPatch",uVar6);
            }
            if (local_78 == 0) goto LAB_1401d14d1;
            NdisCloseFile();
            goto LAB_1401d14ba;
          }
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
            WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xcf,&DAT_14023a980,
                     "AsicConnac3xLoadRomPatch");
          }
          if (local_res10[0] == 0) {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
              uVar5 = 0xd0;
              uVar6 = CONCAT44(uVar10,0x8ff);
              goto LAB_1401d16a1;
            }
          }
          else {
            DAT_14025e601 = local_res10[0];
            uVar5 = CONCAT44(uVar10,0x904);
            FUN_14000feac(&DAT_14025e5f9,local_res10[0],0x35414b4d,
                          "e:\\worktmp\\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\\7295\\wlan_driver\\seattle\\wifi_driver\\windows\\hal\\chips\\mtconnac3x.c"
                          ,uVar5);
            uVar10 = (undefined4)((ulonglong)uVar5 >> 0x20);
            if (DAT_14025e5f9 != 0) {
              FUN_14020d600(DAT_14025e5f9,local_res20,DAT_14025e601);
              bVar2 = bVar4;
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  (bVar2 = true, (PTR_LOOP_140246148[0x2c] & 1) != 0)) &&
                 (2 < (byte)PTR_LOOP_140246148[0x29])) {
                uVar5 = 0xd2;
                bVar3 = bVar4;
                goto LAB_1401d1b6d;
              }
              goto LAB_1401d1b8d;
            }
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0x4a0] & 1) != 0)) && (PTR_LOOP_140246148[0x49d] != '\0')) {
              uVar5 = 0xd1;
              uVar6 = CONCAT44(uVar10,0x908);
              goto LAB_1401d1729;
            }
          }
        }
      }
      if (local_res20 != 0) {
        FUN_1400100e8(local_res20,DAT_14025e601);
        goto LAB_1401d14d1;
      }
    }
  }
  else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
           ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
    uVar5 = 0xc5;
    uVar6 = CONCAT44(uVar10,0x8b9);
LAB_1401d14a7:
    FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar5,&DAT_14023a980,
                  "AsicConnac3xLoadRomPatch",uVar6,local_res8[0]);
  }
LAB_1401d14ba:
  if (local_78 != 0) {
    NdisUnmapFile();
    NdisCloseFile(local_78);
  }
LAB_1401d14d1:
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xe9,&DAT_14023a980,local_res8[0]);
  }
  return local_res8[0];
}


```

## FUN_1401d01d0 @ 1401d01d0

### Immediates (>=0x10000)

- 0x98698
- 0x3550f8
- 0x355180
- 0x355181
- 0x3551d1
- 0x1464d29
- 0x1464d62
- 0x1464d63
- 0x1465077
- 0x146694c
- 0x35414b4d
- 0xc0000001
- 0xffffffff
- 0x140224880
- 0x1402248a0
- 0x14023a980
- 0x140246148
- 0x14025e619

### Disassembly

```asm
1401d01d0: MOV RAX,RSP
1401d01d3: MOV qword ptr [RAX + 0x10],RBX
1401d01d7: MOV qword ptr [RAX + 0x18],RSI
1401d01db: MOV qword ptr [RAX + 0x20],RDI
1401d01df: PUSH RBP
1401d01e0: PUSH R12
1401d01e2: PUSH R13
1401d01e4: PUSH R14
1401d01e6: PUSH R15
1401d01e8: LEA RBP,[RAX + -0x168]
1401d01ef: SUB RSP,0x240
1401d01f6: MOV RAX,qword ptr [0x14024f600]
1401d01fd: XOR RAX,RSP
1401d0200: MOV qword ptr [RBP + 0x130],RAX
1401d0207: MOV R13,RCX
1401d020a: MOV qword ptr [RBP + -0x80],RCX
1401d020e: XOR R15D,R15D
1401d0211: LEA RCX,[RBP + -0x60]
1401d0215: XOR EDX,EDX
1401d0217: MOV dword ptr [RBP + -0x78],R15D
1401d021b: MOV R8D,0x190
1401d0221: MOV R14D,R15D
1401d0224: CALL 0x14020d8c0
1401d0229: OR dword ptr [RSP + 0x78],0xffffffff
1401d022e: XORPS XMM0,XMM0
1401d0231: OR dword ptr [RSP + 0x7c],0xffffffff
1401d0236: MOVUPS xmmword ptr [RBP + -0x70],XMM0
1401d023a: MOV dword ptr [RSP + 0x58],0xc0000001
1401d0242: MOV dword ptr [RSP + 0x50],R15D
1401d0247: MOV qword ptr [RSP + 0x60],R15
1401d024c: MOV dword ptr [RBP + -0x74],R15D
1401d0250: MOV byte ptr [RSP + 0x54],R15B
1401d0255: MOV qword ptr [RSP + 0x70],R15
1401d025a: MOV RCX,qword ptr [0x140246148]
1401d0261: LEA RSI,[0x140246148]
1401d0268: LEA RDI,[0x14023a980]
1401d026f: LEA R12D,[R15 + 0x1]
1401d0273: CMP RCX,RSI
1401d0276: JZ 0x1401d029d
1401d0278: TEST byte ptr [RCX + 0xa4],R12B
1401d027f: JZ 0x1401d029d
1401d0281: CMP byte ptr [RCX + 0xa1],0x5
1401d0288: JC 0x1401d029d
1401d028a: MOV RCX,qword ptr [RCX + 0x90]
1401d0291: LEA EDX,[R15 + 0x7d]
1401d0295: MOV R8,RDI
1401d0298: CALL 0x1400015d8
1401d029d: MOV AL,byte ptr [0x14025e618]
1401d02a3: LEA RBX,[0x140224880]
1401d02aa: CMP AL,R12B
1401d02ad: JNZ 0x1401d02c3
1401d02af: CMP dword ptr [0x14025e621],R15D
1401d02b6: JNZ 0x1401d0494
1401d02bc: MOV byte ptr [0x14025e5f8],R15B
1401d02c3: TEST AL,AL
1401d02c5: JNZ 0x1401d048b
1401d02cb: CMP dword ptr [R13 + 0x146694c],R12D
1401d02d2: JNZ 0x1401d0887
1401d02d8: MOV RCX,qword ptr [0x140246148]
1401d02df: CMP RCX,RSI
1401d02e2: JZ 0x1401d0311
1401d02e4: TEST byte ptr [RCX + 0xa4],R12B
1401d02eb: JZ 0x1401d0311
1401d02ed: CMP byte ptr [RCX + 0xa1],0x3
1401d02f4: JC 0x1401d0311
1401d02f6: MOV R9,qword ptr [R13 + 0x1465077]
1401d02fd: MOV EDX,0x7e
1401d0302: MOV RCX,qword ptr [RCX + 0x90]
1401d0309: MOV R8,RDI
1401d030c: CALL 0x140001600
1401d0311: MOV RDX,qword ptr [R13 + 0x1465077]
1401d0318: LEA R8,[RSP + 0x50]
1401d031d: MOV RCX,R13
1401d0320: CALL 0x1401ff1e0
1401d0325: MOV qword ptr [RSP + 0x60],RAX
1401d032a: TEST RAX,RAX
1401d032d: JZ 0x1401d07c7
1401d0333: MOV RCX,qword ptr [0x140246148]
1401d033a: CMP RCX,RSI
1401d033d: JZ 0x1401d035f
1401d033f: TEST byte ptr [RCX + 0x2c],R12B
1401d0343: JZ 0x1401d035f
1401d0345: CMP byte ptr [RCX + 0x29],0x3
1401d0349: JC 0x1401d035f
1401d034b: MOV RCX,qword ptr [RCX + 0x18]
1401d034f: MOV EDX,0x7f
1401d0354: MOV R9,RBX
1401d0357: MOV R8,RDI
1401d035a: CALL 0x140001600
1401d035f: MOV EDX,dword ptr [RSP + 0x50]
1401d0363: MOV byte ptr [RSP + 0x54],R12B
1401d0368: TEST EDX,EDX
1401d036a: JNZ 0x1401d03b1
1401d036c: MOV RCX,qword ptr [0x140246148]
1401d0373: CMP RCX,RSI
1401d0376: JZ 0x1401d13c2
1401d037c: TEST byte ptr [RCX + 0x2c],R12B
1401d0380: JZ 0x1401d13c2
1401d0386: CMP byte ptr [RCX + 0x29],R12B
1401d038a: JC 0x1401d13c2
1401d0390: MOV EDX,0x81
1401d0395: MOV dword ptr [RSP + 0x20],0x5a6
1401d039d: MOV RCX,qword ptr [RCX + 0x18]
1401d03a1: MOV R9,RBX
1401d03a4: MOV R8,RDI
1401d03a7: CALL 0x140001664
1401d03ac: JMP 0x1401d13c2
1401d03b1: LEA R9,[0x1402248a0]
1401d03b8: MOV dword ptr [0x14025e621],EDX
1401d03be: MOV R8D,0x35414b4d
1401d03c4: MOV dword ptr [RSP + 0x20],0x5ab
1401d03cc: LEA RCX,[0x14025e619]
1401d03d3: CALL 0x14000feac
1401d03d8: MOV RCX,qword ptr [0x14025e619]
1401d03df: TEST RCX,RCX
1401d03e2: JNZ 0x1401d043c
1401d03e4: MOV RCX,qword ptr [0x140246148]
1401d03eb: CMP RCX,RSI
1401d03ee: JZ 0x1401d13c2
1401d03f4: TEST byte ptr [RCX + 0x4a0],R12B
1401d03fb: JZ 0x1401d13c2
1401d0401: CMP byte ptr [RCX + 0x49d],R12B
1401d0408: JC 0x1401d13c2
1401d040e: MOV EAX,dword ptr [0x14025e621]
1401d0414: MOV EDX,0x82
1401d0419: MOV dword ptr [RSP + 0x28],EAX
1401d041d: MOV dword ptr [RSP + 0x20],0x5af
1401d0425: MOV RCX,qword ptr [RCX + 0x48c]
1401d042c: MOV R9,RBX
1401d042f: MOV R8,RDI
1401d0432: CALL 0x140007b24
1401d0437: JMP 0x1401d13c2
1401d043c: MOV R8D,dword ptr [0x14025e621]
1401d0443: MOV RDX,qword ptr [RSP + 0x60]
1401d0448: CALL 0x14020d600
1401d044d: MOV RCX,qword ptr [0x140246148]
1401d0454: CMP RCX,RSI
1401d0457: JZ 0x1401d048b
1401d0459: TEST byte ptr [RCX + 0x2c],R12B
1401d045d: JZ 0x1401d048b
1401d045f: CMP byte ptr [RCX + 0x29],0x3
1401d0463: JC 0x1401d048b
1401d0465: MOV EAX,dword ptr [0x14025e621]
1401d046b: MOV EDX,0x83
1401d0470: MOV RCX,qword ptr [RCX + 0x18]
1401d0474: MOV R9,RBX
1401d0477: MOV dword ptr [RSP + 0x28],EAX
1401d047b: MOV R8,RDI
1401d047e: MOV EAX,dword ptr [RSP + 0x50]
1401d0482: MOV dword ptr [RSP + 0x20],EAX
1401d0486: CALL 0x140007b24
1401d048b: CMP byte ptr [0x14025e618],R12B
1401d0492: JNZ 0x1401d04dd
1401d0494: MOV RAX,qword ptr [0x14025e619]
1401d049b: MOV R8D,dword ptr [0x14025e621]
1401d04a2: MOV qword ptr [RSP + 0x60],RAX
1401d04a7: MOV dword ptr [RSP + 0x50],R8D
1401d04ac: MOV RCX,qword ptr [0x140246148]
1401d04b3: CMP RCX,RSI
1401d04b6: JZ 0x1401d04dd
1401d04b8: TEST byte ptr [RCX + 0x2c],R12B
1401d04bc: JZ 0x1401d04dd
1401d04be: CMP byte ptr [RCX + 0x29],0x3
1401d04c2: JC 0x1401d04dd
1401d04c4: MOV RCX,qword ptr [RCX + 0x18]
1401d04c8: MOV EDX,0x96
1401d04cd: MOV dword ptr [RSP + 0x20],R8D
1401d04d2: MOV R9,RBX
1401d04d5: MOV R8,RDI
1401d04d8: CALL 0x140001664
1401d04dd: MOV RCX,qword ptr [RSP + 0x60]
1401d04e2: TEST RCX,RCX
1401d04e5: JZ 0x1401d0720
1401d04eb: MOV EDI,dword ptr [RSP + 0x50]
1401d04ef: ADD RCX,-0x24
1401d04f3: ADD RDI,RCX
1401d04f6: MOV RCX,qword ptr [0x140246148]
1401d04fd: CMP RCX,RSI
1401d0500: JZ 0x1401d058c
1401d0506: TEST byte ptr [RCX + 0xa4],R12B
1401d050d: JZ 0x1401d0549
1401d050f: CMP byte ptr [RCX + 0xa1],0x3
1401d0516: JC 0x1401d0549
1401d0518: MOVZX R8D,byte ptr [RDI + 0x1]
1401d051d: MOV EDX,0x97
1401d0522: MOVZX EAX,byte ptr [RDI + 0x2]
1401d0526: ADD R8D,R12D
1401d0529: MOVZX R9D,byte ptr [RDI]
1401d052d: MOV RCX,qword ptr [RCX + 0x90]
1401d0534: MOV dword ptr [RSP + 0x28],EAX
1401d0538: MOV dword ptr [RSP + 0x20],R8D
1401d053d: LEA R8,[0x14023a980]
1401d0544: CALL 0x140012b88
1401d0549: MOV RCX,qword ptr [0x140246148]
1401d0550: CMP RCX,RSI
1401d0553: JZ 0x1401d058c
1401d0555: TEST byte ptr [RCX + 0xa4],R12B
1401d055c: JZ 0x1401d058c
1401d055e: CMP byte ptr [RCX + 0xa1],0x3
1401d0565: JC 0x1401d058c
1401d0567: MOV RCX,qword ptr [RCX + 0x90]
1401d056e: LEA RAX,[RDI + 0x7]
1401d0572: LEA R9,[RDI + 0x11]
1401d0576: MOV qword ptr [RSP + 0x20],RAX
1401d057b: MOV EDX,0x98
1401d0580: LEA R8,[0x14023a980]
1401d0587: CALL 0x140012e48
1401d058c: MOV RDX,qword ptr [RSP + 0x60]
1401d0591: LEA RCX,[R13 + 0x355181]
1401d0598: ADD RDX,0x98698
1401d059f: MOV byte ptr [R13 + 0x355180],0x3b
1401d05a7: MOV R8D,0x51
1401d05ad: CALL 0x140010118
1401d05b2: LEA R15,[RDI + 0x2]
1401d05b6: MOV byte ptr [R13 + 0x3551d1],0xa
1401d05be: MOVZX ECX,byte ptr [R15]
1401d05c2: XOR ESI,ESI
1401d05c4: LEA RAX,[RCX + RCX*0x4]
1401d05c8: SHL RAX,0x3
1401d05cc: SUB RDI,RAX
1401d05cf: TEST CL,CL
1401d05d1: JZ 0x1401d0707
1401d05d7: LEA R13,[0x140246148]
1401d05de: MOVUPS XMM0,xmmword ptr [RDI]
1401d05e1: LEA RCX,[RSI + RSI*0x4]
1401d05e5: MOV R14,RDI
1401d05e8: MOVUPS XMM1,xmmword ptr [RDI + 0x10]
1401d05ec: MOVUPS xmmword ptr [RBP + RCX*0x8 + -0x60],XMM0
1401d05f1: MOVSD XMM0,qword ptr [RDI + 0x20]
1401d05f6: MOVUPS xmmword ptr [RBP + RCX*0x8 + -0x50],XMM1
1401d05fb: MOVSD qword ptr [RBP + RCX*0x8 + -0x40],XMM0
1401d0601: MOV RCX,qword ptr [0x140246148]
1401d0608: CMP RCX,R13
1401d060b: JZ 0x1401d069d
1401d0611: TEST byte ptr [RCX + 0xa4],R12B
1401d0618: JZ 0x1401d0656
1401d061a: CMP byte ptr [RCX + 0xa1],0x3
1401d0621: JC 0x1401d0656
1401d0623: MOVZX R8D,byte ptr [RDI + 0x18]
1401d0628: MOV EDX,0x99
1401d062d: MOV EAX,dword ptr [RDI + 0x14]
1401d0630: MOV R9D,ESI
1401d0633: MOV RCX,qword ptr [RCX + 0x90]
1401d063a: MOV dword ptr [RSP + 0x30],EAX
1401d063e: MOV EAX,dword ptr [RDI + 0x10]
1401d0641: MOV dword ptr [RSP + 0x28],R8D
1401d0646: LEA R8,[0x14023a980]
1401d064d: MOV dword ptr [RSP + 0x20],EAX
1401d0651: CALL 0x140007968
1401d0656: MOV RCX,qword ptr [0x140246148]
1401d065d: CMP RCX,R13
1401d0660: JZ 0x1401d069d
1401d0662: TEST byte ptr [RCX + 0xa4],R12B
1401d0669: JZ 0x1401d069d
1401d066b: CMP byte ptr [RCX + 0xa1],0x3
1401d0672: JC 0x1401d069d
1401d0674: MOV EAX,dword ptr [RDI + 0x8]
1401d0677: LEA R8,[0x14023a980]
1401d067e: MOV R9D,dword ptr [RDI]
1401d0681: MOV EDX,0x9a
1401d0686: MOV RCX,qword ptr [RCX + 0x90]
1401d068d: MOV dword ptr [RSP + 0x28],EAX
1401d0691: MOV EAX,dword ptr [RDI + 0x4]
1401d0694: MOV dword ptr [RSP + 0x20],EAX
1401d0698: CALL 0x140012b88
1401d069d: ADD RDI,0x28
1401d06a1: TEST byte ptr [R14 + 0x18],0x20
1401d06a6: JZ 0x1401d06f4
1401d06a8: MOV R12D,dword ptr [R14 + 0x10]
1401d06ac: MOV dword ptr [RBP + -0x74],R12D
1401d06b0: MOV RCX,qword ptr [0x140246148]
1401d06b7: CMP RCX,R13
1401d06ba: JZ 0x1401d06ee
1401d06bc: TEST byte ptr [RCX + 0xa4],0x1
1401d06c3: JZ 0x1401d06ee
1401d06c5: CMP byte ptr [RCX + 0xa1],0x3
1401d06cc: JC 0x1401d06ee
1401d06ce: MOV RCX,qword ptr [RCX + 0x90]
1401d06d5: LEA R8,[0x14023a980]
1401d06dc: MOV EDX,0x9b
1401d06e1: MOV dword ptr [RSP + 0x20],R12D
1401d06e6: MOV R9,RBX
1401d06e9: CALL 0x140001664
1401d06ee: MOV R12D,0x1
1401d06f4: MOVZX ECX,byte ptr [R15]
1401d06f8: ADD ESI,R12D
1401d06fb: CMP ESI,ECX
1401d06fd: JC 0x1401d05de
1401d0703: MOV R13,qword ptr [RBP + -0x80]
1401d0707: MOVZX R14D,CL
1401d070b: LEA RSI,[0x140246148]
1401d0712: MOV dword ptr [RBP + -0x78],R14D
1401d0716: LEA RDI,[0x14023a980]
1401d071d: XOR R15D,R15D
1401d0720: MOV RCX,R13
1401d0723: CALL 0x1401ce900
1401d0728: TEST EAX,EAX
1401d072a: JNZ 0x1401d0e4d
1401d0730: MOV RCX,qword ptr [0x140246148]
1401d0737: CMP RCX,RSI
1401d073a: JZ 0x1401d0765
1401d073c: TEST byte ptr [RCX + 0xa4],R12B
1401d0743: JZ 0x1401d0765
1401d0745: CMP byte ptr [RCX + 0xa1],0x4
1401d074c: JC 0x1401d0765
1401d074e: MOV RCX,qword ptr [RCX + 0x90]
1401d0755: MOV EDX,0x9c
1401d075a: MOV R9,RBX
1401d075d: MOV R8,RDI
1401d0760: CALL 0x140001600
1401d0765: MOV RCX,R13
1401d0768: CALL 0x14000d410
1401d076d: MOV dword ptr [RSP + 0x58],EAX
1401d0771: TEST EAX,EAX
1401d0773: JZ 0x1401d0dc2
1401d0779: MOV RCX,qword ptr [0x140246148]
1401d0780: CMP RCX,RSI
1401d0783: JZ 0x1401d13b7
1401d0789: TEST byte ptr [RCX + 0xa4],R12B
1401d0790: JZ 0x1401d13b7
1401d0796: CMP byte ptr [RCX + 0xa1],R12B
1401d079d: JC 0x1401d13b7
1401d07a3: MOV EDX,0x9d
1401d07a8: MOV dword ptr [RSP + 0x20],0x68f
1401d07b0: MOV R8,RDI
1401d07b3: MOV RCX,qword ptr [RCX + 0x90]
1401d07ba: MOV R9,RBX
1401d07bd: CALL 0x140001664
1401d07c2: JMP 0x1401d13b7
1401d07c7: MOV RCX,qword ptr [0x140246148]
1401d07ce: CMP RCX,RSI
1401d07d1: JZ 0x1401d07ff
1401d07d3: TEST byte ptr [RCX + 0x2c],R12B
1401d07d7: JZ 0x1401d07ff
1401d07d9: CMP byte ptr [RCX + 0x29],R12B
1401d07dd: JC 0x1401d07ff
1401d07df: MOV RCX,qword ptr [RCX + 0x18]
1401d07e3: LEA R9,[0x140224880]
1401d07ea: MOV EDX,0x80
1401d07ef: MOV dword ptr [RSP + 0x20],0x5a0
1401d07f7: MOV R8,RDI
1401d07fa: CALL 0x140001664
1401d07ff: MOV RCX,qword ptr [RSP + 0x70]
1401d0804: TEST RCX,RCX
1401d0807: JZ 0x1401d0818
1401d0809: CALL 0x14020d50e
1401d080e: MOV RCX,qword ptr [RSP + 0x70]
1401d0813: CALL 0x14020d502
1401d0818: MOV RCX,qword ptr [0x140246148]
1401d081f: CMP RCX,RSI
1401d0822: JZ 0x1401d0853
1401d0824: TEST byte ptr [RCX + 0xa4],R12B
1401d082b: JZ 0x1401d0853
1401d082d: CMP byte ptr [RCX + 0xa1],0x5
1401d0834: JC 0x1401d0853
1401d0836: MOV R9D,dword ptr [RSP + 0x58]
1401d083b: LEA R8,[0x14023a980]
1401d0842: MOV RCX,qword ptr [RCX + 0x90]
1401d0849: MOV EDX,0xad
1401d084e: CALL 0x14000764c
1401d0853: MOV EAX,dword ptr [RSP + 0x58]
1401d0857: MOV RCX,qword ptr [RBP + 0x130]
1401d085e: XOR RCX,RSP
1401d0861: CALL 0x14020d560
1401d0866: LEA R11,[RSP + 0x240]
1401d086e: MOV RBX,qword ptr [R11 + 0x38]
1401d0872: MOV RSI,qword ptr [R11 + 0x40]
1401d0876: MOV RDI,qword ptr [R11 + 0x48]
1401d087a: MOV RSP,R11
1401d087d: POP R15
1401d087f: POP R14
1401d0881: POP R13
1401d0883: POP R12
1401d0885: POP RBP
1401d0886: RET
1401d0887: MOV RCX,qword ptr [0x140246148]
1401d088e: CMP RCX,RSI
1401d0891: JZ 0x1401d08c0
1401d0893: TEST byte ptr [RCX + 0xa4],R12B
1401d089a: JZ 0x1401d08c0
1401d089c: CMP byte ptr [RCX + 0xa1],0x3
1401d08a3: JC 0x1401d08c0
1401d08a5: MOV R9,qword ptr [R13 + 0x1465077]
1401d08ac: MOV EDX,0x84
1401d08b1: MOV RCX,qword ptr [RCX + 0x90]
1401d08b8: MOV R8,RDI
1401d08bb: CALL 0x140001600
1401d08c0: MOV RDX,qword ptr [R13 + 0x1465077]
1401d08c7: LEA R8,[RSP + 0x50]
1401d08cc: MOV RCX,R13
1401d08cf: CALL 0x1401ff1e0
1401d08d4: MOV qword ptr [RSP + 0x60],RAX
1401d08d9: TEST RAX,RAX
1401d08dc: JZ 0x1401d0a2a
1401d08e2: MOV RCX,qword ptr [0x140246148]
1401d08e9: CMP RCX,RSI
1401d08ec: JZ 0x1401d090e
1401d08ee: TEST byte ptr [RCX + 0x2c],R12B
1401d08f2: JZ 0x1401d090e
1401d08f4: CMP byte ptr [RCX + 0x29],0x3
1401d08f8: JC 0x1401d090e
1401d08fa: MOV RCX,qword ptr [RCX + 0x18]
1401d08fe: MOV EDX,0x85
1401d0903: MOV R9,RBX
1401d0906: MOV R8,RDI
1401d0909: CALL 0x140001600
1401d090e: MOV EDX,dword ptr [RSP + 0x50]
1401d0912: MOV byte ptr [RSP + 0x54],R12B
1401d0917: TEST EDX,EDX
1401d0919: JNZ 0x1401d0951
1401d091b: MOV RCX,qword ptr [0x140246148]
1401d0922: CMP RCX,RSI
1401d0925: JZ 0x1401d13c2
1401d092b: TEST byte ptr [RCX + 0x2c],R12B
1401d092f: JZ 0x1401d13c2
1401d0935: CMP byte ptr [RCX + 0x29],R12B
1401d0939: JC 0x1401d13c2
1401d093f: MOV EDX,0x86
1401d0944: MOV dword ptr [RSP + 0x20],0x5c4
1401d094c: JMP 0x1401d039d
1401d0951: LEA R9,[0x1402248a0]
1401d0958: MOV dword ptr [0x14025e621],EDX
1401d095e: MOV R8D,0x35414b4d
1401d0964: MOV dword ptr [RSP + 0x20],0x5c9
1401d096c: LEA RCX,[0x14025e619]
1401d0973: CALL 0x14000feac
1401d0978: MOV RCX,qword ptr [0x14025e619]
1401d097f: TEST RCX,RCX
1401d0982: JNZ 0x1401d09ca
1401d0984: MOV RCX,qword ptr [0x140246148]
1401d098b: CMP RCX,RSI
1401d098e: JZ 0x1401d13c2
1401d0994: TEST byte ptr [RCX + 0x4a0],R12B
1401d099b: JZ 0x1401d13c2
1401d09a1: CMP byte ptr [RCX + 0x49d],R12B
1401d09a8: JC 0x1401d13c2
1401d09ae: MOV EAX,dword ptr [0x14025e621]
1401d09b4: MOV EDX,0x87
1401d09b9: MOV dword ptr [RSP + 0x28],EAX
1401d09bd: MOV dword ptr [RSP + 0x20],0x5cd
1401d09c5: JMP 0x1401d0425
1401d09ca: MOV R8D,dword ptr [0x14025e621]
1401d09d1: MOV RDX,qword ptr [RSP + 0x60]
1401d09d6: CALL 0x14020d600
1401d09db: MOV RCX,qword ptr [0x140246148]
1401d09e2: CMP RCX,RSI
1401d09e5: JZ 0x1401d048b
1401d09eb: TEST byte ptr [RCX + 0x2c],R12B
1401d09ef: JZ 0x1401d0cf5
1401d09f5: CMP byte ptr [RCX + 0x29],0x3
1401d09f9: JC 0x1401d0cf5
1401d09ff: MOV EAX,dword ptr [0x14025e621]
1401d0a05: MOV EDX,0x88
1401d0a0a: MOV RCX,qword ptr [RCX + 0x18]
1401d0a0e: MOV R9,RBX
1401d0a11: MOV dword ptr [RSP + 0x28],EAX
1401d0a15: MOV R8,RDI
1401d0a18: MOV EAX,dword ptr [RSP + 0x50]
1401d0a1c: MOV dword ptr [RSP + 0x20],EAX
1401d0a20: CALL 0x140007b24
1401d0a25: JMP 0x1401d0cf5
1401d0a2a: MOV dword ptr [R13 + 0x1464d29],0x6
1401d0a35: LEA RCX,[RBP + -0x70]
1401d0a39: MOV RDX,qword ptr [R13 + 0x1465077]
1401d0a40: CALL 0x14020d514
1401d0a45: MOV RCX,qword ptr [0x140246148]
1401d0a4c: CMP RCX,RSI
1401d0a4f: JZ 0x1401d0a7e
1401d0a51: TEST byte ptr [RCX + 0xa4],R12B
1401d0a58: JZ 0x1401d0a7e
1401d0a5a: CMP byte ptr [RCX + 0xa1],0x3
1401d0a61: JC 0x1401d0a7e
1401d0a63: MOV R9,qword ptr [R13 + 0x1465077]
1401d0a6a: MOV EDX,0x89
1401d0a6f: MOV RCX,qword ptr [RCX + 0x90]
1401d0a76: MOV R8,RDI
1401d0a79: CALL 0x140001600
1401d0a7e: MOV RAX,qword ptr [RSP + 0x78]
1401d0a83: LEA R9,[RBP + -0x70]
1401d0a87: LEA R8,[RSP + 0x50]
1401d0a8c: MOV qword ptr [RSP + 0x20],RAX
1401d0a91: LEA RDX,[RSP + 0x70]
1401d0a96: LEA RCX,[RSP + 0x58]
1401d0a9b: CALL 0x14020d4fc
1401d0aa0: CMP word ptr [RBP + -0x70],R15W
1401d0aa5: JBE 0x1401d0ab7
1401d0aa7: MOVZX EDX,word ptr [RBP + -0x6e]
1401d0aab: XOR R8D,R8D
1401d0aae: MOV RCX,qword ptr [RBP + -0x68]
1401d0ab2: CALL 0x14008d67e
1401d0ab7: MOV RCX,qword ptr [0x140246148]
1401d0abe: CMP RCX,RSI
1401d0ac1: JZ 0x1401d0aeb
1401d0ac3: TEST byte ptr [RCX + 0x2c],R12B
1401d0ac7: JZ 0x1401d0aeb
1401d0ac9: CMP byte ptr [RCX + 0x29],0x3
1401d0acd: JC 0x1401d0aeb
1401d0acf: MOV EAX,dword ptr [RSP + 0x50]
1401d0ad3: MOV EDX,0x8c
1401d0ad8: MOV RCX,qword ptr [RCX + 0x18]
1401d0adc: MOV R9,RBX
1401d0adf: MOV R8,RDI
1401d0ae2: MOV dword ptr [RSP + 0x20],EAX
1401d0ae6: CALL 0x140001664
1401d0aeb: CMP dword ptr [RSP + 0x58],R15D
1401d0af0: JNZ 0x1401d0d8c
1401d0af6: CMP dword ptr [RSP + 0x50],R15D
1401d0afb: JZ 0x1401d0d8c
1401d0b01: MOV R8,qword ptr [RSP + 0x70]
1401d0b06: LEA RDX,[RSP + 0x60]
1401d0b0b: LEA RCX,[RSP + 0x58]
1401d0b10: CALL 0x14020d508
1401d0b15: MOV RCX,qword ptr [0x140246148]
1401d0b1c: CMP RCX,RSI
1401d0b1f: JZ 0x1401d0b49
1401d0b21: TEST byte ptr [RCX + 0x2c],R12B
1401d0b25: JZ 0x1401d0b49
1401d0b27: CMP byte ptr [RCX + 0x29],0x4
1401d0b2b: JC 0x1401d0b49
1401d0b2d: MOV EAX,dword ptr [RSP + 0x50]
1401d0b31: MOV EDX,0x8f
1401d0b36: MOV RCX,qword ptr [RCX + 0x18]
1401d0b3a: MOV R9,RBX
1401d0b3d: MOV R8,RDI
1401d0b40: MOV dword ptr [RSP + 0x20],EAX
1401d0b44: CALL 0x140001664
1401d0b49: CMP dword ptr [RSP + 0x58],R15D
1401d0b4e: JZ 0x1401d0b9c
1401d0b50: MOV RCX,qword ptr [0x140246148]
1401d0b57: CMP RCX,RSI
1401d0b5a: JZ 0x1401d0b84
1401d0b5c: TEST byte ptr [RCX + 0x2c],R12B
1401d0b60: JZ 0x1401d0b84
1401d0b62: CMP byte ptr [RCX + 0x29],R12B
1401d0b66: JC 0x1401d0b84
1401d0b68: MOV EDX,0x90
1401d0b6d: MOV dword ptr [RSP + 0x20],0x617
1401d0b75: MOV RCX,qword ptr [RCX + 0x18]
1401d0b79: MOV R9,RBX
1401d0b7c: MOV R8,RDI
1401d0b7f: CALL 0x140001664
1401d0b84: MOV RCX,qword ptr [RSP + 0x70]
1401d0b89: TEST RCX,RCX
1401d0b8c: JZ 0x1401d0818
1401d0b92: CALL 0x14020d502
1401d0b97: JMP 0x1401d07ff
1401d0b9c: MOV RCX,qword ptr [RSP + 0x60]
1401d0ba1: TEST RCX,RCX
1401d0ba4: JZ 0x1401d0d4b
1401d0baa: MOV EDX,dword ptr [RSP + 0x50]
1401d0bae: ADD RCX,-0x24
1401d0bb2: ADD RDX,RCX
1401d0bb5: MOV R8D,0x24
1401d0bbb: LEA RCX,[R13 + 0x3550f8]
1401d0bc2: CALL 0x140010118
1401d0bc7: MOV RCX,qword ptr [0x140246148]
1401d0bce: CMP RCX,RSI
1401d0bd1: JZ 0x1401d0bfb
1401d0bd3: TEST byte ptr [RCX + 0x2c],R12B
1401d0bd7: JZ 0x1401d0bfb
1401d0bd9: CMP byte ptr [RCX + 0x29],0x4
1401d0bdd: JC 0x1401d0bfb
1401d0bdf: MOV EAX,dword ptr [RSP + 0x50]
1401d0be3: MOV EDX,0x92
1401d0be8: MOV RCX,qword ptr [RCX + 0x18]
1401d0bec: MOV R9,RBX
1401d0bef: MOV R8,RDI
1401d0bf2: MOV dword ptr [RSP + 0x20],EAX
1401d0bf6: CALL 0x140001664
1401d0bfb: MOV EDX,dword ptr [RSP + 0x50]
1401d0bff: TEST EDX,EDX
1401d0c01: JNZ 0x1401d0c59
1401d0c03: MOV RCX,qword ptr [0x140246148]
1401d0c0a: CMP RCX,RSI
1401d0c0d: JZ 0x1401d0c37
1401d0c0f: TEST byte ptr [RCX + 0x2c],R12B
1401d0c13: JZ 0x1401d0c37
1401d0c15: CMP byte ptr [RCX + 0x29],R12B
1401d0c19: JC 0x1401d0c37
1401d0c1b: MOV RCX,qword ptr [RCX + 0x18]
1401d0c1f: MOV EDX,0x93
1401d0c24: MOV R9,RBX
1401d0c27: MOV dword ptr [RSP + 0x20],0x638
1401d0c2f: MOV R8,RDI
1401d0c32: CALL 0x140001664
1401d0c37: MOV RCX,qword ptr [RSP + 0x70]
1401d0c3c: TEST RCX,RCX
1401d0c3f: JZ 0x1401d0d7f
1401d0c45: CALL 0x14020d50e
1401d0c4a: MOV RCX,qword ptr [RSP + 0x70]
1401d0c4f: CALL 0x14020d502
1401d0c54: JMP 0x1401d0d7f
1401d0c59: LEA R9,[0x1402248a0]
1401d0c60: MOV dword ptr [0x14025e621],EDX
1401d0c66: MOV R8D,0x35414b4d
1401d0c6c: MOV dword ptr [RSP + 0x20],0x64a
1401d0c74: LEA RCX,[0x14025e619]
1401d0c7b: CALL 0x14000feac
1401d0c80: MOV RCX,qword ptr [0x14025e619]
1401d0c87: TEST RCX,RCX
1401d0c8a: JNZ 0x1401d0ce4
1401d0c8c: MOV RCX,qword ptr [0x140246148]
1401d0c93: CMP RCX,RSI
1401d0c96: JZ 0x1401d0d7f
1401d0c9c: TEST byte ptr [RCX + 0x4a0],R12B
1401d0ca3: JZ 0x1401d0d7f
1401d0ca9: CMP byte ptr [RCX + 0x49d],R12B
1401d0cb0: JC 0x1401d0d7f
1401d0cb6: MOV EAX,dword ptr [0x14025e621]
1401d0cbc: MOV EDX,0x94
1401d0cc1: MOV RCX,qword ptr [RCX + 0x48c]
1401d0cc8: MOV R9,RBX
1401d0ccb: MOV dword ptr [RSP + 0x28],EAX
1401d0ccf: MOV R8,RDI
1401d0cd2: MOV dword ptr [RSP + 0x20],0x64e
1401d0cda: CALL 0x140007b24
1401d0cdf: JMP 0x1401d0d7f
1401d0ce4: MOV R8D,dword ptr [0x14025e621]
1401d0ceb: MOV RDX,qword ptr [RSP + 0x60]
1401d0cf0: CALL 0x14020d600
1401d0cf5: MOV RCX,qword ptr [0x140246148]
1401d0cfc: CMP RCX,RSI
1401d0cff: JZ 0x1401d048b
1401d0d05: TEST byte ptr [RCX + 0xa4],R12B
1401d0d0c: JZ 0x1401d048b
1401d0d12: CMP byte ptr [RCX + 0xa1],0x4
1401d0d19: JC 0x1401d048b
1401d0d1f: MOV EAX,dword ptr [RSP + 0x50]
1401d0d23: MOV EDX,0x95
1401d0d28: MOV RCX,qword ptr [RCX + 0x90]
1401d0d2f: MOV R9,RBX
1401d0d32: MOV dword ptr [RSP + 0x28],EAX
1401d0d36: MOV R8,RDI
1401d0d39: MOV dword ptr [RSP + 0x20],0x654
1401d0d41: CALL 0x140007b24
1401d0d46: JMP 0x1401d048b
1401d0d4b: MOV RCX,qword ptr [0x140246148]
1401d0d52: CMP RCX,RSI
1401d0d55: JZ 0x1401d0d7f
1401d0d57: TEST byte ptr [RCX + 0x2c],R12B
1401d0d5b: JZ 0x1401d0d7f
1401d0d5d: CMP byte ptr [RCX + 0x29],R12B
1401d0d61: JC 0x1401d0d7f
1401d0d63: MOV RCX,qword ptr [RCX + 0x18]
1401d0d67: MOV EDX,0x91
1401d0d6c: MOV R9,RBX
1401d0d6f: MOV dword ptr [RSP + 0x20],0x62d
1401d0d77: MOV R8,RDI
1401d0d7a: CALL 0x140001664
1401d0d7f: MOV dword ptr [RSP + 0x58],0xc0000001
1401d0d87: JMP 0x1401d07ff
1401d0d8c: MOV RCX,qword ptr [0x140246148]
1401d0d93: CMP RCX,RSI
1401d0d96: JZ 0x1401d0b84
1401d0d9c: TEST byte ptr [RCX + 0x2c],R12B
1401d0da0: JZ 0x1401d0b84
1401d0da6: CMP byte ptr [RCX + 0x29],R12B
1401d0daa: JC 0x1401d0b84
1401d0db0: MOV EDX,0x8d
1401d0db5: MOV dword ptr [RSP + 0x20],0x5f5
1401d0dbd: JMP 0x1401d0b75
1401d0dc2: MOV EDI,R15D
1401d0dc5: MOV RCX,R13
1401d0dc8: CALL 0x1401ce900
1401d0dcd: MOV ESI,EAX
1401d0dcf: CMP EAX,R12D
1401d0dd2: JZ 0x1401d0e88
1401d0dd8: MOV ECX,0x3e8
1401d0ddd: CALL 0x14000ee4c
1401d0de2: ADD EDI,R12D
1401d0de5: CMP EDI,0x1f4
1401d0deb: JBE 0x1401d0dc5
1401d0ded: MOV RCX,qword ptr [0x140246148]
1401d0df4: LEA RAX,[0x140246148]
1401d0dfb: CMP RCX,RAX
1401d0dfe: JZ 0x1401d0e39
1401d0e00: TEST byte ptr [RCX + 0xa4],R12B
1401d0e07: JZ 0x1401d0e39
1401d0e09: CMP byte ptr [RCX + 0xa1],R12B
1401d0e10: JC 0x1401d0e39
1401d0e12: MOV RCX,qword ptr [RCX + 0x90]
1401d0e19: LEA R8,[0x14023a980]
1401d0e20: MOV EDX,0x9e
1401d0e25: MOV dword ptr [RSP + 0x28],ESI
1401d0e29: MOV R9,RBX
1401d0e2c: MOV dword ptr [RSP + 0x20],0x6a3
1401d0e34: CALL 0x140007b24
1401d0e39: MOV dword ptr [RSP + 0x58],0xc0000001
1401d0e41: LEA RSI,[0x140246148]
1401d0e48: JMP 0x1401d13b7
1401d0e4d: MOV RCX,qword ptr [0x140246148]
1401d0e54: CMP RCX,RSI
1401d0e57: JZ 0x1401d0e8f
1401d0e59: TEST byte ptr [RCX + 0xa4],R12B
1401d0e60: JZ 0x1401d0e8f
1401d0e62: CMP byte ptr [RCX + 0xa1],0x4
1401d0e69: JC 0x1401d0e8f
1401d0e6b: MOV RCX,qword ptr [RCX + 0x90]
1401d0e72: MOV EDX,0x9f
1401d0e77: MOV R9,RBX
1401d0e7a: MOV dword ptr [RSP + 0x20],EAX
1401d0e7e: MOV R8,RDI
1401d0e81: CALL 0x140001664
1401d0e86: JMP 0x1401d0e8f
1401d0e88: LEA RSI,[0x140246148]
1401d0e8f: MOV dword ptr [RSP + 0x68],R15D
1401d0e94: MOV R8D,R15D
1401d0e97: MOV dword ptr [RSP + 0x78],R15D
1401d0e9c: TEST R14D,R14D
1401d0e9f: JZ 0x1401d1186
1401d0ea5: LEA R12,[RBP + -0x50]
1401d0ea9: MOVZX EAX,byte ptr [R12 + 0x8]
1401d0eaf: XOR EDI,EDI
1401d0eb1: MOV R14D,dword ptr [R12 + 0x4]
1401d0eb6: MOV dword ptr [RBP + -0x80],R14D
1401d0eba: TEST AL,0x1
1401d0ebc: JZ 0x1401d0ecd
1401d0ebe: MOV EDI,EAX
1401d0ec0: AND EDI,0x6
1401d0ec3: OR EDI,0x9
1401d0ec6: TEST AL,0x10
1401d0ec8: JZ 0x1401d0ecd
1401d0eca: OR EDI,0x40
1401d0ecd: BTS EDI,0x1f
1401d0ed1: MOV ESI,EAX
1401d0ed3: AND ESI,0x80
1401d0ed9: MOV RCX,qword ptr [0x140246148]
1401d0ee0: LEA RAX,[0x140246148]
1401d0ee7: CMP RCX,RAX
1401d0eea: JZ 0x1401d0f3c
1401d0eec: TEST byte ptr [RCX + 0xa4],0x1
1401d0ef3: JZ 0x1401d0f3c
1401d0ef5: CMP byte ptr [RCX + 0xa1],0x3
1401d0efc: JC 0x1401d0f3c
1401d0efe: MOV EAX,dword ptr [R12]
1401d0f02: MOV EDX,0xa0
1401d0f07: MOV RCX,qword ptr [RCX + 0x90]
1401d0f0e: MOV R9,RBX
1401d0f11: MOV dword ptr [RSP + 0x40],EDI
1401d0f15: MOV dword ptr [RSP + 0x38],R14D
1401d0f1a: MOV dword ptr [RSP + 0x30],EAX
1401d0f1e: MOV dword ptr [RSP + 0x28],R8D
1401d0f23: LEA R8,[0x14023a980]
1401d0f2a: MOV dword ptr [RSP + 0x20],0x6c6
1401d0f32: CALL 0x140015c00
1401d0f37: MOV R8D,dword ptr [RSP + 0x68]
1401d0f3c: TEST ESI,ESI
1401d0f3e: JZ 0x1401d0f91
1401d0f40: MOV RCX,qword ptr [0x140246148]
1401d0f47: LEA RSI,[0x140246148]
1401d0f4e: CMP RCX,RSI
1401d0f51: JZ 0x1401d1162
1401d0f57: TEST byte ptr [RCX + 0xa4],0x1
1401d0f5e: JZ 0x1401d1162
1401d0f64: CMP byte ptr [RCX + 0xa1],0x3
1401d0f6b: JC 0x1401d1162
1401d0f71: MOV RCX,qword ptr [RCX + 0x90]
1401d0f78: LEA R8,[0x14023a980]
1401d0f7f: MOV EDX,0xa1
1401d0f84: MOV R9,RBX
1401d0f87: CALL 0x140001600
1401d0f8c: JMP 0x1401d115d
1401d0f91: MOV RCX,qword ptr [0x140246148]
1401d0f98: LEA RSI,[0x140246148]
1401d0f9f: CMP RCX,RSI
1401d0fa2: JZ 0x1401d0fea
1401d0fa4: TEST byte ptr [RCX + 0xa4],0x1
1401d0fab: JZ 0x1401d0fea
1401d0fad: CMP byte ptr [RCX + 0xa1],0x3
1401d0fb4: JC 0x1401d0fea
1401d0fb6: MOV EAX,dword ptr [R12]
1401d0fba: LEA R8,[0x14023a980]
1401d0fc1: MOV RCX,qword ptr [RCX + 0x90]
1401d0fc8: MOV EDX,0xa2
1401d0fcd: MOV dword ptr [RSP + 0x38],EDI
1401d0fd1: MOV R9,RBX
1401d0fd4: MOV dword ptr [RSP + 0x30],R14D
1401d0fd9: MOV dword ptr [RSP + 0x28],EAX
1401d0fdd: MOV dword ptr [RSP + 0x20],0x6d0
1401d0fe5: CALL 0x140007bb4
1401d0fea: MOV EDX,dword ptr [R12]
1401d0fee: MOV R9D,EDI
1401d0ff1: AND dword ptr [RSP + 0x20],0x0
1401d0ff6: MOV R8D,R14D
1401d0ff9: MOV RCX,R13
1401d0ffc: CALL 0x1401cb88c
1401d1001: MOV dword ptr [RSP + 0x58],EAX
1401d1005: TEST EAX,EAX
1401d1007: JNZ 0x1401d1180
1401d100d: MOV RCX,qword ptr [0x140246148]
1401d1014: CMP RCX,RSI
1401d1017: JZ 0x1401d1046
1401d1019: TEST byte ptr [RCX + 0xa4],0x1
1401d1020: JZ 0x1401d1046
1401d1022: CMP byte ptr [RCX + 0xa1],0x3
1401d1029: JC 0x1401d1046
1401d102b: MOV RCX,qword ptr [RCX + 0x90]
1401d1032: LEA R8,[0x14023a980]
1401d1039: MOV EDX,0xa3
1401d103e: MOV R9D,R15D
1401d1041: CALL 0x14000764c
1401d1046: XOR ECX,ECX
1401d1048: TEST R14D,R14D
1401d104b: JZ 0x1401d1156
1401d1051: MOV R15D,dword ptr [RSP + 0x68]
1401d1056: LEA R14D,[RCX + 0x800]
1401d105d: CMP R14D,dword ptr [RBP + -0x80]
1401d1061: JNC 0x1401d106a
1401d1063: MOV ESI,0x800
1401d1068: JMP 0x1401d1073
1401d106a: MOVZX ESI,word ptr [R12 + 0x4]
1401d1070: SUB SI,CX
1401d1073: MOV EDI,ECX
1401d1075: ADD RDI,R15
1401d1078: ADD RDI,qword ptr [RSP + 0x60]
1401d107d: MOV RCX,qword ptr [0x140246148]
1401d1084: LEA RAX,[0x140246148]
1401d108b: CMP RCX,RAX
1401d108e: JZ 0x1401d10d1
1401d1090: TEST byte ptr [RCX + 0xa4],0x1
1401d1097: JZ 0x1401d10d1
1401d1099: CMP byte ptr [RCX + 0xa1],0x3
1401d10a0: JC 0x1401d10d1
1401d10a2: MOV RCX,qword ptr [RCX + 0x90]
1401d10a9: LEA R8,[0x14023a980]
1401d10b0: MOVZX EAX,SI
1401d10b3: MOV EDX,0xa4
1401d10b8: MOV dword ptr [RSP + 0x30],EAX
1401d10bc: MOV R9,RBX
1401d10bf: MOV qword ptr [RSP + 0x28],RDI
1401d10c4: MOV dword ptr [RSP + 0x20],0x6e2
1401d10cc: CALL 0x140030fc8
1401d10d1: MOVZX R8D,SI
1401d10d5: MOV RDX,RDI
1401d10d8: MOV RCX,R13
1401d10db: CALL 0x1401cde70
1401d10e0: TEST EAX,EAX
1401d10e2: JNZ 0x1401d10fd
1401d10e4: MOV ECX,R14D
1401d10e7: MOV R14D,dword ptr [RBP + -0x80]
1401d10eb: CMP ECX,R14D
1401d10ee: JC 0x1401d1056
1401d10f4: LEA RSI,[0x140246148]
1401d10fb: JMP 0x1401d1151
1401d10fd: MOV RCX,qword ptr [0x140246148]
1401d1104: LEA RSI,[0x140246148]
1401d110b: CMP RCX,RSI
1401d110e: JZ 0x1401d1145
1401d1110: TEST byte ptr [RCX + 0xa4],0x1
1401d1117: JZ 0x1401d1145
1401d1119: CMP byte ptr [RCX + 0xa1],0x1
1401d1120: JC 0x1401d1145
1401d1122: MOV RCX,qword ptr [RCX + 0x90]
1401d1129: LEA R8,[0x14023a980]
1401d1130: MOV EDX,0xa5
1401d1135: MOV dword ptr [RSP + 0x20],0x6e6
1401d113d: MOV R9,RBX
1401d1140: CALL 0x140001664
1401d1145: MOV R14D,dword ptr [RBP + -0x80]
1401d1149: MOV dword ptr [RSP + 0x58],0xc0000001
1401d1151: MOV R15D,dword ptr [RSP + 0x78]
1401d1156: CMP dword ptr [RSP + 0x58],0x0
1401d115b: JNZ 0x1401d11d7
1401d115d: MOV R8D,dword ptr [RSP + 0x68]
1401d1162: INC R15D
1401d1165: ADD R8D,R14D
1401d1168: ADD R12,0x28
1401d116c: MOV dword ptr [RSP + 0x78],R15D
1401d1171: MOV dword ptr [RSP + 0x68],R8D
1401d1176: CMP R15D,dword ptr [RBP + -0x78]
1401d117a: JC 0x1401d0ea9
1401d1180: MOV R12D,0x1
1401d1186: XOR R15D,R15D
1401d1189: CMP dword ptr [RSP + 0x58],R15D
1401d118e: JZ 0x1401d1228
1401d1194: MOV RCX,qword ptr [0x140246148]
1401d119b: CMP RCX,RSI
1401d119e: JZ 0x1401d13b7
1401d11a4: TEST byte ptr [RCX + 0xa4],R12B
1401d11ab: JZ 0x1401d13b7
1401d11b1: CMP byte ptr [RCX + 0xa1],R12B
1401d11b8: JC 0x1401d13b7
1401d11be: MOV EDX,0xa7
1401d11c3: MOV dword ptr [RSP + 0x20],0x6f8
1401d11cb: LEA R8,[0x14023a980]
1401d11d2: JMP 0x1401d07b3
1401d11d7: MOV RCX,qword ptr [0x140246148]
1401d11de: MOV R12D,0x1
1401d11e4: CMP RCX,RSI
1401d11e7: JZ 0x1401d1186
1401d11e9: TEST byte ptr [RCX + 0xa4],R12B
1401d11f0: JZ 0x1401d1186
1401d11f2: CMP byte ptr [RCX + 0xa1],R12B
1401d11f9: JC 0x1401d1186
1401d11fb: MOV RCX,qword ptr [RCX + 0x90]
1401d1202: LEA R8,[0x14023a980]
1401d1209: MOV EDX,0xa6
1401d120e: MOV dword ptr [RSP + 0x28],R15D
1401d1213: MOV R9,RBX
1401d1216: MOV dword ptr [RSP + 0x20],0x6ee
1401d121e: CALL 0x140007b24
1401d1223: JMP 0x1401d1186
1401d1228: MOV RCX,qword ptr [0x140246148]
1401d122f: CMP RCX,RSI
1401d1232: JZ 0x1401d125e
1401d1234: TEST byte ptr [RCX + 0xa4],R12B
1401d123b: JZ 0x1401d125e
1401d123d: CMP byte ptr [RCX + 0xa1],0x3
1401d1244: JC 0x1401d125e
1401d1246: MOV RCX,qword ptr [RCX + 0x90]
1401d124d: LEA R8,[0x14023a980]
1401d1254: MOV EDX,0xa8
1401d1259: CALL 0x1400015d8
1401d125e: MOV R8D,dword ptr [RBP + -0x74]
1401d1262: MOV EDX,R15D
1401d1265: TEST R8D,R8D
1401d1268: MOV RCX,R13
1401d126b: SETNZ DL
1401d126e: XOR R9D,R9D
1401d1271: CALL 0x1401ce7c0
1401d1276: MOV dword ptr [RSP + 0x58],EAX
1401d127a: TEST EAX,EAX
1401d127c: JZ 0x1401d12ba
1401d127e: MOV RCX,qword ptr [0x140246148]
1401d1285: CMP RCX,RSI
1401d1288: JZ 0x1401d13b7
1401d128e: TEST byte ptr [RCX + 0xa4],R12B
1401d1295: JZ 0x1401d13b7
1401d129b: CMP byte ptr [RCX + 0xa1],R12B
1401d12a2: JC 0x1401d13b7
1401d12a8: MOV EDX,0xa9
1401d12ad: MOV dword ptr [RSP + 0x20],0x702
1401d12b5: JMP 0x1401d11cb
1401d12ba: MOV EDI,R15D
1401d12bd: MOV R14D,0x1f4
1401d12c3: MOV RCX,R13
1401d12c6: CALL 0x1401ce900
1401d12cb: CMP EAX,0x3
1401d12ce: JZ 0x1401d12e4
1401d12d0: MOV ECX,0x3e8
1401d12d5: CALL 0x14000ee4c
1401d12da: ADD EDI,R12D
1401d12dd: CMP EDI,R14D
1401d12e0: JC 0x1401d12c3
1401d12e2: JMP 0x1401d1337
1401d12e4: MOV RCX,qword ptr [0x140246148]
1401d12eb: CMP RCX,RSI
1401d12ee: JZ 0x1401d131a
1401d12f0: TEST byte ptr [RCX + 0xa4],R12B
1401d12f7: JZ 0x1401d131a
1401d12f9: CMP byte ptr [RCX + 0xa1],0x3
1401d1300: JC 0x1401d131a
1401d1302: MOV RCX,qword ptr [RCX + 0x90]
1401d1309: LEA R8,[0x14023a980]
1401d1310: MOV EDX,0xaa
1401d1315: CALL 0x1400015d8
1401d131a: MOV byte ptr [R13 + 0x1464d62],R12B
1401d1321: MOV RAX,[0xfffff78000000014]
1401d132b: MOV qword ptr [R13 + 0x1464d63],RAX
1401d1332: CMP EDI,R14D
1401d1335: JC 0x1401d136f
1401d1337: MOV dword ptr [RSP + 0x58],0xc0000001
1401d133f: MOV RCX,qword ptr [0x140246148]
1401d1346: CMP RCX,RSI
1401d1349: JZ 0x1401d13b7
1401d134b: TEST byte ptr [RCX + 0xa4],R12B
1401d1352: JZ 0x1401d13b7
1401d1354: CMP byte ptr [RCX + 0xa1],R12B
1401d135b: JC 0x1401d13b7
1401d135d: MOV EDX,0xab
1401d1362: MOV dword ptr [RSP + 0x20],0x71b
1401d136a: JMP 0x1401d11cb
1401d136f: MOV dword ptr [R13 + 0x1464d29],0x12
1401d137a: MOV byte ptr [0x14025e618],R12B
1401d1381: MOV RCX,qword ptr [0x140246148]
1401d1388: CMP RCX,RSI
1401d138b: JZ 0x1401d13b7
1401d138d: TEST byte ptr [RCX + 0xa4],R12B
1401d1394: JZ 0x1401d13b7
1401d1396: CMP byte ptr [RCX + 0xa1],0x3
1401d139d: JC 0x1401d13b7
1401d139f: MOV RCX,qword ptr [RCX + 0x90]
1401d13a6: LEA R8,[0x14023a980]
1401d13ad: MOV EDX,0xac
1401d13b2: CALL 0x1400015d8
1401d13b7: CMP byte ptr [RSP + 0x54],R12B
1401d13bc: JNZ 0x1401d07ff
1401d13c2: MOV RCX,qword ptr [RSP + 0x60]
1401d13c7: TEST RCX,RCX
1401d13ca: JZ 0x1401d07ff
1401d13d0: MOV EDX,dword ptr [0x14025e621]
1401d13d6: CALL 0x1400100e8
1401d13db: JMP 0x1401d0818
```

### Decompiled C

```c

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

int FUN_1401d01d0(longlong param_1)

{
  byte bVar1;
  byte bVar2;
  undefined4 uVar3;
  int iVar4;
  undefined8 uVar5;
  ushort uVar6;
  uint uVar7;
  ulonglong uVar8;
  uint uVar9;
  uint uVar10;
  undefined1 *puVar11;
  undefined8 *puVar12;
  longlong lVar13;
  uint *puVar14;
  uint uVar15;
  undefined1 auStack_268 [32];
  undefined1 *local_248;
  longlong local_240;
  uint local_238;
  uint local_230;
  uint local_228;
  uint local_218;
  char local_214;
  int local_210 [2];
  longlong local_208;
  uint local_200;
  longlong local_1f8;
  uint local_1f0;
  undefined4 uStack_1ec;
  longlong local_1e8;
  uint local_1e0;
  int local_1dc;
  undefined8 local_1d8;
  undefined8 uStack_1d0;
  undefined8 local_1c8 [2];
  uint local_1b8 [4];
  undefined8 auStack_1a8 [46];
  ulonglong local_38;
  
  local_38 = DAT_14024f600 ^ (ulonglong)auStack_268;
  local_1e0 = 0;
  uVar7 = 0;
  local_1e8 = param_1;
  FUN_14020d8c0(local_1c8,0,400);
  local_1f0 = 0xffffffff;
  uStack_1ec = 0xffffffff;
  local_1d8 = 0;
  uStack_1d0 = 0;
  local_210[0] = -0x3fffffff;
  local_218 = 0;
  local_208 = 0;
  local_1dc = 0;
  local_214 = '\0';
  local_1f8 = 0;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x7d,&DAT_14023a980);
  }
  if (DAT_14025e618 == '\x01') {
    if (DAT_14025e621 == 0) {
      DAT_14025e5f8 = 0;
      goto LAB_1401d02c3;
    }
LAB_1401d0494:
    local_208 = DAT_14025e619;
    local_218 = DAT_14025e621;
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
      local_248 = (undefined1 *)CONCAT44(local_248._4_4_,DAT_14025e621);
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x96,&DAT_14023a980,
                    "AsicConnac3xLoadFirmware");
    }
LAB_1401d04dd:
    if (local_208 != 0) {
      puVar11 = (undefined1 *)((ulonglong)local_218 + local_208 + -0x24);
      if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
        if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
          local_240 = CONCAT44(local_240._4_4_,(uint)(byte)puVar11[2]);
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,(byte)puVar11[1] + 1);
          FUN_140012b88(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x97,&DAT_14023a980,*puVar11);
        }
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
          local_248 = puVar11 + 7;
          FUN_140012e48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x98,&DAT_14023a980,
                        puVar11 + 0x11);
        }
      }
      *(undefined1 *)(param_1 + 0x355180) = 0x3b;
      FUN_140010118(param_1 + 0x355181,local_208 + 0x98698,0x51);
      *(undefined1 *)(param_1 + 0x3551d1) = 10;
      bVar1 = puVar11[2];
      uVar8 = 0;
      puVar12 = (undefined8 *)(puVar11 + (ulonglong)bVar1 * -0x28);
      bVar2 = 0;
      if (bVar1 != 0) {
        do {
          uVar5 = puVar12[1];
          uVar7 = *(uint *)(puVar12 + 2);
          uVar9 = *(uint *)((longlong)puVar12 + 0x14);
          uVar10 = *(uint *)(puVar12 + 3);
          uVar3 = *(undefined4 *)((longlong)puVar12 + 0x1c);
          local_1c8[uVar8 * 5] = *puVar12;
          local_1c8[uVar8 * 5 + 1] = uVar5;
          uVar5 = puVar12[4];
          local_1b8[uVar8 * 10] = uVar7;
          local_1b8[uVar8 * 10 + 1] = uVar9;
          local_1b8[uVar8 * 10 + 2] = uVar10;
          *(undefined4 *)((longlong)auStack_1a8 + uVar8 * 0x28 + -4) = uVar3;
          auStack_1a8[uVar8 * 5] = uVar5;
          if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
            if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              local_238 = *(uint *)((longlong)puVar12 + 0x14);
              local_240 = CONCAT44(local_240._4_4_,(uint)*(byte *)(puVar12 + 3));
              local_248 = (undefined1 *)CONCAT44(local_248._4_4_,*(undefined4 *)(puVar12 + 2));
              FID_conflict_WPP_SF_LLDD
                        (*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x99,&DAT_14023a980,uVar8);
            }
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              local_240 = CONCAT44(local_240._4_4_,*(undefined4 *)(puVar12 + 1));
              local_248 = (undefined1 *)
                          CONCAT44(local_248._4_4_,*(undefined4 *)((longlong)puVar12 + 4));
              FUN_140012b88(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x9a,&DAT_14023a980,
                            *(undefined4 *)puVar12);
            }
          }
          if (((((*(byte *)(puVar12 + 3) & 0x20) != 0) &&
               (local_1dc = *(int *)(puVar12 + 2),
               (undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148)) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            local_248 = (undefined1 *)CONCAT44(local_248._4_4_,local_1dc);
            FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x9b,&DAT_14023a980,
                          "AsicConnac3xLoadFirmware");
          }
          bVar2 = puVar11[2];
          uVar7 = (int)uVar8 + 1;
          uVar8 = (ulonglong)uVar7;
          puVar12 = puVar12 + 5;
          param_1 = local_1e8;
        } while (uVar7 < bVar2);
      }
      uVar7 = (uint)bVar2;
      local_1e0 = uVar7;
    }
    uVar9 = 0;
    iVar4 = FUN_1401ce900(param_1);
    if (iVar4 == 0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
        WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x9c,&DAT_14023a980,
                 "AsicConnac3xLoadFirmware");
      }
      local_210[0] = FUN_14000d410(param_1);
      if (local_210[0] == 0) {
        do {
          iVar4 = FUN_1401ce900(param_1);
          if (iVar4 == 1) goto LAB_1401d0e8f;
          FUN_14000ee4c(1000);
          uVar9 = uVar9 + 1;
        } while (uVar9 < 0x1f5);
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
          local_240 = CONCAT44(local_240._4_4_,iVar4);
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6a3);
          FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x9e,&DAT_14023a980,
                        "AsicConnac3xLoadFirmware");
        }
        local_210[0] = -0x3fffffff;
      }
      else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
               ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
        uVar5 = 0x9d;
        local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x68f);
        goto LAB_1401d07b3;
      }
    }
    else {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
        local_248 = (undefined1 *)CONCAT44(local_248._4_4_,iVar4);
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x9f,&DAT_14023a980,
                      "AsicConnac3xLoadFirmware");
      }
LAB_1401d0e8f:
      local_200 = 0;
      local_1f0 = 0;
      if (uVar7 != 0) {
        puVar14 = local_1b8;
        do {
          uVar9 = local_1f0;
          bVar1 = (byte)puVar14[2];
          uVar10 = 0;
          uVar7 = puVar14[1];
          local_1e8 = CONCAT44(local_1e8._4_4_,uVar7);
          if ((bVar1 & 1) != 0) {
            uVar10 = bVar1 & 6 | 9;
            if ((bVar1 & 0x10) != 0) {
              uVar10 = bVar1 & 6 | 0x49;
            }
          }
          uVar10 = uVar10 | 0x80000000;
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            local_238 = *puVar14;
            local_240 = CONCAT44(local_240._4_4_,local_200);
            local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6c6);
            local_230 = uVar7;
            local_228 = uVar10;
            FUN_140015c00(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa0,&DAT_14023a980,
                          "AsicConnac3xLoadFirmware");
          }
          if ((bVar1 & 0x80) == 0) {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              local_240 = CONCAT44(local_240._4_4_,*puVar14);
              local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6d0);
              local_238 = uVar7;
              local_230 = uVar10;
              FUN_140007bb4(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa2,&DAT_14023a980,
                            "AsicConnac3xLoadFirmware");
            }
            local_248 = (undefined1 *)((ulonglong)local_248 & 0xffffffff00000000);
            local_210[0] = FUN_1401cb88c(param_1,*puVar14,uVar7,uVar10);
            if (local_210[0] != 0) break;
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa3,&DAT_14023a980,uVar9);
            }
            if (uVar7 != 0) {
              uVar8 = (ulonglong)local_200;
              uVar10 = 0;
              do {
                uVar15 = uVar10 + 0x800;
                if (uVar15 < (uint)local_1e8) {
                  uVar6 = 0x800;
                }
                else {
                  uVar6 = (short)puVar14[1] - (short)uVar10;
                }
                lVar13 = uVar10 + uVar8 + local_208;
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1]))
                {
                  local_238 = (uint)uVar6;
                  local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6e2);
                  local_240 = lVar13;
                  FUN_140030fc8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa4,&DAT_14023a980,
                                "AsicConnac3xLoadFirmware");
                }
                iVar4 = FUN_1401cde70(param_1,lVar13,uVar6);
                if (iVar4 != 0) {
                  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0'))
                  {
                    local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6e6);
                    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa5,&DAT_14023a980,
                                  "AsicConnac3xLoadFirmware");
                  }
                  local_210[0] = -0x3fffffff;
                  uVar7 = (uint)local_1e8;
                  uVar9 = local_1f0;
                  break;
                }
                uVar7 = (uint)local_1e8;
                uVar10 = uVar15;
                uVar9 = local_1f0;
              } while (uVar15 < (uint)local_1e8);
            }
            if (local_210[0] != 0) {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
                local_240 = CONCAT44(local_240._4_4_,uVar9);
                local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6ee);
                FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa6,&DAT_14023a980,
                              "AsicConnac3xLoadFirmware");
              }
              break;
            }
          }
          else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                   ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa1,&DAT_14023a980,
                     "AsicConnac3xLoadFirmware");
          }
          local_1f0 = uVar9 + 1;
          local_200 = local_200 + uVar7;
          puVar14 = puVar14 + 10;
        } while (local_1f0 < local_1e0);
      }
      if (local_210[0] == 0) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
          FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xa8,&DAT_14023a980);
        }
        local_210[0] = FUN_1401ce7c0(param_1,local_1dc != 0,local_1dc,0);
        if (local_210[0] == 0) {
          uVar7 = 0;
          do {
            iVar4 = FUN_1401ce900(param_1);
            if (iVar4 == 3) {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xaa,&DAT_14023a980);
              }
              *(undefined1 *)(param_1 + 0x1464d62) = 1;
              *(undefined8 *)(param_1 + 0x1464d63) = _DAT_fffff78000000014;
              if (uVar7 < 500) {
                *(undefined4 *)(param_1 + 0x1464d29) = 0x12;
                DAT_14025e618 = '\x01';
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1]))
                {
                  FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xac,&DAT_14023a980);
                }
                goto LAB_1401d13b7;
              }
              break;
            }
            FUN_14000ee4c(1000);
            uVar7 = uVar7 + 1;
          } while (uVar7 < 500);
          local_210[0] = -0x3fffffff;
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
            uVar5 = 0xab;
            local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x71b);
            goto LAB_1401d07b3;
          }
        }
        else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                 ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
          uVar5 = 0xa9;
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x702);
LAB_1401d07b3:
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),uVar5,&DAT_14023a980,
                        "AsicConnac3xLoadFirmware");
        }
      }
      else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
               ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
        uVar5 = 0xa7;
        local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x6f8);
        goto LAB_1401d07b3;
      }
    }
LAB_1401d13b7:
    if (local_214 == '\x01') {
LAB_1401d13c2:
      if (local_208 != 0) {
        FUN_1400100e8(local_208,DAT_14025e621);
        goto LAB_1401d0818;
      }
    }
  }
  else {
LAB_1401d02c3:
    if (DAT_14025e618 != '\0') {
LAB_1401d048b:
      if (DAT_14025e618 == '\x01') goto LAB_1401d0494;
      goto LAB_1401d04dd;
    }
    if (*(int *)(param_1 + 0x146694c) != 1) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
        WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x84,&DAT_14023a980,
                 *(undefined8 *)(param_1 + 0x1465077));
      }
      local_208 = FUN_1401ff1e0(param_1,*(undefined8 *)(param_1 + 0x1465077),&local_218);
      if (local_208 == 0) {
        *(undefined4 *)(param_1 + 0x1464d29) = 6;
        NdisInitializeString(&local_1d8,*(undefined8 *)(param_1 + 0x1465077));
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
          WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x89,&DAT_14023a980,
                   *(undefined8 *)(param_1 + 0x1465077));
        }
        local_248 = (undefined1 *)CONCAT44(uStack_1ec,local_1f0);
        NdisOpenFile(local_210,&local_1f8,&local_218,&local_1d8);
        if ((short)local_1d8 != 0) {
          NdisFreeMemory(uStack_1d0,local_1d8._2_2_,0);
        }
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,local_218);
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x8c,&DAT_14023a980,
                        "AsicConnac3xLoadFirmware");
        }
        if ((local_210[0] == 0) && (local_218 != 0)) {
          NdisMapFile(local_210,&local_208,local_1f8);
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0x29])) {
            local_248 = (undefined1 *)CONCAT44(local_248._4_4_,local_218);
            FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x8f,&DAT_14023a980,
                          "AsicConnac3xLoadFirmware");
          }
          if (local_210[0] == 0) {
            if (local_208 == 0) {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
                local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x62d);
                FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x91,&DAT_14023a980,
                              "AsicConnac3xLoadFirmware");
              }
            }
            else {
              FUN_140010118(param_1 + 0x3550f8,(ulonglong)local_218 + local_208 + -0x24,0x24);
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0x29])) {
                local_248 = (undefined1 *)CONCAT44(local_248._4_4_,local_218);
                FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x92,&DAT_14023a980,
                              "AsicConnac3xLoadFirmware");
              }
              if (local_218 == 0) {
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
                  local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x638);
                  FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x93,&DAT_14023a980,
                                "AsicConnac3xLoadFirmware");
                }
                if (local_1f8 != 0) {
                  NdisUnmapFile();
                  NdisCloseFile(local_1f8);
                }
              }
              else {
                DAT_14025e621 = local_218;
                local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x64a);
                FUN_14000feac(&DAT_14025e619,local_218,0x35414b4d,
                              "e:\\worktmp\\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\\7295\\wlan_driver\\seattle\\wifi_driver\\windows\\hal\\chips\\mtconnac3x.c"
                             );
                if (DAT_14025e619 != 0) {
                  FUN_14020d600(DAT_14025e619,local_208,DAT_14025e621);
                  goto LAB_1401d0cf5;
                }
                if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                    ((PTR_LOOP_140246148[0x4a0] & 1) != 0)) && (PTR_LOOP_140246148[0x49d] != '\0'))
                {
                  local_240 = CONCAT44(local_240._4_4_,DAT_14025e621);
                  local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x64e);
                  FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x48c),0x94,&DAT_14023a980,
                                "AsicConnac3xLoadFirmware");
                }
              }
            }
            local_210[0] = -0x3fffffff;
            goto LAB_1401d07ff;
          }
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
            uVar5 = 0x90;
            local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x617);
            goto LAB_1401d0b75;
          }
        }
        else if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                (((PTR_LOOP_140246148[0x2c] & 1) != 0 && (PTR_LOOP_140246148[0x29] != '\0')))) {
          uVar5 = 0x8d;
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5f5);
LAB_1401d0b75:
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar5,&DAT_14023a980,
                        "AsicConnac3xLoadFirmware");
        }
        if (local_1f8 == 0) goto LAB_1401d0818;
        NdisCloseFile();
        goto LAB_1401d07ff;
      }
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x85,&DAT_14023a980,
                 "AsicConnac3xLoadFirmware");
      }
      local_214 = '\x01';
      if (local_218 == 0) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
          uVar5 = 0x86;
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5c4);
LAB_1401d039d:
          local_214 = '\x01';
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar5,&DAT_14023a980,
                        "AsicConnac3xLoadFirmware");
        }
      }
      else {
        DAT_14025e621 = local_218;
        local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5c9);
        FUN_14000feac(&DAT_14025e619,local_218,0x35414b4d,
                      "e:\\worktmp\\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\\7295\\wlan_driver\\seattle\\wifi_driver\\windows\\hal\\chips\\mtconnac3x.c"
                     );
        if (DAT_14025e619 != 0) {
          FUN_14020d600(DAT_14025e619,local_208,DAT_14025e621);
          if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
            if (((PTR_LOOP_140246148[0x2c] & 1) != 0) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
              local_240 = CONCAT44(local_240._4_4_,DAT_14025e621);
              local_248 = (undefined1 *)CONCAT44(local_248._4_4_,local_218);
              FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x88,&DAT_14023a980,
                            "AsicConnac3xLoadFirmware");
            }
LAB_1401d0cf5:
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
              local_240 = CONCAT44(local_240._4_4_,local_218);
              local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x654);
              FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x95,&DAT_14023a980,
                            "AsicConnac3xLoadFirmware");
            }
          }
          goto LAB_1401d048b;
        }
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x4a0] & 1) != 0)) && (PTR_LOOP_140246148[0x49d] != '\0')) {
          uVar5 = 0x87;
          local_240 = CONCAT44(local_240._4_4_,DAT_14025e621);
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5cd);
LAB_1401d0425:
          FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x48c),uVar5,&DAT_14023a980,
                        "AsicConnac3xLoadFirmware");
        }
      }
      goto LAB_1401d13c2;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x7e,&DAT_14023a980,
               *(undefined8 *)(param_1 + 0x1465077));
    }
    local_208 = FUN_1401ff1e0(param_1,*(undefined8 *)(param_1 + 0x1465077),&local_218);
    if (local_208 != 0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x7f,&DAT_14023a980,
                 "AsicConnac3xLoadFirmware");
      }
      local_214 = '\x01';
      if (local_218 == 0) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
          uVar5 = 0x81;
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5a6);
          goto LAB_1401d039d;
        }
      }
      else {
        DAT_14025e621 = local_218;
        local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5ab);
        FUN_14000feac(&DAT_14025e619,local_218,0x35414b4d,
                      "e:\\worktmp\\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\\7295\\wlan_driver\\seattle\\wifi_driver\\windows\\hal\\chips\\mtconnac3x.c"
                     );
        if (DAT_14025e619 != 0) {
          FUN_14020d600(DAT_14025e619,local_208,DAT_14025e621);
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
            local_240 = CONCAT44(local_240._4_4_,DAT_14025e621);
            local_248 = (undefined1 *)CONCAT44(local_248._4_4_,local_218);
            FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x83,&DAT_14023a980,
                          "AsicConnac3xLoadFirmware");
          }
          goto LAB_1401d048b;
        }
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x4a0] & 1) != 0)) && (PTR_LOOP_140246148[0x49d] != '\0')) {
          uVar5 = 0x82;
          local_240 = CONCAT44(local_240._4_4_,DAT_14025e621);
          local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5af);
          goto LAB_1401d0425;
        }
      }
      goto LAB_1401d13c2;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
      local_248 = (undefined1 *)CONCAT44(local_248._4_4_,0x5a0);
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x80,&DAT_14023a980,
                    "AsicConnac3xLoadFirmware");
    }
  }
LAB_1401d07ff:
  if (local_1f8 != 0) {
    NdisUnmapFile();
    NdisCloseFile(local_1f8);
  }
LAB_1401d0818:
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xad,&DAT_14023a980,local_210[0]);
  }
  return local_210[0];
}


```

## FUN_1401d89c0 @ 1401d89c0

### Immediates (>=0x10000)

- 0x7c024208
- 0xcccccccd
- 0x140223a00
- 0x14023a980
- 0x140246148

### Disassembly

```asm
1401d89c0: MOV qword ptr [RSP + 0x8],RBX
1401d89c5: MOV qword ptr [RSP + 0x18],RBP
1401d89ca: MOV qword ptr [RSP + 0x20],RSI
1401d89cf: PUSH RDI
1401d89d0: PUSH R14
1401d89d2: PUSH R15
1401d89d4: SUB RSP,0x30
1401d89d8: XOR ESI,ESI
1401d89da: MOV R15D,R9D
1401d89dd: MOV EBP,R8D
1401d89e0: MOV R14,RCX
1401d89e3: TEST DL,DL
1401d89e5: JNZ 0x1401d8aff
1401d89eb: LEA R8,[RSP + 0x58]
1401d89f0: MOV EDX,0x7c024208
1401d89f5: MOV RCX,R14
1401d89f8: CALL 0x1400099ac
1401d89fd: MOV ECX,dword ptr [RSP + 0x58]
1401d8a01: TEST CL,0x2
1401d8a04: JNZ 0x1401d8a0f
1401d8a06: TEST CL,0x8
1401d8a09: JZ 0x1401d8ab4
1401d8a0f: MOV EAX,0xcccccccd
1401d8a14: MOV EDI,R15D
1401d8a17: MUL R15D
1401d8a1a: SHR EDX,0x3
1401d8a1d: LEA EAX,[RDX + RDX*0x4]
1401d8a20: ADD EAX,EAX
1401d8a22: SUB EDI,EAX
1401d8a24: TEST EDX,EDX
1401d8a26: JZ 0x1401d8a42
1401d8a28: LFENCE
1401d8a2b: MOV EBX,EDX
1401d8a2d: MOV ECX,0xa
1401d8a32: CALL qword ptr [0x14022a008]
1401d8a38: SUB RBX,0x1
1401d8a3c: JNZ 0x1401d8a2d
1401d8a3e: MOV ECX,dword ptr [RSP + 0x58]
1401d8a42: TEST EDI,EDI
1401d8a44: JZ 0x1401d8a52
1401d8a46: MOV ECX,EDI
1401d8a48: CALL qword ptr [0x14022a008]
1401d8a4e: MOV ECX,dword ptr [RSP + 0x58]
1401d8a52: MOV EAX,ESI
1401d8a54: INC ESI
1401d8a56: CMP EAX,EBP
1401d8a58: JL 0x1401d89eb
1401d8a5a: MOV R10,qword ptr [0x140246148]
1401d8a61: LEA RAX,[0x140246148]
1401d8a68: CMP R10,RAX
1401d8a6b: JZ 0x1401d8b94
1401d8a71: TEST byte ptr [R10 + 0x2c],0x1
1401d8a76: JZ 0x1401d8b94
1401d8a7c: CMP byte ptr [R10 + 0x29],0x3
1401d8a81: JC 0x1401d8b94
1401d8a87: MOV dword ptr [RSP + 0x28],ECX
1401d8a8b: LEA R9,[0x140223a00]
1401d8a92: MOV RCX,qword ptr [R10 + 0x18]
1401d8a96: LEA R8,[0x14023a980]
1401d8a9d: MOV EDX,0x15
1401d8aa2: MOV dword ptr [RSP + 0x20],0x123
1401d8aaa: CALL 0x140007b24
1401d8aaf: JMP 0x1401d8b94
1401d8ab4: MOV R10,qword ptr [0x140246148]
1401d8abb: LEA RAX,[0x140246148]
1401d8ac2: CMP R10,RAX
1401d8ac5: JZ 0x1401d8b46
1401d8ac7: TEST byte ptr [R10 + 0x2c],0x1
1401d8acc: JZ 0x1401d8b46
1401d8ace: CMP byte ptr [R10 + 0x29],0x3
1401d8ad3: JC 0x1401d8b46
1401d8ad5: MOV dword ptr [RSP + 0x28],ECX
1401d8ad9: LEA R9,[0x140223a00]
1401d8ae0: MOV RCX,qword ptr [R10 + 0x18]
1401d8ae4: LEA R8,[0x14023a980]
1401d8aeb: MOV EDX,0x14
1401d8af0: MOV dword ptr [RSP + 0x20],0x11b
1401d8af8: CALL 0x140007b24
1401d8afd: JMP 0x1401d8b46
1401d8aff: CMP DL,0x1
1401d8b02: JNZ 0x1401d8b4a
1401d8b04: MOV RCX,qword ptr [0x140246148]
1401d8b0b: LEA RAX,[0x140246148]
1401d8b12: CMP RCX,RAX
1401d8b15: JZ 0x1401d8b46
1401d8b17: TEST byte ptr [RCX + 0x2c],DL
1401d8b1a: JZ 0x1401d8b46
1401d8b1c: CMP byte ptr [RCX + 0x29],0x2
1401d8b20: JC 0x1401d8b46
1401d8b22: MOV RCX,qword ptr [RCX + 0x18]
1401d8b26: LEA R9,[0x140223a00]
1401d8b2d: MOV EDX,0x12
1401d8b32: MOV dword ptr [RSP + 0x20],0x10d
1401d8b3a: LEA R8,[0x14023a980]
1401d8b41: CALL 0x140001664
1401d8b46: MOV AL,0x1
1401d8b48: JMP 0x1401d8b96
1401d8b4a: MOV RCX,qword ptr [0x140246148]
1401d8b51: LEA RAX,[0x140246148]
1401d8b58: CMP RCX,RAX
1401d8b5b: JZ 0x1401d8b94
1401d8b5d: TEST byte ptr [RCX + 0x2c],0x1
1401d8b61: JZ 0x1401d8b94
1401d8b63: CMP byte ptr [RCX + 0x29],0x1
1401d8b67: JC 0x1401d8b94
1401d8b69: MOV RCX,qword ptr [RCX + 0x18]
1401d8b6d: LEA R9,[0x140223a00]
1401d8b74: MOVZX EAX,DL
1401d8b77: LEA R8,[0x14023a980]
1401d8b7e: MOV dword ptr [RSP + 0x28],EAX
1401d8b82: MOV EDX,0x13
1401d8b87: MOV dword ptr [RSP + 0x20],0x112
1401d8b8f: CALL 0x140007b24
1401d8b94: XOR AL,AL
1401d8b96: MOV RBX,qword ptr [RSP + 0x50]
1401d8b9b: MOV RBP,qword ptr [RSP + 0x60]
1401d8ba0: MOV RSI,qword ptr [RSP + 0x68]
1401d8ba5: ADD RSP,0x30
1401d8ba9: POP R15
1401d8bab: POP R14
1401d8bad: POP RDI
1401d8bae: RET
```

### Decompiled C

```c

undefined8 FUN_1401d89c0(undefined8 param_1,char param_2,int param_3,uint param_4)

{
  bool bVar1;
  ulonglong uVar2;
  int iVar3;
  int iVar4;
  uint local_res10 [2];
  
  iVar3 = 0;
  if (param_2 == '\0') {
    do {
      FUN_1400099ac(param_1,0x7c024208,local_res10);
      if (((local_res10[0] & 2) == 0) && ((local_res10[0] & 8) == 0)) {
        if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
          return 1;
        }
        if ((PTR_LOOP_140246148[0x2c] & 1) == 0) {
          return 1;
        }
        if ((byte)PTR_LOOP_140246148[0x29] < 3) {
          return 1;
        }
        FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x14,&DAT_14023a980,
                      "AsicConnac3xWfdmaWaitIdle",0x11b,local_res10[0]);
        return 1;
      }
      uVar2 = (ulonglong)param_4 / 10;
      iVar4 = param_4 + (int)uVar2 * -10;
      if ((int)uVar2 != 0) {
        do {
          KeStallExecutionProcessor(10);
          uVar2 = uVar2 - 1;
        } while (uVar2 != 0);
      }
      if (iVar4 != 0) {
        KeStallExecutionProcessor(iVar4);
      }
      bVar1 = iVar3 < param_3;
      iVar3 = iVar3 + 1;
    } while (bVar1);
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x15,&DAT_14023a980,
                    "AsicConnac3xWfdmaWaitIdle",0x123,local_res10[0]);
    }
  }
  else {
    if (param_2 == '\x01') {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x29])) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x12,&DAT_14023a980,
                      "AsicConnac3xWfdmaWaitIdle",0x10d);
      }
      return 1;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (PTR_LOOP_140246148[0x29] != '\0')) {
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x13,&DAT_14023a980,
                    "AsicConnac3xWfdmaWaitIdle",0x112,param_2);
    }
  }
  return 0;
}


```

## FUN_1401d8bb0 @ 1401d8bb0

### Immediates (>=0x10000)

- 0x1464df7
- 0x1464dff
- 0x1464e0f
- 0x1402239e0
- 0x14023a980
- 0x140246148

### Disassembly

```asm
1401d8bb0: MOV RAX,RSP
1401d8bb3: MOV qword ptr [RAX + 0x8],RBX
1401d8bb7: MOV qword ptr [RAX + 0x10],RBP
1401d8bbb: MOV qword ptr [RAX + 0x18],RDI
1401d8bbf: MOV qword ptr [RAX + 0x20],R14
1401d8bc3: PUSH R15
1401d8bc5: SUB RSP,0x30
1401d8bc9: MOV RBX,qword ptr [RCX]
1401d8bcc: LEA R14,[0x1402239e0]
1401d8bd3: MOV RDI,RCX
1401d8bd6: LEA R15,[0x14023a980]
1401d8bdd: LEA RBP,[0x140246148]
1401d8be4: MOV RAX,qword ptr [RBX + 0x1464dff]
1401d8beb: TEST RAX,RAX
1401d8bee: JZ 0x1401d8bfd
1401d8bf0: XOR EDX,EDX
1401d8bf2: MOV RCX,RBX
1401d8bf5: CALL qword ptr [0x14022a3f8]
1401d8bfb: JMP 0x1401d8c3a
1401d8bfd: MOV RCX,qword ptr [0x140246148]
1401d8c04: CMP RCX,RBP
1401d8c07: JZ 0x1401d8c3a
1401d8c09: TEST byte ptr [RCX + 0x2fc],0x1
1401d8c10: JZ 0x1401d8c3a
1401d8c12: CMP byte ptr [RCX + 0x2f9],0x1
1401d8c19: JC 0x1401d8c3a
1401d8c1b: MOV RCX,qword ptr [RCX + 0x2e8]
1401d8c22: MOV EDX,0xd
1401d8c27: MOV R9,R14
1401d8c2a: MOV dword ptr [RSP + 0x20],0x73
1401d8c32: MOV R8,R15
1401d8c35: CALL 0x140001664
1401d8c3a: MOV RAX,qword ptr [RDI]
1401d8c3d: MOV RAX,qword ptr [RAX + 0x1464df7]
1401d8c44: TEST RAX,RAX
1401d8c47: JZ 0x1401d8c52
1401d8c49: MOV RCX,RDI
1401d8c4c: CALL qword ptr [0x14022a3f8]
1401d8c52: MOV RCX,qword ptr [0x140246148]
1401d8c59: MOV DIL,0x3
1401d8c5c: CMP RCX,RBP
1401d8c5f: JZ 0x1401d8c89
1401d8c61: TEST byte ptr [RCX + 0x2c],0x1
1401d8c65: JZ 0x1401d8c89
1401d8c67: CMP byte ptr [RCX + 0x29],DIL
1401d8c6b: JC 0x1401d8c89
1401d8c6d: MOV RCX,qword ptr [RCX + 0x18]
1401d8c71: MOV EDX,0xe
1401d8c76: MOV R9,R14
1401d8c79: MOV dword ptr [RSP + 0x20],0x77
1401d8c81: MOV R8,R15
1401d8c84: CALL 0x140001664
1401d8c89: MOV RAX,qword ptr [RBX + 0x1464e0f]
1401d8c90: TEST RAX,RAX
1401d8c93: JZ 0x1401d8c9e
1401d8c95: MOV RCX,RBX
1401d8c98: CALL qword ptr [0x14022a3f8]
1401d8c9e: MOV RCX,qword ptr [0x140246148]
1401d8ca5: CMP RCX,RBP
1401d8ca8: JZ 0x1401d8cd2
1401d8caa: TEST byte ptr [RCX + 0x2c],0x1
1401d8cae: JZ 0x1401d8cd2
1401d8cb0: CMP byte ptr [RCX + 0x29],DIL
1401d8cb4: JC 0x1401d8cd2
1401d8cb6: MOV RCX,qword ptr [RCX + 0x18]
1401d8cba: MOV EDX,0xf
1401d8cbf: MOV R9,R14
1401d8cc2: MOV dword ptr [RSP + 0x20],0x7a
1401d8cca: MOV R8,R15
1401d8ccd: CALL 0x140001664
1401d8cd2: MOV RAX,qword ptr [RBX + 0x1464dff]
1401d8cd9: TEST RAX,RAX
1401d8cdc: JZ 0x1401d8ceb
1401d8cde: MOV DL,0x1
1401d8ce0: MOV RCX,RBX
1401d8ce3: CALL qword ptr [0x14022a3f8]
1401d8ce9: JMP 0x1401d8d28
1401d8ceb: MOV RCX,qword ptr [0x140246148]
1401d8cf2: CMP RCX,RBP
1401d8cf5: JZ 0x1401d8d5c
1401d8cf7: TEST byte ptr [RCX + 0x2fc],0x1
1401d8cfe: JZ 0x1401d8d28
1401d8d00: CMP byte ptr [RCX + 0x2f9],0x1
1401d8d07: JC 0x1401d8d28
1401d8d09: MOV RCX,qword ptr [RCX + 0x2e8]
1401d8d10: MOV EDX,0x10
1401d8d15: MOV R9,R14
1401d8d18: MOV dword ptr [RSP + 0x20],0x7e
1401d8d20: MOV R8,R15
1401d8d23: CALL 0x140001664
1401d8d28: MOV RCX,qword ptr [0x140246148]
1401d8d2f: CMP RCX,RBP
1401d8d32: JZ 0x1401d8d5c
1401d8d34: TEST byte ptr [RCX + 0x2c],0x1
1401d8d38: JZ 0x1401d8d5c
1401d8d3a: CMP byte ptr [RCX + 0x29],DIL
1401d8d3e: JC 0x1401d8d5c
1401d8d40: MOV RCX,qword ptr [RCX + 0x18]
1401d8d44: MOV EDX,0x11
1401d8d49: MOV R9,R14
1401d8d4c: MOV dword ptr [RSP + 0x20],0x80
1401d8d54: MOV R8,R15
1401d8d57: CALL 0x140001664
1401d8d5c: MOV RBX,qword ptr [RSP + 0x40]
1401d8d61: MOV RBP,qword ptr [RSP + 0x48]
1401d8d66: MOV RDI,qword ptr [RSP + 0x50]
1401d8d6b: MOV R14,qword ptr [RSP + 0x58]
1401d8d70: ADD RSP,0x30
1401d8d74: POP R15
1401d8d76: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

void FUN_1401d8bb0(longlong *param_1)

{
  longlong lVar1;
  
  lVar1 = *param_1;
  if (*(code **)(lVar1 + 0x1464dff) == (code *)0x0) {
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (PTR_LOOP_140246148[0x2f9] != '\0')) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0xd,&DAT_14023a980,
                    "AsicConnac3xWpdmaInitRing",0x73);
    }
  }
  else {
    (**(code **)(lVar1 + 0x1464dff))(lVar1,0);
  }
  if (*(code **)(*param_1 + 0x1464df7) != (code *)0x0) {
    (**(code **)(*param_1 + 0x1464df7))(param_1);
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xe,&DAT_14023a980,
                  "AsicConnac3xWpdmaInitRing",0x77);
  }
  if (*(code **)(lVar1 + 0x1464e0f) != (code *)0x0) {
    (**(code **)(lVar1 + 0x1464e0f))(lVar1);
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0xf,&DAT_14023a980,
                  "AsicConnac3xWpdmaInitRing",0x7a);
  }
  if (*(code **)(lVar1 + 0x1464dff) == (code *)0x0) {
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return;
    }
    if (((PTR_LOOP_140246148[0x2fc] & 1) != 0) && (PTR_LOOP_140246148[0x2f9] != '\0')) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0x10,&DAT_14023a980,
                    "AsicConnac3xWpdmaInitRing",0x7e);
    }
  }
  else {
    (**(code **)(lVar1 + 0x1464dff))(lVar1,1);
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x11,&DAT_14023a980,
                  "AsicConnac3xWpdmaInitRing",0x80);
  }
  return;
}


```

