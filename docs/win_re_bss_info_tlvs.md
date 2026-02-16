# Windows mtkwecx.sys BSS_INFO TLV 完整结构
**分析日期**: 2026-02-17
**驱动文件**: mtkwecx.sys
**Ghidra 项目**: /home/user/mt7927/tmp/ghidra_project/mt7927_re

## 概述
通过 Ghidra headless 分析找到 21 个 BSS_INFO 相关函数

---

## 1. nicUniCmdSetBssInfo
- **实际函数名**: `FUN_1401444a0`
- **地址**: `1401444a0`

### 反编译代码
```c

undefined8 FUN_1401444a0(undefined8 param_1,char *param_2)

{
  undefined1 *puVar1;
  undefined8 *puVar2;
  uint uVar3;
  longlong *plVar4;
  undefined8 uVar5;
  int *piVar6;
  uint uVar7;
  undefined1 *puVar8;
  int iVar9;
  
  if ((*param_2 == '\x12') && (*(int *)(param_2 + 0x10) == 0x74)) {
    puVar1 = *(undefined1 **)(param_2 + 0x18);
    uVar7 = 0;
    uVar3 = 0;
    piVar6 = &DAT_1402505b0;
    iVar9 = 4;
    do {
      iVar9 = iVar9 + *piVar6;
      uVar3 = uVar3 + 1;
      piVar6 = piVar6 + 4;
    } while (uVar3 < 0xe);
    plVar4 = (longlong *)FUN_14014f788(param_1,CONCAT71((int7)((ulonglong)piVar6 >> 8),2),iVar9);
    if (plVar4 == (longlong *)0x0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x72,&DAT_1402387a0,
                      "nicUniCmdSetBssInfo",0xeb5);
      }
      uVar5 = 0xc000009a;
    }
    else {
      puVar8 = (undefined1 *)plVar4[3];
      *puVar8 = *puVar1;
      puVar8 = puVar8 + 4;
      do {
        uVar3 = (*(code *)PTR__guard_dispatch_icall_14022a3f8)(param_1,puVar8,puVar1);
        uVar7 = uVar7 + 1;
        puVar8 = puVar8 + uVar3;
      } while (uVar7 < 0xe);
      *(int *)((longlong)plVar4 + 0x14) = (int)puVar8 - (int)plVar4[3];
      puVar2 = *(undefined8 **)(param_2 + 0x38);
      *(longlong **)(param_2 + 0x38) = plVar4;
      *plVar4 = (longlong)(param_2 + 0x30);
      plVar4[1] = (longlong)puVar2;
      *puVar2 = plVar4;
      *(int *)(param_2 + 0x40) = *(int *)(param_2 + 0x40) + 1;
      uVar5 = 0;
    }
  }
  else {
    uVar5 = 0x10003;
  }
  return uVar5;
}

 (GhidraScript)  
```

## 2. nicUniCmdBssInfoTagBasic
- **实际函数名**: `FUN_14014c610`
- **地址**: `14014c610`

### 反编译代码
```c

undefined2 FUN_14014c610(longlong *param_1,undefined4 *param_2,undefined1 *param_3)

{
  longlong lVar1;
  undefined1 uVar2;
  char cVar3;
  undefined2 uVar4;
  int iVar5;
  uint uVar6;
  longlong lVar7;
  longlong lVar8;
  ulonglong uVar9;
  
  lVar1 = *param_1;
  lVar7 = 0;
  if ((int)param_1[2] == 1) {
    lVar7 = FUN_14018b15c(lVar1,*param_3);
    if (lVar7 == 0) {
      if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
        return 0;
      }
      if ((PTR_LOOP_140246148[0x2fc] & 1) == 0) {
        return 0;
      }
      if (PTR_LOOP_140246148[0x2f9] == '\0') {
        return 0;
      }
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0x61,&DAT_1402387a0,
                    "nicUniCmdBssInfoTagBasic",0xc74,*param_3);
      return 0;
    }
    lVar8 = *(longlong *)(lVar7 + 0x18);
  }
  else {
    lVar8 = FUN_140103fb8(param_1,param_1 + 0x5c);
  }
  if (lVar8 == 0) {
    uVar4 = FUN_1401540c0(param_1);
    lVar8 = FUN_1401044cc(param_1,uVar4);
  }
  if ((int)param_1[2] == 4) {
    uVar4 = FUN_1401540c0(param_1);
    lVar8 = FUN_1401044cc(param_1,uVar4);
  }
  if (lVar8 == 0) {
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x62,&DAT_1402387a0,
               "nicUniCmdBssInfoTagBasic");
    }
    return 0;
  }
  *param_2 = 0x200000;
  *(undefined1 *)(param_2 + 1) = param_3[0x5e];
  if ((lVar7 == 0) || ((int)param_1[2] != 1)) {
    *(undefined1 *)((longlong)param_2 + 5) = *(undefined1 *)((longlong)param_1 + 0x2d2);
    uVar2 = *(undefined1 *)((longlong)param_1 + 0x2d2);
  }
  else {
    *(undefined1 *)((longlong)param_2 + 5) = *(undefined1 *)(lVar7 + 0x20);
    uVar2 = *(undefined1 *)(lVar7 + 0x20);
  }
  *(undefined1 *)((longlong)param_2 + 6) = uVar2;
  cVar3 = param_3[0x5a];
  if (cVar3 == '\x04') {
    cVar3 = -1;
  }
  else if (cVar3 == '\x03') {
    cVar3 = -2;
  }
  *(char *)((longlong)param_2 + 7) = cVar3;
  iVar5 = FUN_14014fa20(param_1);
  param_2[2] = iVar5;
  *(byte *)(param_2 + 3) = ~(byte)(*(uint *)((longlong)param_1 + 0x2e6964) >> 7) & 1;
  *(undefined1 *)((longlong)param_2 + 0xd) = param_3[0x5b];
  *(undefined4 *)((longlong)param_2 + 0xe) = *(undefined4 *)(param_3 + 0x24);
  uVar4 = *(undefined2 *)(param_3 + 0x28);
  *(undefined2 *)((longlong)param_2 + 0x12) = uVar4;
  *(ushort *)(param_2 + 5) = (ushort)(byte)param_3[0x3a];
  *(short *)((longlong)param_2 + 0x16) = (short)param_1[0xb8612];
  *(char *)(param_2 + 6) = (char)param_1[0xb862d];
  *(ushort *)((longlong)param_2 + 0x1a) = (ushort)*(byte *)(lVar8 + 9);
  *(ushort *)(param_2 + 7) = (ushort)(byte)param_3[0x34];
  uVar6 = FUN_14014fdfc(CONCAT11((char)((ushort)uVar4 >> 8),*(undefined1 *)(lVar8 + 0x4a7)));
  uVar9 = (ulonglong)uVar6;
  if (iVar5 == 0x20002) {
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x29])) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),99,&DAT_1402387a0,
                    "nicUniCmdBssInfoTagBasic",*(undefined1 *)((longlong)param_1 + 0x2e6109));
    }
    *(undefined2 *)((longlong)param_2 + 0x16) = 100;
    *(undefined1 *)(param_2 + 6) = *(undefined1 *)((longlong)param_1 + 0x2e6109);
    if (((*(char *)(lVar1 + 0x1466a99) == '\0') || ((int)param_1[2] != 4)) ||
       ((*(char *)((longlong)param_1 + 0x2e4a71) == '\0' ||
        (*(char *)((longlong)param_1 + 0x2e4a55) != '\x03')))) {
      uVar9 = (ulonglong)(uVar6 & 0xfffffeff);
    }
  }
  *(char *)((longlong)param_2 + 0x19) = (char)uVar9;
  *(char *)((longlong)param_2 + 0x1e) = (char)(uVar9 >> 8);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_14000d92c(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),100,&DAT_1402387a0,
                  "nicUniCmdBssInfoTagBasic",(uint)uVar9 & 0xff,(uint)(uVar9 >> 8) & 0xff,
                  *(undefined1 *)(param_2 + 1),*(undefined1 *)((longlong)param_2 + 5),
                  *(undefined1 *)((longlong)param_2 + 7),*(undefined2 *)((longlong)param_2 + 0x1a),
                  *(undefined1 *)(param_2 + 3));
  }
  return *(undefined2 *)((longlong)param_2 + 2);
}

 (GhidraScript)  
```

## 3. nicUniCmdBssInfoTagHe
- **实际函数名**: `FUN_14014cd50`
- **地址**: `14014cd50`

### 反编译代码
```c

undefined2 FUN_14014cd50(longlong *param_1,undefined4 *param_2,undefined1 *param_3)

{
  byte *pbVar1;
  undefined2 uVar2;
  longlong lVar3;
  int iVar4;
  
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0x2f9])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0x66,&DAT_1402387a0);
  }
  if ((int)param_1[2] == 1) {
    lVar3 = FUN_14018b15c(*param_1,*param_3);
    if (lVar3 != 0) {
      lVar3 = *(longlong *)(lVar3 + 0x18);
      goto LAB_14014ce39;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (PTR_LOOP_140246148[0x2f9] != '\0')) {
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0x67,&DAT_1402387a0,
                    "nicUniCmdBssInfoTagHe",0xd95,*param_3);
    }
LAB_14014ce90:
    uVar2 = 0;
  }
  else {
    lVar3 = FUN_140103fb8(param_1,param_1 + 0x5c);
LAB_14014ce39:
    if (lVar3 == 0) {
      uVar2 = FUN_1401540c0(param_1);
      lVar3 = FUN_1401044cc(param_1,uVar2);
      if (lVar3 == 0) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
          WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x68,&DAT_1402387a0,
                   "nicUniCmdBssInfoTagHe");
        }
        goto LAB_14014ce90;
      }
    }
    *param_2 = 0x100005;
    *(ushort *)(param_2 + 1) =
         (ushort)(*(byte *)(lVar3 + 0x5c0) & 0x3f) << 4 | (ushort)(*(byte *)(lVar3 + 0x5bf) >> 4);
    *(undefined2 *)(param_2 + 2) = *(undefined2 *)(lVar3 + 0x5c3);
    *(undefined2 *)((longlong)param_2 + 10) = *(undefined2 *)(lVar3 + 0x5c3);
    *(undefined2 *)(param_2 + 3) = *(undefined2 *)(lVar3 + 0x5c3);
    *(byte *)((longlong)param_2 + 6) = *(byte *)(lVar3 + 0x5bf) & 7;
    *(byte *)((longlong)param_2 + 7) = *(byte *)(lVar3 + 0x5c1) & 1;
    if (((*(char *)(*param_1 + 0x18cf9dd) != '\0') && (iVar4 = (int)param_1[2], iVar4 == 4)) ||
       (((char)param_1[0x58] == '\x02' && (iVar4 = 1, (int)param_1[2] == 1)))) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
        FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x69,&DAT_1402387a0,
                      "nicUniCmdBssInfoTagHe",*param_3,iVar4);
      }
      pbVar1 = param_3 + 0x3e;
      *pbVar1 = *pbVar1 | 2;
      if (*(char *)(*param_1 + 0x18cf9dc) != '\0') {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
          FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x6a,&DAT_1402387a0,
                        "nicUniCmdBssInfoTagHe",*param_3,(int)param_1[2]);
        }
        *pbVar1 = *pbVar1 | 1;
      }
    }
    *(undefined1 *)((longlong)param_2 + 0xe) = param_3[0x3e];
    uVar2 = *(undefined2 *)((longlong)param_2 + 2);
  }
  return uVar2;
}

 (GhidraScript)  
```

## 4. nicUniCmdBssInfoTagEht
- **实际函数名**: `FUN_14014d150`
- **地址**: `14014d150`

### 反编译代码
```c

undefined2 FUN_14014d150(undefined8 *param_1,undefined4 *param_2,undefined1 *param_3)

{
  undefined1 uVar1;
  undefined1 uVar2;
  undefined1 uVar3;
  undefined2 uVar4;
  longlong lVar5;
  byte bVar6;
  byte bVar7;
  
  lVar5 = FUN_14018b15c(*param_1,*param_3);
  if (lVar5 == 0) {
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (PTR_LOOP_140246148[0x2f9] != '\0')) {
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0x6d,&DAT_1402387a0,
                    "nicUniCmdBssInfoTagEht",0xe0b,*param_3);
    }
  }
  else {
    lVar5 = *(longlong *)(lVar5 + 0x18);
    if (lVar5 != 0) {
      *param_2 = 0x10001e;
      bVar7 = *(byte *)(lVar5 + 0x772) & 1;
      *(byte *)(param_2 + 1) = bVar7;
      uVar1 = *(undefined1 *)(lVar5 + 0x777);
      *(undefined1 *)((longlong)param_2 + 6) = uVar1;
      uVar2 = *(undefined1 *)(lVar5 + 0x778);
      *(undefined1 *)((longlong)param_2 + 7) = uVar2;
      uVar3 = *(undefined1 *)(lVar5 + 0x779);
      *(undefined1 *)(param_2 + 2) = uVar3;
      bVar6 = *(byte *)(lVar5 + 0x772) >> 1 & 1;
      *(byte *)((longlong)param_2 + 5) = bVar6;
      uVar4 = *(undefined2 *)(lVar5 + 0x77a);
      *(undefined2 *)((longlong)param_2 + 10) = uVar4;
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
        FUN_14000d92c(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x6f,&DAT_1402387a0,
                      "nicUniCmdBssInfoTagEht",0x1e,bVar7,uVar1,uVar2,uVar3,bVar6,uVar4);
      }
      return *(undefined2 *)((longlong)param_2 + 2);
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
      WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x6e,&DAT_1402387a0,
               "nicUniCmdBssInfoTagEht");
    }
  }
  return 0;
}

 (GhidraScript)  
```

## 5. nicUniCmdBssInfoTagBssColor
- **实际函数名**: `FUN_14014d010`
- **地址**: `14014d010`

### 反编译代码
```c

undefined8 FUN_14014d010(undefined8 *param_1,undefined4 *param_2,undefined1 *param_3)

{
  undefined2 uVar1;
  longlong lVar2;
  undefined8 uVar3;
  
  if (*(int *)(param_1 + 2) == 1) {
    lVar2 = FUN_14018b15c(*param_1,*param_3);
    if (lVar2 != 0) {
      lVar2 = *(longlong *)(lVar2 + 0x18);
      goto LAB_14014d0b1;
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (PTR_LOOP_140246148[0x2f9] != '\0')) {
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8),0x6b,&DAT_1402387a0,
                    "nicUniCmdBssInfoTagBssColor",0xde1,*param_3);
    }
LAB_14014d112:
    uVar3 = 0;
  }
  else {
    lVar2 = FUN_140103fb8(param_1,param_1 + 0x5c);
LAB_14014d0b1:
    if (lVar2 == 0) {
      uVar1 = FUN_1401540c0(param_1);
      lVar2 = FUN_1401044cc(param_1,uVar1);
      if (lVar2 == 0) {
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0x158] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0x155])) {
          WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x144),0x6c,&DAT_1402387a0,
                   "nicUniCmdBssInfoTagBssColor");
        }
        goto LAB_14014d112;
      }
    }
    *param_2 = 0x80004;
    *(byte *)(param_2 + 1) = ~(*(byte *)(lVar2 + 0x5c2) >> 7) & 1;
    uVar3 = 8;
    *(byte *)((longlong)param_2 + 5) = *(byte *)(lVar2 + 0x5c2) & 0x3f;
  }
  return uVar3;
}

 (GhidraScript)  
```

## 6. nicUniCmdBssInfoConnType
- **实际函数名**: `FUN_14014fa20`
- **地址**: `14014fa20`

### 反编译代码
```c

undefined8 FUN_14014fa20(longlong param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x10);
  if ((iVar1 == 1) && (*(int *)(param_1 + 0x28) == 0)) {
    return 0x10001;
  }
  if (*(int *)(param_1 + 0x28) == 1) {
    iVar2 = *(int *)(param_1 + 0x2dc);
    if (iVar2 == 8) {
      return 0x20000;
    }
    if ((iVar1 == 5) || (iVar2 == 0x20)) {
      return 0x20001;
    }
    if ((iVar1 == 4) || (iVar2 == 0x10)) {
      if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
         (((PTR_LOOP_140246148[0xa4] & 1) != 0 && (1 < (byte)PTR_LOOP_140246148[0xa1])))) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x2a,&DAT_1402387a0,
                      "nicUniCmdBssInfoConnType",0x43a);
      }
      return 0x20002;
    }
  }
  return 0;
}

 (GhidraScript)  
```

## 7. nicUniCmdBssInfoMld
- **实际函数名**: `FUN_14014fad0`
- **地址**: `14014fad0`

### 反编译代码
```c

undefined2 FUN_14014fad0(longlong *param_1,undefined4 *param_2,undefined1 param_3)

{
  longlong lVar1;
  undefined2 uVar2;
  longlong lVar3;
  longlong lVar4;
  
  lVar1 = *param_1;
  *param_2 = 0x14001a;
  lVar3 = FUN_14018b15c(lVar1,param_3);
  if (((char)param_1[0xb95fb] == '\0') || (lVar3 == 0)) {
LAB_14014fcb9:
    *(undefined1 *)(param_2 + 1) = 0xff;
    *(undefined1 *)((longlong)param_2 + 5) = *(undefined1 *)((longlong)param_1 + 0x24);
    *(undefined4 *)((longlong)param_2 + 6) = *(undefined4 *)((longlong)param_1 + 0x2cc);
    *(short *)((longlong)param_2 + 10) = (short)param_1[0x5a];
    *(undefined1 *)(param_2 + 4) = 0;
    param_2[3] = 0xffff;
  }
  else {
    lVar3 = FUN_140177344(lVar1,lVar3);
    uVar2 = FUN_1401540c0(param_1);
    lVar4 = FUN_14010434c(param_1,uVar2,param_3);
    if ((lVar4 == 0) || (lVar3 == 0)) goto LAB_14014fcb9;
    if (*(int *)(lVar1 + 0x14647ac) == 3) {
      *(undefined1 *)(param_2 + 1) = *(undefined1 *)(lVar3 + 2);
      *(undefined1 *)(param_2 + 3) = *(undefined1 *)(lVar3 + 3);
      *(undefined1 *)((longlong)param_2 + 0xd) = *(undefined1 *)(lVar4 + 0x8fb);
      *(undefined1 *)((longlong)param_2 + 0xe) = *(undefined1 *)(lVar3 + 0xd);
      if ((int)param_1[0xb95fc] == 1) {
        *(undefined1 *)((longlong)param_2 + 0xf) = 1;
      }
      else if ((int)param_1[0xb95fc] - 2U < 2) {
        *(undefined1 *)((longlong)param_2 + 0xf) = 0;
      }
      *(undefined1 *)((longlong)param_2 + 0x11) = *(undefined1 *)(lVar3 + 1);
      *(undefined1 *)(param_2 + 4) = *(undefined1 *)(lVar3 + 0x10);
      if (*(char *)(lVar1 + 0x1466a5d) != '\0') {
        *(undefined1 *)((longlong)param_2 + 0xf) = 0;
      }
      if ((*(char *)((longlong)param_1 + 0x5cafee) == '\0') && (*(char *)(lVar3 + 0x10) != '\0')) {
        *(undefined2 *)((longlong)param_2 + 0xe) = 0;
        if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
           (((PTR_LOOP_140246148[0x2c] & 1) != 0 && (2 < (byte)PTR_LOOP_140246148[0x29])))) {
          WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x2b,&DAT_1402387a0,
                   "nicUniCmdBssInfoMld");
        }
      }
    }
    else {
      *(undefined1 *)(param_2 + 1) = 0xff;
      param_2[3] = 0xffff;
      *(undefined1 *)(param_2 + 4) = 0;
    }
    *(char *)((longlong)param_2 + 5) = *(char *)(lVar4 + 0x908) + ' ';
    *(undefined4 *)((longlong)param_2 + 6) = *(undefined4 *)(lVar3 + 4);
    *(undefined2 *)((longlong)param_2 + 10) = *(undefined2 *)(lVar3 + 8);
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) goto LAB_14014fddb;
    if (((PTR_LOOP_140246148[0x2c] & 1) != 0) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
      FUN_140015c00(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x2c,&DAT_1402387a0,
                    "nicUniCmdBssInfoMld",param_3,*(undefined1 *)(lVar4 + 0x908),
                    *(undefined1 *)(lVar4 + 0x8fb),*(undefined1 *)(lVar3 + 2),
                    *(undefined1 *)(lVar4 + 0x8f9));
    }
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_14006c328(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x2d,&DAT_1402387a0,
                  "nicUniCmdBssInfoMld",*(undefined1 *)((longlong)param_1 + 0x24),
                  *(undefined1 *)(param_2 + 1),*(undefined1 *)((longlong)param_2 + 5),
                  *(undefined1 *)(param_2 + 3),*(undefined1 *)((longlong)param_2 + 0xd),
                  *(undefined1 *)((longlong)param_2 + 0xe),*(undefined1 *)(param_2 + 4),
                  *(undefined1 *)((longlong)param_2 + 0xf),*(undefined1 *)((longlong)param_2 + 6),
                  *(undefined1 *)((longlong)param_2 + 7),*(undefined1 *)(param_2 + 2),
                  *(undefined1 *)((longlong)param_2 + 9),*(undefined1 *)((longlong)param_2 + 10),
                  *(undefined1 *)((longlong)param_2 + 0xb));
  }
LAB_14014fddb:
  return *(undefined2 *)((longlong)param_2 + 2);
}

 (GhidraScript)  
```

## 8. nicUniCmdBssInfoTagSTAIoT
- **实际函数名**: `FUN_14014d350`
- **地址**: `14014d350`

### 反编译代码
```c

undefined2 FUN_14014d350(undefined8 param_1,undefined4 *param_2,longlong param_3)

{
  undefined1 uVar1;
  undefined1 uVar2;
  
  *param_2 = 0x80018;
  uVar1 = *(undefined1 *)(param_3 + 0x3d);
  *(undefined1 *)((longlong)param_2 + 5) = uVar1;
  uVar2 = *(undefined1 *)(param_3 + 0x31);
  *(undefined1 *)(param_2 + 1) = uVar2;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x71,&DAT_1402387a0,
                  "nicUniCmdBssInfoTagSTAIoT",0x18,uVar1,uVar2);
  }
  return *(undefined2 *)((longlong)param_2 + 2);
}

 (GhidraScript)  
```

## 9. MtCmdSetBssInfo
- **实际函数名**: `FUN_1400cf928`
- **地址**: `1400cf928`

### 反编译代码
```c

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

ulonglong FUN_1400cf928(longlong param_1,longlong param_2,longlong param_3,byte param_4)

{
  undefined2 uVar1;
  int iVar2;
  undefined4 uVar3;
  char *pcVar4;
  longlong lVar5;
  longlong lVar6;
  longlong lVar7;
  longlong lVar8;
  byte bVar9;
  undefined8 uVar10;
  longlong lVar11;
  ushort uVar12;
  undefined1 auStack_198 [32];
  uint local_178;
  uint local_170;
  undefined8 local_168;
  uint local_160;
  undefined8 local_158;
  undefined8 local_150;
  uint local_148;
  uint local_140;
  uint local_138;
  uint local_130;
  uint local_128;
  uint local_120;
  uint local_118;
  uint local_110;
  uint local_108;
  uint local_100;
  byte local_f8;
  undefined *local_f0;
  longlong local_e8;
  longlong local_e0;
  longlong local_d8;
  byte local_c8;
  byte local_c7;
  byte local_c6;
  byte local_c5;
  undefined1 local_c4 [32];
  uint local_a4;
  undefined2 local_a0;
  byte local_9e;
  ushort local_9c;
  ushort local_9a;
  byte local_98;
  undefined1 local_97;
  undefined2 local_96;
  byte local_94;
  byte local_93;
  byte local_92;
  byte local_91;
  undefined2 local_90;
  byte local_8e;
  byte local_8d;
  undefined1 local_8b;
  undefined4 local_88;
  undefined1 local_84 [22];
  undefined1 local_6e;
  undefined1 local_6d;
  byte local_6b;
  undefined1 local_6a;
  byte local_68;
  byte local_67;
  byte local_66;
  byte local_65;
  ushort local_64;
  byte local_62;
  byte local_61;
  ulonglong local_48;
  
  local_48 = DAT_14024f600 ^ (ulonglong)auStack_198;
  uVar3 = 0xc0000001;
  local_f0 = (undefined *)CONCAT44(local_f0._4_4_,0xc0000001);
  local_f8 = param_4;
  local_e8 = param_1;
  local_e0 = param_3;
  local_d8 = param_2;
  FUN_14020d8c0(&local_c8,0,0x74);
  lVar7 = 0;
  lVar8 = 0;
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xbd,&DAT_140238608);
  }
  pcVar4 = (char *)FUN_14018b184(param_1,param_2);
  if ((pcVar4 == (char *)0x0) || (*pcVar4 == '\0')) {
    if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) goto LAB_1400cfbc4;
    if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (PTR_LOOP_140246148[0xa1] != '\0')) {
      local_178 = 0xddd;
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xbe,&DAT_140238608,"MtCmdSetBssInfo"
                   );
    }
  }
  else {
    lVar5 = FUN_1401026e4(param_1,1);
    if ((((lVar5 == 0) && ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148)) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
      local_170 = (uint)local_f8;
      local_178 = 0xde4;
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xbf,&DAT_140238608,"MtCmdSetBssInfo"
                   );
    }
    lVar6 = FUN_1401026e4(param_1,5);
    if (((lVar6 == 0) && ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148)) &&
       (((PTR_LOOP_140246148[0xa4] & 1) != 0 && (PTR_LOOP_140246148[0xa1] != '\0')))) {
      local_170 = (uint)local_f8;
      local_178 = 0xdeb;
      FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xc0,&DAT_140238608,"MtCmdSetBssInfo"
                   );
    }
    lVar11 = local_e0;
    if (local_e0 == 0) {
      if (local_f8 == 0) {
        uVar1 = FUN_1401540c0(param_2);
        lVar7 = FUN_1401044cc(param_2,uVar1);
LAB_1400cfc06:
        lVar11 = lVar7;
        if (lVar7 != 0) goto LAB_1400cfc53;
        if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) goto LAB_1400cfbc4;
        if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (PTR_LOOP_140246148[0xa1] != '\0')) {
          uVar10 = 0xc2;
          local_178 = 0xe12;
          goto LAB_1400cfb69;
        }
      }
      else {
        if (((lVar5 == 0) || (*(char *)(lVar5 + 0x30) == '\0')) ||
           ((uint)*(byte *)(param_2 + 0x2e9d47) != *(uint *)(lVar5 + 0x10))) {
          if ((lVar6 != 0) && (*(char *)(lVar6 + 0x30) != '\0')) {
            uVar1 = FUN_1401540c0(lVar6);
            lVar7 = FUN_1401044cc(lVar6,uVar1);
          }
        }
        else {
          uVar1 = FUN_1401540c0(lVar5);
          lVar7 = FUN_1401044cc(lVar5,uVar1);
        }
        uVar1 = FUN_1401540c0(param_2);
        lVar8 = FUN_1401044cc(param_2,uVar1);
        if (lVar8 != 0) goto LAB_1400cfc06;
        if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) goto LAB_1400cfbc4;
        if (((PTR_LOOP_140246148[0xa4] & 1) != 0) && (PTR_LOOP_140246148[0xa1] != '\0')) {
          uVar10 = 0xc1;
          local_178 = 0xe03;
LAB_1400cfb69:
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),uVar10,&DAT_140238608,
                        "MtCmdSetBssInfo");
        }
      }
      uVar3 = 0xc0000001;
    }
    else {
LAB_1400cfc53:
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x1d0] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x1cd])) {
        local_168 = (byte *)CONCAT44(local_168._4_4_,(uint)*(byte *)(lVar11 + 0x4a7));
        local_170 = (uint)*(byte *)(lVar11 + 0x4aa);
        local_178 = (uint)*(byte *)(lVar11 + 8);
        FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x1bc),0xc3,&DAT_140238608,
                      "MtCmdSetBssInfo");
      }
      FUN_14001022c(&local_c8,0x74);
      if (((*(char *)(param_2 + 0x5cafd8) == '\0') ||
          (lVar7 = FUN_14018b15c(local_e8,*(undefined1 *)(lVar11 + 0x908)),
          *(char *)(param_2 + 0x5cafd8) == '\0')) || ((local_e0 == 0 || (lVar7 == 0)))) {
        local_c8 = *(byte *)(param_2 + 0x24);
        local_a4 = *(uint *)(param_2 + 0x2e0);
        local_a0 = *(undefined2 *)(param_2 + 0x2e4);
        local_8e = *(byte *)(param_2 + 0x2d3);
        local_6a = *(undefined1 *)(param_2 + 0x30);
      }
      else {
        local_c8 = *(byte *)(lVar11 + 0x908);
        local_a4 = *(uint *)(lVar11 + 0x2ac);
        local_a0 = *(undefined2 *)(lVar11 + 0x2b0);
        local_8e = *(byte *)(lVar7 + 0x21);
        local_6a = *(undefined1 *)(lVar7 + 0x12);
      }
      local_c5 = *(byte *)(param_2 + 0x307);
      local_c7 = ~(byte)(*(uint *)(param_2 + 0x2e6964) >> 7) & 1;
      local_c6 = (*(int *)(param_2 + 0x10) != 4) - 1U & 2;
      FUN_140010118(local_c4);
      local_9e = (byte)((uint)*(undefined4 *)(param_2 + 0x2e6964) >> 8) & 1;
      local_9c = *(ushort *)(lVar11 + 0x4a2);
      local_9a = *(ushort *)(lVar11 + 0x4a4);
      local_94 = *(byte *)(lVar11 + 0x4ac);
      local_91 = *(byte *)(lVar11 + 0x4a7);
      if ((local_f8 == 0) || (lVar8 == 0)) {
        local_98 = *(byte *)(lVar11 + 9);
      }
      else {
        local_98 = *(byte *)(lVar8 + 9);
      }
      local_96 = *(undefined2 *)(lVar11 + 0x4ae);
      if ((local_f8 == 0) || (lVar8 == 0)) {
        uVar3 = *(undefined4 *)(lVar11 + 0x168);
      }
      else {
        uVar3 = *(undefined4 *)(lVar8 + 0x168);
      }
      local_93 = FUN_14013ecd4(uVar3);
      lVar7 = local_e8;
      local_90 = 0;
      local_92 = *(byte *)(param_2 + 0x374);
      local_8d = *(byte *)(param_2 + 0x4ae);
      local_6d = *(undefined1 *)(param_2 + 0x5caa80);
      local_88 = 0;
      local_6e = 4;
      if (*(int *)(param_2 + 0x10) == 4) {
        if (*(char *)(local_e8 + 0x1887681) == '\x01') {
          local_8d = 1;
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            uVar10 = 0xc4;
            local_178 = 0xe62;
LAB_1400cfe9b:
            FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),uVar10,&DAT_140238608,
                          "MtCmdSetBssInfo");
          }
        }
        else {
          local_8d = 0;
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            uVar10 = 0xc5;
            local_178 = 0xe67;
            goto LAB_1400cfe9b;
          }
        }
      }
      if (*(char *)(lVar11 + 0x5ec) == '\0') {
        if (*(char *)(lVar11 + 0x441) != '\0') goto LAB_1400cfec9;
        local_6b = 2 - (*(char *)(lVar11 + 0x440) != '\0');
      }
      else {
        bVar9 = (byte)*(undefined2 *)(lVar11 + 0x5d6);
        if ((bVar9 & 0xc) == 0xc) {
          local_6b = (bVar9 & 3) != 3;
        }
        else {
LAB_1400cfec9:
          local_6b = 2;
        }
      }
      if (((*(short *)(lVar7 + 0x1f72) == 0x7925) || (*(short *)(lVar7 + 0x1f72) == 0x717)) &&
         ((*(char *)(lVar7 + 0x14726ce) != '\0' &&
          ((*(int *)(param_2 + 0x14) == 0 && (*(char *)(param_2 + 0x5cafd8) == '\x01')))))) {
        local_6b = 1;
      }
      uVar12 = *(ushort *)(lVar7 + 0x14651f7) >> 4;
      if ((uVar12 & 0xf) < (ushort)local_6b) {
        local_6b = (byte)uVar12 & 0xf;
        *(undefined1 *)(lVar11 + 0x441) = 0;
      }
      *(uint *)(param_2 + 0x20) = (uint)local_6b;
      if (*(char *)(param_2 + 0x5cafd8) == '\0') {
        local_62 = *(byte *)(param_2 + 0x5caa81);
        local_61 = *(byte *)(param_2 + 0x5caa82);
      }
      else {
        local_62 = *(byte *)(lVar11 + 0x914);
        local_61 = *(byte *)(lVar11 + 0x915);
      }
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
        local_140 = (uint)local_a0._1_1_;
        local_148 = (uint)(byte)local_a0;
        local_150 = CONCAT44(local_150._4_4_,local_a4 >> 0x18);
        local_158 = CONCAT44(local_158._4_4_,local_a4 >> 0x10) & 0xffffffff000000ff;
        local_160 = local_a4 >> 8 & 0xff;
        local_168 = (byte *)(CONCAT44(local_168._4_4_,local_a4) & 0xffffffff000000ff);
        local_170 = (uint)local_61;
        local_178 = (uint)local_62;
        FUN_14000810c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xc6,&DAT_140238608,
                      "MtCmdSetBssInfo");
        param_2 = local_d8;
      }
      FUN_140010118(&local_68,lVar11 + 0x5bf,3);
      local_65 = *(byte *)(lVar11 + 0x5c2);
      FUN_140010118(&local_64,lVar11 + 0x5c3,2);
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
        local_158 = CONCAT44(local_158._4_4_,(uint)local_65);
        local_160 = (uint)local_66;
        local_168 = (byte *)CONCAT44(local_168._4_4_,(uint)local_67);
        local_170 = (uint)local_68;
        local_178 = (uint)local_64;
        FUN_140015c00(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),199,&DAT_140238608,
                      "MtCmdSetBssInfo");
      }
      if (((char)*(undefined4 *)(param_2 + 0x2e6964) < '\0') &&
         (iVar2 = FUN_14018b674(lVar7,param_2), iVar2 == 7)) {
        local_8b = 7;
        local_97 = *(undefined1 *)(lVar7 + 0x358c2d);
      }
      local_f0 = PTR_LOOP_140246148;
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
        local_100 = (uint)*(byte *)(lVar11 + 0x441);
        local_108 = (uint)*(byte *)(lVar11 + 0x440);
        local_110 = (uint)local_6b;
        local_118 = (uint)local_8d;
        local_120 = (uint)local_8e;
        local_128 = (uint)local_92;
        local_130 = (uint)local_93;
        local_138 = (uint)local_98;
        local_140 = (uint)local_91;
        local_148 = (uint)local_94;
        local_150 = CONCAT44(local_150._4_4_,(uint)local_9a);
        local_158 = CONCAT44(local_158._4_4_,(uint)local_9c);
        local_160 = (uint)local_9e;
        local_168 = (byte *)CONCAT44(local_168._4_4_,(uint)local_c5);
        local_170 = (uint)local_c7;
        local_178 = (uint)local_c8;
        FUN_140030e24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),200,&DAT_140238608,
                      "MtCmdSetBssInfo");
        param_2 = local_d8;
        lVar7 = local_e8;
      }
      local_178._0_1_ = local_f8;
      FUN_1400c1e88(lVar7,param_2,local_e0,local_84);
      local_148 = 0;
      local_168 = &local_c8;
      local_150 = 0;
      local_158 = 0;
      local_160 = CONCAT22(local_160._2_2_,0x74);
      local_170 = CONCAT31(local_170._1_3_,0x10);
      local_178 = CONCAT31(local_178._1_3_,1);
      uVar3 = FUN_1400cdc4c(param_2,0x12,0xed,0);
      local_f0 = (undefined *)CONCAT44(local_f0._4_4_,uVar3);
    }
  }
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0xc9,&DAT_140238608,uVar3);
  }
LAB_1400cfbc4:
  return (ulonglong)local_f0 & 0xffffffff;
}

 (GhidraScript)  
```

## 10. nicUniCmdSetBssRlm
- **实际函数名**: `FUN_1401445e0`
- **地址**: `1401445e0`

### 反编译代码
```c

undefined8 FUN_1401445e0(undefined8 param_1,char *param_2)

{
  undefined1 *puVar1;
  undefined1 *puVar2;
  undefined8 *puVar3;
  longlong *plVar4;
  undefined8 uVar5;
  
  if ((*param_2 == '\x19') && (*(int *)(param_2 + 0x10) == 0x16)) {
    puVar1 = *(undefined1 **)(param_2 + 0x18);
    plVar4 = (longlong *)FUN_14014f788(param_1,2,0x30);
    if (plVar4 == (longlong *)0x0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x73,&DAT_1402387a0,
                      "nicUniCmdSetBssRlm",0xedd);
      }
      uVar5 = 0xc000009a;
    }
    else {
      puVar2 = (undefined1 *)plVar4[3];
      *puVar2 = *puVar1;
      FUN_140150edc(param_1,puVar2 + 4,puVar1);
      puVar3 = *(undefined8 **)(param_2 + 0x38);
      *(longlong **)(param_2 + 0x38) = plVar4;
      *plVar4 = (longlong)(param_2 + 0x30);
      plVar4[1] = (longlong)puVar3;
      *puVar3 = plVar4;
      *(int *)(param_2 + 0x40) = *(int *)(param_2 + 0x40) + 1;
      uVar5 = 0;
    }
  }
  else {
    uVar5 = 0x10003;
  }
  return uVar5;
}

 (GhidraScript)  
```

## 11. nicUniCmdSetBssRlmImpl
- **实际函数名**: `FUN_140150edc`
- **地址**: `140150edc`

### 反编译代码
```c

int FUN_140150edc(undefined8 param_1,undefined4 *param_2,undefined1 *param_3)

{
  uint *puVar1;
  undefined1 uVar2;
  undefined1 uVar3;
  undefined1 uVar4;
  char cVar5;
  undefined1 uVar6;
  
  *param_2 = 0x100002;
  uVar2 = param_3[2];
  uVar6 = 0;
  *(undefined1 *)(param_2 + 1) = uVar2;
  uVar3 = param_3[0x10];
  *(undefined1 *)((longlong)param_2 + 5) = uVar3;
  uVar4 = param_3[0x11];
  *(undefined1 *)((longlong)param_2 + 6) = uVar4;
  *(undefined1 *)(param_2 + 2) = param_3[0x14];
  *(undefined1 *)((longlong)param_2 + 9) = param_3[0x15];
  *(undefined1 *)((longlong)param_2 + 10) = param_3[0xc];
  *(undefined1 *)((longlong)param_2 + 0xb) = param_3[3];
  *(undefined1 *)(param_2 + 3) = param_3[1];
  *(undefined1 *)((longlong)param_2 + 7) = 0;
  cVar5 = param_3[0xf];
  if (cVar5 == '\0') {
    if (param_3[3] != '\0') {
      *(undefined1 *)((longlong)param_2 + 7) = 1;
      uVar6 = 1;
    }
  }
  else if (cVar5 == '\x01') {
    *(undefined1 *)((longlong)param_2 + 7) = 2;
    uVar6 = 2;
  }
  else if (cVar5 == '\x02') {
    *(undefined1 *)((longlong)param_2 + 7) = 3;
    uVar6 = 3;
  }
  else {
    if (cVar5 == '\x03') {
      uVar6 = 6;
    }
    else {
      uVar6 = 0;
      if (cVar5 != '\x04') goto LAB_140150f96;
      uVar6 = 7;
    }
    *(undefined1 *)((longlong)param_2 + 7) = uVar6;
  }
LAB_140150f96:
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0x2c] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x29])) {
    FUN_140015c00(*(undefined8 *)(PTR_LOOP_140246148 + 0x18),0x65,&DAT_1402387a0,
                  "nicUniCmdSetBssRlmImpl",*param_3,uVar2,uVar3,uVar4,uVar6);
  }
  param_2[4] = 0x80003;
  puVar1 = param_2 + 5;
  if (param_3[4] != '\0') {
    *puVar1 = *puVar1 | 0x20;
  }
  cVar5 = param_3[5];
  if (cVar5 == '\x01') {
    *puVar1 = *puVar1 | 2;
  }
  else if (cVar5 == '\x02') {
    *puVar1 = *puVar1 | 4;
  }
  else if (cVar5 == '\x03') {
    *puVar1 = *puVar1 | 8;
  }
  if (param_3[6] == '\x01') {
    *puVar1 = *puVar1 | 0x80;
  }
  param_2[6] = 0x140017;
  *(undefined1 *)(param_2 + 7) = 1;
  *(ushort *)(param_2 + 8) = (-(ushort)(param_3[0xe] != '\0') & 0xfff5) + 0x14;
  return ((int)(param_2 + 6) - (int)param_2) + 0x14;
}

 (GhidraScript)  
```

## 12. nicUniCmdBssInfoTagRlm
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 13. nicUniCmdBssInfoTagProtect
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 14. nicUniCmdBssInfoTagIfsTime
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 15. nicUniCmdBssInfoTagRate
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 16. nicUniCmdBssInfoTagSec
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 17. nicUniCmdBssInfoTagQbss
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 18. nicUniCmdBssInfoTagSap
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 19. nicUniCmdBssInfoTagP2P
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 20. nicUniCmdBssInfoTag11vMbssid
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```

## 21. nicUniCmdBssInfoTagWapi
- **实际函数名**: ``
- **地址**: ``

### 反编译代码
```c
```
