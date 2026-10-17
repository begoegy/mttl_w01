#!/usr/bin/env python3
"""Timer-backed SFN Full Local Engine 1.0.152 machine-code layout.

Host-only research module. It deliberately does NOT run the local engine inside
check_ota_ssl_task. Instead it disables that conditional vendor task creator and
uses the physically-proven one-shot software-timer primitives used by SFN Local
Queue 1.0.81.
"""
from __future__ import annotations
import re
import tools.sfn_full_local_engine_segments as old
from tools.sfn_scheduler_segments import assemble
from tools.sfn_scheduler_payload_probe import asm as MONO

TASK=0x10009914; CREATOR=0x10009A12; CREATOR_END=0x10009A36
WRAPPER=0x10023686; WRAPPER_END=0x100236B8
MEM=0x1001600C; NEXT=0x10016010; MEM_END=0x1001602E
TEST=0x1001606A; TEST_END=0x10016130
MAIN=0x1001EB6C; MAIN_END=0x1001EE54
ACK=0x1001EE5C; ACK_END=0x1001EEA8
LEGACY=0x100233D6; LEGACY_END=0x10023634
TEXT=0x100166DC; TEXT_END=0x10016714
HOOK=0x10012918; HOOK_RETURN=0x1001291C
RELAY_API=0x1001AB42; RELAY_RETURN=0x1001AB46
DATA_PTR=0x1001EE98; TIMER_NAME=0x1001669C
ACTIVE=80; STAGE=424; IMAGE=344; MAX_ITEMS=38
VGSTATE=768; VGSTATUS=768; VGSINCE=772; STATE_SIZE=776
STATE_MAGIC=0x5346
A=0xFC000; B=0xFD000; MAGIC=0x454E4653; FORMAT=2
FLASH_WRITE=0x100157E6; FLASH_READ=0x1001587A; TICK=0x100103F8
MEMSET=0x1000A9F0; MEMCPY=0x1000AA08
ALLOC=0x100120A0; TIMER_CREATE=0x1001227E; TIMER_CMD=0x100122B8
ENTER=0x1001094C; EXITC=0x10010964
PARSER_EXIT=0x100165BA; FMT=0x100166A4

# Reused generators read their module globals at call time.
old.VGSTATE=VGSTATE; old.VGSTATUS=VGSTATUS; old.VGSINCE=VGSINCE
old.ACTIVE=ACTIVE; old.STAGE=STAGE; old.IMAGE=IMAGE; old.MAX_ITEMS=MAX_ITEMS
old.DATA_PTR=DATA_PTR; old.A=A; old.B=B; old.MAGIC=MAGIC; old.FORMAT=FORMAT
old.FLASH_WRITE=FLASH_WRITE; old.FLASH_READ=FLASH_READ; old.TICK=TICK
old.MEMSET=MEMSET; old.MEMCPY=MEMCPY; old.PARSER_EXIT=PARSER_EXIT; old.FMT=FMT
old.RELAY_API=RELAY_API; old.RELAY_RETURN=RELAY_RETURN; old.HOOK_RETURN=HOOK_RETURN
old.NEXT=NEXT; old.ACK=ACK


def Aasm(src:str, addr:int)->bytes:
    return assemble(src,addr)


def persist_src(valid_addr:int)->str:
    # STAGE is reused for flash read-back verification. ACTIVE stays untouched until
    # the freshly-written sector validates, so A/B commit remains atomic without SCRATCH.
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
 mov r0,r5
 mov r1,r6
 movw r2,#{IMAGE}
 bl 0x{FLASH_READ:x}
 mov r0,r6
 bl 0x{valid_addr:x}
 cbz r0,p_fail
 add.w r0,r4,#{ACTIVE}
 mov r1,r6
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


def get_state_src(valid_addr:int)->str:
    # DATA_PTR=1 is an initialization sentinel. A concurrent caller fails closed
    # rather than allocating a second engine instance.
    return f"""get_state:
 push {{r4,r5,r6,r7,lr}}
 sub sp,#4
 movw r4,#{DATA_PTR&0xffff}
 movt r4,#{DATA_PTR>>16}
 ldr r0,[r4]
 cmp r0,#1
 beq gs_busy
 cmp r0,#0
 bne gs_existing
 bl 0x{ENTER:x}
 ldr r5,[r4]
 cmp r5,#0
 bne gs_unlock_existing
 movs r1,#1
 str r1,[r4]
 bl 0x{EXITC:x}
 movw r0,#{STATE_SIZE}
 bl 0x{ALLOC:x}
 cmp r0,#0
 beq gs_fail
 mov r5,r0
 mov r0,r5
 movs r1,#0
 movw r2,#{STATE_SIZE}
 bl 0x{MEMSET:x}
 movw r0,#{STATE_MAGIC&0xffff}
 str r0,[r5,#72]
 mov.w r0,#-1
 str r0,[r5,#32]
 add.w r6,r5,#{ACTIVE}
 mov.w r0,#{A}
 mov r1,r6
 movw r2,#{IMAGE}
 bl 0x{FLASH_READ:x}
 mov r0,r6
 bl 0x{valid_addr:x}
 cbz r0,gs_b
 movs r0,#1
 str r0,[r5,#40]
 mov.w r0,#{A}
 str r0,[r5,#36]
gs_b:
 add.w r7,r5,#{STAGE}
 mov.w r0,#{B}
 mov r1,r7
 movw r2,#{IMAGE}
 bl 0x{FLASH_READ:x}
 mov r0,r7
 bl 0x{valid_addr:x}
 cbz r0,gs_publish
 ldr r0,[r5,#40]
 cbz r0,gs_use_b
 ldr r0,[r7,#12]
 ldr r1,[r6,#12]
 cmp r0,r1
 bls gs_publish
gs_use_b:
 mov r0,r6
 mov r1,r7
 movw r2,#{IMAGE}
 bl 0x{MEMCPY:x}
 movs r0,#1
 str r0,[r5,#40]
 mov.w r0,#{B}
 str r0,[r5,#36]
gs_publish:
 bl 0x{ENTER:x}
 str r5,[r4]
 bl 0x{EXITC:x}
 mov r0,r5
 b gs_done
gs_fail:
 bl 0x{ENTER:x}
 movs r0,#0
 str r0,[r4]
 bl 0x{EXITC:x}
 movs r0,#0
 b gs_done
gs_unlock_existing:
 bl 0x{EXITC:x}
 mov r0,r5
 cmp r0,#1
 beq gs_busy
gs_existing:
 ldr r1,[r0,#72]
 movw r2,#{STATE_MAGIC&0xffff}
 cmp r1,r2
 bne gs_busy
 b gs_done
gs_busy:
 movs r0,#0
gs_done:
 add sp,#4
 pop {{r4,r5,r6,r7,pc}}
"""


def timer_arm_src()->str:
    # Caller provides timer handle in r0. This is the exact control-call shape used
    # by the physically proven Local Queue timer engine.
    return f"""timer_arm:
 push {{r4,lr}}
 sub sp,#8
 movs r3,#0
 str r3,[sp]
 bl 0x{TIMER_CMD:x}
 cmp r0,#0
 it ne
 movne r0,#1
 add sp,#8
 pop {{r4,pc}}
"""


def timer_create_src(callback_addr:int)->str:
    return f"""timer_create:
 push {{r4,lr}}
 mov r4,r0
 sub sp,#8
 movw r0,#{(callback_addr|1)&0xffff}
 movt r0,#{(callback_addr|1)>>16}
 str r0,[sp]
 movw r0,#{TIMER_NAME&0xffff}
 movt r0,#{TIMER_NAME>>16}
 movs r1,#1
 movs r2,#0
 mov r3,r4
 bl 0x{TIMER_CREATE:x}
 add sp,#8
 cbz r0,tc_create_done
 str r0,[r4,#68]
tc_create_done:
 pop {{r4,pc}}
"""


def timer_get_src(timer_create_addr:int)->str:
    # r7 is the handler state pointer. Tail-branch to timer_create preserves caller LR.
    return f"""timer_get:
 ldr r0,[r7,#68]
 cbnz r0,tg_done
 mov r0,r7
 b.w 0x{timer_create_addr:x}
tg_done:
 bx lr
"""


def callback_src(timer_arm_addr:int)->str:
    runtime='t_loop:'+MONO.split('t_loop:',1)[1].split('t_dead:',1)[0]
    runtime=runtime.replace(
        't_loop:',
        'timer_callback:\n push {r4,r5,r6,r7,lr}\n sub sp,#4\n ldr r4,[r0,#0x20]\n cmp r4,#0\n beq tc_done',1)
    runtime=runtime.replace(' cbz r0,t_sleep',' cmp r0,#0\n beq tc_rearm').replace(' beq t_sleep',' beq tc_rearm')
    runtime=runtime.replace('adds r5,#24','adds r5,#40')
    runtime=runtime.replace('bl 0x10010408',f'bl 0x{TICK:x}')
    runtime=runtime.replace(' cmp r7,#0\n beq tc_rearm',' cbz r7,tc_rearm',1)
    runtime=runtime.replace(' cmp r0,#4\n bhi t_next',' cbz r0,t_next\n cmp r0,#4\n bhi t_next',1)
    runtime=runtime.replace(' cmp r2,#0\n beq t_once',' cbz r2,t_once',1)
    fire="t_fire:\n bl relay\n"
    direct=f"""t_fire:
 mov r2,r0
 movs r0,#12
 subs r0,r0,r1
 mov r1,r2
 bl 0x{RELAY_API:x}
"""
    if fire not in runtime: raise AssertionError('scheduler relay block changed')
    runtime=runtime.replace(fire,direct,1)
    tail="t_sleep:\n movw r0,#1000\n bl 0x100101c6\n b t_loop\n"
    if tail not in runtime: raise AssertionError('scheduler loop tail changed')
    runtime=runtime.replace(tail,f"""tc_rearm:
 ldr r0,[r4,#68]
 cbz r0,tc_done
 movs r1,#4
 movw r2,#1000
 bl 0x{timer_arm_addr:x}
tc_done:
 add sp,#4
 pop {{r4,r5,r6,r7,pc}}
""")
    return runtime


def gate_compact_src()->str:
    # r2 keeps the original relay command, so allowed ON and all non-ON commands
    # share one stock-dispatch tail. VGSTATUS: bit0 trip, bits16..19 protected mask.
    return f"""relay_gate:
 mov r2,r0
 cmp r2,#11
 bne rg_dispatch
 movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 ldr r0,[r0]
 cmp r0,#1
 bls rg_dispatch
 ldr r3,[r0,#{VGSTATUS}]
 lsrs r0,r3,#16
 cbz r0,rg_dispatch
 subs r3,r1,#1
 movs r2,#1
 lsl r2,r2,r3
 tst r0,r2
 beq rg_on_allow
 movs r0,#0
 bx lr
rg_on_allow:
 movs r2,#11
rg_dispatch:
 mov r0,r1
 b.w 0x{RELAY_RETURN:x}
"""


def hook_eval_src(get_state_addr:int)->str:
    src=old.hook_eval_src()
    old_lookup=f""" movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 ldr r5,[r0]
 cbz r5,vg_done
"""
    new_lookup=f""" bl 0x{get_state_addr:x}
 mov r5,r0
 cbz r5,vg_done
"""
    if old_lookup not in src: raise AssertionError('voltage state lookup changed')
    src=src.replace(old_lookup,new_lookup,1)
    old_trip=f""" lsls r4,r4,#8
 orr r4,r4,#1
 str r4,[r5,#{VGSTATUS}]
"""
    new_trip=f""" lsls r4,r4,#8
 orr r4,r4,r7,lsl #16
 orr r4,r4,#1
 str r4,[r5,#{VGSTATUS}]
"""
    if old_trip not in src: raise AssertionError('voltage trip status changed')
    src=src.replace(old_trip,new_trip,1)
    src=src.replace(' cbz r5,vg_done\n',' cmp r5,#0\n beq vg_done\n',1)
    src=src.replace(' cbz r0,vg_done\n',' cmp r0,#0\n beq vg_done\n',1)
    return src


def vguard_handler_src(seal_addr:int,persist_addr:int,valid_addr:int,get_state_addr:int)->str:
    src=old.vguard_handler_src(seal_addr,persist_addr,valid_addr)
    old_lookup=f""" movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 ldr r7,[r0]
 cbz r7,vh_exit
"""
    new_lookup=f""" bl 0x{get_state_addr:x}
 mov r7,r0
 cbz r7,vh_exit
"""
    if old_lookup not in src: raise AssertionError('vguard state lookup changed')
    src=src.replace(old_lookup,new_lookup,1)
    src=src.replace(' cbz r7,vh_exit\n',' cmp r7,#0\n beq vh_exit\n',1)
    return src


def scheduler_handler_src(seal_addr:int,persist_addr:int,get_state_addr:int,timer_get_addr:int,timer_arm_addr:int)->str:
    src=old.scheduler_handler_src(seal_addr,persist_addr)
    old_head=f"""handler:
 push {{r3,r5,r6,r7}}
 movw r0,#{DATA_PTR&0xffff}
 movt r0,#{DATA_PTR>>16}
 ldr r7,[r0]
 cbz r7,h_exit
"""
    new_head=f"""handler:
 push {{r3,r5,r6,r7}}
 bl 0x{get_state_addr:x}
 mov r7,r0
 cbz r7,h_exit
"""
    if old_head not in src: raise AssertionError('scheduler state lookup changed')
    src=src.replace(old_head,new_head,1)
    src=src.replace(' cbz r7,h_exit\n',' cmp r7,#0\n beq h_exit\n',1)
    src=src.replace('bl 0x10010408',f'bl 0x{TICK:x}')
    # 1.0.152 firmware accepts physical outlets 1..4. Server-side capability
    # expansion maps a logical "all" schedule into four physical items.
    old_outlet=' cmp r0,#4\n bhi h_exit\n strb r0,[r6]\n'
    new_outlet=' cmp r0,#4\n bhi h_exit\n strb r0,[r6]\n'
    if old_outlet not in src: raise AssertionError('schedule outlet guard changed')
    src=src.replace(old_outlet,new_outlet,1)
    old_clock=f""" movs r0,#1
 str r0,[r7,#28]
 movs r0,#90
 ldr r1,[r7]
 movs r2,#1
 bl 0x{ACK:x}
 b h_exit
"""
    new_clock=f""" bl 0x{timer_get_addr:x}
 cbz r0,hc_timer_result
 movs r1,#4
 movw r2,#1000
 bl 0x{timer_arm_addr:x}
hc_timer_result:
 str r0,[r7,#28]
 mov r2,r0
 movs r0,#90
 ldr r1,[r7]
 bl 0x{ACK:x}
 b h_exit
"""
    if old_clock not in src: raise AssertionError('clock ACK block changed')
    return src.replace(old_clock,new_clock,1)


def build_segments():
    # Probe get_state only to determine the owned task-body split.
    get_probe=Aasm(get_state_src(MAIN+0x280),TASK)
    timer_arm_addr=TASK+len(get_probe)
    timer_arm_bin=Aasm(timer_arm_src(),timer_arm_addr)
    if timer_arm_addr+len(timer_arm_bin)>CREATOR:
        raise AssertionError('get_state/timer_arm exceed disabled task body')
    seal_addr=CREATOR+2
    seal_bin=Aasm(old.seal_src(),seal_addr)
    if seal_addr+len(seal_bin)>CREATOR_END:
        raise AssertionError('seal exceeds disabled creator body')

    callback_addr=MAIN
    callback_bin=Aasm(callback_src(timer_arm_addr),callback_addr)

    timer_create_addr=WRAPPER
    timer_create_bin=Aasm(timer_create_src(callback_addr),timer_create_addr)
    if timer_create_addr+len(timer_create_bin)>WRAPPER_END:
        raise AssertionError('timer_create exceeds isolated wrapper function')
    timer_get_addr=DATA_PTR+4
    timer_get_bin=Aasm(timer_get_src(timer_create_addr),timer_get_addr)
    if timer_get_addr+len(timer_get_bin)>ACK_END:
        raise AssertionError('timer_get exceeds ACK tail')

    persist_addr=LEGACY
    persist_probe=Aasm(persist_src(MAIN+0x280),persist_addr)
    hook_addr=persist_addr+len(persist_probe)
    hook_probe=Aasm(hook_eval_src(TASK),hook_addr)
    policy_addr=hook_addr+len(hook_probe)
    policy_bin=Aasm(old.valid_policy_src(),policy_addr)

    handler_addr=callback_addr+len(callback_bin)
    handler_probe=Aasm(scheduler_handler_src(seal_addr,persist_addr,TASK,timer_get_addr,timer_arm_addr),handler_addr)
    valid_addr=handler_addr+len(handler_probe)
    valid_bin=Aasm(old.validate_src(policy_addr),valid_addr)
    handler_bin=Aasm(scheduler_handler_src(seal_addr,persist_addr,TASK,timer_get_addr,timer_arm_addr),handler_addr)
    main=callback_bin+handler_bin+valid_bin

    get_state_bin=Aasm(get_state_src(valid_addr),TASK)
    if len(get_state_bin)!=len(get_probe): raise AssertionError('get_state size drift')
    if TASK+len(get_state_bin)!=timer_arm_addr: raise AssertionError('timer_arm moved')
    timer_arm_bin=Aasm(timer_arm_src(),timer_arm_addr)
    task=get_state_bin+timer_arm_bin

    persist_bin=Aasm(persist_src(valid_addr),persist_addr)
    if len(persist_bin)!=len(persist_probe): raise AssertionError('persist size drift')
    if persist_addr+len(persist_bin)!=hook_addr: raise AssertionError('hook moved')
    hook_bin=Aasm(hook_eval_src(TASK),hook_addr)
    if hook_addr+len(hook_bin)!=policy_addr: raise AssertionError('policy moved')
    policy_bin=Aasm(old.valid_policy_src(),policy_addr)
    legacy=persist_bin+hook_bin+policy_bin

    ni=old.section('next_int','validate').replace('next_int:','')
    mem=Aasm(f'b.w 0x{handler_addr:x}\nnext_int:\n'+ni,MEM)
    ack_src=old.section('ack','handler')
    ack_src,n_lo=re.subn(r'movw r2,\s*#(?:60976|0xee30)\b',f'movw r2, #{FMT & 0xffff}',ack_src,count=1)
    ack_src,n_hi=re.subn(r'movt r2,\s*#(?:4097|0x1001)\b',f'movt r2, #{FMT >> 16}',ack_src,count=1)
    if (n_lo,n_hi)!=(1,1): raise AssertionError('ACK format pointer rewrite failed')
    ack=Aasm(ack_src,ACK)
    vg=Aasm(vguard_handler_src(seal_addr,persist_addr,valid_addr,TASK),TEST)
    gate=Aasm(gate_compact_src(),TEXT)
    hook_patch=Aasm(f'b.w 0x{hook_addr:x}',HOOK)
    gate_patch=Aasm(f'b.w 0x{TEXT:x}',RELAY_API)
    creator_patch=Aasm('bx lr',CREATOR)

    return {
        'task':(TASK,task), 'creator_patch':(CREATOR,creator_patch), 'creator_helper':(seal_addr,seal_bin),
        'memory':(MEM,mem), 'main':(MAIN,main), 'ack':(ACK,ack), 'vguard':(TEST,vg),
        'legacy':(LEGACY,legacy), 'gate':(TEXT,gate), 'timer_create':(timer_create_addr,timer_create_bin),
        'timer_get':(timer_get_addr,timer_get_bin), 'hook':(HOOK,hook_patch), 'relay_patch':(RELAY_API,gate_patch),
        'get_state_addr':TASK, 'timer_arm_addr':timer_arm_addr, 'seal_addr':seal_addr,
        'callback_addr':callback_addr, 'timer_create_addr':timer_create_addr, 'timer_get_addr':timer_get_addr, 'handler_addr':handler_addr,
        'gate_addr':TEXT, 'persist_addr':persist_addr, 'hook_addr':hook_addr,
        'policy_addr':policy_addr, 'valid_addr':valid_addr,
    }


if __name__=='__main__':
    s=build_segments()
    for k in ('task','creator_patch','creator_helper','memory','main','ack','vguard','legacy','gate','timer_create','timer_get','hook','relay_patch'):
        a,b=s[k]; print(k,len(b),hex(a),hex(a+len(b)))
    print({k:hex(s[k]) for k in ('get_state_addr','timer_arm_addr','seal_addr','callback_addr','timer_create_addr','timer_get_addr','handler_addr','gate_addr','persist_addr','hook_addr','policy_addr','valid_addr')})
    assert TASK+len(s['task'][1])<=CREATOR
    assert len(s['creator_patch'][1])==2
    assert s['creator_helper'][0]>=CREATOR+2 and s['creator_helper'][0]+len(s['creator_helper'][1])<=CREATOR_END
    assert MEM+len(s['memory'][1])<=MEM_END
    assert MAIN+len(s['main'][1])<=MAIN_END
    assert ACK+len(s['ack'][1])<=ACK_END
    assert TEST+len(s['vguard'][1])<=TEST_END
    assert LEGACY+len(s['legacy'][1])<=LEGACY_END
    assert TEXT+len(s['gate'][1])<=TEXT_END
    assert WRAPPER+len(s['timer_create'][1])<=WRAPPER_END
    assert len(s['hook'][1])==4 and len(s['relay_patch'][1])==4
    print('timer_layout=PASS')
