#!/usr/bin/env python3
"""Host model for the 1.0.151 unified local engine. No device I/O."""
from __future__ import annotations
from dataclasses import dataclass, replace
import struct, zlib

MAGIC=0x454E4653  # SFNE
FORMAT=1
SECTOR_A=0xFC000
SECTOR_B=0xFD000
SECTOR_SIZE=4096
MAX_SCHEDULES=40
NONE=0xFFFF
HEADER=struct.Struct('<IIIIIIHHHHII')
# magic, format, seq, sched_gen, voltage_rev, flags, low10, high10, hyst10, hold10, recovery, protected_mask
ITEM=struct.Struct('<BBBBHHI')
# slot,outlet,repeat,days,date_days,minute,reserved/action bit
CRC=struct.Struct('<I')

@dataclass(frozen=True)
class VoltagePolicy:
    revision:int=0; enabled:bool=False; low10:int=1800; high10:int=2500; hyst10:int=50; hold10:int=20
    recovery:int=30; protected_mask:int=15; auto_restore:bool=False
    def validate(self):
        if not 0<=self.revision<=0x7fffffff: raise ValueError('revision')
        if not 1000<=self.low10<=2300 or not 2100<=self.high10<=3000 or self.low10>=self.high10-100: raise ValueError('threshold')
        if not 10<=self.hyst10<=200 or not 5<=self.hold10<=300 or not 1<=self.recovery<=3600: raise ValueError('timing')
        if not 1<=self.protected_mask<=15: raise ValueError('mask')
        return self

@dataclass(frozen=True)
class ScheduleItem:
    slot:int; outlet:int; repeat:int; days:int; date_days:int; minute:int; on:bool
    def validate(self):
        if not 0<=self.slot<MAX_SCHEDULES: raise ValueError('slot')
        if not 0<=self.outlet<=4: raise ValueError('outlet')
        if self.repeat not in (0,1,2): raise ValueError('repeat')
        if not 0<=self.days<=0x7f or not 0<=self.date_days<=0xffff or not 0<=self.minute<=1439: raise ValueError('fields')
        if self.repeat==2 and self.days==0: raise ValueError('weekly days')
        if self.repeat!=2 and self.days!=0: raise ValueError('days unexpected')
        if self.repeat==0 and self.date_days==NONE: raise ValueError('once date')
        if self.repeat!=0 and self.date_days!=NONE: raise ValueError('date unexpected')
        return self

@dataclass(frozen=True)
class EngineImage:
    seq:int=0; schedule_generation:int=0; voltage:VoltagePolicy=VoltagePolicy(); schedules:tuple[ScheduleItem,...]=()
    def validate(self):
        if not 0<=self.seq<=0x7fffffff or not 0<=self.schedule_generation<=0x7fffffff: raise ValueError('generation')
        self.voltage.validate()
        if len(self.schedules)>MAX_SCHEDULES: raise ValueError('count')
        if [x.slot for x in self.schedules] != list(range(len(self.schedules))): raise ValueError('slots')
        for x in self.schedules: x.validate()
        return self

def encode(image:EngineImage)->bytes:
    image.validate(); v=image.voltage
    flags=(1 if v.enabled else 0)|((1 if v.auto_restore else 0)<<1)|(len(image.schedules)<<8)
    body=bytearray(HEADER.pack(MAGIC,FORMAT,image.seq,image.schedule_generation,v.revision,flags,
                               v.low10,v.high10,v.hyst10,v.hold10,v.recovery,v.protected_mask))
    for x in image.schedules:
        body += ITEM.pack(x.slot,x.outlet,x.repeat,x.days,x.date_days,x.minute,1 if x.on else 0)
    body += CRC.pack(zlib.crc32(body)&0xffffffff)
    if len(body)>SECTOR_SIZE: raise ValueError('sector overflow')
    return bytes(body)

def decode(raw:bytes)->EngineImage|None:
    if len(raw)<HEADER.size+CRC.size: return None
    try: fields=HEADER.unpack_from(raw)
    except struct.error: return None
    magic,fmt,seq,gen,vrev,flags,low,high,hyst,hold,recovery,pmask=fields
    if magic!=MAGIC or fmt!=FORMAT: return None
    count=(flags>>8)&0xff
    n=HEADER.size+count*ITEM.size+CRC.size
    if count>MAX_SCHEDULES or n>len(raw): return None
    stored=CRC.unpack_from(raw,n-CRC.size)[0]
    if (zlib.crc32(raw[:n-CRC.size])&0xffffffff)!=stored: return None
    v=VoltagePolicy(vrev,bool(flags&1),low,high,hyst,hold,recovery,pmask,bool(flags&2))
    items=[]
    try:
        for i in range(count):
            slot,outlet,repeat,days,date_days,minute,action=ITEM.unpack_from(raw,HEADER.size+i*ITEM.size)
            if action not in (0,1): return None
            items.append(ScheduleItem(slot,outlet,repeat,days,date_days,minute,bool(action)))
        return EngineImage(seq,gen,v,tuple(items)).validate()
    except (ValueError,struct.error): return None

def choose(a:bytes,b:bytes):
    aa,bb=decode(a),decode(b)
    if aa is None: return (SECTOR_B,bb) if bb else (None,None)
    if bb is None: return SECTOR_A,aa
    return (SECTOR_A,aa) if aa.seq>=bb.seq else (SECTOR_B,bb)

def next_sector(current:int|None)->int: return SECTOR_B if current==SECTOR_A else SECTOR_A

def with_voltage(image:EngineImage, policy:VoltagePolicy)->EngineImage:
    policy.validate(); return replace(image,seq=image.seq+1,voltage=policy)

def with_schedules(image:EngineImage,generation:int,items:list[ScheduleItem])->EngineImage:
    return replace(image,seq=image.seq+1,schedule_generation=generation,schedules=tuple(items)).validate()

class EngineRuntime:
    """Safety/arbitration model. child_lock is vendor-owned 5-bit persistent mask."""
    def __init__(self,image:EngineImage,child_lock:int=0):
        self.image=image.validate(); self.child_lock=child_lock; self.tripped=False; self.relays={1:False,2:False,3:False,4:False}
    def set_trip(self,value:bool): self.tripped=bool(value)
    def button_allowed(self,channel:int)->bool:
        if not 0<=channel<=4: raise ValueError('button')
        return not bool(self.child_lock & (1<<channel))
    def relay(self,outlet:int,on:bool,source:str)->bool:
        if outlet not in (1,2,3,4): raise ValueError('outlet')
        protected=bool(self.image.voltage.protected_mask & (1<<(outlet-1)))
        if on and self.image.voltage.enabled and self.tripped and protected:
            return False
        self.relays[outlet]=bool(on); return True
