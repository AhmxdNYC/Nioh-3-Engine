"""Private dump direct-call inventory. Does not cover indirect references.
Candidate bytes must decode at an instruction boundary within a pdata function.
"""
from pathlib import Path
import bisect,json,struct
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
root=Path(__file__).resolve().parents[2]
b=(root/'re/Nioh3.dump.exe').read_bytes()
u32=lambda p:struct.unpack_from('<I',b,p)[0]
pe=u32(0x3c); opt=pe+24
pdata=u32(opt+112+24); size=u32(opt+116+24)
funcs=[struct.unpack_from('<III',b,p)[:2] for p in range(pdata,pdata+size,12)]
starts=[f[0] for f in funcs]
targets={0x58c0d8:'find',0x58ba50:'insert',0x2efc3d4:'remove',0x2efa3a0:'trim',
         0x2f00ac8:'clear',0x1899bd4:'reinit',0x2f00bc4:'teardown',0x189912c:'shader cleanup',
         0x2efd04c:'leaf cleanup wrapper'}
sections=[]
for k in range(struct.unpack_from('<H',b,pe+6)[0]):
    p=opt+struct.unpack_from('<H',b,pe+20)[0]+k*40
    _,_,va,n,raw=struct.unpack_from('<8sIIII',b,p)
    if u32(p+36)&0x20000000:
        assert raw==va
        sections.append((va,n))
candidates={};unbounded=[]
for va,n in sections:
    for opcode in (b'\xe8',b'\xe9'):
        at=b.find(opcode,va,va+n)
        while at>=0:
            if at+5<=va+n:
                target=at+5+struct.unpack_from('<i',b,at+1)[0]
                if target in targets:
                    idx=bisect.bisect_right(starts,at)-1
                    if idx>=0 and funcs[idx][0]<=at<funcs[idx][1]:
                        candidates.setdefault(funcs[idx],{})[at]=target
                    else:unbounded.append(dict(site=hex(at),target=hex(target)))
            at=b.find(opcode,at+1,va+n)
md=Cs(CS_ARCH_X86,CS_MODE_64);edges=[]
for (start,end),sites in candidates.items():
    for ins in md.disasm(b[start:end],start):
        if ins.address in sites and ins.mnemonic in ('call','jmp') and ins.op_str==hex(sites[ins.address]):
            edges.append(dict(function=hex(start),site=hex(ins.address),kind=ins.mnemonic,target=hex(sites[ins.address]),name=targets[sites[ins.address]]))
report=dict(edges=edges,unverified_outside_pdata=unbounded,scope='Direct calls/tail jumps only; indirect and unverified sites unresolved')
# This leaf has no pdata entry. Review its exact complete 12-byte sequence
# separately, without pretending arbitrary surrounding bytes are a function.
leaf=list(md.disasm(b[0x2efd04c:0x2efd058],0x2efd04c))
assert [(i.mnemonic,i.op_str) for i in leaf]==[('lea','r8, [rcx + 0x7d8]'),('jmp','0x189912c')]
report['reviewed_leaf_cleanup_wrapper']={'entry':'0x2efd04c','size':12,'target':'0x189912c','reachability':'unresolved'}
out=root/'scratch/compute-direct-edges.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(report,indent=2))
for target,name in targets.items():
    selected=[e for e in edges if e['target']==hex(target)]
    print(name,hex(target),len(selected),'edges:',', '.join(e['function']+'@'+e['site'] for e in selected))
print('unverified outside pdata:',len(unbounded),'report:',out)
