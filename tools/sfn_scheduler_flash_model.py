#!/usr/bin/env python3
"""Pure model for SFN 1.0.149 durable event table and local-time execution."""
from __future__ import annotations
import dataclasses, struct
from typing import Iterable, Optional
MAGIC=0x53464E53; FORMAT=1; MAX_ITEMS=40; NONE=0xFFFF
SECTOR_A=0xFC000; SECTOR_B=0xFD000
HEADER=struct.Struct('<6I') # magic, format, generation, seq, count, checksum
ITEM=struct.Struct('<4B2H') # outlet, repeat, days, action(on=1), date_days, minute
MAX_IMAGE_SIZE=HEADER.size+MAX_ITEMS*ITEM.size
@dataclasses.dataclass(frozen=True)
class Item:
    outlet:int; repeat:int; days:int; on:int; date_days:int; minute:int
    def validate(self):
        if not 0<=self.outlet<=4: raise ValueError('outlet')
        if self.repeat not in (0,1,2): raise ValueError('repeat')
        if not 0<=self.days<=0x7f: raise ValueError('days')
        if self.repeat==2 and not self.days: raise ValueError('weekly_days')
        if self.repeat!=2 and self.days: raise ValueError('non_weekly_days')
        if self.on not in (0,1): raise ValueError('action')
        if not 0<=self.date_days<=0xffff: raise ValueError('date')
        if self.repeat==0 and self.date_days==NONE: raise ValueError('once_date')
        if self.repeat!=0 and self.date_days!=NONE: raise ValueError('repeat_date')
        if not 0<=self.minute<=1439: raise ValueError('minute')
    def pack(self): self.validate(); return ITEM.pack(self.outlet,self.repeat,self.days,self.on,self.date_days,self.minute)
def _words(buf:bytes): return struct.unpack('<'+'I'*(len(buf)//4),buf)
def encode(generation:int,seq:int,items:Iterable[Item])->bytes:
    items=list(items)
    if not 0<generation<=0x7fffffff: raise ValueError('generation')
    if not 0<seq<=0x7fffffff: raise ValueError('seq')
    if len(items)>MAX_ITEMS: raise ValueError('count')
    out=bytearray(HEADER.pack(MAGIC,FORMAT,generation,seq,len(items),0)+b''.join(i.pack() for i in items))
    total=sum(_words(out)) & 0xffffffff
    struct.pack_into('<I',out,20,(-total)&0xffffffff)
    assert (sum(_words(out))&0xffffffff)==0
    return bytes(out)
def decode(buf:bytes)->Optional[tuple[int,int,list[Item]]]:
    if len(buf)<HEADER.size: return None
    magic,fmt,gen,seq,count,chk=HEADER.unpack_from(buf)
    if magic!=MAGIC or fmt!=FORMAT or not 0<gen<=0x7fffffff or not 0<seq<=0x7fffffff or count>MAX_ITEMS: return None
    n=HEADER.size+count*ITEM.size
    if len(buf)<n or (sum(_words(buf[:n]))&0xffffffff)!=0: return None
    items=[]
    try:
        for i in range(count):
            v=Item(*ITEM.unpack_from(buf,HEADER.size+i*ITEM.size)); v.validate(); items.append(v)
    except (ValueError,struct.error): return None
    return gen,seq,items
def choose(a:bytes,b:bytes):
    aa,bb=decode(a),decode(b)
    if aa is None:return (SECTOR_B,bb) if bb else (None,None)
    if bb is None:return SECTOR_A,aa
    return (SECTOR_A,aa) if aa[1]>=bb[1] else (SECTOR_B,bb)
def due(items,date_days,weekday,minute):
    out=[]
    for i in items:
        if i.minute!=minute: continue
        if i.repeat==0 and i.date_days!=date_days: continue
        if i.repeat==2 and not (i.days&(1<<weekday)): continue
        out.append((i.outlet,bool(i.on)))
    return out
def selftest():
    daily_on=Item(1,1,0,1,NONE,420); daily_off=Item(1,1,0,0,NONE,1380)
    weekly=Item(4,2,(1<<0)|(1<<4),1,NONE,390); once=Item(0,0,0,1,20742,754)
    a=encode(100,1,[daily_on,daily_off,weekly])
    max_items=[Item((i%4)+1,1,0,i%2,NONE,(i*31)%1440) for i in range(MAX_ITEMS)]
    b=encode(101,2,max_items)
    assert decode(a)[:2]==(100,1) and choose(a,b)[0]==SECTOR_B
    corrupt=bytearray(b); corrupt[-1]^=0x55; assert choose(a,corrupt)[0]==SECTOR_A
    # Test every byte cut of a maximum-size generation. A prefix may only validate early
    # when the still-erased suffix is already byte-identical to the intended 0xFF bytes.
    erased=b'\xff'*MAX_IMAGE_SIZE
    for cut in range(len(b)):
        partial=bytearray(erased); partial[:cut]=b[:cut]
        parsed=decode(partial)
        if parsed is None:
            assert choose(a,partial)[0]==SECTOR_A
        else:
            used=HEADER.size+parsed[2].__len__()*ITEM.size
            assert bytes(partial[:used])==b[:used]
            assert choose(a,partial)[0]==SECTOR_B
    assert due([daily_on],20000,2,420)==[(1,True)]
    assert due([daily_off],20000,2,1380)==[(1,False)]
    assert due([weekly],20000,0,390)==[(4,True)] and due([weekly],20000,1,390)==[]
    assert due([once],20742,4,754)==[(0,True)]
    for bad in [Item(5,1,0,1,NONE,1),Item(1,3,0,1,NONE,1),Item(1,2,0,1,NONE,1),Item(1,1,1,1,NONE,1),Item(1,0,0,1,NONE,1),Item(1,1,0,1,NONE,1440)]:
        try: encode(1,1,[bad])
        except ValueError: pass
        else: raise AssertionError(bad)
    print(f'flash_ab_interrupted_commit=PASS ({len(b)} cut points, max table)')
    print('flash_highest_valid_sequence=PASS')
    print('event_daily_weekly_once=PASS')
    print('event_invalid_rejected=PASS')
    print(f'max_image_size={MAX_IMAGE_SIZE} sectors=0x{SECTOR_A:x}/0x{SECTOR_B:x}')
if __name__=='__main__': selftest()
