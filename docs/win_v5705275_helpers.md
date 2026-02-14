# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_1401ce900 @ 1401ce900

### Immediates (>=0x10000)

- 0x7c0600f0
- 0x140224800
- 0x14023a980
- 0x140246148

### Disassembly

```asm
1401ce900: MOV qword ptr [RSP + 0x8],RBX
1401ce905: PUSH RSI
1401ce906: SUB RSP,0x30
1401ce90a: AND dword ptr [RSP + 0x48],0x0
1401ce90f: MOV RBX,RCX
1401ce912: MOV RCX,qword ptr [0x140246148]
1401ce919: LEA RSI,[0x140246148]
1401ce920: CMP RCX,RSI
1401ce923: JZ 0x1401ce94f
1401ce925: TEST byte ptr [RCX + 0xa4],0x1
1401ce92c: JZ 0x1401ce94f
1401ce92e: CMP byte ptr [RCX + 0xa1],0x5
1401ce935: JC 0x1401ce94f
1401ce937: MOV RCX,qword ptr [RCX + 0x90]
1401ce93e: LEA R8,[0x14023a980]
1401ce945: MOV EDX,0x61
1401ce94a: CALL 0x1400015d8
1401ce94f: LEA R8,[RSP + 0x48]
1401ce954: MOV EDX,0x7c0600f0
1401ce959: MOV RCX,RBX
1401ce95c: CALL 0x1400099ac
1401ce961: MOV RCX,qword ptr [0x140246148]
1401ce968: CMP RCX,RSI
1401ce96b: JZ 0x1401ce9e1
1401ce96d: TEST byte ptr [RCX + 0xa4],0x1
1401ce974: JZ 0x1401ce9a6
1401ce976: CMP byte ptr [RCX + 0xa1],0x3
1401ce97d: JC 0x1401ce9a6
1401ce97f: MOV EAX,dword ptr [RSP + 0x48]
1401ce983: LEA R9,[0x140224800]
1401ce98a: MOV RCX,qword ptr [RCX + 0x90]
1401ce991: LEA R8,[0x14023a980]
1401ce998: MOV EDX,0x62
1401ce99d: MOV dword ptr [RSP + 0x20],EAX
1401ce9a1: CALL 0x140001664
1401ce9a6: MOV RCX,qword ptr [0x140246148]
1401ce9ad: CMP RCX,RSI
1401ce9b0: JZ 0x1401ce9e1
1401ce9b2: TEST byte ptr [RCX + 0xa4],0x1
1401ce9b9: JZ 0x1401ce9e1
1401ce9bb: CMP byte ptr [RCX + 0xa1],0x5
1401ce9c2: JC 0x1401ce9e1
1401ce9c4: MOV R9D,dword ptr [RSP + 0x48]
1401ce9c9: LEA R8,[0x14023a980]
1401ce9d0: MOV RCX,qword ptr [RCX + 0x90]
1401ce9d7: MOV EDX,0x63
1401ce9dc: CALL 0x14000764c
1401ce9e1: MOV EAX,dword ptr [RSP + 0x48]
1401ce9e5: MOV RBX,qword ptr [RSP + 0x40]
1401ce9ea: ADD RSP,0x30
1401ce9ee: POP RSI
1401ce9ef: RET
```

### Decompiled C

```c

undefined4 FUN_1401ce900(undefined8 param_1)

{
  undefined4 local_res10 [6];
  
  local_res10[0] = 0;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x61,&DAT_14023a980);
  }
  FUN_1400099ac(param_1,0x7c0600f0,local_res10);
  if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
    if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x62,&DAT_14023a980,
                    "AsicConnac3xGetFwSyncValue",local_res10[0]);
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
      FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),99,&DAT_14023a980,local_res10[0]);
    }
  }
  return local_res10[0];
}


```

## FUN_14000d410 @ 14000d410

### Immediates (>=0x10000)

- 0x140230248
- 0x140246148

### Disassembly

```asm
14000d410: MOV qword ptr [RSP + 0x8],RBX
14000d415: PUSH RDI
14000d416: SUB RSP,0x20
14000d41a: MOV RBX,RCX
14000d41d: MOV RCX,qword ptr [0x140246148]
14000d424: LEA RDI,[0x140246148]
14000d42b: CMP RCX,RDI
14000d42e: JZ 0x14000d45a
14000d430: TEST byte ptr [RCX + 0xa4],0x1
14000d437: JZ 0x14000d45a
14000d439: CMP byte ptr [RCX + 0xa1],0x5
14000d440: JC 0x14000d45a
14000d442: MOV RCX,qword ptr [RCX + 0x90]
14000d449: LEA R8,[0x140230248]
14000d450: MOV EDX,0x45
14000d455: CALL 0x1400015d8
14000d45a: MOV RCX,RBX
14000d45d: CALL 0x140058eb8
14000d462: MOV EBX,EAX
14000d464: MOV RCX,qword ptr [0x140246148]
14000d46b: CMP RCX,RDI
14000d46e: JZ 0x14000d49d
14000d470: TEST byte ptr [RCX + 0xa4],0x1
14000d477: JZ 0x14000d49d
14000d479: CMP byte ptr [RCX + 0xa1],0x5
14000d480: JC 0x14000d49d
14000d482: MOV RCX,qword ptr [RCX + 0x90]
14000d489: LEA R8,[0x140230248]
14000d490: MOV EDX,0x46
14000d495: MOV R9D,EAX
14000d498: CALL 0x14000764c
14000d49d: MOV EAX,EBX
14000d49f: MOV RBX,qword ptr [RSP + 0x30]
14000d4a4: ADD RSP,0x20
14000d4a8: POP RDI
14000d4a9: RET
```

### Decompiled C

```c

undefined4 FUN_14000d410(undefined8 param_1)

{
  undefined4 uVar1;
  
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x45,&DAT_140230248);
  }
  uVar1 = FUN_140058eb8(param_1);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x46,&DAT_140230248,uVar1);
  }
  return uVar1;
}


```

## FUN_1401d8724 @ 1401d8724

### Immediates (>=0x10000)

- 0x208000
- 0x50001070
- 0x7c024208
- 0x7c02420c
- 0x7c024280
- 0xe7df7ffa
- 0xffffffff

### Disassembly

```asm
1401d8724: MOV qword ptr [RSP + 0x8],RBX
1401d8729: MOV qword ptr [RSP + 0x10],RSI
1401d872e: PUSH RDI
1401d872f: SUB RSP,0x20
1401d8733: MOV DIL,R8B
1401d8736: MOV SIL,DL
1401d8739: LEA R8,[RSP + 0x40]
1401d873e: MOV EDX,0x7c024208
1401d8743: MOV RBX,RCX
1401d8746: CALL 0x1400099ac
1401d874b: CMP DIL,0x1
1401d874f: JNZ 0x1401d8772
1401d8751: MOV EAX,dword ptr [RSP + 0x40]
1401d8755: OR EAX,0x50001070
1401d875a: MOV R8D,EAX
1401d875d: BTS R8D,0x1b
1401d8762: CMP SIL,DIL
1401d8765: CMOVNZ R8D,EAX
1401d8769: OR R8D,0x208000
1401d8770: JMP 0x1401d877e
1401d8772: MOV R8D,dword ptr [RSP + 0x40]
1401d8777: AND R8D,0xe7df7ffa
1401d877e: MOV EDX,0x7c024208
1401d8783: MOV dword ptr [RSP + 0x40],R8D
1401d8788: MOV RCX,RBX
1401d878b: CALL 0x140009a18
1401d8790: TEST DIL,DIL
1401d8793: JNZ 0x1401d87cf
1401d8795: MOV R9D,0x3e8
1401d879b: MOV R8D,0x64
1401d87a1: MOV DL,SIL
1401d87a4: MOV RCX,RBX
1401d87a7: CALL 0x1401d89c0
1401d87ac: OR EDI,0xffffffff
1401d87af: MOV EDX,0x7c02420c
1401d87b4: MOV R8D,EDI
1401d87b7: MOV RCX,RBX
1401d87ba: CALL 0x140009a18
1401d87bf: MOV R8D,EDI
1401d87c2: MOV EDX,0x7c024280
1401d87c7: MOV RCX,RBX
1401d87ca: CALL 0x140009a18
1401d87cf: MOV RBX,qword ptr [RSP + 0x30]
1401d87d4: MOV RSI,qword ptr [RSP + 0x38]
1401d87d9: ADD RSP,0x20
1401d87dd: POP RDI
1401d87de: RET
```

### Decompiled C

```c

void FUN_1401d8724(undefined8 param_1,char param_2,char param_3)

{
  uint uVar1;
  uint local_res18 [4];
  
  FUN_1400099ac(param_1,0x7c024208,local_res18);
  if (param_3 == '\x01') {
    uVar1 = local_res18[0] | 0x58001070;
    if (param_2 != '\x01') {
      uVar1 = local_res18[0] | 0x50001070;
    }
    local_res18[0] = uVar1 | 0x208000;
  }
  else {
    local_res18[0] = local_res18[0] & 0xe7df7ffa;
  }
  FUN_140009a18(param_1,0x7c024208);
  if (param_3 == '\0') {
    FUN_1401d89c0(param_1,param_2,100,1000);
    FUN_140009a18(param_1,0x7c02420c,0xffffffff);
    FUN_140009a18(param_1,0x7c024280,0xffffffff);
  }
  return;
}


```

## FUN_140009a18 @ 140009a18

### Immediates (>=0x10000)

- 0x1464fe7
- 0x1465e0a

### Disassembly

```asm
140009a18: MOV qword ptr [RSP + 0x8],RBX
140009a1d: MOV qword ptr [RSP + 0x10],RSI
140009a22: PUSH RDI
140009a23: SUB RSP,0x20
140009a27: CMP byte ptr [RCX + 0x1465e0a],0x0
140009a2e: MOV ESI,R8D
140009a31: MOV EDI,EDX
140009a33: MOV RBX,RCX
140009a36: JZ 0x140009a4c
140009a38: MOV RAX,qword ptr [RCX + 0x1464fe7]
140009a3f: TEST RAX,RAX
140009a42: JZ 0x140009a4c
140009a44: MOV ECX,EDX
140009a46: CALL qword ptr [0x14022a3f8]
140009a4c: MOV RCX,qword ptr [RBX + 0x1f80]
140009a53: MOV R8D,ESI
140009a56: MOV EDX,EDI
140009a58: MOV RBX,qword ptr [RSP + 0x30]
140009a5d: MOV RSI,qword ptr [RSP + 0x38]
140009a62: ADD RSP,0x20
140009a66: POP RDI
140009a67: JMP 0x140057d48
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

void FUN_140009a18(longlong param_1,undefined4 param_2,undefined4 param_3)

{
  if ((*(char *)(param_1 + 0x1465e0a) != '\0') && (*(code **)(param_1 + 0x1464fe7) != (code *)0x0))
  {
    (**(code **)(param_1 + 0x1464fe7))(param_2);
  }
  FUN_140057d48(*(undefined8 *)(param_1 + 0x1f80),param_2,param_3);
  return;
}


```

## FUN_1400099ac @ 1400099ac

### Immediates (>=0x10000)

- 0x1464fe7
- 0x1465e0a
- 0xc00002b6

### Disassembly

```asm
1400099ac: MOV qword ptr [RSP + 0x8],RBX
1400099b1: MOV qword ptr [RSP + 0x10],RSI
1400099b6: PUSH RDI
1400099b7: SUB RSP,0x20
1400099bb: CMP byte ptr [RCX + 0x1465e0a],0x0
1400099c2: MOV RSI,R8
1400099c5: MOV EBX,EDX
1400099c7: MOV RDI,RCX
1400099ca: JZ 0x1400099e0
1400099cc: MOV RAX,qword ptr [RCX + 0x1464fe7]
1400099d3: TEST RAX,RAX
1400099d6: JZ 0x1400099e0
1400099d8: MOV ECX,EDX
1400099da: CALL qword ptr [0x14022a3f8]
1400099e0: AND dword ptr [RSI],0x0
1400099e3: MOV R8,RSI
1400099e6: MOV RCX,qword ptr [RDI + 0x1f80]
1400099ed: MOV EDX,EBX
1400099ef: CALL 0x140054ee4
1400099f4: CMP EAX,0xc00002b6
1400099f9: MOV RCX,RDI
1400099fc: MOV EBX,EAX
1400099fe: SETZ DL
140009a01: CALL 0x140009a6c
140009a06: MOV RSI,qword ptr [RSP + 0x38]
140009a0b: MOV EAX,EBX
140009a0d: MOV RBX,qword ptr [RSP + 0x30]
140009a12: ADD RSP,0x20
140009a16: POP RDI
140009a17: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

int FUN_1400099ac(longlong param_1,undefined4 param_2,undefined4 *param_3)

{
  int iVar1;
  
  if ((*(char *)(param_1 + 0x1465e0a) != '\0') && (*(code **)(param_1 + 0x1464fe7) != (code *)0x0))
  {
    (**(code **)(param_1 + 0x1464fe7))(param_2);
  }
  *param_3 = 0;
  iVar1 = FUN_140054ee4(*(undefined8 *)(param_1 + 0x1f80),param_2,param_3);
  FUN_140009a6c(param_1,iVar1 == -0x3ffffd4a);
  return iVar1;
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

