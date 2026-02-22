# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_1401cb88c @ 1401cb88c

### Immediates (>=0x10000)

- 0x140224840
- 0x14023a980
- 0x140246148

### Disassembly

```asm
1401cb88c: PUSH RBX
1401cb88e: PUSH RBP
1401cb88f: PUSH RSI
1401cb890: PUSH RDI
1401cb891: PUSH R12
1401cb893: PUSH R13
1401cb895: PUSH R14
1401cb897: SUB RSP,0x80
1401cb89e: MOV RAX,qword ptr [0x14024f600]
1401cb8a5: XOR RAX,RSP
1401cb8a8: MOV qword ptr [RSP + 0x70],RAX
1401cb8ad: MOV ESI,R9D
1401cb8b0: MOV EDI,R8D
1401cb8b3: MOV EBX,EDX
1401cb8b5: MOV RBP,RCX
1401cb8b8: MOV RCX,qword ptr [0x140246148]
1401cb8bf: LEA R13,[0x140246148]
1401cb8c6: MOV R14D,0x1
1401cb8cc: CMP RCX,R13
1401cb8cf: JZ 0x1401cb8fa
1401cb8d1: TEST byte ptr [RCX + 0xa4],R14B
1401cb8d8: JZ 0x1401cb8fa
1401cb8da: CMP byte ptr [RCX + 0xa1],0x5
1401cb8e1: JC 0x1401cb8fa
1401cb8e3: MOV RCX,qword ptr [RCX + 0x90]
1401cb8ea: LEA EDX,[R14 + 0x6a]
1401cb8ee: LEA R8,[0x14023a980]
1401cb8f5: CALL 0x1400015d8
1401cb8fa: MOV R12D,0xc
1401cb900: LEA RCX,[RSP + 0x60]
1401cb905: MOV EDX,R12D
1401cb908: CALL 0x14001022c
1401cb90d: MOV dword ptr [RSP + 0x60],EBX
1401cb911: MOV dword ptr [RSP + 0x64],EDI
1401cb915: MOV dword ptr [RSP + 0x68],ESI
1401cb919: CMP dword ptr [RSP + 0xe0],0x2
1401cb921: LEA EBX,[R12 + -0x7]
1401cb926: MOV RCX,qword ptr [0x140246148]
1401cb92d: CMOVNZ EBX,R14D
1401cb931: CMP RCX,R13
1401cb934: JZ 0x1401cb96e
1401cb936: TEST byte ptr [RCX + 0xa4],R14B
1401cb93d: JZ 0x1401cb96e
1401cb93f: CMP byte ptr [RCX + 0xa1],0x3
1401cb946: JC 0x1401cb96e
1401cb948: MOV RCX,qword ptr [RCX + 0x90]
1401cb94f: LEA EDX,[R12 + 0x60]
1401cb954: MOVZX EAX,BL
1401cb957: LEA R9,[0x140224840]
1401cb95e: LEA R8,[0x14023a980]
1401cb965: MOV dword ptr [RSP + 0x20],EAX
1401cb969: CALL 0x140001664
1401cb96e: XOR ECX,ECX
1401cb970: LEA RAX,[RSP + 0x60]
1401cb975: MOV dword ptr [RSP + 0x50],ECX
1401cb979: XOR R9D,R9D
1401cb97c: MOV qword ptr [RSP + 0x48],RCX
1401cb981: MOV R8B,R14B
1401cb984: MOV qword ptr [RSP + 0x40],RCX
1401cb989: MOV DL,BL
1401cb98b: MOV word ptr [RSP + 0x38],R12W
1401cb991: MOV qword ptr [RSP + 0x30],RAX
1401cb996: MOV byte ptr [RSP + 0x28],0xd
1401cb99b: MOV byte ptr [RSP + 0x20],CL
1401cb99f: MOV RCX,qword ptr [RBP + 0x14c0]
1401cb9a6: CALL 0x1400cdc4c
1401cb9ab: MOV EBX,EAX
1401cb9ad: MOV RCX,qword ptr [0x140246148]
1401cb9b4: CMP RCX,R13
1401cb9b7: JZ 0x1401cb9e6
1401cb9b9: TEST byte ptr [RCX + 0xa4],R14B
1401cb9c0: JZ 0x1401cb9e6
1401cb9c2: CMP byte ptr [RCX + 0xa1],0x5
1401cb9c9: JC 0x1401cb9e6
1401cb9cb: MOV RCX,qword ptr [RCX + 0x90]
1401cb9d2: LEA R8,[0x14023a980]
1401cb9d9: MOV EDX,0x6d
1401cb9de: MOV R9D,EAX
1401cb9e1: CALL 0x14000764c
1401cb9e6: MOV EAX,EBX
1401cb9e8: MOV RCX,qword ptr [RSP + 0x70]
1401cb9ed: XOR RCX,RSP
1401cb9f0: CALL 0x14020d560
1401cb9f5: ADD RSP,0x80
1401cb9fc: POP R14
1401cb9fe: POP R13
1401cba00: POP R12
1401cba02: POP RDI
1401cba03: POP RSI
1401cba04: POP RBP
1401cba05: POP RBX
1401cba06: RET
```

### Decompiled C

```c

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

undefined4
FUN_1401cb88c(longlong param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,int param_5)

{
  undefined4 uVar1;
  byte bVar2;
  undefined1 auStack_b8 [32];
  uint local_98;
  undefined1 local_90;
  undefined4 *local_88;
  undefined2 local_80;
  undefined8 local_78;
  undefined8 local_70;
  undefined4 local_68;
  undefined4 local_58;
  undefined4 local_54;
  undefined4 local_50;
  ulonglong local_48;
  
  local_48 = DAT_14024f600 ^ (ulonglong)auStack_b8;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x6b,&DAT_14023a980);
  }
  FUN_14001022c(&local_58,0xc);
  bVar2 = 5;
  if (param_5 != 2) {
    bVar2 = 1;
  }
  local_58 = param_2;
  local_54 = param_3;
  local_50 = param_4;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
    local_98 = (uint)bVar2;
    FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x6c,&DAT_14023a980,
                  "AsicConnac3xAddressLenReqCmd");
  }
  local_88 = &local_58;
  local_68 = 0;
  local_70 = 0;
  local_78 = 0;
  local_80 = 0xc;
  local_90 = 0xd;
  local_98 = local_98 & 0xffffff00;
  uVar1 = FUN_1400cdc4c(*(undefined8 *)(param_1 + 0x14c0),bVar2,1,0);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x6d,&DAT_14023a980,uVar1);
  }
  return uVar1;
}


```

## FUN_1401cde70 @ 1401cde70

### Immediates (>=0x10000)

- 0x14023a980
- 0x140246148

### Disassembly

```asm
1401cde70: MOV qword ptr [RSP + 0x8],RBX
1401cde75: MOV qword ptr [RSP + 0x10],RBP
1401cde7a: MOV qword ptr [RSP + 0x18],RSI
1401cde7f: PUSH RDI
1401cde80: SUB RSP,0x60
1401cde84: MOVZX EBX,R8W
1401cde88: MOV RDI,RDX
1401cde8b: MOV RSI,RCX
1401cde8e: MOV RCX,qword ptr [0x140246148]
1401cde95: LEA RBP,[0x140246148]
1401cde9c: CMP RCX,RBP
1401cde9f: JZ 0x1401cdecb
1401cdea1: TEST byte ptr [RCX + 0xa4],0x1
1401cdea8: JZ 0x1401cdecb
1401cdeaa: CMP byte ptr [RCX + 0xa1],0x5
1401cdeb1: JC 0x1401cdecb
1401cdeb3: MOV RCX,qword ptr [RCX + 0x90]
1401cdeba: LEA R8,[0x14023a980]
1401cdec1: MOV EDX,0x66
1401cdec6: CALL 0x1400015d8
1401cdecb: MOV RCX,qword ptr [RSI + 0x14c0]
1401cded2: XOR EAX,EAX
1401cded4: MOV dword ptr [RSP + 0x50],EAX
1401cded8: XOR R9D,R9D
1401cdedb: MOV qword ptr [RSP + 0x48],RAX
1401cdee0: MOV R8B,0xee
1401cdee3: MOV qword ptr [RSP + 0x40],RAX
1401cdee8: XOR EDX,EDX
1401cdeea: MOV word ptr [RSP + 0x38],BX
1401cdeef: MOV qword ptr [RSP + 0x30],RDI
1401cdef4: MOV byte ptr [RSP + 0x28],0x10
1401cdef9: MOV byte ptr [RSP + 0x20],0x1
1401cdefe: CALL 0x1400cdc4c
1401cdf03: MOV EBX,EAX
1401cdf05: MOV RCX,qword ptr [0x140246148]
1401cdf0c: CMP RCX,RBP
1401cdf0f: JZ 0x1401cdf3e
1401cdf11: TEST byte ptr [RCX + 0xa4],0x1
1401cdf18: JZ 0x1401cdf3e
1401cdf1a: CMP byte ptr [RCX + 0xa1],0x5
1401cdf21: JC 0x1401cdf3e
1401cdf23: MOV RCX,qword ptr [RCX + 0x90]
1401cdf2a: LEA R8,[0x14023a980]
1401cdf31: MOV EDX,0x67
1401cdf36: MOV R9D,EAX
1401cdf39: CALL 0x14000764c
1401cdf3e: LEA R11,[RSP + 0x60]
1401cdf43: MOV EAX,EBX
1401cdf45: MOV RBX,qword ptr [R11 + 0x10]
1401cdf49: MOV RBP,qword ptr [R11 + 0x18]
1401cdf4d: MOV RSI,qword ptr [R11 + 0x20]
1401cdf51: MOV RSP,R11
1401cdf54: POP RDI
1401cdf55: RET
```

### Decompiled C

```c

undefined4 FUN_1401cde70(longlong param_1,undefined8 param_2,undefined2 param_3)

{
  undefined4 uVar1;
  
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x66,&DAT_14023a980);
  }
  uVar1 = FUN_1400cdc4c(*(undefined8 *)(param_1 + 0x14c0),0,0xee,0,1,0x10,param_2,param_3,0,0,0);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x67,&DAT_14023a980,uVar1);
  }
  return uVar1;
}


```

## FUN_1401ce7c0 @ 1401ce7c0

### Immediates (>=0x10000)

- 0x14023a980
- 0x140246148

### Disassembly

```asm
1401ce7c0: MOV qword ptr [RSP + 0x10],RBX
1401ce7c5: MOV qword ptr [RSP + 0x18],RBP
1401ce7ca: MOV qword ptr [RSP + 0x20],RSI
1401ce7cf: PUSH RDI
1401ce7d0: PUSH R14
1401ce7d2: PUSH R15
1401ce7d4: SUB RSP,0x60
1401ce7d8: MOV BL,R9B
1401ce7db: MOV ESI,R8D
1401ce7de: MOV EDI,EDX
1401ce7e0: MOV RBP,RCX
1401ce7e3: MOV RCX,qword ptr [0x140246148]
1401ce7ea: LEA R15,[0x140246148]
1401ce7f1: CMP RCX,R15
1401ce7f4: JZ 0x1401ce820
1401ce7f6: TEST byte ptr [RCX + 0xa4],0x1
1401ce7fd: JZ 0x1401ce820
1401ce7ff: CMP byte ptr [RCX + 0xa1],0x5
1401ce806: JC 0x1401ce820
1401ce808: MOV RCX,qword ptr [RCX + 0x90]
1401ce80f: LEA R8,[0x14023a980]
1401ce816: MOV EDX,0x64
1401ce81b: CALL 0x1400015d8
1401ce820: MOV R14D,0x8
1401ce826: LEA RCX,[RSP + 0x80]
1401ce82e: MOV EDX,R14D
1401ce831: CALL 0x14001022c
1401ce836: XOR ECX,ECX
1401ce838: CMP BL,0x1
1401ce83b: JNZ 0x1401ce849
1401ce83d: MOV EAX,dword ptr [RSP + 0x80]
1401ce844: OR EAX,0x4
1401ce847: JMP 0x1401ce84b
1401ce849: MOV EAX,ECX
1401ce84b: OR EAX,0x2
1401ce84e: MOV dword ptr [RSP + 0x80],EAX
1401ce855: TEST EDI,EDI
1401ce857: JZ 0x1401ce863
1401ce859: OR EAX,0x1
1401ce85c: MOV dword ptr [RSP + 0x80],EAX
1401ce863: MOV dword ptr [RSP + 0x50],ECX
1401ce867: LEA RAX,[RSP + 0x80]
1401ce86f: MOV qword ptr [RSP + 0x48],RCX
1401ce874: MOV R8B,0x2
1401ce877: MOV qword ptr [RSP + 0x40],RCX
1401ce87c: XOR R9D,R9D
1401ce87f: MOV word ptr [RSP + 0x38],R14W
1401ce885: MOV DL,R8B
1401ce888: MOV qword ptr [RSP + 0x30],RAX
1401ce88d: MOV byte ptr [RSP + 0x28],0x17
1401ce892: MOV byte ptr [RSP + 0x20],CL
1401ce896: MOV RCX,qword ptr [RBP + 0x14c0]
1401ce89d: MOV dword ptr [RSP + 0x84],ESI
1401ce8a4: CALL 0x1400cdc4c
1401ce8a9: MOV EBX,EAX
1401ce8ab: MOV RCX,qword ptr [0x140246148]
1401ce8b2: CMP RCX,R15
1401ce8b5: JZ 0x1401ce8e4
1401ce8b7: TEST byte ptr [RCX + 0xa4],0x1
1401ce8be: JZ 0x1401ce8e4
1401ce8c0: CMP byte ptr [RCX + 0xa1],0x5
1401ce8c7: JC 0x1401ce8e4
1401ce8c9: MOV RCX,qword ptr [RCX + 0x90]
1401ce8d0: LEA R8,[0x14023a980]
1401ce8d7: MOV EDX,0x65
1401ce8dc: MOV R9D,EAX
1401ce8df: CALL 0x14000764c
1401ce8e4: LEA R11,[RSP + 0x60]
1401ce8e9: MOV EAX,EBX
1401ce8eb: MOV RBX,qword ptr [R11 + 0x28]
1401ce8ef: MOV RBP,qword ptr [R11 + 0x30]
1401ce8f3: MOV RSI,qword ptr [R11 + 0x38]
1401ce8f7: MOV RSP,R11
1401ce8fa: POP R15
1401ce8fc: POP R14
1401ce8fe: POP RDI
1401ce8ff: RET
```

### Decompiled C

```c

undefined4 FUN_1401ce7c0(longlong param_1,int param_2,undefined4 param_3,char param_4)

{
  uint uVar1;
  undefined4 uVar2;
  uint local_res8;
  undefined4 local_resc;
  
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),100,&DAT_14023a980);
  }
  FUN_14001022c(&local_res8,8);
  uVar1 = 0;
  if (param_4 == '\x01') {
    uVar1 = local_res8 | 4;
  }
  local_res8 = uVar1 | 2;
  if (param_2 != 0) {
    local_res8 = uVar1 | 3;
  }
  local_resc = param_3;
  uVar2 = FUN_1400cdc4c(*(undefined8 *)(param_1 + 0x14c0),2,2,0,0,0x17,&local_res8,8,0,0,0);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x65,&DAT_14023a980,uVar2);
  }
  return uVar2;
}


```

