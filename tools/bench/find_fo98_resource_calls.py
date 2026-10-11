"""Find bounded native indirect vtable+28 call sites with nearby mode setup.
Private inert dump analysis only; candidates are not proven resource callers.
Not exhaustive: scans disp8/non-SIB encodings, unwind-covered functions <=64KiB.
Nearby register mentions are search hints, not dataflow or lock proofs.
"""
from pathlib import Path
import struct,json,bisect,re
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
ROOT=Path(__file__).resolve().parents[2]
b=(ROOT/'re/Nioh3.dump.exe').read_bytes();u32=lambda p:struct.unpack_from('<I',b,p)[0]
pe=u32(0x3c);opt=pe+24;nr=struct.unpack_from('<H',b,pe+6)[0];osz=struct.unpack_from('<H',b,pe+20)[0]
sections=[]
for k in range(nr):
 p=opt+osz+k*40;name,vs,va,sz,raw=struct.unpack_from('<8sIIII',b,p)
 if u32(p+36)&0x20000000:
  assert va==raw
  sections.append((va,sz))
pdata=u32(opt+112+3*8);psz=u32(opt+116+3*8)
funcs=[struct.unpack_from('<III',b,p)[:2] for p in range(pdata,pdata+psz,12)];starts=[f[0] for f in funcs]
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=False
candidate_funcs={}
for va,sz in sections:
 for m in re.finditer(rb'\xff[\x50-\x57]\x28',b[va:va+sz]):
  address=va+m.start();i=bisect.bisect_right(starts,address)-1
  if i>=0 and funcs[i][0]<=address<funcs[i][1]:candidate_funcs[funcs[i]]=True
out=[]
for start,end in candidate_funcs:
 if end-start>65536:continue
 ins=list(md.disasm(b[start:end],start))
 for idx,i in enumerate(ins):
  if i.mnemonic=='call' and re.fullmatch(r'qword ptr \[(?:r\w+) \+ 0x28\]',i.op_str):
   context=ins[max(0,idx-10):idx+1]
   modes=[f'{hex(x.address)} {x.mnemonic} {x.op_str}' for x in context if re.search(r'\br9[dwb]?\b',x.op_str)]
   out.append(dict(function=hex(start),call=hex(i.address),operand=i.op_str,mode_context=modes,
      context=[f'{hex(x.address)} {x.mnemonic} {x.op_str}' for x in context]))
path=ROOT/'scratch/fo98-vtable28-call-candidates.json'
path.parent.mkdir(parents=True,exist_ok=True)
path.write_text(json.dumps(out,indent=2))
print('native indirect+28 candidates:',len(out),'functions:',len(candidate_funcs),'report:',path)
for row in out:
 if row['function'] in ('0x1290b8','0x4906e8','0x2f08094','0x2f11c9c'): print(json.dumps(row))
