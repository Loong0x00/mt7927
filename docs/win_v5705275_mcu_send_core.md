# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_1400cdc4c @ 1400cdc4c

### Immediates (>=0x10000)

- 0x146cde9

### Disassembly

```asm
1400cdc4c: TEST RCX,RCX
1400cdc4f: JZ 0x1400cdcb1
1400cdc51: MOV R10,qword ptr [RCX]
1400cdc54: MOV R11D,0x6639
1400cdc5a: MOVZX EAX,word ptr [R10 + 0x1f72]
1400cdc62: CMP AX,R11W
1400cdc66: JZ 0x1400cdc98
1400cdc68: MOV R11D,0x738
1400cdc6e: CMP AX,R11W
1400cdc72: JZ 0x1400cdc98
1400cdc74: MOV R11D,0x7927
1400cdc7a: CMP AX,R11W
1400cdc7e: JZ 0x1400cdc98
1400cdc80: MOV R11D,0x7925
1400cdc86: CMP AX,R11W
1400cdc8a: JZ 0x1400cdc98
1400cdc8c: MOV R11D,0x717
1400cdc92: CMP AX,R11W
1400cdc96: JNZ 0x1400cdcb1
1400cdc98: CMP byte ptr [R10 + 0x146cde9],0x1
1400cdca0: JNZ 0x1400cdcb1
1400cdca2: MOVZX EAX,word ptr [RSP + 0x40]
1400cdca7: MOV word ptr [RSP + 0x40],AX
1400cdcac: JMP 0x14014e644
1400cdcb1: MOVZX EAX,word ptr [RSP + 0x40]
1400cdcb6: MOV word ptr [RSP + 0x40],AX
1400cdcbb: JMP 0x1400cd2a8
```

### Decompiled C

```c

void FUN_1400cdc4c(longlong *param_1)

{
  short sVar1;
  
  if (param_1 != (longlong *)0x0) {
    sVar1 = *(short *)(*param_1 + 0x1f72);
    if (((((sVar1 == 0x6639) || (sVar1 == 0x738)) || (sVar1 == 0x7927)) ||
        ((sVar1 == 0x7925 || (sVar1 == 0x717)))) && (*(char *)(*param_1 + 0x146cde9) == '\x01')) {
      FUN_14014e644();
      return;
    }
  }
  FUN_1400cd2a8();
  return;
}


```

