"""Private bounded native audit: compute release order and FO98 resource locks."""
from pathlib import Path
import struct,hashlib,json
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
ROOT=Path(__file__).resolve().parents[2]
b=(ROOT/'re/Nioh3.dump.exe').read_bytes()
u32=lambda p:struct.unpack_from('<I',b,p)[0]
u64=lambda p:struct.unpack_from('<Q',b,p)[0]
pe=u32(0x3c); opt=pe+24
assert b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,opt)[0]==0x20b
# The retained dump is mapped flat; prove RVA/raw identity for every section used.
count=struct.unpack_from('<H',b,pe+6)[0];sz=struct.unpack_from('<H',b,pe+20)[0]
for i in range(count):
    _,vs,va,rawsz,ptr=struct.unpack_from('<8sIIII',b,opt+sz+i*40)
    if rawsz: assert va==ptr, 'Audit expects the flat private dump'
imp=u32(opt+120);names={}
while u32(imp+12):
    oft,_,_,nr,ft=struct.unpack_from('<IIIII',b,imp);cur=oft or ft;idx=0
    while u64(cur+idx*8):
        val=u64(cur+idx*8)
        if not val>>63: names[ft+idx*8]=b[val+2:b.index(0,val+2)].decode()
        idx+=1
    imp+=20
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
out=[]
for rva,expected in [(0x12b0d0,['AcquireSRWLockExclusive','AcquireSRWLockShared']),
                     (0x12af40,['ReleaseSRWLockExclusive','ReleaseSRWLockShared'])]:
    ins=list(md.disasm(b[rva:rva+0x2b],rva));found=[]
    assert ins[1].op_str=='rcx, 0x198'
    assert ins[2].mnemonic=='sub' and ins[2].op_str=='edx, 1'
    assert ins[3].mnemonic=='je'
    assert ins[4].mnemonic=='cmp' and ins[4].op_str=='edx, 1'
    assert ins[5].mnemonic=='jne'
    for i in ins:
        if i.mnemonic in ('call','jmp') and i.operands[0].type==3:
            found.append(names[i.address+i.size+i.operands[0].mem.disp])
    assert found==expected,(hex(rva),found)
    out.append(dict(rva=hex(rva),size=0x2b,sha256=hashlib.sha256(b[rva:rva+0x2b]).hexdigest(),imports=found))
trim=list(md.disasm(b[0x2efa3a0:0x2efa3a0+166],0x2efa3a0))
release=next(i.address for i in trim if i.mnemonic=='call' and i.op_str=='qword ptr [rdx + 0x10]')
remove=next(i.address for i in trim if i.mnemonic=='call' and i.op_str=='0x2efc3d4')
assert release<remove
print(json.dumps(dict(result='PASS',resource_lock_modes={'1':'shared','2':'exclusive','other':'no helper lock'},
    resource_lock_windows=out,compute_trim_release=hex(release),compute_trim_remove=hex(remove),
    scope='Bounded native structure only; no complete producer/lifetime proof or FPS result'),indent=2))
