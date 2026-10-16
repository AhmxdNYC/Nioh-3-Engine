"""Bounded read-only native binding for a stateless fixed-size hash reduction."""
import hashlib,json,mmap,pathlib,sys
from capstone import Cs,CS_ARCH_X86,CS_MODE_64

source=pathlib.Path(sys.argv[1])
with source.open('rb') as f:
    b=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
    md=Cs(CS_ARCH_X86,CS_MODE_64)
    body=b[0x1199a4:0x1199a4+164]
    assert hashlib.sha256(body).hexdigest()=='91feb6ed7555a21750613844e53e01158565fb1d0a9081d7803dd207fad0d24e','hash body changed'
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
    # Bind complete caller bytes, including key/context provenance, subsequent
    # key reread and lock order; checking length alone was insufficient.
    for start,size,pin,anchors in (
        (0x58be90,365,'7229eed9b2b7dd173ad27ec44a12b2bf962434b5e751e2db9d0aaf8ed9925a47',{
            0x58bf28:('lea','rdi, [rbx + 0x810]'),0x58bf35:('mov','rdx, rdi'),
            0x58bf38:('mov','rcx, rbx'),0x58bf4a:('mov','rax, qword ptr [rdi]')}),
        (0x58c000,216,'d386ab2c69771389534c7b3a7c6d30e9cb27ac2c6ce326edaa7ed14548d9324c',{
            0x58c01a:('mov','rsi, rdx'),0x58c01d:('mov','rbx, rcx'),
            0x58c030:('mov','rax, qword ptr [rsi]')}),
        (0x2efe1cc,220,'6e0e642318fe754719fa8de9cfb7fdac08d61e77a5fc1e55ba046cf145a7bc8a',{
            0x2efe1e5:('mov','rdi, rcx'),0x2efe1e8:('mov','r14, rdx'),
            0x2efe209:('mov','rdx, r14'),0x2efe20c:('mov','rcx, rdi'),
            0x2efe217:('mov','rcx, qword ptr [r14]')})):
        raw=b[start:start+size]
        assert hashlib.sha256(raw).hexdigest()==pin,('caller changed',hex(start))
        decoded={i.address:(i.mnemonic,i.op_str) for i in md.disasm(raw,start)}
        for at,expected in anchors.items(): assert decoded[at]==expected,hex(at)
    for start,call in ((0x58bf2f,0x58bf3b),(0x58c014,0x58c020),(0x2efe203,0x2efe212)):
        window=b[start:call+5];rows=list(md.disasm(window,start))
        assert (rows[0].mnemonic,rows[0].op_str)==('mov','r8d, 8')
        assert rows[-1].address==call and rows[-1].mnemonic=='call' and rows[-1].op_str=='0x1199a4'
        assert all(i.mnemonic not in ('call','jmp') and not i.mnemonic.startswith('j') for i in rows[:-1])
        sites.append({'call':hex(call),'window_sha256':hashlib.sha256(window).hexdigest()})
    print(json.dumps({'body_rva':'0x1199a4','bytes':164,'body_sha256':hashlib.sha256(body).hexdigest(),'sites':sites,
                     'scope':'Exact length8 only; raw CRC state/no final complement; mode read every invocation; no cached native pointers'},indent=2))
