"""Bounded native root-signature serializer census; no game process access."""
import hashlib,json,mmap,pathlib,struct
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
ROOT=pathlib.Path(__file__).resolve().parents[2];OUT=pathlib.Path(__file__).resolve().parent/'out'
def main():
 with (ROOT/'re/Nioh3.dump.exe').open('rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
    u32=lambda p:struct.unpack_from('<I',b,p)[0]
    u64=lambda p:struct.unpack_from('<Q',b,p)[0]
    pe=u32(0x3c);opt=pe+24;count=struct.unpack_from('<H',b,pe+6)[0];sz=struct.unpack_from('<H',b,pe+20)[0]
    assert b[pe:pe+4]==b'PE\0\0'
    for i in range(count):
        _,_,va,rawsz,ptr=struct.unpack_from('<8sIIII',b,opt+sz+i*40)
        if rawsz:assert va==ptr
    imp=u32(opt+120);names={}
    while u32(imp+12):
        oft,_,_,_,ft=struct.unpack_from('<IIIII',b,imp);cur=oft or ft;j=0
        while u64(cur+j*8):
            val=u64(cur+j*8)
            if not val>>63:names[ft+j*8]=b[val+2:b.find(b'\0',val+2)].decode()
            j+=1
        imp+=20
    md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True;receipt=[]
    for start,size in [(0x2efea14,4017),(0x2ef8448,2704)]:
        ins=list(md.disasm(b[start:start+size],start))
        for j,i in enumerate(ins):
            if i.mnemonic=='call' and i.operands[0].type==3:
                slot=i.address+i.size+i.operands[0].mem.disp
                if names.get(slot)=='D3D12SerializeRootSignature':
                    tracked='esi' if start==0x2ef8448 else 'edi'
                    writes=[x for x in ins[:j] if x.operands and x.operands[0].type==1 and
                            x.reg_name(x.operands[0].reg) in (tracked,'r'+tracked[1:]) and x.operands[0].access&2]
                    assert writes[-1].mnemonic=='mov' and writes[-1].op_str==tracked+', 1'
                    edx=[x for x in ins[:j] if x.operands and x.operands[0].type==1 and
                         x.reg_name(x.operands[0].reg) in ('edx','rdx') and x.operands[0].access&2]
                    assert edx[-1].mnemonic=='mov' and edx[-1].op_str=='edx, '+tracked
                    window=ins[max(0,j-12):j+1]
                    receipt.append(dict(function=hex(start),call=hex(i.address),import_name=names[slot],
                                        serializer_version=1,version_definition=hex(writes[-1].address),version_argument=hex(edx[-1].address),
                                        window=[f'{x.address:x}: {x.mnemonic} {x.op_str}' for x in window],
                                        window_sha256=hashlib.sha256(b[window[0].address:i.address+i.size]).hexdigest()))
    assert len(receipt)==3
    OUT.mkdir(exist_ok=True);(OUT/'root-signature.json').write_text(json.dumps(receipt,indent=2))
    print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
