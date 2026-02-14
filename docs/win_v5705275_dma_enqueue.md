# Ghidra Function Dump

Program: mtkwecx.sys

## FUN_1400c8340 @ 1400c8340

### Immediates (>=0x10000)

- 0xd9853
- 0xd9867
- 0x1464b98
- 0x1464ba0
- 0x1464be0
- 0x1464d62
- 0x1465296
- 0x14652a6
- 0x14652ad
- 0x14652ae
- 0x14652af
- 0x14652d6
- 0x14652de
- 0x14652e2
- 0x14669e8
- 0x14669e9
- 0x146ad74
- 0x14726c0
- 0x1887d73
- 0x10000040
- 0xaabbccdd
- 0xc0000001
- 0xfa0a1f00
- 0xfbd3e280
- 0xfd9da600
- 0xfeced300
- 0xff676980
- 0x140219b20
- 0x140238608
- 0x140246148

### Disassembly

```asm
1400c8340: MOV qword ptr [RSP + 0x20],RBX
1400c8345: MOV qword ptr [RSP + 0x10],RDX
1400c834a: PUSH RBP
1400c834b: PUSH RSI
1400c834c: PUSH RDI
1400c834d: PUSH R12
1400c834f: PUSH R13
1400c8351: PUSH R14
1400c8353: PUSH R15
1400c8355: SUB RSP,0x40
1400c8359: LEA RSI,[R8 + 0x68]
1400c835d: MOV R15,R8
1400c8360: MOV RAX,qword ptr [RSI]
1400c8363: MOV RBX,RCX
1400c8366: XOR EDI,EDI
1400c8368: MOVZX R13D,byte ptr [RAX + 0x24]
1400c836d: MOV R12B,byte ptr [RAX + 0x25]
1400c8371: MOV R14B,byte ptr [RAX + 0x2b]
1400c8375: MOV AL,byte ptr [R8 + 0xa]
1400c8379: MOV byte ptr [RSP + 0x80],AL
1400c8380: MOV RCX,qword ptr [0x140246148]
1400c8387: LEA R8,[0x140246148]
1400c838e: LEA R10,[0x140238608]
1400c8395: MOV RDX,RSI
1400c8398: CMP RCX,R8
1400c839b: JZ 0x1400c83d3
1400c839d: TEST byte ptr [RCX + 0xa4],0x1
1400c83a4: JZ 0x1400c83d3
1400c83a6: CMP byte ptr [RCX + 0xa1],0x5
1400c83ad: JC 0x1400c83d3
1400c83af: MOV RCX,qword ptr [RCX + 0x90]
1400c83b6: LEA EDX,[RDI + 0x14]
1400c83b9: MOV R8,R10
1400c83bc: CALL 0x1400015d8
1400c83c1: LEA RDX,[R15 + 0x68]
1400c83c5: LEA R8,[0x140246148]
1400c83cc: LEA R10,[0x140238608]
1400c83d3: MOV BPL,0x2
1400c83d6: CMP byte ptr [RBX + 0x1887d73],DIL
1400c83dd: JZ 0x1400c8473
1400c83e3: CMP byte ptr [RBX + 0x14669e9],DIL
1400c83ea: JZ 0x1400c8428
1400c83ec: TEST BPL,R14B
1400c83ef: JBE 0x1400c8428
1400c83f1: MOV RAX,qword ptr [RSI]
1400c83f4: CMP word ptr [RAX + 0x22],0xd
1400c83f9: JZ 0x1400c847a
1400c83fb: MOV RCX,qword ptr [0x140246148]
1400c8402: CMP RCX,R8
1400c8405: JZ 0x1400c8469
1400c8407: TEST byte ptr [RCX + 0xa4],0x1
1400c840e: JZ 0x1400c8469
1400c8410: CMP byte ptr [RCX + 0xa1],BPL
1400c8417: JC 0x1400c8469
1400c8419: MOV EDX,0x15
1400c841e: MOV dword ptr [RSP + 0x20],0x268
1400c8426: JMP 0x1400c8453
1400c8428: MOV RCX,qword ptr [0x140246148]
1400c842f: CMP RCX,R8
1400c8432: JZ 0x1400c8469
1400c8434: TEST byte ptr [RCX + 0xa4],0x1
1400c843b: JZ 0x1400c8469
1400c843d: CMP byte ptr [RCX + 0xa1],BPL
1400c8444: JC 0x1400c8469
1400c8446: MOV EDX,0x16
1400c844b: MOV dword ptr [RSP + 0x20],0x26e
1400c8453: MOV RCX,qword ptr [RCX + 0x90]
1400c845a: LEA R9,[0x140219b20]
1400c8461: MOV R8,R10
1400c8464: CALL 0x140001664
1400c8469: MOV EAX,0xc0000001
1400c846e: JMP 0x1400c8c84
1400c8473: XOR CL,CL
1400c8475: TEST BPL,R14B
1400c8478: JBE 0x1400c8484
1400c847a: MOV RAX,qword ptr [RDX]
1400c847d: MOV CL,0x1
1400c847f: MOVZX R13D,word ptr [RAX + 0x22]
1400c8484: CMP byte ptr [RBX + 0x146ad74],DIL
1400c848b: JNZ 0x1400c8c01
1400c8491: MOV EAX,dword ptr [RBX + 0x1310]
1400c8497: TEST EAX,0x10000040
1400c849c: JNZ 0x1400c8c01
1400c84a2: CMP dword ptr [RBX + 0x12f0],0xaabbccdd
1400c84ac: JNZ 0x1400c8c01
1400c84b2: BT EAX,0xb
1400c84b6: JNC 0x1400c851b
1400c84b8: MOV EAX,0x90
1400c84bd: CMP R13W,AX
1400c84c1: JZ 0x1400c851b
1400c84c3: CMP R12B,0xa0
1400c84c7: JZ 0x1400c851b
1400c84c9: TEST CL,CL
1400c84cb: JZ 0x1400c850f
1400c84cd: MOV RCX,qword ptr [0x140246148]
1400c84d4: CMP RCX,R8
1400c84d7: JZ 0x1400c850f
1400c84d9: TEST byte ptr [RCX + 0xa4],0x1
1400c84e0: JZ 0x1400c850f
1400c84e2: MOV R15B,0x3
1400c84e5: CMP byte ptr [RCX + 0xa1],R15B
1400c84ec: JC 0x1400c850f
1400c84ee: MOV RCX,qword ptr [RCX + 0x90]
1400c84f5: LEA EDX,[RAX + -0x78]
1400c84f8: LEA R9,[0x140219b20]
1400c84ff: MOV dword ptr [RSP + 0x20],0x28b
1400c8507: MOV R8,R10
1400c850a: CALL 0x140001664
1400c850f: LEA R12,[0x140246148]
1400c8516: JMP 0x1400c8c44
1400c851b: CALL qword ptr [0x14022a368]
1400c8521: MOV RDX,R15
1400c8524: MOV RCX,RBX
1400c8527: TEST AL,AL
1400c8529: JZ 0x1400c8598
1400c852b: MOV R14D,0x103
1400c8531: MOV R8D,R14D
1400c8534: CALL 0x1400d316c
1400c8539: MOV EBX,EAX
1400c853b: MOV RCX,qword ptr [0x140246148]
1400c8542: LEA R12,[0x140246148]
1400c8549: CMP RCX,R12
1400c854c: JZ 0x1400c8587
1400c854e: TEST byte ptr [RCX + 0xa4],0x1
1400c8555: JZ 0x1400c8587
1400c8557: CMP byte ptr [RCX + 0xa1],BPL
1400c855e: JC 0x1400c8587
1400c8560: MOV RCX,qword ptr [RCX + 0x90]
1400c8567: LEA R9,[0x140219b20]
1400c856e: MOV EDX,0x19
1400c8573: MOV dword ptr [RSP + 0x20],0x298
1400c857b: LEA R8,[0x140238608]
1400c8582: CALL 0x140001664
1400c8587: CMP EBX,0x14
1400c858a: MOV EDI,0xc0000001
1400c858f: CMOVNZ EDI,R14D
1400c8593: JMP 0x1400c8c49
1400c8598: MOV R8D,0xc0000001
1400c859e: CALL 0x1400d316c
1400c85a3: MOV R14D,EAX
1400c85a6: CMP EAX,0x14
1400c85a9: JZ 0x1400c850f
1400c85af: LEA RCX,[R14 + R14*0x2]
1400c85b3: MOV R15B,0x3
1400c85b6: SHL RCX,0x5
1400c85ba: LEA RSI,[0x140219b20]
1400c85c1: CMP byte ptr [RCX + RBX*0x1 + 0x14652af],DIL
1400c85c9: JNZ 0x1400c880b
1400c85cf: MOV RCX,qword ptr [0x140246148]
1400c85d6: LEA R12,[0x140246148]
1400c85dd: CMP RCX,R12
1400c85e0: JZ 0x1400c861c
1400c85e2: TEST byte ptr [RCX + 0xa4],0x1
1400c85e9: JZ 0x1400c861c
1400c85eb: CMP byte ptr [RCX + 0xa1],R15B
1400c85f2: JC 0x1400c861c
1400c85f4: MOV RCX,qword ptr [RCX + 0x90]
1400c85fb: LEA R8,[0x140238608]
1400c8602: MOVZX EAX,R13W
1400c8606: MOV EDX,0x1a
1400c860b: MOV dword ptr [RSP + 0x28],EAX
1400c860f: MOV R9,RSI
1400c8612: MOV dword ptr [RSP + 0x20],R14D
1400c8617: CALL 0x140007b24
1400c861c: MOV EDX,0x8
1400c8621: LEA RCX,[RSP + 0x90]
1400c8629: CALL 0x14001022c
1400c862e: CMP byte ptr [RBX + 0x1464d62],0x1
1400c8635: JNZ 0x1400c867e
1400c8637: MOV RCX,qword ptr [0x140246148]
1400c863e: CMP RCX,R12
1400c8641: JZ 0x1400c8670
1400c8643: TEST byte ptr [RCX + 0xa4],0x1
1400c864a: JZ 0x1400c8670
1400c864c: CMP byte ptr [RCX + 0xa1],R15B
1400c8653: JC 0x1400c8670
1400c8655: MOV RCX,qword ptr [RCX + 0x90]
1400c865c: LEA R8,[0x140238608]
1400c8663: MOV EDX,0x1b
1400c8668: MOV R9,RSI
1400c866b: CALL 0x140001600
1400c8670: MOV qword ptr [RSP + 0x90],-0x42c1d80
1400c867c: JMP 0x1400c86da
1400c867e: CMP byte ptr [RBX + 0x14726c0],0x1
1400c8685: JNZ 0x1400c86ce
1400c8687: MOV RCX,qword ptr [0x140246148]
1400c868e: CMP RCX,R12
1400c8691: JZ 0x1400c86c0
1400c8693: TEST byte ptr [RCX + 0xa4],0x1
1400c869a: JZ 0x1400c86c0
1400c869c: CMP byte ptr [RCX + 0xa1],R15B
1400c86a3: JC 0x1400c86c0
1400c86a5: MOV RCX,qword ptr [RCX + 0x90]
1400c86ac: LEA R8,[0x140238608]
1400c86b3: MOV EDX,0x1c
1400c86b8: MOV R9,RSI
1400c86bb: CALL 0x140001600
1400c86c0: MOV qword ptr [RSP + 0x90],-0x989680
1400c86cc: JMP 0x1400c86da
1400c86ce: MOV qword ptr [RSP + 0x90],-0x2625a00
1400c86da: LEA RCX,[RBX + 0x1464be0]
1400c86e1: CALL 0x14008d69c
1400c86e6: MOV R8,qword ptr [RSP + 0x90]
1400c86ee: LEA RAX,[R14 + 0xd9867]
1400c86f5: LEA RAX,[RAX + RAX*0x2]
1400c86f9: MOV RCX,RBX
1400c86fc: LEA RDX,[RBX + RAX*0x8]
1400c8700: CALL 0x1400c9810
1400c8705: MOV EDI,EAX
1400c8707: TEST EAX,EAX
1400c8709: JZ 0x1400c8812
1400c870f: MOV R10,qword ptr [0x140246148]
1400c8716: CMP R10,R12
1400c8719: JZ 0x1400c876a
1400c871b: TEST byte ptr [R10 + 0xa4],0x1
1400c8723: JZ 0x1400c876a
1400c8725: CMP byte ptr [R10 + 0xa1],0x1
1400c872d: JC 0x1400c876a
1400c872f: MOV RCX,qword ptr [R10 + 0x90]
1400c8736: LEA RDX,[R14 + R14*0x2]
1400c873a: SHL RDX,0x5
1400c873e: LEA R8,[0x140238608]
1400c8745: MOV R9,RSI
1400c8748: MOVZX EAX,byte ptr [RDX + RBX*0x1 + 0x14652e2]
1400c8750: MOV EDX,0x1d
1400c8755: MOV dword ptr [RSP + 0x30],EAX
1400c8759: MOV dword ptr [RSP + 0x28],EDI
1400c875d: MOV dword ptr [RSP + 0x20],0x2c6
1400c8765: CALL 0x140007d48
1400c876a: CMP EDI,0x102
1400c8770: JNZ 0x1400c8812
1400c8776: LEA RCX,[R14 + R14*0x2]
1400c877a: SHL RCX,0x5
1400c877e: CMP byte ptr [RCX + RBX*0x1 + 0x14652e2],0x0
1400c8786: JNZ 0x1400c8812
1400c878c: LEA RCX,[R14 + R14*0x2]
1400c8790: SHL RCX,0x5
1400c8794: CMP byte ptr [RCX + RBX*0x1 + 0x14652af],0x0
1400c879c: JNZ 0x1400c8812
1400c879e: CMP byte ptr [RBX + 0x14669e8],0x0
1400c87a5: JNZ 0x1400c8812
1400c87a7: MOV RCX,qword ptr [0x140246148]
1400c87ae: CMP RCX,R12
1400c87b1: JZ 0x1400c87e8
1400c87b3: TEST byte ptr [RCX + 0xa4],0x1
1400c87ba: JZ 0x1400c87e8
1400c87bc: CMP byte ptr [RCX + 0xa1],BPL
1400c87c3: JC 0x1400c87e8
1400c87c5: MOV RCX,qword ptr [RCX + 0x90]
1400c87cc: LEA R8,[0x140238608]
1400c87d3: MOV EDX,0x1e
1400c87d8: MOV dword ptr [RSP + 0x20],0x2ce
1400c87e0: MOV R9,RSI
1400c87e3: CALL 0x140001664
1400c87e8: MOV R8,qword ptr [RSP + 0x90]
1400c87f0: LEA RAX,[R14 + 0xd9867]
1400c87f7: LEA RAX,[RAX + RAX*0x2]
1400c87fb: MOV RCX,RBX
1400c87fe: LEA RDX,[RBX + RAX*0x8]
1400c8802: CALL 0x1400c9810
1400c8807: MOV EDI,EAX
1400c8809: JMP 0x1400c8812
1400c880b: LEA R12,[0x140246148]
1400c8812: LEA RAX,[R14 + R14*0x2]
1400c8816: SHL RAX,0x5
1400c881a: MOV DL,byte ptr [RAX + RBX*0x1 + 0x14652ad]
1400c8821: CMP DL,0x1
1400c8824: JNZ 0x1400c8aa6
1400c882a: LEA RAX,[R14 + R14*0x2]
1400c882e: SHL RAX,0x5
1400c8832: MOV CL,byte ptr [RAX + RBX*0x1 + 0x14652ae]
1400c8839: TEST CL,CL
1400c883b: JNZ 0x1400c8aa1
1400c8841: TEST EDI,EDI
1400c8843: JNZ 0x1400c8b2d
1400c8849: CMP byte ptr [RBX + 0x146ad74],DIL
1400c8850: JNZ 0x1400c8bc9
1400c8856: MOV RCX,qword ptr [0x140246148]
1400c885d: CMP RCX,R12
1400c8860: JZ 0x1400c88a8
1400c8862: TEST byte ptr [RCX + 0xa4],DL
1400c8868: JZ 0x1400c88a8
1400c886a: CMP byte ptr [RCX + 0xa1],0x4
1400c8871: JC 0x1400c88a8
1400c8873: MOVZX EBP,byte ptr [RSP + 0x80]
1400c887b: LEA EDX,[RDI + 0x1f]
1400c887e: MOV RCX,qword ptr [RCX + 0x90]
1400c8885: MOV R9,RSI
1400c8888: MOVZX R8D,R13W
1400c888c: MOV dword ptr [RSP + 0x30],EBP
1400c8890: MOV dword ptr [RSP + 0x28],R8D
1400c8895: LEA R8,[0x140238608]
1400c889c: MOV dword ptr [RSP + 0x20],R14D
1400c88a1: CALL 0x140007d48
1400c88a6: JMP 0x1400c88b0
1400c88a8: MOV BPL,byte ptr [RSP + 0x80]
1400c88b0: MOV EDX,0x8
1400c88b5: LEA RCX,[RSP + 0x90]
1400c88bd: CALL 0x14001022c
1400c88c2: CMP byte ptr [RBX + 0x14726c0],0x1
1400c88c9: MOV R8D,0x7
1400c88cf: JNZ 0x1400c8917
1400c88d1: MOV RCX,qword ptr [0x140246148]
1400c88d8: CMP RCX,R12
1400c88db: JZ 0x1400c8909
1400c88dd: TEST byte ptr [RCX + 0xa4],0x1
1400c88e4: JZ 0x1400c8909
1400c88e6: CMP byte ptr [RCX + 0xa1],R15B
1400c88ed: JC 0x1400c8909
1400c88ef: MOV RCX,qword ptr [RCX + 0x90]
1400c88f6: LEA EDX,[R8 + 0x19]
1400c88fa: LEA R8,[0x140238608]
1400c8901: MOV R9,RSI
1400c8904: CALL 0x140001600
1400c8909: MOV qword ptr [RSP + 0x90],-0x989680
1400c8915: JMP 0x1400c898b
1400c8917: CMP BPL,0xed
1400c891b: JNZ 0x1400c897f
1400c891d: CMP R13W,R8W
1400c8921: JNZ 0x1400c897f
1400c8923: MOV RCX,qword ptr [0x140246148]
1400c892a: CMP RCX,R12
1400c892d: JZ 0x1400c8971
1400c892f: TEST byte ptr [RCX + 0xa4],0x1
1400c8936: JZ 0x1400c8971
1400c8938: CMP byte ptr [RCX + 0xa1],0x1
1400c893f: JC 0x1400c8971
1400c8941: MOV RCX,qword ptr [RCX + 0x90]
1400c8948: MOV EDX,0x21
1400c894d: MOV dword ptr [RSP + 0x30],0xed
1400c8955: MOV R9,RSI
1400c8958: MOV dword ptr [RSP + 0x28],R8D
1400c895d: LEA R8,[0x140238608]
1400c8964: MOV dword ptr [RSP + 0x20],0x7d0
1400c896c: CALL 0x140007d48
1400c8971: MOV qword ptr [RSP + 0x90],-0x1312d00
1400c897d: JMP 0x1400c898b
1400c897f: MOV qword ptr [RSP + 0x90],-0x5f5e100
1400c898b: LEA RCX,[RBX + 0x1464be0]
1400c8992: CALL 0x14008d69c
1400c8997: MOV R8,qword ptr [RSP + 0x90]
1400c899f: LEA RAX,[R14 + 0xd9853]
1400c89a6: LEA RAX,[RAX + RAX*0x2]
1400c89aa: MOV RCX,RBX
1400c89ad: LEA RDX,[RBX + RAX*0x8]
1400c89b1: CALL 0x1400c9810
1400c89b6: MOV EDI,EAX
1400c89b8: CMP EAX,0x102
1400c89bd: JNZ 0x1400c8b25
1400c89c3: CMP BPL,0xed
1400c89c7: JNZ 0x1400c8b2d
1400c89cd: MOV R8D,0x7
1400c89d3: CMP R13W,R8W
1400c89d7: JNZ 0x1400c8b2d
1400c89dd: MOV RCX,qword ptr [0x140246148]
1400c89e4: CMP RCX,R12
1400c89e7: JZ 0x1400c8a22
1400c89e9: TEST byte ptr [RCX + 0xa4],0x1
1400c89f0: JZ 0x1400c8a22
1400c89f2: CMP byte ptr [RCX + 0xa1],0x1
1400c89f9: JC 0x1400c8a22
1400c89fb: MOV RCX,qword ptr [RCX + 0x90]
1400c8a02: LEA EDX,[R8 + 0x1b]
1400c8a06: MOV dword ptr [RSP + 0x28],0xed
1400c8a0e: MOV R9,RSI
1400c8a11: MOV dword ptr [RSP + 0x20],R8D
1400c8a16: LEA R8,[0x140238608]
1400c8a1d: CALL 0x140007b24
1400c8a22: MOV RCX,qword ptr [RSP + 0x88]
1400c8a2a: CALL 0x1401540c0
1400c8a2f: MOV RCX,qword ptr [RSP + 0x88]
1400c8a37: MOVZX EDX,AX
1400c8a3a: CALL 0x1401044cc
1400c8a3f: TEST RAX,RAX
1400c8a42: JZ 0x1400c8b2d
1400c8a48: MOV byte ptr [RAX + 0x1dc],0x1
1400c8a4f: MOV RCX,qword ptr [0x140246148]
1400c8a56: CMP RCX,R12
1400c8a59: JZ 0x1400c8b2d
1400c8a5f: TEST byte ptr [RCX + 0xa4],0x1
1400c8a66: JZ 0x1400c8b2d
1400c8a6c: CMP byte ptr [RCX + 0xa1],0x1
1400c8a73: JC 0x1400c8b2d
1400c8a79: MOV RCX,qword ptr [RCX + 0x90]
1400c8a80: LEA R8,[0x140238608]
1400c8a87: MOV EDX,0x23
1400c8a8c: MOV dword ptr [RSP + 0x20],0x1
1400c8a94: MOV R9,RSI
1400c8a97: CALL 0x140001664
1400c8a9c: JMP 0x1400c8b2d
1400c8aa1: CMP CL,0x1
1400c8aa4: JZ 0x1400c8abc
1400c8aa6: TEST DL,DL
1400c8aa8: JNZ 0x1400c8b25
1400c8aaa: LEA RCX,[R14 + R14*0x2]
1400c8aae: SHL RCX,0x5
1400c8ab2: CMP byte ptr [RCX + RBX*0x1 + 0x14652af],0x1
1400c8aba: JNZ 0x1400c8b25
1400c8abc: MOV RCX,qword ptr [0x140246148]
1400c8ac3: CMP RCX,R12
1400c8ac6: JZ 0x1400c8b16
1400c8ac8: TEST byte ptr [RCX + 0xa4],0x1
1400c8acf: JZ 0x1400c8b16
1400c8ad1: CMP byte ptr [RCX + 0xa1],0x4
1400c8ad8: JC 0x1400c8b16
1400c8ada: MOV RCX,qword ptr [RCX + 0x90]
1400c8ae1: LEA RAX,[R14 + R14*0x2]
1400c8ae5: SHL RAX,0x5
1400c8ae9: MOV EDX,0x24
1400c8aee: MOVZX R8D,R13W
1400c8af2: MOV R9,RSI
1400c8af5: MOV EAX,dword ptr [RAX + RBX*0x1 + 0x14652de]
1400c8afc: MOV dword ptr [RSP + 0x30],EAX
1400c8b00: MOV dword ptr [RSP + 0x28],R8D
1400c8b05: LEA R8,[0x140238608]
1400c8b0c: MOV dword ptr [RSP + 0x20],R14D
1400c8b11: CALL 0x140007d48
1400c8b16: LEA RCX,[R14 + R14*0x2]
1400c8b1a: SHL RCX,0x5
1400c8b1e: MOV EDI,dword ptr [RCX + RBX*0x1 + 0x14652de]
1400c8b25: TEST EDI,EDI
1400c8b27: JZ 0x1400c8bc9
1400c8b2d: LEA RBP,[RBX + 0x1464b98]
1400c8b34: MOV RCX,RBP
1400c8b37: CALL qword ptr [0x14022a360]
1400c8b3d: MOV byte ptr [RBX + 0x1464ba0],AL
1400c8b43: LEA RCX,[R14 + R14*0x2]
1400c8b47: SHL RCX,0x5
1400c8b4b: MOV dword ptr [RCX + RBX*0x1 + 0x14652de],EDI
1400c8b52: LEA RCX,[R14 + R14*0x2]
1400c8b56: SHL RCX,0x5
1400c8b5a: MOV byte ptr [RCX + RBX*0x1 + 0x1465296],0x0
1400c8b62: MOV RCX,qword ptr [0x140246148]
1400c8b69: CMP RCX,R12
1400c8b6c: JZ 0x1400c8bb8
1400c8b6e: TEST byte ptr [RCX + 0xa4],0x1
1400c8b75: JZ 0x1400c8bb8
1400c8b77: CMP byte ptr [RCX + 0xa1],R15B
1400c8b7e: JC 0x1400c8bb8
1400c8b80: MOV RCX,qword ptr [RCX + 0x90]
1400c8b87: LEA RDX,[R14 + R14*0x2]
1400c8b8b: SHL RDX,0x5
1400c8b8f: LEA R8,[0x140238608]
1400c8b96: MOV dword ptr [RSP + 0x30],EDI
1400c8b9a: MOV R9,RSI
1400c8b9d: MOVZX EAX,word ptr [RDX + RBX*0x1 + 0x14652a6]
1400c8ba5: MOV EDX,0x25
1400c8baa: MOV dword ptr [RSP + 0x28],EAX
1400c8bae: MOV dword ptr [RSP + 0x20],R14D
1400c8bb3: CALL 0x140007d48
1400c8bb8: MOV DL,byte ptr [RBX + 0x1464ba0]
1400c8bbe: MOV RCX,RBP
1400c8bc1: CALL qword ptr [0x14022a358]
1400c8bc7: JMP 0x1400c8bd0
1400c8bc9: LEA RBP,[RBX + 0x1464b98]
1400c8bd0: MOV RCX,RBP
1400c8bd3: CALL qword ptr [0x14022a360]
1400c8bd9: MOV byte ptr [RBX + 0x1464ba0],AL
1400c8bdf: LEA RDX,[R14 + R14*0x2]
1400c8be3: SHL RDX,0x5
1400c8be7: MOV RCX,RBP
1400c8bea: AND qword ptr [RDX + RBX*0x1 + 0x14652d6],0x0
1400c8bf3: MOV DL,byte ptr [RBX + 0x1464ba0]
1400c8bf9: CALL qword ptr [0x14022a358]
1400c8bff: JMP 0x1400c8c49
1400c8c01: MOV RCX,qword ptr [0x140246148]
1400c8c08: LEA R12,[0x140246148]
1400c8c0f: CMP RCX,R12
1400c8c12: JZ 0x1400c8c44
1400c8c14: TEST byte ptr [RCX + 0xa4],0x1
1400c8c1b: JZ 0x1400c8c44
1400c8c1d: MOV R15B,0x3
1400c8c20: CMP byte ptr [RCX + 0xa1],R15B
1400c8c27: JC 0x1400c8c44
1400c8c29: MOV RCX,qword ptr [RCX + 0x90]
1400c8c30: LEA R9,[0x140219b20]
1400c8c37: MOV EDX,0x17
1400c8c3c: MOV R8,R10
1400c8c3f: CALL 0x140001600
1400c8c44: MOV EDI,0xc0000001
1400c8c49: MOV RCX,qword ptr [0x140246148]
1400c8c50: CMP RCX,R12
1400c8c53: JZ 0x1400c8c82
1400c8c55: TEST byte ptr [RCX + 0xa4],0x1
1400c8c5c: JZ 0x1400c8c82
1400c8c5e: CMP byte ptr [RCX + 0xa1],0x5
1400c8c65: JC 0x1400c8c82
1400c8c67: MOV RCX,qword ptr [RCX + 0x90]
1400c8c6e: LEA R8,[0x140238608]
1400c8c75: MOV EDX,0x26
1400c8c7a: MOV R9D,EDI
1400c8c7d: CALL 0x14000764c
1400c8c82: MOV EAX,EDI
1400c8c84: MOV RBX,qword ptr [RSP + 0x98]
1400c8c8c: ADD RSP,0x40
1400c8c90: POP R15
1400c8c92: POP R14
1400c8c94: POP R13
1400c8c96: POP R12
1400c8c98: POP RDI
1400c8c99: POP RSI
1400c8c9a: POP RBP
1400c8c9b: RET
```

### Decompiled C

```c

int FUN_1400c8340(longlong param_1,undefined8 param_2,longlong param_3)

{
  byte bVar1;
  char cVar2;
  char cVar3;
  bool bVar4;
  char cVar5;
  undefined1 uVar6;
  undefined2 uVar7;
  int iVar8;
  uint uVar9;
  int iVar10;
  longlong lVar11;
  undefined8 uVar12;
  ushort uVar13;
  ulonglong uVar14;
  undefined8 local_res18;
  undefined4 uVar15;
  
  lVar11 = *(longlong *)(param_3 + 0x68);
  iVar10 = 0;
  uVar13 = (ushort)*(byte *)(lVar11 + 0x24);
  cVar5 = *(char *)(lVar11 + 0x25);
  bVar1 = *(byte *)(lVar11 + 0x2b);
  cVar2 = *(char *)(param_3 + 10);
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x14,&DAT_140238608);
  }
  if (*(char *)(param_1 + 0x1887d73) == '\0') {
    bVar4 = false;
    if ((bVar1 & 2) != 0) goto LAB_1400c847a;
  }
  else {
    if ((*(char *)(param_1 + 0x14669e9) == '\0') || ((bVar1 & 2) == 0)) {
      if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
        return -0x3fffffff;
      }
      if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
        return -0x3fffffff;
      }
      if ((byte)PTR_LOOP_140246148[0xa1] < 2) {
        return -0x3fffffff;
      }
      uVar12 = 0x16;
      uVar15 = 0x26e;
LAB_1400c8453:
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),uVar12,&DAT_140238608,
                    "MtCmdEnqueueFWCmd",uVar15);
      return -0x3fffffff;
    }
    if (*(short *)(*(longlong *)(param_3 + 0x68) + 0x22) != 0xd) {
      if ((undefined **)PTR_LOOP_140246148 == &PTR_LOOP_140246148) {
        return -0x3fffffff;
      }
      if ((PTR_LOOP_140246148[0xa4] & 1) == 0) {
        return -0x3fffffff;
      }
      if ((byte)PTR_LOOP_140246148[0xa1] < 2) {
        return -0x3fffffff;
      }
      uVar12 = 0x15;
      uVar15 = 0x268;
      goto LAB_1400c8453;
    }
LAB_1400c847a:
    bVar4 = true;
    uVar13 = *(ushort *)(*(longlong *)(param_3 + 0x68) + 0x22);
  }
  if (((*(char *)(param_1 + 0x146ad74) == '\0') && ((*(uint *)(param_1 + 0x1310) & 0x10000040) == 0)
      ) && (*(int *)(param_1 + 0x12f0) == -0x55443323)) {
    if ((((*(uint *)(param_1 + 0x1310) >> 0xb & 1) == 0) || (uVar13 == 0x90)) || (cVar5 == -0x60)) {
      cVar5 = KeGetCurrentIrql();
      if (cVar5 != '\0') {
        iVar8 = FUN_1400d316c(param_1,param_3,0x103);
        if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
            ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
          FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x19,&DAT_140238608,
                        "MtCmdEnqueueFWCmd",0x298);
        }
        iVar10 = -0x3fffffff;
        if (iVar8 != 0x14) {
          iVar10 = 0x103;
        }
        goto LAB_1400c8c49;
      }
      uVar9 = FUN_1400d316c(param_1,param_3,0xc0000001);
      uVar14 = (ulonglong)uVar9;
      if (uVar9 != 0x14) {
        if (*(char *)(uVar14 * 0x60 + 0x14652af + param_1) == '\0') {
          if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
              ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
            FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1a,&DAT_140238608,
                          "MtCmdEnqueueFWCmd",uVar9,uVar13);
          }
          FUN_14001022c(&local_res18,8);
          if (*(char *)(param_1 + 0x1464d62) == '\x01') {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1b,&DAT_140238608,
                       "MtCmdEnqueueFWCmd");
            }
            local_res18 = 0xfffffffffbd3e280;
          }
          else if (*(char *)(param_1 + 0x14726c0) == '\x01') {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1c,&DAT_140238608,
                       "MtCmdEnqueueFWCmd");
            }
            local_res18 = 0xffffffffff676980;
          }
          else {
            local_res18 = 0xfffffffffd9da600;
          }
          NdisSetEvent(param_1 + 0x1464be0);
          iVar10 = FUN_1400c9810(param_1,param_1 + (uVar14 + 0xd9867) * 0x18,local_res18);
          if (iVar10 != 0) {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
              FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1d,&DAT_140238608,
                            "MtCmdEnqueueFWCmd",0x2c6,iVar10,
                            *(undefined1 *)(uVar14 * 0x60 + 0x14652e2 + param_1));
            }
            if (((iVar10 == 0x102) && (*(char *)(uVar14 * 0x60 + 0x14652e2 + param_1) == '\0')) &&
               ((*(char *)(uVar14 * 0x60 + 0x14652af + param_1) == '\0' &&
                (*(char *)(param_1 + 0x14669e8) == '\0')))) {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
                FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1e,&DAT_140238608,
                              "MtCmdEnqueueFWCmd",0x2ce);
              }
              iVar10 = FUN_1400c9810(param_1,param_1 + (uVar14 + 0xd9867) * 0x18,local_res18);
            }
          }
        }
        cVar5 = *(char *)(uVar14 * 0x60 + 0x14652ad + param_1);
        if (cVar5 == '\x01') {
          cVar3 = *(char *)(uVar14 * 0x60 + 0x14652ae + param_1);
          if (cVar3 != '\0') {
            if (cVar3 != '\x01') goto LAB_1400c8aa6;
            goto LAB_1400c8abc;
          }
          if (iVar10 != 0) goto LAB_1400c8b2d;
          if (*(char *)(param_1 + 0x146ad74) == '\0') {
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (3 < (byte)PTR_LOOP_140246148[0xa1])) {
              FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x1f,&DAT_140238608,
                            "MtCmdEnqueueFWCmd",uVar9,uVar13,cVar2);
            }
            FUN_14001022c(&local_res18,8);
            if (*(char *)(param_1 + 0x14726c0) == '\x01') {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
                WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x20,&DAT_140238608,
                         "MtCmdEnqueueFWCmd");
              }
              local_res18 = 0xffffffffff676980;
            }
            else if ((cVar2 == -0x13) && (uVar13 == 7)) {
              if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                  ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
                FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x21,&DAT_140238608,
                              "MtCmdEnqueueFWCmd",2000,7,0xed);
              }
              local_res18 = 0xfffffffffeced300;
            }
            else {
              local_res18 = 0xfffffffffa0a1f00;
            }
            NdisSetEvent(param_1 + 0x1464be0);
            iVar10 = FUN_1400c9810(param_1,param_1 + (uVar14 + 0xd9853) * 0x18,local_res18);
            if (iVar10 == 0x102) {
              if ((cVar2 == -0x13) && (uVar13 == 7)) {
                if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                   (((PTR_LOOP_140246148[0xa4] & 1) != 0 && (PTR_LOOP_140246148[0xa1] != '\0')))) {
                  FUN_140007b24(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x22,&DAT_140238608,
                                "MtCmdEnqueueFWCmd",7,0xed);
                }
                uVar7 = FUN_1401540c0(param_2);
                lVar11 = FUN_1401044cc(param_2,uVar7);
                if ((((lVar11 != 0) &&
                     (*(undefined1 *)(lVar11 + 0x1dc) = 1,
                     (undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148)) &&
                    ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
                  FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x23,&DAT_140238608,
                                "MtCmdEnqueueFWCmd",1);
                }
              }
              goto LAB_1400c8b2d;
            }
            goto LAB_1400c8b25;
          }
        }
        else {
LAB_1400c8aa6:
          if ((cVar5 == '\0') && (*(char *)(uVar14 * 0x60 + 0x14652af + param_1) == '\x01')) {
LAB_1400c8abc:
            if (((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
               (((PTR_LOOP_140246148[0xa4] & 1) != 0 && (3 < (byte)PTR_LOOP_140246148[0xa1])))) {
              FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x24,&DAT_140238608,
                            "MtCmdEnqueueFWCmd",uVar9,uVar13,
                            *(undefined4 *)(uVar14 * 0x60 + 0x14652de + param_1));
            }
            iVar10 = *(int *)(uVar14 * 0x60 + 0x14652de + param_1);
          }
LAB_1400c8b25:
          if (iVar10 != 0) {
LAB_1400c8b2d:
            uVar6 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x1464b98);
            *(undefined1 *)(param_1 + 0x1464ba0) = uVar6;
            *(int *)(uVar14 * 0x60 + 0x14652de + param_1) = iVar10;
            *(undefined1 *)(uVar14 * 0x60 + 0x1465296 + param_1) = 0;
            if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
                ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
              FUN_140007d48(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x25,&DAT_140238608,
                            "MtCmdEnqueueFWCmd",uVar9,
                            *(undefined2 *)(uVar14 * 0x60 + 0x14652a6 + param_1),iVar10);
            }
            KeReleaseSpinLock(param_1 + 0x1464b98,*(undefined1 *)(param_1 + 0x1464ba0));
          }
        }
        uVar6 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x1464b98);
        *(undefined1 *)(param_1 + 0x1464ba0) = uVar6;
        *(undefined8 *)(uVar14 * 0x60 + 0x14652d6 + param_1) = 0;
        KeReleaseSpinLock(param_1 + 0x1464b98,
                          CONCAT71((int7)(uVar14 * 0x60 >> 8),*(undefined1 *)(param_1 + 0x1464ba0)))
        ;
        goto LAB_1400c8c49;
      }
    }
    else if (((bVar4) && ((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148)) &&
            (((PTR_LOOP_140246148[0xa4] & 1) != 0 && (2 < (byte)PTR_LOOP_140246148[0xa1])))) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x18,&DAT_140238608,
                    "MtCmdEnqueueFWCmd",0x28b);
    }
  }
  else if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
           ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0xa1])) {
    WPP_SF_s(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x17,&DAT_140238608,"MtCmdEnqueueFWCmd");
  }
  iVar10 = -0x3fffffff;
LAB_1400c8c49:
  if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
      ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (4 < (byte)PTR_LOOP_140246148[0xa1])) {
    FUN_14000764c(*(undefined8 *)(PTR_LOOP_140246148 + 0x90),0x26,&DAT_140238608,iVar10);
  }
  return iVar10;
}


```

