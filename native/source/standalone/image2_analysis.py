"""Extract analysis-only Image2 bytes. This does not create a release/boot proof."""
import struct
from standalone.ota import LOAD,RAM_SPAN,TABLE_TAIL,transform_fwr,inspect_image,ImageError

REQUIRED=('.image2.start.table','.ram_image2.text','.ram.data')

def initialized_body(elf):
    elf=bytes(elf)
    if len(elf)<52 or elf[:7]!=b'\x7fELF\x01\x01\x01':raise ImageError('ELF32 little endian required')
    kind,machine,version=struct.unpack_from('<HHI',elf,16)
    if (kind,machine,version)!=(2,40,1):raise ImageError('ARM executable required')
    start=struct.unpack_from('<I',elf,32)[0]
    width,count,string_index=struct.unpack_from('<HHH',elf,46)
    if width!=40 or not 0<string_index<count or start>len(elf) or count>(len(elf)-start)//40:raise ImageError('section table bounds')
    rows=[struct.unpack_from('<10I',elf,start+k*40) for k in range(count)]
    def contents(row):
        offset,size=row[4:6]
        if offset>len(elf) or size>len(elf)-offset:raise ImageError('section file bounds')
        return elf[offset:offset+size]
    if rows[string_index][1]!=3:raise ImageError('section names are not STRTAB')
    names=contents(rows[string_index]);selected={}
    for row in rows:
        index=row[0]
        if index>=len(names):raise ImageError('name offset')
        end=names.find(b'\0',index)
        if end<0:raise ImageError('unterminated name')
        name=names[index:end].decode('ascii',errors='strict')
        if name in REQUIRED:
            if name in selected:raise ImageError('duplicate section')
            if row[1]!=1 or not row[2]&2 or row[5]==0:raise ImageError('required section is not allocated PROGBITS')
            selected[name]=(row[3],contents(row))
    if set(selected)!=set(REQUIRED):raise ImageError('missing initialized section')
    table,text,data=[selected[key] for key in REQUIRED]
    if table[0]!=LOAD or len(table[1])!=24 or table[1][4:]!=TABLE_TAIL:raise ImageError('start table contract')
    if text[0]!=LOAD+24 or data[0]<text[0]+len(text[1]):raise ImageError('overlapping/out-of-order image')
    end=data[0]+len(data[1])
    if not LOAD+24<end<=LOAD+RAM_SPAN:raise ImageError('RAM span exceeded')
    entry=struct.unpack_from('<I',table[1])[0]
    if not entry&1 or not text[0]<=entry&~1<text[0]+len(text[1]):raise ImageError('entry outside code')
    body=bytearray(end-LOAD)
    for address,content in (table,text,data):body[address-LOAD:address-LOAD+len(content)]=content
    return bytes(body),[{'section':name,'address':address,'bytes':len(content)} for name,(address,content) in selected.items()]

def package_body(body):
    raw=struct.pack('<II',len(body),LOAD)+b'\xff'*8+bytes(body)
    raw+=struct.pack('<I',sum(raw)&0xffffffff)
    inspect_image(raw)
    encoded=transform_fwr(raw)
    if inspect_image(encoded).decoded!=raw:raise ImageError('FWR round-trip mismatch')
    return raw,encoded
