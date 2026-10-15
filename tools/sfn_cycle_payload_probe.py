#!/usr/bin/env python3
from keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB, KS_MODE_LITTLE_ENDIAN

KS=Ks(KS_ARCH_ARM, KS_MODE_THUMB|KS_MODE_LITTLE_ENDIAN)
HANDLER=0x1001606a
TASK=0x10016100
EXIT=0x100165ba
FMT=0x100166e4
NAME=0x10016700
handler=f'''\n    bl 0x10016bec\n    cmp r0, #0\n    beq fail\n    adds r5, r0, #1\n    mov r0, r5\n    bl 0x10015498\n    mov r6, r0\n    cmp r6, #1\n    blt fail\n    cmp r6, #4\n    bgt fail\n    bl 0x10016bec\n    cmp r0, #0\n    beq fail\n    adds r0, #1\n    bl 0x10015498\n    mov r7, r0\n    cmp r7, #5\n    blt fail\n    movw r0, #900\n    cmp r7, r0\n    bgt fail\n    lsls r3, r7, #8\n    orrs r3, r6\n    sub sp, #8\n    movs r0, #1\n    str r0, [sp]\n    movs r0, #0\n    str r0, [sp, #4]\n    movw r0, #{(TASK|1)&0xffff}\n    movt r0, #{(TASK|1)>>16}\n    movw r1, #{NAME&0xffff}\n    movt r1, #{NAME>>16}\n    movw r2, #256\n    bl 0x10010068\n    add sp, #8\n    cmp r0, #1\n    bne fail\n    add r0, sp, #0xa4\n    movw r1, #256\n    movw r2, #{FMT&0xffff}\n    movt r2, #{FMT>>16}\n    mov r3, r6\n    sub sp, #8\n    str r7, [sp]\n    bl 0x1000a8cc\n    add sp, #8\n    add r0, sp, #0xa4\n    bl 0x1000aa00\n    mov r2, r0\n    movs r3, #0\n    add r1, sp, #0xa4\n    ldr r0, [r4, #0x10]\n    bl 0x10018c12\nfail:\n    b 0x{EXIT:x}\n'''
task=f'''\n    push {{r4, r5, r6, lr}}\n    mov r4, r0\n    movs r0, #100\n    bl 0x100101c6\n    uxtb r5, r4\n    lsrs r4, r4, #8\n    movs r0, #12\n    mov r1, r5\n    bl 0x1001ab42\n    movw r0, #1000\n    muls r0, r4, r0\n    bl 0x100101c6\n    movs r0, #11\n    mov r1, r5\n    bl 0x1001ab42\n    movs r0, #0\n    bl 0x10010182\nforever:\n    b forever\n'''
for label,src,addr in [('handler',handler,HANDLER),('task',task,TASK)]:
    enc,_=KS.asm(src,addr=addr,as_bytes=True)
    print(label,len(enc),hex(addr),hex(addr+len(enc)))
    print(enc.hex())
