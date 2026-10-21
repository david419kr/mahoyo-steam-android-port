"""Read bounded PE resources without loading or executing the supplied program."""


from pathlib import Path


import struct


def resource(path, kind, name):
    raw=Path(path).read_bytes()
    def fields(fmt,offset):
        size=struct.calcsize(fmt)
        if offset<0 or offset+size>len(raw):raise ValueError('Truncated PE data')
        return struct.unpack_from(fmt,raw,offset)
    if raw[:2]!=b'MZ':raise ValueError('Not a PE image')
    pe=fields('<I',0x3c)[0]
    if raw[pe:pe+4]!=b'PE\0\0':raise ValueError('Invalid PE signature')
    sections=fields('<H',pe+6)[0];optional_size=fields('<H',pe+20)[0];optional=pe+24
    magic=fields('<H',optional)[0]
    if magic not in (0x10b,0x20b) or not 1<=sections<=96:raise ValueError('Unsupported PE header')
    directories=optional+(112 if magic==0x20b else 96)
    if directories+24>optional+optional_size:raise ValueError('Missing PE resource directory')
    base,budget=fields('<II',directories+16)
    if not base or not 16<=budget<=64*1024*1024:raise ValueError('Missing/oversized PE resources')
    def offset(rva,length):
        for i in range(sections):
            virtual_size,start,size,first=fields('<IIII',optional+optional_size+i*40+8)
            if start<=rva and 0<=rva-start<=size and length<=size-(rva-start):
                result=first+rva-start
                if result+length>len(raw):raise ValueError('PE section exceeds file')
                return result
        raise ValueError('PE resource points outside a file-backed section')
    def relative(at,size):
        if not 0<=at<=budget or size>budget-at:raise ValueError('PE resource exceeds directory bounds')
        return offset(base+at,size)
    def entries(at):
        header=relative(at,16);named,count=fields('<HH',header+12)
        if named+count>4096:raise ValueError('PE resource directory entry budget')
        result=[]
        for i in range(named+count):
            key,target=fields('<II',relative(at+16+i*8,8))
            if key&0x80000000:
                position=key&0x7fffffff;length=fields('<H',relative(position,2))[0]
                if length>256:raise ValueError('PE resource name budget')
                first=relative(position+2,length*2);key=raw[first:first+length*2].decode('utf-16le')
            result.append((key,target))
        return result
    at=0
    for wanted in (kind,name):
        matches=[target for key,target in entries(at) if key==wanted]
        if len(matches)!=1 or not matches[0]&0x80000000:raise ValueError('Requested PE resource is missing or ambiguous')
        at=matches[0]&0x7fffffff
    leaves=entries(at)
    if len(leaves)!=1 or leaves[0][1]&0x80000000:raise ValueError('Ambiguous PE resource language')
    record=relative(leaves[0][1],16);rva,size,_,_=fields('<IIII',record)
    if not 0<size<=32*1024*1024:raise ValueError('PE resource payload budget')
    first=offset(rva,size);return raw[first:first+size]


