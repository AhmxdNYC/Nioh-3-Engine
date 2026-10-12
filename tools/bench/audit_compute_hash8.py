"""Bounded read-only native binding for a stateless fixed-size hash reduction."""
import hashlib,json,mmap,pathlib,sys
from capstone import Cs,CS_ARCH_X86,CS_MODE_64

source=pathlib.Path(sys.argv[1])
with source.open('rb') as f:
    b=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
    md=Cs(CS_ARCH_X86,CS_MODE_64)
    body=b[0x1199a4:0x1199a4+164]
    ins={i.address:(i.mnemonic,i.op_str) for i in md.disasm(body,0x1199a4)}
    for at,pair in {
        0x1199a7:('cmp','byte ptr [rcx + 0x2490], r9b'),
        0x1199b0:('mov','eax, 0xffffffff'),
        0x1199fc:('crc32','eax, dword ptr [rdx]'),
        0x119a01:('add','rdx, rcx'),
        0x119a06:('add','r8d, -4'),
        0x119a38:('xor','r9d, dword ptr [rdx]'),
    }.items():
        assert ins[at]==pair,(hex(at),ins.get(at))
    sites=[]
    for start,call in ((0x58bf2f,0x58bf3b),(0x58c014,0x58c020),(0x2efe203,0x2efe212)):
        window=b[start:call+5];rows=list(md.disasm(window,start))
        assert (rows[0].mnemonic,rows[0].op_str)==('mov','r8d, 8')
        assert rows[-1].address==call and rows[-1].mnemonic=='call' and rows[-1].op_str=='0x1199a4'
        assert all(i.mnemonic not in ('call','jmp') and not i.mnemonic.startswith('j') for i in rows[:-1])
        sites.append({'call':hex(call),'window_sha256':hashlib.sha256(window).hexdigest()})
    print(json.dumps({'body_rva':'0x1199a4','bytes':164,'body_sha256':hashlib.sha256(body).hexdigest(),'sites':sites,
                     'scope':'Exact length8 only; raw CRC state/no final complement; mode read every invocation; no cached native pointers'},indent=2))
