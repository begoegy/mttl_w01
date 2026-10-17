#!/usr/bin/env python3
"""Exact host storage/arbitration model for SFN Full Local Engine 1.0.151. No device I/O."""
from __future__ import annotations
from dataclasses import dataclass, replace
import struct
MAGIC=0x454E4653; FORMAT=2; SECTOR_A=0xFC000; SECTOR_B=0xFD000; IMAGE_SIZE=344; MAX_SCHEDULES=38; NONE=0xFFFF
# 10 words = 40 bytes. Exact 344-byte image leaves 304 bytes = 38 x 8-byte items.
HEADER=struct.Struct('<10I')
ITEM=struct.Struct('<4B2H')
@dataclass(frozen=True)
class VoltagePolicy:
    revision:int=0; enabled:bool=False; low10:int=1800; high10:int=2500; hyst10:int=50; hold10:int=20; recovery:int=30; protected_mask:int=15; auto_restore:bool=False
    def validate(self):
        if not 0<=self.revision<=0x7fffffff: raise ValueError('revision')
        if self.enabled:
            if not 1000<=self.low10<=2300 or not 2100<=self.high10<=3000 or self.low10>=self.high10-100: raise ValueError('threshold')
            if not 10<=self.hyst10<=200 or not 5<=self.hold10<=300 or not 1<=self.recovery<=3600: raise ValueError('timing')
            if not 1<=self.protected_mask<=15: raise ValueError('mask')
        return self
@dataclass(frozen=True)
class ScheduleItem:
    outlet:int; repeat:int; days:int; on:bool; date_days:int; minute:int
    def validate(self):
        if not 0<=self.outlet<=4 or self.repeat not in (0,1,2) or not 0<=self.days<=0x7f or not 0<=self.date_days<=0xffff or not 0<=self.minute<=1439: raise ValueError('item')
        if self.repeat==2 and self.days==0: raise ValueError('weekly')
        if self.repeat!=2 and self.days!=0: raise ValueError('days')
        if self.repeat==0 and self.date_days==NONE: raise ValueError('date')
        if self.repeat!=0 and self.date_days!=NONE: raise ValueError('date')
        return self
@dataclass(frozen=True)
class EngineImage:
    seq:int=0; schedule_generation:int=0; voltage:VoltagePolicy=VoltagePolicy(); schedules:tuple[ScheduleItem,...]=()
    def validate(self):
        if not 0<self.seq<=0x7fffffff or not 0<=self.schedule_generation<=0x7fffffff: raise ValueError('generation')
        if len(self.schedules)>MAX_SCHEDULES or (self.schedules and self.schedule_generation==0): raise ValueError('count/gen')
        self.voltage.validate(); [x.validate() for x in self.schedules]; return self

def _words(b): return struct.unpack('<'+'I'*(len(b)//4),b)
def encode(x:EngineImage)->bytes:
    x.validate(); v=x.voltage
    # flags word: enabled bit0, auto bit1, protected mask bits8..11, recovery seconds bits16..31
    vflags=(1 if v.enabled else 0)|((1 if v.auto_restore else 0)<<1)|(v.protected_mask<<8)|(v.recovery<<16)
    thresholds=v.low10|(v.high10<<16); timing=v.hyst10|(v.hold10<<16)
    raw=bytearray(HEADER.pack(MAGIC,FORMAT,x.schedule_generation,x.seq,len(x.schedules),0,v.revision,vflags,thresholds,timing))
    for it in x.schedules: raw += ITEM.pack(it.outlet,it.repeat,it.days,1 if it.on else 0,it.date_days,it.minute)
    total=sum(_words(raw))&0xffffffff; struct.pack_into('<I',raw,20,(-total)&0xffffffff)
    if len(raw)>IMAGE_SIZE: raise ValueError('overflow')
    return bytes(raw)+b'\xff'*(IMAGE_SIZE-len(raw))
def decode(raw:bytes):
    if len(raw)<IMAGE_SIZE: return None
    try: magic,fmt,gen,seq,count,chk,vrev,vflags,thresholds,timing=HEADER.unpack_from(raw)
    except struct.error: return None
    if magic!=MAGIC or fmt!=FORMAT or not 0<seq<=0x7fffffff or count>MAX_SCHEDULES or (count and gen==0): return None
    used=HEADER.size+count*ITEM.size
    if sum(_words(raw[:used]))&0xffffffff: return None
    enabled=bool(vflags&1); auto=bool(vflags&2); mask=(vflags>>8)&15; recovery=(vflags>>16)&0xffff
    v=VoltagePolicy(vrev,enabled,thresholds&0xffff,(thresholds>>16)&0xffff,timing&0xffff,(timing>>16)&0xffff,recovery,mask,auto)
    items=[]
    try:
        v.validate()
        for i in range(count):
            outlet,repeat,days,on,date_days,minute=ITEM.unpack_from(raw,HEADER.size+i*ITEM.size)
            if on not in (0,1): return None
            items.append(ScheduleItem(outlet,repeat,days,bool(on),date_days,minute).validate())
        return EngineImage(seq,gen,v,tuple(items)).validate()
    except ValueError: return None
def choose(a,b):
    aa,bb=decode(a),decode(b)
    if aa is None:return (SECTOR_B,bb) if bb else (None,None)
    if bb is None:return SECTOR_A,aa
    return (SECTOR_A,aa) if aa.seq>=bb.seq else (SECTOR_B,bb)
def with_voltage(x,p): p.validate(); return replace(x,seq=x.seq+1,voltage=p)
def with_schedules(x,g,items): return replace(x,seq=x.seq+1,schedule_generation=g,schedules=tuple(items)).validate()
class EngineRuntime:
    def __init__(self,image,child_lock=0): self.image=image.validate(); self.child_lock=child_lock; self.tripped=False; self.relays={1:False,2:False,3:False,4:False}
    def set_trip(self,v): self.tripped=bool(v)
    def button_allowed(self,ch):
        if not 0<=ch<=4: raise ValueError('button')
        return not bool(self.child_lock&(1<<ch))
    def relay(self,outlet,on,source):
        protected=bool(self.image.voltage.protected_mask&(1<<(outlet-1)))
        if on and self.image.voltage.enabled and self.tripped and protected:return False
        self.relays[outlet]=bool(on);return True
