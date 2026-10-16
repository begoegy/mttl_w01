#!/usr/bin/env python3
from keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB, KS_MODE_LITTLE_ENDIAN, KS_MODE_V8
BASE=0x1001EB6C
DATA_PTR=0x1001EE98
ACTIVE=80; STAGE=424; SCRATCH=768; IMAGE_MAX=344
A=0xFC000; B=0xFD000; MAGIC=0x53464E53
FMT=0x1001EE30
EXIT=0x100165BA
asm=f'''
get_data:
 movw r0, #{DATA_PTR & 0xffff}
 movt r0, #{DATA_PTR >> 16}
 ldr r0, [r0]
 bx lr
next_int:
 push {{r3,lr}}
 bl 0x10016bec
 cbz r0, ni_bad
 adds r5, r0, #1
 mov r0, r5
 bl 0x10015498
 pop {{r3,pc}}
ni_bad:
 movw r0, #0
 movt r0, #0x8000
 pop {{r3,pc}}
validate:
 push {{r4,r5,r6,lr}}
 mov r4,r0
 ldr r0,[r4]
 movw r1, #{MAGIC & 0xffff}
 movt r1, #{MAGIC >> 16}
 cmp r0,r1
 bne v_bad
 ldr r0,[r4,#4]
 cmp r0,#1
 bne v_bad
 ldr r0,[r4,#8]
 cmp r0,#0
 ble v_bad
 ldr r0,[r4,#12]
 cmp r0,#0
 ble v_bad
 ldr r5,[r4,#16]
 cmp r5,#40
 bhi v_bad
 lsls r5,r5,#1
 adds r5,#6
 movs r6,#0
 mov r0,r4
v_sum:
 ldr r1,[r0],#4
 adds r6,r6,r1
 subs r5,#1
 bne v_sum
 cmp r6,#0
 bne v_bad
 movs r0,#1
 pop {{r4,r5,r6,pc}}
v_bad:
 movs r0,#0
 pop {{r4,r5,r6,pc}}
relay:
 push {{r3,r4,r5,lr}}
 mov r4,r0
 mov r5,r1
 cmp r4,#0
 bne rel_one
 movs r4,#1
rel_all:
 cmp r5,#0
 ite eq
 moveq r0,#12
 movne r0,#11
 mov r1,r4
 bl 0x1001ab42
 adds r4,#1
 cmp r4,#5
 blt rel_all
 pop {{r3,r4,r5,pc}}
rel_one:
 cmp r4,#4
 bhi rel_done
 cmp r5,#0
 ite eq
 moveq r0,#12
 movne r0,#11
 mov r1,r4
 bl 0x1001ab42
rel_done:
 pop {{r3,r4,r5,pc}}
ack:
 push {{r3,r4,r5,r6,r7,lr}}
 mov r5,r0
 mov r6,r1
 mov r7,r2
 sub sp,#72
 str r6,[sp]
 str r7,[sp,#4]
 add r0,sp,#8
 movs r1,#64
 movw r2, #{FMT & 0xffff}
 movt r2, #{FMT >> 16}
 mov r3,r5
 bl 0x1000a8cc
 add r0,sp,#8
 bl 0x1000aa00
 mov r2,r0
 movs r3,#0
 add r1,sp,#8
 ldr r0,[r4,#0x10]
 bl 0x10018c12
 add sp,#72
 pop {{r3,r4,r5,r6,r7,pc}}
handler:
 bl get_data
 cbz r0,h_exit
 mov r7,r0
 bl next_int
 cmp r0,#90
 beq h_clock
 cmp r0,#91
 beq h_begin
 cmp r0,#92
 beq h_item
 cmp r0,#93
 beq h_commit
 cmp r0,#94
 beq h_status
 b h_exit
h_clock:
 bl next_int
 cmp r0,#0
 ble h_exit
 str r0,[r7]
 bl 0x10010408
 str r0,[r7,#4]
 bl next_int
 str r0,[r7,#8]
 bl next_int
 str r0,[r7,#12]
 bl next_int
 str r0,[r7,#16]
 bl next_int
 str r0,[r7,#20]
 bl next_int
 str r0,[r7,#24]
 movs r0,#1
 str r0,[r7,#28]
 movs r0,#90
 ldr r1,[r7]
 movs r2,#1
 bl ack
 b h_exit
h_begin:
 bl next_int
 cmp r0,#0
 ble h_exit
 str r0,[r7,#44]
 bl next_int
 cmp r0,#40
 bhi h_exit
 str r0,[r7,#48]
 movs r0,#0
 str r0,[r7,#52]
 b h_exit
'''
# continue assembly
asm += f'''
h_item:
 ldr r0,[r7,#44]
 cmp r0,#0
 ble h_exit
 bl next_int
 ldr r1,[r7,#52]
 cmp r0,r1
 bne h_exit
 ldr r2,[r7,#48]
 cmp r0,r2
 bhs h_exit
 add.w r6,r7,#{STAGE+24}
 add r6,r6,r0,lsl #3
 bl next_int
 cmp r0,#4
 bhi h_exit
 strb r0,[r6]
 bl next_int
 cmp r0,#2
 bhi h_exit
 strb r0,[r6,#1]
 mov r5,r0
 bl next_int
 cmp r0,#0x7f
 bhi h_exit
 cmp r5,#2
 bne hi_not_week
 cmp r0,#0
 beq h_exit
 b hi_days_ok
hi_not_week:
 cmp r0,#0
 bne h_exit
hi_days_ok:
 strb r0,[r6,#2]
 bl next_int
 movw r1,#0xffff
 cmp r0,r1
 bhi h_exit
 cmp r5,#0
 bne hi_repeat_date
 cmp r0,r1
 beq h_exit
 b hi_date_ok
hi_repeat_date:
 cmp r0,r1
 bne h_exit
hi_date_ok:
 strh r0,[r6,#4]
 bl next_int
 movw r1,#1439
 cmp r0,r1
 bhi h_exit
 strh r0,[r6,#6]
 bl next_int
 cmp r0,#1
 bhi h_exit
 strb r0,[r6,#3]
 ldr r0,[r7,#52]
 adds r0,#1
 str r0,[r7,#52]
 b h_exit
h_commit:
 bl next_int
 ldr r1,[r7,#44]
 cmp r0,r1
 bne h_exit
 ldr r2,[r7,#48]
 ldr r3,[r7,#52]
 cmp r2,r3
 bne h_exit
 add.w r6,r7,#{STAGE}
 movw r3,#{MAGIC & 0xffff}
 movt r3,#{MAGIC >> 16}
 str r3,[r6]
 movs r3,#1
 str r3,[r6,#4]
 str r0,[r6,#8]
 ldr r3,[r7,#40]
 cbz r3,hc_seq1
 add.w r5,r7,#{ACTIVE}
 ldr r3,[r5,#12]
 adds r3,#1
 b hc_seq
hc_seq1:
 movs r3,#1
hc_seq:
 str r3,[r6,#12]
 str r2,[r6,#16]
 movs r3,#0
 str r3,[r6,#20]
 lsls r3,r2,#1
 adds r3,#6
 movs r5,#0
 mov r0,r6
hc_sum:
 ldr r1,[r0],#4
 adds r5,r5,r1
 subs r3,#1
 bne hc_sum
 rsbs r5,r5,#0
 str r5,[r6,#20]
 ldr r0,[r7,#36]
 mov.w r5,#{A}
 cmp r0,r5
 beq hc_target_b
 mov.w r5,#{A}
 b hc_target_ok
hc_target_b:
 mov.w r5,#{B}
hc_target_ok:
 lsls r2,r2,#3
 adds r2,#24
 mov r0,r5
 mov r1,r6
 bl 0x100157e6
 cmp r0,#0
 bne h_exit
 add.w r6,r7,#{SCRATCH}
 mov r0,r5
 mov r1,r6
 movw r2,#{IMAGE_MAX}
 bl 0x1001587a
 mov r0,r6
 bl validate
 cbz r0,h_exit
 ldr r0,[r6,#8]
 ldr r1,[r7,#44]
 cmp r0,r1
 bne h_exit
 add.w r0,r7,#{ACTIVE}
 mov r1,r6
 movw r2,#{IMAGE_MAX}
 bl 0x1000aa08
 str r5,[r7,#36]
 movs r0,#1
 str r0,[r7,#40]
 movs r0,#93
 ldr r1,[r7,#44]
 ldr r2,[r7,#48]
 ldr r3,[r7,#28]
 lsls r3,r3,#8
 orrs r2,r2,r3
 movw r3,#0x200
 orrs r2,r2,r3
 bl ack
 b h_exit
h_status:
 movs r1,#0
 movs r2,#0
 ldr r0,[r7,#40]
 cbz r0,hs_flags
 add.w r6,r7,#{ACTIVE}
 ldr r1,[r6,#8]
 ldr r2,[r6,#16]
 movw r0,#0x200
 orrs r2,r2,r0
hs_flags:
 ldr r0,[r7,#28]
 lsls r0,r0,#8
 orrs r2,r2,r0
 movs r0,#94
 bl ack
h_exit:
 b 0x{EXIT:x}
'''
asm += f'''
task:
 push {{r4,r5,r6,r7,lr}}
 sub sp,#4
 movw r0,#1152
 bl 0x100120a0
 cbz r0,t_dead
 mov r4,r0
 mov r2,r0
 movw r0,#{DATA_PTR & 0xffff}
 movt r0,#{DATA_PTR >> 16}
 str r2,[r0]
 mov r0,r4
 movs r1,#0
 movw r2,#1152
 bl 0x1000a9f0
 mov.w r0,#-1
 str r0,[r4,#32]
 add.w r5,r4,#{ACTIVE}
 mov.w r0,#{A}
 mov r1,r5
 movw r2,#{IMAGE_MAX}
 bl 0x1001587a
 mov r0,r5
 bl validate
 cbz r0,t_b
 movs r0,#1
 str r0,[r4,#40]
 mov.w r0,#{A}
 str r0,[r4,#36]
t_b:
 add.w r6,r4,#{SCRATCH}
 mov.w r0,#{B}
 mov r1,r6
 movw r2,#{IMAGE_MAX}
 bl 0x1001587a
 mov r0,r6
 bl validate
 cbz r0,t_loop
 ldr r0,[r4,#40]
 cbz r0,t_use_b
 ldr r0,[r6,#12]
 ldr r1,[r5,#12]
 cmp r0,r1
 bls t_loop
t_use_b:
 mov r0,r5
 mov r1,r6
 movw r2,#{IMAGE_MAX}
 bl 0x1000aa08
 movs r0,#1
 str r0,[r4,#40]
 mov.w r0,#{B}
 str r0,[r4,#36]
t_loop:
 ldr r0,[r4,#28]
 cbz r0,t_sleep
 ldr r0,[r4,#40]
 cbz r0,t_sleep
 bl 0x10010408
 ldr r1,[r4,#4]
 subs r0,r0,r1
 movw r1,#1000
 udiv r0,r0,r1
 ldr r1,[r4]
 adds r0,r0,r1
 mov r6,r0
 ldr r2,[r4,#8]
 ldr r1,[r4,#12]
 cbz r1,t_t2
 cmp r6,r1
 blo t_t2
 ldr r2,[r4,#16]
t_t2:
 ldr r1,[r4,#20]
 cbz r1,t_off
 cmp r6,r1
 blo t_off
 ldr r2,[r4,#24]
t_off:
 movs r1,#60
 muls r2,r2,r1
 adds r0,r6,r2
 udiv r0,r0,r1
 ldr r2,[r4,#32]
 cmp r0,r2
 beq t_sleep
 str r0,[r4,#32]
 movw r1,#1440
 udiv r7,r0,r1
 mls r6,r7,r1,r0
 str r6,[r4,#60]
 str r7,[r4,#56]
 adds r0,r7,#3
 movs r1,#7
 udiv r2,r0,r1
 mls r0,r1,r2,r0
 str r0,[r4,#64]
 add.w r5,r4,#{ACTIVE}
 ldr r7,[r5,#16]
 adds r5,#24
t_event:
 cmp r7,#0
 beq t_sleep
 ldrh r0,[r5,#6]
 ldr r1,[r4,#60]
 cmp r0,r1
 bne t_next
 ldrb r0,[r5]
 cmp r0,#4
 bhi t_next
 ldrb r1,[r5,#3]
 cmp r1,#1
 bhi t_next
 ldrb r2,[r5,#1]
 cmp r2,#0
 beq t_once
 cmp r2,#1
 beq t_fire
 cmp r2,#2
 bne t_next
 ldrb r2,[r5,#2]
 ldr r3,[r4,#64]
 movs r6,#1
 lsl r6,r6,r3
 tst r2,r6
 beq t_next
 b t_fire
t_once:
 ldrh r2,[r5,#4]
 ldr r3,[r4,#56]
 cmp r2,r3
 bne t_next
t_fire:
 bl relay
t_next:
 adds r5,#8
 subs r7,#1
 b t_event
t_sleep:
 movw r0,#1000
 bl 0x100101c6
 b t_loop
t_dead:
 movw r0,#5000
 bl 0x100101c6
 b t_dead
'''
if __name__ == '__main__':
    ks=Ks(KS_ARCH_ARM,KS_MODE_THUMB|KS_MODE_LITTLE_ENDIAN|KS_MODE_V8)
    enc,count=ks.asm(asm,addr=BASE,as_bytes=True)
    print('monolithic_probe_bytes',len(enc),'end',hex(BASE+len(enc)))
