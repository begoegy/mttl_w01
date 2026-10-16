#!/usr/bin/env python3
"""Host-only Thumb layout for New 1.0.71 Local Event Queue. No device I/O."""
from tools.sfn_scheduler_segments import assemble
TEST=0x1001606A; TEST_END=0x10016130
MAIN=0x1001EB6C; MAIN_END=0x1001EE54
FMT=0x1001EE5C; NAME=0x1001EE80; DATA_PTR=0x1001EE98
ALLOC=0x100120A0; FREE=0x10012176; TIMER_CREATE=0x1001227E; TIMER_CMD=0x100122B8
TICK=0x100103F8; ENTER=0x1001094C; EXITC=0x10010964; NEXTTOK=0x10016BEC; ATOI=0x10015498
RELAY=0x1001AB42; FORMAT=0x1000A8CC; STRLEN=0x1000AA00; SEND=0x10018C12
MAGIC=0x514E4653

def A(src,addr): return assemble(src,addr)
# Pass 1 helper positions; callback absolute address does not affect helper size.
GET=TEST+8
get_src=lambda cb: f'''
 push {{r4,r5,r6,lr}}
 movw r4,#{DATA_PTR&0xffff}
 movt r4,#{DATA_PTR>>16}
 ldr r0,[r4]
 cbz r0,gs_new
 ldr r1,[r0]
 movw r2,#{MAGIC&0xffff}
 movt r2,#{MAGIC>>16}
 cmp r1,r2
 beq gs_done
 movs r0,#0
 b gs_done
gs_new:
 movw r0,#256
 bl 0x{ALLOC:x}
 cbz r0,gs_done
 mov r5,r0
 mov r2,r0
 movs r1,#0
 movs r3,#64
gs_zero:
 str r1,[r2],#4
 subs r3,#1
 bne gs_zero
 movw r0,#{MAGIC&0xffff}
 movt r0,#{MAGIC>>16}
 str r0,[r5]
 sub sp,#8
 movw r0,#{(cb+1)&0xffff}
 movt r0,#{(cb+1)>>16}
 str r0,[sp]
 movw r0,#{NAME&0xffff}
 movt r0,#{NAME>>16}
 movs r1,#1
 movs r2,#0
 mov r3,r5
 bl 0x{TIMER_CREATE:x}
 add sp,#8
 cbnz r0,gs_timer_ok
 mov r0,r5
 bl 0x{FREE:x}
 movs r0,#0
 b gs_done
gs_timer_ok:
 str r0,[r5,#4]
 str r5,[r4]
 mov r0,r5
gs_done:
 pop {{r4,r5,r6,pc}}
'''
get0=A(get_src(MAIN),GET); NEXT=GET+len(get0)
next_src=f'''
 push {{r3,r4,r6,lr}}
 bl 0x{NEXTTOK:x}
 cbz r0,ni_bad
 adds r5,r0,#1
 movs r0,#0
 mov r4,r5
 movs r6,#0
ni_loop:
 ldrb r1,[r4]
 cbz r1,ni_done
 cmp r1,#58
 beq ni_done
 subs r1,#48
 cmp r1,#9
 bhi ni_bad
 adds r6,#1
 cmp r6,#9
 bhi ni_bad
 movs r2,#10
 muls r0,r2,r0
 adds r0,r0,r1
 adds r4,#1
 b ni_loop
ni_done:
 cmp r6,#0
 beq ni_bad
 pop {{r3,r4,r6,pc}}
ni_bad:
 movw r0,#0
 movt r0,#0x8000
 pop {{r3,r4,r6,pc}}
'''
next_b=A(next_src,NEXT)

# Main functions; fixed helper addresses above.
CONTROL=MAIN
control_src=f'''
 push {{r4,lr}}
 sub sp,#8
 movs r3,#0
 str r3,[sp]
 bl 0x{TIMER_CMD:x}
 add sp,#8
 pop {{r4,pc}}
'''
control_b=A(control_src,CONTROL); RES=CONTROL+len(control_b)
res_src=f'''
 push {{r4,r5,r6,r7,lr}}
 sub sp,#4
 mov r4,r0
rs_retry:
 bl 0x{TICK:x}
 mov r5,r0
 bl 0x{ENTER:x}
 ldr r6,[r4,#56]
 ldr r7,[r4,#4]
 ldr r0,[r4,#24]
 ldr r1,[r4,#28]
 cmp r1,r0
 bhs rs_none_locked
 ldr r1,[r4,#20]
 cbz r1,rs_none_locked
 subs r1,r1,r5
 bgt rs_delay_locked
 movs r1,#1
rs_delay_locked:
 str r1,[sp]
 bl 0x{EXITC:x}
 mov r0,r7
 ldr r2,[sp]
 movs r1,#4
 bl 0x{CONTROL:x}
 str r0,[sp]
 b rs_check
rs_none_locked:
 bl 0x{EXITC:x}
 mov r0,r7
 movs r2,#0
 movs r1,#3
 bl 0x{CONTROL:x}
 str r0,[sp]
rs_check:
 bl 0x{ENTER:x}
 ldr r5,[r4,#56]
 bl 0x{EXITC:x}
 cmp r5,r6
 bne rs_retry
 ldr r0,[sp]
 cbnz r0,rs_done
 movs r0,#1
 str r0,[r4,#60]
rs_done:
 add sp,#4
 pop {{r4,r5,r6,r7,pc}}
'''
res_b=A(res_src,RES); CALLBACK=RES+len(res_b)
callback_src=f'''
 push {{r4,r5,r6,r7,lr}}
 sub sp,#4
 ldr r4,[r0,#0x20]
 cbz r4,tc_done
 ldr r0,[r4]
 movw r1,#{MAGIC&0xffff}
 movt r1,#{MAGIC>>16}
 cmp r0,r1
 bne tc_done
 bl 0x{TICK:x}
 mov r5,r0
 bl 0x{ENTER:x}
 ldr r0,[r4,#24]
 ldr r1,[r4,#28]
 cmp r1,r0
 bhs tc_unlock
 ldr r6,[r4,#20]
 subs r2,r5,r6
 bmi tc_early
 ldr r0,[r4,#44]
 cmp r0,#0
 beq tc_bank0
 add.w r3,r4,#128
 b tc_bank
tc_bank0:
 add.w r3,r4,#64
tc_bank:
 add r3,r3,r1,lsl #3
 ldrb r5,[r3,#4]
 ldrb r6,[r3,#5]
 adds r1,#1
 str r1,[r4,#28]
 ldr r0,[r4,#24]
 cmp r1,r0
 bhs tc_last
 add r3,#8
 ldr r0,[r3]
 movw r2,#1000
 mul r0,r2,r0
 ldr r2,[r4,#16]
 adds r0,r0,r2
 str r0,[r4,#20]
 b tc_claimed
tc_last:
 movs r0,#0
 str r0,[r4,#20]
tc_claimed:
 ldr r0,[r4,#56]
 adds r0,#1
 str r0,[r4,#56]
 bl 0x{EXITC:x}
 cmp r6,#0
 ite eq
 moveq r0,#12
 movne r0,#11
 mov r1,r5
 bl 0x{RELAY:x}
 cbnz r0,tc_rearm
 movs r0,#2
 str r0,[r4,#60]
tc_rearm:
 mov r0,r4
 bl 0x{RES:x}
 b tc_done
tc_early:
 bl 0x{EXITC:x}
 mov r0,r4
 bl 0x{RES:x}
 b tc_done
tc_unlock:
 bl 0x{EXITC:x}
tc_done:
 add sp,#4
 pop {{r4,r5,r6,r7,pc}}
'''
callback_b=A(callback_src,CALLBACK); ACK4=CALLBACK+len(callback_b)
ack4_src=f'''
 push {{r3,r4,r5,r6,r7,lr}}
 sub sp,#80
 str r1,[sp]
 str r2,[sp,#4]
 str r3,[sp,#8]
 mov r3,r0
 add r0,sp,#16
 movs r1,#64
 movw r2,#{FMT&0xffff}
 movt r2,#{FMT>>16}
 bl 0x{FORMAT:x}
 add r0,sp,#16
 bl 0x{STRLEN:x}
 mov r2,r0
 movs r3,#0
 add r1,sp,#16
 ldr r0,[r4,#0x10]
 bl 0x{SEND:x}
 add sp,#80
 pop {{r3,r4,r5,r6,r7,pc}}
'''
ack4_b=A(ack4_src,ACK4); ACKSTATE=ACK4+len(ack4_b)
ackstate_src=f'''
 push {{r3,r4,r5,lr}}
 mov r5,r0
 mov r0,r1
 ldr r1,[r5,#8]
 ldr r2,[r5,#24]
 ldr r3,[r5,#28]
 ldr r5,[r5,#60]
 orr r3,r3,r5,lsl #8
 bl 0x{ACK4:x}
 pop {{r3,r4,r5,pc}}
'''
ackstate_b=A(ackstate_src,ACKSTATE); HANDLER=ACKSTATE+len(ackstate_b)
handler_src=f'''
 push.w {{r3,r4,r5,r6,r7,r8,r9,lr}}
 bl 0x{GET:x}
 cbnz r0,h_have
 movs r0,#0
 movs r1,#0
 movs r2,#0
 movs r3,#0
 bl 0x{ACK4:x}
 b h_exit
h_have:
 mov r7,r0
 bl 0x{NEXT:x}
 cmp r0,#1
 beq h_begin
 cmp r0,#2
 beq h_item
 cmp r0,#3
 beq h_commit
 cmp r0,#4
 beq h_status
 b h_bad
h_begin:
 bl 0x{NEXT:x}
 cmp r0,#0
 ble h_bad
 str r0,[r7,#12]
 bl 0x{NEXT:x}
 cmp r0,#8
 bhi h_bad
 str r0,[r7,#32]
 movs r1,#0
 str r1,[r7,#36]
 str r1,[r7,#52]
 movs r1,#1
 str r1,[r7,#48]
 ldr r1,[r7,#44]
 eors r1,r1,#1
 str r1,[r7,#40]
 b h_exit
h_item:
 ldr r0,[r7,#48]
 cbz r0,h_bad
 bl 0x{NEXT:x}
 ldr r1,[r7,#36]
 cmp r0,r1
 bne h_bad_stage
 ldr r2,[r7,#32]
 cmp r0,r2
 bhs h_bad_stage
 mov r8,r0
 bl 0x{NEXT:x}
 cmp r0,#0
 blt h_bad_stage
 movw r1,#0x5180
 movt r1,#1
 cmp r0,r1
 bhi h_bad_stage
 ldr r1,[r7,#52]
 cmp r0,r1
 blo h_bad_stage
 mov r9,r0
 bl 0x{NEXT:x}
 cmp r0,#1
 blo h_bad_stage
 cmp r0,#4
 bhi h_bad_stage
 mov r6,r0
 bl 0x{NEXT:x}
 cmp r0,#1
 bhi h_bad_stage
 ldr r1,[r7,#40]
 cmp r1,#0
 beq hi_bank0
 add.w r2,r7,#128
 b hi_bank
hi_bank0:
 add.w r2,r7,#64
hi_bank:
 add r2,r2,r8,lsl #3
 str r9,[r2]
 strb r6,[r2,#4]
 strb r0,[r2,#5]
 str r9,[r7,#52]
 ldr r0,[r7,#36]
 adds r0,#1
 str r0,[r7,#36]
 b h_exit
h_bad_stage:
 movs r0,#0
 str r0,[r7,#48]
 b h_bad
h_commit:
 bl 0x{NEXT:x}
 ldr r1,[r7,#8]
 cmp r0,r1
 beq hc_idempotent
 ldr r1,[r7,#12]
 cmp r0,r1
 bne h_bad
 ldr r1,[r7,#48]
 cbz r1,h_bad
 ldr r1,[r7,#32]
 ldr r2,[r7,#36]
 cmp r1,r2
 bne h_bad
 bl 0x{TICK:x}
 mov r6,r0
 bl 0x{ENTER:x}
 ldr r0,[r7,#12]
 str r0,[r7,#8]
 ldr r0,[r7,#40]
 str r0,[r7,#44]
 str r6,[r7,#16]
 movs r0,#0
 str r0,[r7,#28]
 str r0,[r7,#60]
 ldr r0,[r7,#32]
 str r0,[r7,#24]
 str r0,[r7,#48]
 ldr r1,[r7,#56]
 adds r1,#1
 str r1,[r7,#56]
 cbz r0,hc_empty
 ldr r1,[r7,#44]
 cmp r1,#0
 beq hc_bank0
 add.w r2,r7,#128
 b hc_bank
hc_bank0:
 add.w r2,r7,#64
hc_bank:
 ldr r1,[r2]
 movw r2,#1000
 mul r1,r2,r1
 adds r1,r1,r6
 str r1,[r7,#20]
 b hc_unlock
hc_empty:
 str r0,[r7,#20]
hc_unlock:
 bl 0x{EXITC:x}
 mov r0,r7
 bl 0x{RES:x}
hc_idempotent:
 mov r0,r7
 movs r1,#3
 bl 0x{ACKSTATE:x}
 b h_exit
h_status:
 mov r0,r7
 movs r1,#4
 bl 0x{ACKSTATE:x}
 b h_exit
h_bad:
 movs r0,#0
 ldr r1,[r7,#8]
 ldr r2,[r7,#24]
 ldr r3,[r7,#28]
 ldr r6,[r7,#60]
 orr r3,r3,r6,lsl #8
 bl 0x{ACK4:x}
h_exit:
 pop.w {{r3,r4,r5,r6,r7,r8,r9,pc}}
'''
handler_b=A(handler_src,HANDLER)
# Reassemble GET with true callback and shim with true handler.
get_b=A(get_src(CALLBACK),GET)
assert len(get_b)==len(get0)
shim=A(f'bl 0x{HANDLER:x}\nb.w 0x100165ba',TEST)

test_blob=shim+get_b+next_b
main_blob=control_b+res_b+callback_b+ack4_b+ackstate_b+handler_b
print('TEST',hex(TEST),len(test_blob),hex(TEST+len(test_blob)),'limit',hex(TEST_END),'free',TEST_END-(TEST+len(test_blob)))
print(' MAIN',hex(MAIN),len(main_blob),hex(MAIN+len(main_blob)),'limit',hex(MAIN_END),'free',MAIN_END-(MAIN+len(main_blob)))
print('FUNC')
for n,a,b in [('shim',TEST,shim),('get',GET,get_b),('next',NEXT,next_b),('control',CONTROL,control_b),('res',RES,res_b),('callback',CALLBACK,callback_b),('ack4',ACK4,ack4_b),('ackstate',ACKSTATE,ackstate_b),('handler',HANDLER,handler_b)]: print(n,hex(a),len(b),hex(a+len(b)))
fmt=b'up:sfnq:%d:%d:%d:%d\\r\\n\\x00'; name=b'sfnqueue\\x00'
# textual literals above are only sizing placeholders: actual byte literals measured below.
fmt2=b'up:sfnq:%d:%d:%d:%d\r\n\x00'; name2=b'sfnqueue\x00'
print('ACK data',len(fmt2)+len(name2),'fmt',len(fmt2),'name',len(name2),'pre-pointer room',DATA_PTR-FMT,'fits',FMT+len(fmt2)<=NAME and NAME+len(name2)<=DATA_PTR)
print('TOTAL code',len(test_blob)+len(main_blob),'capacity',(TEST_END-TEST)+(MAIN_END-MAIN))
assert TEST+len(test_blob)<=TEST_END
assert MAIN+len(main_blob)<=MAIN_END
assert FMT+len(fmt2)<=NAME and NAME+len(name2)<=DATA_PTR
