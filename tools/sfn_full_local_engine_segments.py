#!/usr/bin/env python3
import re
from tools.sfn_scheduler_segments import assemble
from tools.sfn_scheduler_payload_probe import asm as MONO

TASK=0x10009914; TASK_END=0x10009A12
MEM=0x1001600C; NEXT=0x10016010; MEM_END=0x1001602E
TEST=0x1001606A; TEST_END=0x10016130
MAIN=0x1001EB6C; MAIN_END=0x1001EE54
ACK=0x1001EE5C; ACK_END=0x1001EEA8
LEGACY=0x100233D6; LEGACY_END=0x10023634
WRAPPER=0x10023686
TEXT=0x100166DC; TEXT_END=0x10016714
HOOK=0x10012918; HOOK_RETURN=0x1001291C
RELAY_API=0x1001AB42; RELAY_RETURN=0x1001AB46
DATA_PTR=0x1001EE98
ACTIVE=80; STAGE=424; SCRATCH=768; IMAGE=344; MAX_ITEMS=38
VGSTATE=1112; VGSTATUS=VGSTATE; VGSINCE=VGSTATE+4
A=0xFC000; B=0xFD000; MAGIC=0x454E4653; FORMAT=2
FLASH_WRITE=0x100157E6; FLASH_READ=0x1001587A; TICK=0x10010408
MEMSET=0x1000A9F0; MEMCPY=0x1000AA08
PARSER_EXIT=0x100165BA
FMT=0x100166A4

def section(start,end):
    return start+':'+MONO.split(start+':',1)[1].split(end+':',1)[0]

def valid_policy_src():
    return """valid_policy:
 push {r4,r5,lr}
 sub sp,#4
 mov r4,r0
 ldr r5,[r4,#28]
 uxth r0,r5
 mov r1,r0
 and r2,r0,#3
 lsrs r3,r0,#8
 and r3,r3,#15
 lsls r3,r3,#8
 orrs r2,r2,r3
 cmp r1,r2
 bne vp_bad
 tst r5,#1
 beq vp_ok
 ldr r0,[r4,#24]
 cmp r0,#0
 ble vp_bad
 lsrs r0,r5,#8
 and r0,r0,#15
 cbz r0,vp_bad
 lsrs r0,r5,#16
 cmp r0,#1
 blo vp_bad
 movw r1,#3600
 cmp r0,r1
 bhi vp_bad
 ldr r0,[r4,#32]
 uxth r1,r0
 lsrs r2,r0,#16
 movw r3,#1000
 cmp r1,r3
 blo vp_bad
 movw r3,#3000
 cmp r2,r3
 bhi vp_bad
 subs r2,r2,r1
 cmp r2,#100
 blo vp_bad
 ldr r0,[r4,#36]
 uxth r1,r0
 lsrs r3,r0,#16
 cmp r1,#10
 blo vp_bad
 cmp r1,#200
 bhi vp_bad
 lsls r1,r1,#1
 cmp r2,r1
 bls vp_bad
 cmp r3,#5
 blo vp_bad
 movw r1,#300
 cmp r3,r1
 bhi vp_bad
vp_ok:
 movs r0,#1
 add sp,#4
 pop {r4,r5,pc}
vp_bad:
 movs r0,#0
 add sp,#4
 pop {r4,r5,pc}
"""

def validate_src(policy_addr):
    return f"""valid:
 push {{r4,r5,r6,lr}}
 mov r4,r0
 ldr r0,[r4]
 movw r1,#{MAGIC&0xffff}
 movt r1,#{MAGIC>>16}
 cmp r0,r1
 bne v_bad
 ldr r0,[r4,#4]
 cmp r0,#{FORMAT}
 bne v_bad
 ldr r0,[r4,#12]
 cmp r0,#0
 ble v_bad
 ldr r5,[r4,#16]
 cmp r5,#{MAX_ITEMS}
 bhi v_bad
 cbz r5,v_policy
 ldr r0,[r4,#8]
 cmp r0,#0
 ble v_bad
v_policy:
 mov r0,r4
 bl 0x{policy_addr:x}
 cbz r0,v_bad
 lsls r5,r5,#1
 adds r5,#10
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
"""

def seal_src():
    return """seal:
 movs r1,#0
 str r1,[r0,#20]
 ldr r1,[r0,#16]
 lsls r1,r1,#1
 adds r1,#10
 movs r2,#0
 mov r3,r0
seal_loop:
 ldr r12,[r3],#4
 adds r2,r2,r12
 subs r1,#1
 bne seal_loop
 rsbs r2,r2,#0
 str r2,[r0,#20]
 bx lr
"""

def persist_src(valid_addr):
    return f"""persist:
 push {{r4,r5,r6,r7,lr}}
 sub sp,#4
 mov r4,r0
 mov r6,r1
 ldr r0,[r4,#36]
 mov.w r5,#{A}
 cmp r0,r5
 ite eq
 moveq r5,#{B}
 movne r5,#{A}
 ldr r2,[r6,#16]
 lsls r2,r2,#3
 adds r2,#40
 mov r0,r5
 mov r1,r6
 bl 0x{FLASH_WRITE:x}
 cbnz r0,p_fail
 add.w r7,r4,#{SCRATCH}
 mov r0,r5
 mov r1,r7
 movw r2,#{IMAGE}
 bl 0x{FLASH_READ:x}
 mov r0,r7
 bl 0x{valid_addr:x}
 cbz r0,p_fail
 ldr r0,[r7,#12]
 ldr r1,[r6,#12]
 cmp r0,r1
 bne p_fail
 add.w r0,r4,#{ACTIVE}
 mov r1,r7
 movw r2,#{IMAGE}
 bl 0x{MEMCPY:x}
 str r5,[r4,#36]
 movs r0,#1
 str r0,[r4,#40]
 b p_done
p_fail:
 movs r0,#0
p_done:
 add sp,#4
 pop {{r4,r5,r6,r7,pc}}
"""

def hook_eval_src():
    return f"""vg_hook:
 push {{r0,r1,r2,r3,r4,r5,r6,r7,lr}}
 sub sp,#4
 cmp r5,#0
 bne vg_done
 mov r4,r0
 movs r1,#100
 udiv r4,r4,r1
 movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 ldr r5,[r0]
 cbz r5,vg_done
 ldr r0,[r5,#40]
 cbz r0,vg_done
 add.w r6,r5,#{ACTIVE}
 ldr r7,[r6,#28]
 tst r7,#1
 beq vg_done
 bl 0x{TICK:x}
 mov r3,r0
 ldr r0,[r5,#{VGSTATUS}]
 tst r0,#1
 bne vg_tripped
 movw r1,#500
 cmp r4,r1
 blo vg_clear
 movw r1,#3500
 cmp r4,r1
 bhi vg_clear
 movs r2,#0
 ldr r1,[r6,#32]
 uxth r12,r1
 cmp r4,r12
 it lo
 movlo r2,#1
 blo vg_bad
 lsrs r12,r1,#16
 cmp r4,r12
 it hi
 movhi r2,#2
 bhi vg_bad
vg_clear:
 movs r0,#0
 str r0,[r5,#{VGSTATUS}]
 str r0,[r5,#{VGSINCE}]
 b vg_done
vg_bad:
 ldr r0,[r5,#{VGSTATUS}]
 lsrs r1,r0,#1
 and r1,r1,#3
 cmp r1,r2
 beq vg_hold
 lsls r0,r2,#1
 str r0,[r5,#{VGSTATUS}]
 str r3,[r5,#{VGSINCE}]
 b vg_done
vg_hold:
 ldr r0,[r5,#{VGSINCE}]
 subs r0,r3,r0
 ldr r1,[r6,#36]
 lsrs r1,r1,#16
 movs r2,#100
 muls r1,r2,r1
 cmp r0,r1
 blo vg_done
 lsrs r7,r7,#8
 and r7,r7,#15
 movs r1,#1
 movs r2,#1
 movs r4,#0
vg_off_loop:
 tst r7,r2
 beq vg_off_next
 movs r0,#13
 bl 0x{RELAY_API:x}
 cmp r0,#1
 it eq
 orreq r4,r4,r2
 movs r0,#12
 bl 0x{RELAY_API:x}
vg_off_next:
 adds r1,#1
 lsls r2,r2,#1
 cmp r1,#5
 blt vg_off_loop
 lsls r4,r4,#8
 orr r4,r4,#1
 str r4,[r5,#{VGSTATUS}]
 movs r0,#0
 str r0,[r5,#{VGSINCE}]
 b vg_done
vg_tripped:
 movw r1,#500
 cmp r4,r1
 blo vg_not_safe
 movw r1,#3500
 cmp r4,r1
 bhi vg_not_safe
 ldr r1,[r6,#32]
 uxth r2,r1
 ldr r12,[r6,#36]
 uxth r12,r12
 adds r2,r2,r12
 cmp r4,r2
 blo vg_not_safe
 lsrs r2,r1,#16
 subs r2,r2,r12
 cmp r4,r2
 bhi vg_not_safe
 tst r7,#2
 beq vg_done
 ldr r0,[r5,#{VGSINCE}]
 cbnz r0,vg_safe_wait
 str r3,[r5,#{VGSINCE}]
 b vg_done
vg_safe_wait:
 subs r0,r3,r0
 lsrs r1,r7,#16
 movw r2,#1000
 muls r1,r2,r1
 cmp r0,r1
 blo vg_done
 ldr r4,[r5,#{VGSTATUS}]
 lsrs r4,r4,#8
 and r4,r4,#15
 movs r0,#0
 str r0,[r5,#{VGSTATUS}]
 str r0,[r5,#{VGSINCE}]
 movs r1,#1
 movs r2,#1
vg_restore:
 tst r4,r2
 beq vg_res_next
 movs r0,#11
 bl 0x{RELAY_API:x}
vg_res_next:
 adds r1,#1
 lsls r2,r2,#1
 cmp r1,#5
 blt vg_restore
 b vg_done
vg_not_safe:
 movs r0,#0
 str r0,[r5,#{VGSINCE}]
vg_done:
 add sp,#4
 pop {{r0,r1,r2,r3,r4,r5,r6,r7,lr}}
 add.w r2,r2,r5,lsl #4
 b.w 0x{HOOK_RETURN:x}
"""

def gate_src():
    return f"""relay_gate:
 cmp r0,#11
 bne rg_allow
 movw r2,#{DATA_PTR&0xffff}
 movt r2,#{DATA_PTR>>16}
 ldr r2,[r2]
 cbz r2,rg_allow
 ldr r3,[r2,#40]
 cbz r3,rg_allow
 ldr r3,[r2,#{VGSTATUS}]
 tst r3,#1
 beq rg_allow
 add.w r2,r2,#{ACTIVE}
 ldr r2,[r2,#28]
 tst r2,#1
 beq rg_allow
 lsrs r2,r2,#8
 and r2,r2,#15
 subs r3,r1,#1
 movs r12,#1
 lsl.w r12,r12,r3
 tst r2,r12
 beq rg_allow
 movs r0,#0
 bx lr
rg_allow:
 mov r2,r0
 mov r0,r1
 b.w 0x{RELAY_RETURN:x}
"""

def relay_src():
    return section('relay','ack')

def vguard_handler_src(seal,persist,valid):
    return f"""vg_handler:
 push {{r3,r5,r6,r7}}
 movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 ldr r7,[r0]
 cbz r7,vh_exit
 add.w r6,r7,#{STAGE}
 ldr r0,[r7,#40]
 cbz r0,vh_new
 mov r0,r6
 add.w r1,r7,#{ACTIVE}
 movw r2,#{IMAGE}
 bl 0x{MEMCPY:x}
 b vh_parse
vh_new:
 mov r0,r6
 movs r1,#0
 movw r2,#{IMAGE}
 bl 0x{MEMSET:x}
 movw r0,#{MAGIC&0xffff}
 movt r0,#{MAGIC>>16}
 str r0,[r6]
 movs r0,#{FORMAT}
 str r0,[r6,#4]
 movs r0,#1
 str r0,[r6,#12]
vh_parse:
 bl 0x{NEXT:x}
 cmp r0,#0
 ble vh_exit
 str r0,[r6,#24]
 bl 0x{NEXT:x}
 mov r3,r0
 str r3,[r6,#28]
 bl 0x{NEXT:x}
 str r0,[r6,#32]
 bl 0x{NEXT:x}
 str r0,[r6,#36]
 bl 0x{NEXT:x}
 cmp r0,#1
 blo vh_exit
 movw r1,#3600
 cmp r0,r1
 bhi vh_exit
 lsls r0,r0,#16
 ldrh r3,[r6,#28]
 orrs r0,r0,r3
 str r0,[r6,#28]
 ldr r0,[r6,#12]
 adds r0,#1
 bne vh_seq
 movs r0,#1
vh_seq:
 str r0,[r6,#12]
 mov r0,r6
 bl 0x{seal:x}
 mov r0,r6
 bl 0x{valid:x}
 cbz r0,vh_exit
 mov r0,r7
 mov r1,r6
 bl 0x{persist:x}
 cbz r0,vh_exit
 ldr r0,[r6,#28]
 tst r0,#1
 bne vh_ack
 movs r0,#0
 str r0,[r7,#{VGSTATUS}]
 str r0,[r7,#{VGSINCE}]
vh_ack:
 movs r0,#95
 ldr r1,[r6,#24]
 movs r2,#0
 bl 0x{ACK:x}
vh_exit:
 pop {{r3,r5,r6,r7}}
 b.w 0x{PARSER_EXIT:x}
"""

def scheduler_handler_src(seal_addr,persist_addr):
    hs=section('handler','task')
    hs=hs.replace('handler:\n bl get_data\n cbz r0,h_exit\n mov r7,r0\n',f'''handler:\n push {{r3,r5,r6,r7}}\n movw r0,#{DATA_PTR&0xffff}\n movt r0,#{DATA_PTR>>16}\n ldr r7,[r0]\n cbz r7,h_exit\n''',1)
    hs=hs.replace(' cmp r0,#40\n',' cmp r0,#38\n',1)
    hs=hs.replace(f'add.w r6,r7,#{STAGE+24}',f'add.w r6,r7,#{STAGE+40}')
    hs=hs.replace(' mov r5,r0\n','',1)
    hs=hs.replace(''' cmp r5,#2\n bne hi_not_week\n cmp r0,#0\n beq h_exit\n b hi_days_ok\nhi_not_week:\n cmp r0,#0\n bne h_exit\nhi_days_ok:\n strb r0,[r6,#2]\n''',' strb r0,[r6,#2]\n')
    hs=hs.replace(''' cmp r5,#0\n bne hi_repeat_date\n cmp r0,r1\n beq h_exit\n b hi_date_ok\nhi_repeat_date:\n cmp r0,r1\n bne h_exit\nhi_date_ok:\n strh r0,[r6,#4]\n''',' strh r0,[r6,#4]\n')
    a=hs.index('h_commit:'); b=hs.index('h_status:')
    commit=f'''h_commit:\n bl 0x{NEXT:x}\n ldr r1,[r7,#44]\n cmp r0,r1\n bne h_exit\n ldr r2,[r7,#48]\n ldr r3,[r7,#52]\n cmp r2,r3\n bne h_exit\n add.w r6,r7,#{STAGE}\n movw r3,#{MAGIC&0xffff}\n movt r3,#{MAGIC>>16}\n str r3,[r6]\n movs r3,#{FORMAT}\n str r3,[r6,#4]\n str r0,[r6,#8]\n ldr r3,[r7,#40]\n cbz r3,hc_new\n add.w r5,r7,#{ACTIVE}\n ldr r3,[r5,#12]\n adds r3,#1\n ldr r12,[r5,#24]\n str r12,[r6,#24]\n ldr r12,[r5,#28]\n str r12,[r6,#28]\n ldr r12,[r5,#32]\n str r12,[r6,#32]\n ldr r12,[r5,#36]\n str r12,[r6,#36]\n b hc_seq\nhc_new:\n movs r3,#1\n movs r5,#0\n str r5,[r6,#24]\n str r5,[r6,#28]\n str r5,[r6,#32]\n str r5,[r6,#36]\nhc_seq:\n str r3,[r6,#12]\n str r2,[r6,#16]\n mov r0,r6\n bl 0x{seal_addr:x}\n mov r0,r7\n mov r1,r6\n bl 0x{persist_addr:x}\n cbz r0,h_exit\n movs r0,#93\n ldr r1,[r7,#44]\n ldr r2,[r7,#48]\n ldr r3,[r7,#28]\n lsls r3,r3,#8\n orrs r2,r2,r3\n movw r3,#0x200\n orrs r2,r2,r3\n bl 0x{ACK:x}\n b h_exit\n'''
    hs=hs[:a]+commit+hs[b:]
    hs=hs.replace('bl next_int',f'bl 0x{NEXT:x}').replace('bl ack',f'bl 0x{ACK:x}')
    hs=hs.replace(f'h_exit:\n b 0x{PARSER_EXIT:x}',f'h_exit:\n pop {{r3,r5,r6,r7}}\n b 0x{PARSER_EXIT:x}')
    return hs

def boot_src(valid_addr):
    return f"""task:
 push {{r4,r5,r6,r7,lr}}
 sub sp,#4
 sub.w sp,sp,#1152
 mov r4,sp
 mov r0,r4
 movs r1,#0
 movw r2,#1152
 bl 0x{MEMSET:x}
 mov r2,r4
 movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 str r2,[r0]
 mov.w r0,#-1
 str r0,[r4,#32]
 add.w r5,r4,#{ACTIVE}
 mov.w r0,#{A}
 mov r1,r5
 movw r2,#{IMAGE}
 bl 0x{FLASH_READ:x}
 mov r0,r5
 bl 0x{valid_addr:x}
 cbz r0,t_b
 movs r0,#1
 str r0,[r4,#40]
 mov.w r0,#{A}
 str r0,[r4,#36]
t_b:
 add.w r6,r4,#{SCRATCH}
 mov.w r0,#{B}
 mov r1,r6
 movw r2,#{IMAGE}
 bl 0x{FLASH_READ:x}
 mov r0,r6
 bl 0x{valid_addr:x}
 cbz r0,t_go
 ldr r0,[r4,#40]
 cbz r0,t_use_b
 ldr r0,[r6,#12]
 ldr r1,[r5,#12]
 cmp r0,r1
 bls t_go
t_use_b:
 mov r0,r5
 mov r1,r6
 movw r2,#{IMAGE}
 bl 0x{MEMCPY:x}
 movs r0,#1
 str r0,[r4,#40]
 mov.w r0,#{B}
 str r0,[r4,#36]
t_go:
 b.w 0x{MAIN:x}
"""

def build_segments():
    relay_addr=TEXT
    relay_bin=assemble(relay_src(),relay_addr)

    # Measure boot/gate so the seal helper can occupy remaining task bytes.
    boot_probe=assemble(boot_src(MAIN),TASK)
    gate_addr=TASK+len(boot_probe)
    gate_bin=assemble(gate_src(),gate_addr)
    seal_addr=gate_addr+len(gate_bin)
    seal_bin=assemble(seal_src(),seal_addr)

    persist_addr=LEGACY
    # persist size is branch-target invariant; hook follows it.
    persist_probe=assemble(persist_src(MAIN),persist_addr)
    hook_addr=persist_addr+len(persist_probe)
    hook_bin=assemble(hook_eval_src(),hook_addr)
    policy_addr=hook_addr+len(hook_bin)
    policy_bin=assemble(valid_policy_src(),policy_addr)

    runtime='t_loop:'+MONO.split('t_loop:',1)[1].split('t_dead:',1)[0]
    runtime=runtime.replace(' cbz r0,t_sleep',' cmp r0,#0\n beq.w t_sleep').replace('bl relay',f'bl 0x{relay_addr:x}').replace('adds r5,#24','adds r5,#40')
    runtime_bin=assemble(runtime,MAIN)
    handler_addr=MAIN+len(runtime_bin)
    handler_bin=assemble(scheduler_handler_src(seal_addr,persist_addr),handler_addr)
    main_no_valid=runtime_bin+handler_bin
    valid_addr=MAIN+len(main_no_valid)
    valid_bin=assemble(validate_src(policy_addr),valid_addr)
    main=main_no_valid+valid_bin

    persist_bin=assemble(persist_src(valid_addr),persist_addr)
    assert len(persist_bin)==len(persist_probe)
    hook_addr=persist_addr+len(persist_bin)
    hook_bin=assemble(hook_eval_src(),hook_addr)
    policy_addr2=hook_addr+len(hook_bin); assert policy_addr2==policy_addr
    policy_bin=assemble(valid_policy_src(),policy_addr)
    legacy=persist_bin+hook_bin+policy_bin

    boot_bin=assemble(boot_src(valid_addr),TASK)
    assert len(boot_bin)==len(boot_probe)
    gate_addr=TASK+len(boot_bin)
    gate_bin=assemble(gate_src(),gate_addr)
    seal_addr2=gate_addr+len(gate_bin)
    assert seal_addr2==seal_addr
    seal_bin=assemble(seal_src(),seal_addr)
    task=boot_bin+gate_bin+seal_bin

    ni=section('next_int','validate').replace('next_int:','')
    mem=assemble(f'b.w 0x{handler_addr:x}\nnext_int:\n'+ni,MEM)
    ack_src=section('ack','handler')
    ack_src,n_lo=re.subn(r'movw r2,\s*#(?:60976|0xee30)\b',f'movw r2, #{FMT & 0xffff}',ack_src,count=1)
    ack_src,n_hi=re.subn(r'movt r2,\s*#(?:4097|0x1001)\b',f'movt r2, #{FMT >> 16}',ack_src,count=1)
    if (n_lo,n_hi)!=(1,1):
        raise AssertionError(f'ACK format pointer rewrite failed: low={n_lo} high={n_hi}')
    ack=assemble(ack_src,ACK)
    vg=assemble(vguard_handler_src(seal_addr,persist_addr,valid_addr),TEST)
    hook_patch=assemble(f'b.w 0x{hook_addr:x}',HOOK)
    gate_patch=assemble(f'b.w 0x{gate_addr:x}',RELAY_API)
    wrapper_patch=assemble('bx lr',WRAPPER)
    return {
        'task':(TASK,task),'memory':(MEM,mem),'main':(MAIN,main),'ack':(ACK,ack),
        'vguard':(TEST,vg),'legacy':(LEGACY,legacy),'relay':(TEXT,relay_bin),
        'hook':(HOOK,hook_patch),'relay_patch':(RELAY_API,gate_patch),'wrapper_patch':(WRAPPER,wrapper_patch),
        'handler_addr':handler_addr,'relay_addr':relay_addr,'gate_addr':gate_addr,
        'valid_addr':valid_addr,'policy_addr':policy_addr,'persist_addr':persist_addr,'hook_addr':hook_addr,'seal_addr':seal_addr,
    }

if __name__=='__main__':
    s=build_segments()
    for k in ('task','memory','main','ack','vguard','legacy','relay','hook','relay_patch','wrapper_patch'):
        a,b=s[k]; print(k,len(b),hex(a),hex(a+len(b)))
    print({k:hex(s[k]) for k in ('handler_addr','relay_addr','gate_addr','valid_addr','policy_addr','persist_addr','hook_addr','seal_addr')})
    assert TASK+len(s['task'][1])<=TASK_END
    assert MEM+len(s['memory'][1])<=MEM_END
    assert MAIN+len(s['main'][1])<=MAIN_END
    assert ACK+len(s['ack'][1])<=ACK_END
    assert TEST+len(s['vguard'][1])<=TEST_END
    assert LEGACY+len(s['legacy'][1])<=LEGACY_END
    assert TEXT+len(s['relay'][1])<=TEXT_END
    assert len(s['hook'][1])==4 and len(s['relay_patch'][1])==4 and len(s['wrapper_patch'][1])==2
    print('layout=PASS')
