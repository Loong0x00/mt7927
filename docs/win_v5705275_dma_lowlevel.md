# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_1400d316c @ 1400d316c

### Immediates (>=0x10000)

- 0x14647c8
- 0x14649a8
- 0x1464b98
- 0x1464ba0
- 0x1465296
- 0x146529e
- 0x14652a6
- 0x14652a8
- 0x14652aa
- 0x14652ac
- 0x14652ad
- 0x14652ae
- 0x14652b6
- 0x14652be
- 0x14652c6
- 0x14652ce
- 0x14652d6
- 0x14652de
- 0x14652e2
- 0x14652e4
- 0x14652ee
- 0x14652f0
- 0x1465a16
- 0x1465a17
- 0xcccccccd
- 0x140219b80
- 0x140238608
- 0x140246148

### Disassembly

```asm
1400d316c: MOV qword ptr [RSP + 0x20],RBX
1400d3171: MOV dword ptr [RSP + 0x18],R8D
1400d3176: PUSH RBP
1400d3177: PUSH RSI
1400d3178: PUSH RDI
1400d3179: PUSH R12
1400d317b: PUSH R13
1400d317d: PUSH R14
1400d317f: PUSH R15
1400d3181: SUB RSP,0x50
1400d3185: MOV R14,qword ptr [RDX + 0x68]
1400d3189: XOR ESI,ESI
1400d318b: MOV RBP,RDX
1400d318e: MOV byte ptr [RSP + 0x90],SIL
1400d3196: MOV RBX,RCX
1400d3199: MOV R15D,ESI
1400d319c: MOV DIL,byte ptr [R14 + 0x2b]
1400d31a0: MOV RCX,qword ptr [0x140246148]
1400d31a7: LEA RAX,[0x140246148]
1400d31ae: CMP RCX,RAX
1400d31b1: JZ 0x1400d31fa
1400d31b3: TEST byte ptr [RCX + 0xa4],0x1
1400d31ba: JZ 0x1400d31fa
1400d31bc: CMP byte ptr [RCX + 0xa1],0x5
1400d31c3: JC 0x1400d31fa
1400d31c5: MOVZX R8D,byte ptr [RBX + 0x1465a17]
1400d31cd: LEA EDX,[RSI + 0x30]
1400d31d0: MOVZX EAX,byte ptr [RBX + 0x1465a16]
1400d31d7: LEA R9,[0x140219b80]
1400d31de: MOV RCX,qword ptr [RCX + 0x90]
1400d31e5: MOV dword ptr [RSP + 0x28],EAX
1400d31e9: MOV dword ptr [RSP + 0x20],R8D
1400d31ee: LEA R8,[0x140238608]
1400d31f5: CALL 0x140007b24
1400d31fa: TEST DIL,0x2
1400d31fe: JBE 0x1400d3217
1400d3200: MOV R15,qword ptr [RBP + 0x68]
1400d3204: MOV byte ptr [RSP + 0x90],0x1
1400d320c: MOVZX R12D,word ptr [R15 + 0x22]
1400d3211: MOV AL,byte ptr [R15 + 0x27]
1400d3215: JMP 0x1400d3220
1400d3217: MOVZX R12D,byte ptr [R14 + 0x24]
1400d321c: MOV AL,byte ptr [R14 + 0x27]
1400d3220: LEA RCX,[RBX + 0x1464b98]
1400d3227: MOV byte ptr [RSP + 0x98],AL
1400d322e: CALL qword ptr [0x14022a360]
1400d3234: MOVZX R13D,byte ptr [RBX + 0x1465a16]
1400d323c: MOV byte ptr [RBX + 0x1464ba0],AL
1400d3242: LEA RDI,[R13*0x2]
1400d324a: ADD RDI,R13
1400d324d: SHL RDI,0x5
1400d3251: LEA R8,[RDI + RBX*0x1]
1400d3255: CMP byte ptr [R8 + 0x1465296],0x1
1400d325d: JNZ 0x1400d32e9
1400d3263: MOV EAX,ESI
1400d3265: LEA RCX,[RBX + 0x1465296]
1400d326c: MOV EDI,0x14
1400d3271: CMP byte ptr [RCX],0x1
1400d3274: JNZ 0x1400d3279
1400d3276: BTS ESI,EAX
1400d3279: INC EAX
1400d327b: ADD RCX,0x60
1400d327f: CMP EAX,EDI
1400d3281: JL 0x1400d3271
1400d3283: MOV RCX,qword ptr [0x140246148]
1400d328a: LEA RAX,[0x140246148]
1400d3291: CMP RCX,RAX
1400d3294: JZ 0x1400d353c
1400d329a: TEST byte ptr [RCX + 0xa4],0x1
1400d32a1: JZ 0x1400d353c
1400d32a7: CMP byte ptr [RCX + 0xa1],0x1
1400d32ae: JC 0x1400d353c
1400d32b4: MOV RCX,qword ptr [RCX + 0x90]
1400d32bb: LEA R9,[0x140219b80]
1400d32c2: MOV dword ptr [RSP + 0x30],ESI
1400d32c6: LEA R8,[0x140238608]
1400d32cd: MOV dword ptr [RSP + 0x28],R13D
1400d32d2: MOV EDX,0x31
1400d32d7: MOV dword ptr [RSP + 0x20],0x3e4
1400d32df: CALL 0x140007d48
1400d32e4: JMP 0x1400d353c
1400d32e9: LEA ECX,[R13 + 0x1]
1400d32ed: MOV EAX,0xcccccccd
1400d32f2: MUL ECX
1400d32f4: SHR EDX,0x4
1400d32f7: LEA EAX,[RDX + RDX*0x4]
1400d32fa: MOV EDX,0x60
1400d32ff: SHL EAX,0x2
1400d3302: SUB ECX,EAX
1400d3304: MOV byte ptr [RBX + 0x1465a16],CL
1400d330a: LEA RCX,[R8 + 0x1465296]
1400d3311: CALL 0x14001022c
1400d3316: MOV CL,byte ptr [RSP + 0x90]
1400d331d: MOV byte ptr [RDI + RBX*0x1 + 0x14652ee],CL
1400d3324: TEST CL,CL
1400d3326: MOV byte ptr [RDI + RBX*0x1 + 0x1465296],0x1
1400d332e: MOV RCX,RBX
1400d3331: MOV RAX,qword ptr [RBP + 0x80]
1400d3338: MOV dword ptr [RBP + 0x88],R13D
1400d333f: MOV qword ptr [RDI + RBX*0x1 + 0x146529e],RAX
1400d3347: MOVZX EAX,R12W
1400d334b: MOV EDX,EAX
1400d334d: MOV word ptr [RDI + RBX*0x1 + 0x14652a6],R12W
1400d3356: JZ 0x1400d33a5
1400d3358: MOVZX R9D,word ptr [RBP + 0x8c]
1400d3360: MOV R8,R15
1400d3363: CALL 0x1400ca878
1400d3368: MOV word ptr [RDI + RBX*0x1 + 0x14652a8],AX
1400d3370: MOV word ptr [RDI + RBX*0x1 + 0x14652e4],SI
1400d3378: MOVZX ECX,word ptr [RBP + 0x8c]
1400d337f: MOV word ptr [RDI + RBX*0x1 + 0x14652f0],CX
1400d3387: MOV RCX,qword ptr [RBP + 0x28]
1400d338b: CMP AX,0x1a
1400d338f: JNZ 0x1400d339b
1400d3391: MOV qword ptr [RDI + RBX*0x1 + 0x14652ce],RCX
1400d3399: JMP 0x1400d340b
1400d339b: MOV qword ptr [RDI + RBX*0x1 + 0x14652c6],RCX
1400d33a3: JMP 0x1400d340b
1400d33a5: CALL 0x1400ca344
1400d33aa: MOV R15D,0xed
1400d33b0: MOV word ptr [RDI + RBX*0x1 + 0x14652a8],AX
1400d33b8: MOV word ptr [RDI + RBX*0x1 + 0x14652f0],0xff
1400d33c2: CMP byte ptr [R14 + 0x24],R15B
1400d33c6: JNZ 0x1400d33e4
1400d33c8: MOVZX EAX,byte ptr [R14 + 0x29]
1400d33cd: TEST AL,AL
1400d33cf: JZ 0x1400d33e4
1400d33d1: MOV EDX,EAX
1400d33d3: MOVZX ECX,R12W
1400d33d7: CALL 0x1400ca418
1400d33dc: MOV word ptr [RDI + RBX*0x1 + 0x14652e4],AX
1400d33e4: CMP R12W,R15W
1400d33e8: JNZ 0x1400d33ff
1400d33ea: CMP byte ptr [R14 + 0x29],0x3c
1400d33ef: JNZ 0x1400d33ff
1400d33f1: MOV RAX,qword ptr [RBP + 0x28]
1400d33f5: MOV qword ptr [RDI + RBX*0x1 + 0x14652ce],RAX
1400d33fd: JMP 0x1400d340b
1400d33ff: MOV RAX,qword ptr [RBP + 0x28]
1400d3403: MOV qword ptr [RDI + RBX*0x1 + 0x14652c6],RAX
1400d340b: MOV AL,byte ptr [RBP + 0xa]
1400d340e: MOV byte ptr [RDI + RBX*0x1 + 0x14652aa],AL
1400d3415: MOV AL,byte ptr [RSP + 0x98]
1400d341c: MOV byte ptr [RDI + RBX*0x1 + 0x14652ac],AL
1400d3423: MOV AL,byte ptr [RBP + 0xc]
1400d3426: AND AL,0x1
1400d3428: MOV byte ptr [RDI + RBX*0x1 + 0x14652ad],AL
1400d342f: MOV RAX,qword ptr [RBP + 0x18]
1400d3433: MOV qword ptr [RDI + RBX*0x1 + 0x14652b6],RAX
1400d343b: MOV EAX,dword ptr [RBP + 0x20]
1400d343e: MOV dword ptr [RDI + RBX*0x1 + 0x14652be],EAX
1400d3445: MOV EAX,dword ptr [RSP + 0xa0]
1400d344c: MOV dword ptr [RDI + RBX*0x1 + 0x14652de],EAX
1400d3453: MOV qword ptr [RDI + RBX*0x1 + 0x14652d6],RBP
1400d345b: MOV word ptr [RDI + RBX*0x1 + 0x14652ae],SI
1400d3463: MOV byte ptr [RDI + RBX*0x1 + 0x14652e2],SIL
1400d346b: CALL qword ptr [0x14022a368]
1400d3471: CMP AL,0x2
1400d3473: JA 0x1400d34ac
1400d3475: LEA RSI,[R13*0x2]
1400d347d: ADD RSI,R13
1400d3480: LEA RCX,[RBX + 0x14649a8]
1400d3487: LEA RCX,[RCX + RSI*0x8]
1400d348b: CALL qword ptr [0x14022a288]
1400d3491: CMP byte ptr [RDI + RBX*0x1 + 0x14652ad],0x1
1400d3499: JNZ 0x1400d34ac
1400d349b: LEA RCX,[RBX + 0x14647c8]
1400d34a2: LEA RCX,[RCX + RSI*0x8]
1400d34a6: CALL qword ptr [0x14022a288]
1400d34ac: MOV RCX,qword ptr [0x140246148]
1400d34b3: LEA RAX,[0x140246148]
1400d34ba: CMP RCX,RAX
1400d34bd: JZ 0x1400d3539
1400d34bf: TEST byte ptr [RCX + 0xa4],0x1
1400d34c6: JZ 0x1400d3539
1400d34c8: CMP byte ptr [RCX + 0xa1],0x3
1400d34cf: JC 0x1400d3539
1400d34d1: MOVZX R8D,word ptr [RDI + RBX*0x1 + 0x14652e4]
1400d34da: MOV EDX,0x32
1400d34df: MOVZX R9D,word ptr [RDI + RBX*0x1 + 0x14652a8]
1400d34e8: MOVZX EAX,word ptr [RDI + RBX*0x1 + 0x14652f0]
1400d34f0: MOVZX R10D,byte ptr [RSP + 0x90]
1400d34f9: MOVZX R11D,byte ptr [RDI + RBX*0x1 + 0x14652ad]
1400d3502: MOV RCX,qword ptr [RCX + 0x90]
1400d3509: MOV dword ptr [RSP + 0x48],EAX
1400d350d: MOV dword ptr [RSP + 0x40],R8D
1400d3512: LEA R8,[0x140238608]
1400d3519: MOV dword ptr [RSP + 0x38],R9D
1400d351e: LEA R9,[0x140219b80]
1400d3525: MOV dword ptr [RSP + 0x30],R10D
1400d352a: MOV dword ptr [RSP + 0x28],R13D
1400d352f: MOV dword ptr [RSP + 0x20],R11D
1400d3534: CALL 0x14000da28
1400d3539: MOV EDI,R13D
1400d353c: MOV DL,byte ptr [RBX + 0x1464ba0]
1400d3542: LEA RCX,[RBX + 0x1464b98]
1400d3549: CALL qword ptr [0x14022a358]
1400d354f: MOV RBX,qword ptr [RSP + 0xa8]
1400d3557: MOV EAX,EDI
1400d3559: ADD RSP,0x50
1400d355d: POP R15
1400d355f: POP R14
1400d3561: POP R13
1400d3563: POP R12
1400d3565: POP RDI
1400d3566: POP RSI
1400d3567: POP RBP
1400d3568: RET
```

### Decompiled C

```c

ulonglong FUN_1400d316c(longlong param_1,longlong param_2,undefined4 param_3)

{
  longlong lVar1;
  undefined1 uVar2;
  undefined1 uVar3;
  byte bVar4;
  short sVar5;
  undefined2 uVar6;
  uint uVar7;
  char *pcVar8;
  uint uVar9;
  longlong lVar10;
  ulonglong uVar11;
  ushort uVar12;
  uint uVar13;
  longlong lVar14;
  bool local_res8;
  
  lVar1 = *(longlong *)(param_2 + 0x68);
  uVar9 = 0;
  lVar14 = 0;
  bVar4 = *(byte *)(lVar1 + 0x2b);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x30,&DAT_140238608,"MtCmdStoreInfo",
                  *(undefined1 *)(param_1 + 0x1465a17),*(undefined1 *)(param_1 + 0x1465a16));
  }
  local_res8 = (bVar4 & 2) == 0;
  if (local_res8) {
    uVar12 = (ushort)*(byte *)(lVar1 + 0x24);
    uVar2 = *(undefined1 *)(lVar1 + 0x27);
  }
  else {
    lVar14 = *(longlong *)(param_2 + 0x68);
    uVar12 = *(ushort *)(lVar14 + 0x22);
    uVar2 = *(undefined1 *)(lVar14 + 0x27);
  }
  local_res8 = !local_res8;
  uVar3 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x1464b98);
  bVar4 = *(byte *)(param_1 + 0x1465a16);
  uVar11 = (ulonglong)bVar4;
  *(undefined1 *)(param_1 + 0x1464ba0) = uVar3;
  lVar10 = uVar11 * 0x60;
  uVar13 = (uint)bVar4;
  if (*(char *)(lVar10 + param_1 + 0x1465296) == '\x01') {
    uVar7 = 0;
    pcVar8 = (char *)(param_1 + 0x1465296);
    uVar11 = 0x14;
    do {
      if (*pcVar8 == '\x01') {
        uVar9 = uVar9 | 1 << (uVar7 & 0x1f);
      }
      uVar7 = uVar7 + 1;
      pcVar8 = pcVar8 + 0x60;
    } while ((int)uVar7 < 0x14);
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
      FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x31,&DAT_140238608,"MtCmdStoreInfo",
                    0x3e4,uVar13,uVar9);
    }
  }
  else {
    *(char *)(param_1 + 0x1465a16) =
         (char)(bVar4 + 1) + (char)((ulonglong)(bVar4 + 1) / 0x14) * -0x14;
    FUN_14001022c(lVar10 + param_1 + 0x1465296,0x60);
    *(bool *)(lVar10 + 0x14652ee + param_1) = local_res8;
    *(undefined1 *)(lVar10 + 0x1465296 + param_1) = 1;
    *(uint *)(param_2 + 0x88) = uVar13;
    *(undefined8 *)(lVar10 + 0x146529e + param_1) = *(undefined8 *)(param_2 + 0x80);
    *(ushort *)(lVar10 + 0x14652a6 + param_1) = uVar12;
    if (local_res8) {
      sVar5 = FUN_1400ca878(param_1,uVar12,lVar14,*(undefined2 *)(param_2 + 0x8c));
      *(short *)(lVar10 + 0x14652a8 + param_1) = sVar5;
      *(undefined2 *)(lVar10 + 0x14652e4 + param_1) = 0;
      *(undefined2 *)(lVar10 + 0x14652f0 + param_1) = *(undefined2 *)(param_2 + 0x8c);
      if (sVar5 == 0x1a) {
        *(undefined8 *)(lVar10 + 0x14652ce + param_1) = *(undefined8 *)(param_2 + 0x28);
      }
      else {
        *(undefined8 *)(lVar10 + 0x14652c6 + param_1) = *(undefined8 *)(param_2 + 0x28);
      }
    }
    else {
      uVar6 = FUN_1400ca344(param_1,uVar12);
      *(undefined2 *)(lVar10 + 0x14652a8 + param_1) = uVar6;
      *(undefined2 *)(lVar10 + 0x14652f0 + param_1) = 0xff;
      if ((*(char *)(lVar1 + 0x24) == -0x13) && (*(char *)(lVar1 + 0x29) != '\0')) {
        uVar6 = FUN_1400ca418(uVar12,*(char *)(lVar1 + 0x29));
        *(undefined2 *)(lVar10 + 0x14652e4 + param_1) = uVar6;
      }
      if ((uVar12 == 0xed) && (*(char *)(lVar1 + 0x29) == '<')) {
        *(undefined8 *)(lVar10 + 0x14652ce + param_1) = *(undefined8 *)(param_2 + 0x28);
      }
      else {
        *(undefined8 *)(lVar10 + 0x14652c6 + param_1) = *(undefined8 *)(param_2 + 0x28);
      }
    }
    *(undefined1 *)(lVar10 + 0x14652aa + param_1) = *(undefined1 *)(param_2 + 10);
    *(undefined1 *)(lVar10 + 0x14652ac + param_1) = uVar2;
    *(byte *)(lVar10 + 0x14652ad + param_1) = *(byte *)(param_2 + 0xc) & 1;
    *(undefined8 *)(lVar10 + 0x14652b6 + param_1) = *(undefined8 *)(param_2 + 0x18);
    *(undefined4 *)(lVar10 + 0x14652be + param_1) = *(undefined4 *)(param_2 + 0x20);
    *(undefined4 *)(lVar10 + 0x14652de + param_1) = param_3;
    *(longlong *)(lVar10 + 0x14652d6 + param_1) = param_2;
    *(undefined2 *)(lVar10 + 0x14652ae + param_1) = 0;
    *(undefined1 *)(lVar10 + 0x14652e2 + param_1) = 0;
    bVar4 = KeGetCurrentIrql();
    if ((bVar4 < 3) &&
       (KeClearEvent(param_1 + 0x14649a8 + uVar11 * 0x18),
       *(char *)(lVar10 + 0x14652ad + param_1) == '\x01')) {
      KeClearEvent(param_1 + 0x14647c8 + uVar11 * 0x18);
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
      FUN_14000da28(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x32,&DAT_140238608,"MtCmdStoreInfo",
                    *(undefined1 *)(lVar10 + 0x14652ad + param_1),uVar13,local_res8,
                    *(undefined2 *)(lVar10 + 0x14652a8 + param_1),
                    *(undefined2 *)(lVar10 + 0x14652e4 + param_1),
                    *(undefined2 *)(lVar10 + 0x14652f0 + param_1));
    }
  }
  KeReleaseSpinLock(param_1 + 0x1464b98,*(undefined1 *)(param_1 + 0x1464ba0));
  return uVar11;
}


```

## FUN_1400c9810 @ 1400c9810

### Immediates (>=0x10000)

- 0x21370
- 0x21378
- 0x21380
- 0x21384
- 0x2138c
- 0x348ec0
- 0x1464ed7
- 0x1465ad8
- 0x1465b4b
- 0x1465b4f
- 0x146ad88
- 0x146ad8c
- 0x146ad94
- 0x146ada0
- 0x146ada8
- 0x146adf0
- 0x1887d74
- 0x10000044
- 0xee1e5d00
- 0xffffffff
- 0x140219b10
- 0x140238608
- 0x140246148

### Disassembly

```asm
1400c9810: MOV R11,RSP
1400c9813: MOV qword ptr [R11 + 0x8],RBX
1400c9817: MOV qword ptr [R11 + 0x10],RBP
1400c981b: MOV qword ptr [R11 + 0x18],R8
1400c981f: PUSH RSI
1400c9820: PUSH RDI
1400c9821: PUSH R12
1400c9823: PUSH R13
1400c9825: PUSH R14
1400c9827: SUB RSP,0x30
1400c982b: XOR R14D,R14D
1400c982e: MOV RAX,RDX
1400c9831: MOV RBX,RCX
1400c9834: TEST R8,R8
1400c9837: JNZ 0x1400c9841
1400c9839: MOV qword ptr [R11 + 0x18],-0x11e1a300
1400c9841: LEA RCX,[RSP + 0x70]
1400c9846: XOR R9D,R9D
1400c9849: MOV qword ptr [RSP + 0x20],RCX
1400c984e: XOR R8D,R8D
1400c9851: MOV RCX,RAX
1400c9854: XOR EDX,EDX
1400c9856: CALL qword ptr [0x14022a218]
1400c985c: MOV EDI,0x102
1400c9861: MOV EBP,EAX
1400c9863: CMP EAX,EDI
1400c9865: JNZ 0x1400c9b96
1400c986b: MOV RCX,qword ptr [0x140246148]
1400c9872: LEA R12,[0x140246148]
1400c9879: LEA R13,[0x140219b10]
1400c9880: LEA RSI,[0x140238608]
1400c9887: CMP RCX,R12
1400c988a: JZ 0x1400c98c7
1400c988c: TEST byte ptr [RCX + 0x158],0x1
1400c9893: JZ 0x1400c98c7
1400c9895: CMP byte ptr [RCX + 0x155],0x1
1400c989c: JC 0x1400c98c7
1400c989e: MOV RAX,qword ptr [RSP + 0x70]
1400c98a3: MOV EDX,0xf
1400c98a8: MOV RCX,qword ptr [RCX + 0x144]
1400c98af: MOV R9,R13
1400c98b2: MOV qword ptr [RSP + 0x28],RAX
1400c98b7: MOV R8,RSI
1400c98ba: MOV dword ptr [RSP + 0x20],0x1cb
1400c98c2: CALL 0x140012bdc
1400c98c7: CMP dword ptr [RBX + 0x1465b4b],R14D
1400c98ce: JLE 0x1400c98ef
1400c98d0: MOV RCX,qword ptr [0x140246148]
1400c98d7: CMP RCX,R12
1400c98da: JZ 0x1400c9924
1400c98dc: TEST byte ptr [RCX + 0x2c],0x1
1400c98e0: JZ 0x1400c9924
1400c98e2: CMP byte ptr [RCX + 0x29],0x2
1400c98e6: JC 0x1400c9924
1400c98e8: MOV EDX,0x10
1400c98ed: JMP 0x1400c9915
1400c98ef: CMP dword ptr [RBX + 0x1465ad8],0x2
1400c98f6: JNZ 0x1400c992b
1400c98f8: MOV RCX,qword ptr [0x140246148]
1400c98ff: CMP RCX,R12
1400c9902: JZ 0x1400c9924
1400c9904: TEST byte ptr [RCX + 0x2c],0x1
1400c9908: JZ 0x1400c9924
1400c990a: CMP byte ptr [RCX + 0x29],0x2
1400c990e: JC 0x1400c9924
1400c9910: MOV EDX,0x11
1400c9915: MOV RCX,qword ptr [RCX + 0x18]
1400c9919: MOV R9,R13
1400c991c: MOV R8,RSI
1400c991f: CALL 0x140001600
1400c9924: MOV EAX,EDI
1400c9926: JMP 0x1400c9b98
1400c992b: MOV byte ptr [RBX + 0x1887d74],0x1
1400c9932: CMP byte ptr [RBX + 0x1465b4f],R14B
1400c9939: JZ 0x1400c9943
1400c993b: MOV RCX,RBX
1400c993e: CALL 0x14000d018
1400c9943: MOV RCX,RBX
1400c9946: CALL 0x140200250
1400c994b: CMP byte ptr [RBX + 0x1465b4f],R14B
1400c9952: JZ 0x1400c995e
1400c9954: XOR EDX,EDX
1400c9956: MOV RCX,RBX
1400c9959: CALL 0x14000d150
1400c995e: MOV RAX,qword ptr [RBX + 0x1464ed7]
1400c9965: TEST RAX,RAX
1400c9968: JZ 0x1400c9975
1400c996a: MOV DL,0x1
1400c996c: MOV RCX,RBX
1400c996f: CALL qword ptr [0x14022a3f8]
1400c9975: CMP qword ptr [RBX + 0x146adf0],R14
1400c997c: JNZ 0x1400c9aa6
1400c9982: CMP dword ptr [RBX + 0x146ad88],R14D
1400c9989: JBE 0x1400c9a82
1400c998f: LEA RSI,[RBX + 0x146ada0]
1400c9996: MOV RCX,RSI
1400c9999: CALL qword ptr [0x14022a360]
1400c999f: MOV RDI,qword ptr [RBX + 0x146ad8c]
1400c99a6: MOV byte ptr [RBX + 0x146ada8],AL
1400c99ac: TEST RDI,RDI
1400c99af: JZ 0x1400c99cc
1400c99b1: MOV RCX,qword ptr [RDI + 0x18]
1400c99b5: ADD dword ptr [RBX + 0x146ad88],-0x1
1400c99bc: MOV qword ptr [RBX + 0x146ad8c],RCX
1400c99c3: JNZ 0x1400c99cc
1400c99c5: MOV qword ptr [RBX + 0x146ad94],R14
1400c99cc: MOV DL,AL
1400c99ce: MOV RCX,RSI
1400c99d1: CALL qword ptr [0x14022a358]
1400c99d7: TEST RDI,RDI
1400c99da: JZ 0x1400c99f0
1400c99dc: MOV RDX,RDI
1400c99df: MOV RCX,RBX
1400c99e2: CALL 0x14009b44c
1400c99e7: CMP dword ptr [RBX + 0x146ad88],R14D
1400c99ee: JA 0x1400c9996
1400c99f0: LEA RSI,[0x140238608]
1400c99f7: JMP 0x1400c9a82
1400c99fc: LEA RCX,[RAX + 0x21370]
1400c9a03: CALL qword ptr [0x14022a360]
1400c9a09: MOV RCX,qword ptr [RBX + 0x348ec0]
1400c9a10: MOV byte ptr [RCX + 0x21378],AL
1400c9a16: MOV RCX,qword ptr [RBX + 0x348ec0]
1400c9a1d: MOV RDI,qword ptr [RCX + 0x21384]
1400c9a24: TEST RDI,RDI
1400c9a27: JZ 0x1400c9a58
1400c9a29: MOV RAX,qword ptr [RDI + 0x18]
1400c9a2d: MOV qword ptr [RCX + 0x21384],RAX
1400c9a34: MOV RAX,qword ptr [RBX + 0x348ec0]
1400c9a3b: DEC dword ptr [RAX + 0x21380]
1400c9a41: MOV RAX,qword ptr [RBX + 0x348ec0]
1400c9a48: CMP dword ptr [RAX + 0x21380],R14D
1400c9a4f: JNZ 0x1400c9a58
1400c9a51: MOV qword ptr [RAX + 0x2138c],R14
1400c9a58: MOV RDX,qword ptr [RBX + 0x348ec0]
1400c9a5f: LEA RCX,[RDX + 0x21370]
1400c9a66: MOV DL,byte ptr [RDX + 0x21378]
1400c9a6c: CALL qword ptr [0x14022a358]
1400c9a72: TEST RDI,RDI
1400c9a75: JZ 0x1400c9a96
1400c9a77: MOV RDX,RDI
1400c9a7a: MOV RCX,RBX
1400c9a7d: CALL 0x14009b44c
1400c9a82: MOV RAX,qword ptr [RBX + 0x348ec0]
1400c9a89: CMP dword ptr [RAX + 0x21380],R14D
1400c9a90: JA 0x1400c99fc
1400c9a96: CMP dword ptr [RBX + 0x1465ad8],0x1
1400c9a9d: JNZ 0x1400c9aa6
1400c9a9f: MOV dword ptr [RBX + 0x1465ad8],R14D
1400c9aa6: MOVZX EAX,word ptr [RBX + 0x1f72]
1400c9aad: MOV ECX,0x6639
1400c9ab2: CMP AX,CX
1400c9ab5: JZ 0x1400c9ae3
1400c9ab7: MOV ECX,0x738
1400c9abc: CMP AX,CX
1400c9abf: JZ 0x1400c9ae3
1400c9ac1: MOV ECX,0x7927
1400c9ac6: CMP AX,CX
1400c9ac9: JZ 0x1400c9ae3
1400c9acb: MOV ECX,0x7925
1400c9ad0: CMP AX,CX
1400c9ad3: JZ 0x1400c9ae3
1400c9ad5: MOV ECX,0x717
1400c9ada: CMP AX,CX
1400c9add: JNZ 0x1400c9b96
1400c9ae3: CMP dword ptr [RBX + 0x1465ad8],R14D
1400c9aea: JLE 0x1400c9b26
1400c9aec: MOV RCX,qword ptr [0x140246148]
1400c9af3: CMP RCX,R12
1400c9af6: JZ 0x1400c9b96
1400c9afc: TEST byte ptr [RCX + 0x2c],0x1
1400c9b00: JZ 0x1400c9b96
1400c9b06: CMP byte ptr [RCX + 0x29],0x2
1400c9b0a: JC 0x1400c9b96
1400c9b10: MOV RCX,qword ptr [RCX + 0x18]
1400c9b14: MOV EDX,0x12
1400c9b19: MOV R9,R13
1400c9b1c: MOV R8,RSI
1400c9b1f: CALL 0x140001600
1400c9b24: JMP 0x1400c9b96
1400c9b26: MOV RDI,qword ptr [RBX + 0x14c0]
1400c9b2d: MOV dword ptr [RBX + 0x1465ad8],0x1
1400c9b37: MOV RCX,qword ptr [0x140246148]
1400c9b3e: CMP RCX,R12
1400c9b41: JZ 0x1400c9b63
1400c9b43: TEST byte ptr [RCX + 0x2c],0x1
1400c9b47: JZ 0x1400c9b63
1400c9b49: CMP byte ptr [RCX + 0x29],0x3
1400c9b4d: JC 0x1400c9b63
1400c9b4f: MOV RCX,qword ptr [RCX + 0x18]
1400c9b53: MOV EDX,0x13
1400c9b58: MOV R9,R13
1400c9b5b: MOV R8,RSI
1400c9b5e: CALL 0x140001600
1400c9b63: MOV EDX,0x2
1400c9b68: MOV dword ptr [RSP + 0x78],0x2
1400c9b70: MOV RCX,RBX
1400c9b73: CALL 0x14007dae8
1400c9b78: LEA R9,[RSP + 0x78]
1400c9b7d: MOV dword ptr [RSP + 0x20],0x4
1400c9b85: MOV R8D,0x10000044
1400c9b8b: MOV RDX,RDI
1400c9b8e: MOV RCX,RBX
1400c9b91: CALL 0x140099714
1400c9b96: MOV EAX,EBP
1400c9b98: MOV RBX,qword ptr [RSP + 0x60]
1400c9b9d: MOV RBP,qword ptr [RSP + 0x68]
1400c9ba2: ADD RSP,0x30
1400c9ba6: POP R14
1400c9ba8: POP R13
1400c9baa: POP R12
1400c9bac: POP RDI
1400c9bad: POP RSI
1400c9bae: RET
```

### Decompiled C

```c

/* WARNING: Function: _guard_dispatch_icall replaced with injection: guard_dispatch_icall */

int FUN_1400c9810(longlong param_1,undefined8 param_2,longlong param_3)

{
  int *piVar1;
  short sVar2;
  longlong lVar3;
  longlong lVar4;
  undefined1 uVar5;
  int iVar6;
  undefined8 uVar7;
  longlong local_res18;
  undefined4 local_res20 [2];
  longlong *plVar8;
  undefined4 uVar9;
  
  local_res18 = param_3;
  if (param_3 == 0) {
    local_res18 = -300000000;
  }
  plVar8 = &local_res18;
  iVar6 = KeWaitForSingleObject(param_2,0,0,0,plVar8);
  if (iVar6 != 0x102) {
    return iVar6;
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (PTR_LOOP_140246148[0x155] != '\0')) {
    plVar8 = (longlong *)CONCAT44((int)((ulonglong)plVar8 >> 0x20),0x1cb);
    FUN_140012bdc(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0xf,&DAT_140238608,"MtCmdEventWait",
                  plVar8,local_res18);
  }
  if (*(int *)(param_1 + 0x1465b4b) < 1) {
    if (*(int *)(param_1 + 0x1465ad8) != 2) {
      *(undefined1 *)(param_1 + 0x1887d74) = 1;
      if (*(char *)(param_1 + 0x1465b4f) != '\0') {
        FUN_14000d018(param_1);
      }
      FUN_140200250(param_1);
      if (*(char *)(param_1 + 0x1465b4f) != '\0') {
        FUN_14000d150(param_1,0);
      }
      if (*(code **)(param_1 + 0x1464ed7) != (code *)0x0) {
        (**(code **)(param_1 + 0x1464ed7))(param_1,1);
      }
      uVar9 = (undefined4)((ulonglong)plVar8 >> 0x20);
      if (*(longlong *)(param_1 + 0x146adf0) == 0) {
        if (*(int *)(param_1 + 0x146ad88) != 0) {
          do {
            uVar5 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x146ada0);
            lVar3 = *(longlong *)(param_1 + 0x146ad8c);
            *(undefined1 *)(param_1 + 0x146ada8) = uVar5;
            if (lVar3 != 0) {
              uVar7 = *(undefined8 *)(lVar3 + 0x18);
              piVar1 = (int *)(param_1 + 0x146ad88);
              *piVar1 = *piVar1 + -1;
              *(undefined8 *)(param_1 + 0x146ad8c) = uVar7;
              if (*piVar1 == 0) {
                *(undefined8 *)(param_1 + 0x146ad94) = 0;
              }
            }
            KeReleaseSpinLock(param_1 + 0x146ada0,uVar5);
          } while ((lVar3 != 0) &&
                  (FUN_14009b44c(param_1,lVar3), *(int *)(param_1 + 0x146ad88) != 0));
        }
        while( true ) {
          uVar9 = (undefined4)((ulonglong)plVar8 >> 0x20);
          if (*(int *)(*(longlong *)(param_1 + 0x348ec0) + 0x21380) == 0) break;
          uVar5 = KeAcquireSpinLockRaiseToDpc(*(longlong *)(param_1 + 0x348ec0) + 0x21370);
          *(undefined1 *)(*(longlong *)(param_1 + 0x348ec0) + 0x21378) = uVar5;
          lVar3 = *(longlong *)(*(longlong *)(param_1 + 0x348ec0) + 0x21384);
          if (lVar3 != 0) {
            *(undefined8 *)(*(longlong *)(param_1 + 0x348ec0) + 0x21384) =
                 *(undefined8 *)(lVar3 + 0x18);
            piVar1 = (int *)(*(longlong *)(param_1 + 0x348ec0) + 0x21380);
            *piVar1 = *piVar1 + -1;
            if (*(int *)(*(longlong *)(param_1 + 0x348ec0) + 0x21380) == 0) {
              *(undefined8 *)(*(longlong *)(param_1 + 0x348ec0) + 0x2138c) = 0;
            }
          }
          lVar4 = *(longlong *)(param_1 + 0x348ec0);
          KeReleaseSpinLock(lVar4 + 0x21370,
                            CONCAT71((int7)((ulonglong)lVar4 >> 8),*(undefined1 *)(lVar4 + 0x21378))
                           );
          uVar9 = (undefined4)((ulonglong)plVar8 >> 0x20);
          if (lVar3 == 0) break;
          FUN_14009b44c(param_1,lVar3);
        }
        if (*(int *)(param_1 + 0x1465ad8) == 1) {
          *(undefined4 *)(param_1 + 0x1465ad8) = 0;
        }
      }
      sVar2 = *(short *)(param_1 + 0x1f72);
      if (((sVar2 != 0x6639) && (sVar2 != 0x738)) &&
         ((sVar2 != 0x7927 && ((sVar2 != 0x7925 && (sVar2 != 0x717)))))) {
        return 0x102;
      }
      if (*(int *)(param_1 + 0x1465ad8) < 1) {
        uVar7 = *(undefined8 *)(param_1 + 0x14c0);
        *(undefined4 *)(param_1 + 0x1465ad8) = 1;
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
          WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x13,&DAT_140238608,"MtCmdEventWait");
        }
        local_res20[0] = 2;
        FUN_14007dae8(param_1,2);
        FUN_140099714(param_1,uVar7,0x10000044,local_res20,CONCAT44(uVar9,4));
        return 0x102;
      }
      if ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) {
        if ((PTR_LOOP_140246148[0x2c] & 1) == 0) {
          return 0x102;
        }
        if (1 < (byte)PTR_LOOP_140246148[0x29]) {
          WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x12,&DAT_140238608,"MtCmdEventWait");
          return 0x102;
        }
        return 0x102;
      }
      return 0x102;
    }
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return 0x102;
    }
    if ((PTR_LOOP_140246148[0x2c] & 1) == 0) {
      return 0x102;
    }
    if ((byte)PTR_LOOP_140246148[0x29] < 2) {
      return 0x102;
    }
    uVar7 = 0x11;
  }
  else {
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
      return 0x102;
    }
    if ((PTR_LOOP_140246148[0x2c] & 1) == 0) {
      return 0x102;
    }
    if ((byte)PTR_LOOP_140246148[0x29] < 2) {
      return 0x102;
    }
    uVar7 = 0x10;
  }
  WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),uVar7,&DAT_140238608,"MtCmdEventWait");
  return 0x102;
}


```

