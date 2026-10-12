"""Reject real-number normal cancellation using the bound output slice.

Binary32 per-op model, sequential dot and rounded reciprocal sqrt. This is
an adversarial semantics gate, not an emulator of AMD's chosen fast-math ISA.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import re
import struct
import sys
sys.dont_write_bytecode=True
SHA='d14932706805fa9e3736bf32c40ac0a2fc42637e2c3688dd26714c3386ac1110'
BODY='2bbda09d89e10bfa44e5374c0af0bf554c651d5f966a179ab7de19559949b931'
def f(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def bits(x):return struct.pack('<f',x).hex()


def evaluate(ll,seed,remove):
    defs=dict(re.findall(r'^  (%\d+) = (.+?)(?:  ;.*)?$',ll,re.M));memo=dict(seed)
    used=set()
    def v(s):
        if s in memo:return memo[s]
        if not s.startswith('%'):return f(float(s))
        used.add(s)
        if remove and s in ('%123','%124','%125'):
            x=v({'%123':'%118','%124':'%119','%125':'%120'}[s])
        else:
            line=defs[s]
            m=re.fullmatch(r'(fmul|fadd|fsub|fdiv) fast float ([^,]+), (.+)',line)
            if m:
                a,b=v(m[2]),v(m[3]);x=f({'fmul':lambda:a*b,'fadd':lambda:a+b,'fsub':lambda:a-b,'fdiv':lambda:a/b}[m[1]]())
            elif '@dx.op.dot3.f32' in line:
                args=[v(z) for z in re.findall(r'float ([^,)]+)',line.split('(',1)[1])];x=f(f(f(args[0]*args[3])+f(args[1]*args[4]))+f(args[2]*args[5]))
            elif '@dx.op.unary.f32' in line:
                op=int(re.search(r'i32 (\d+)',line)[1]);a=v(re.search(r'float ([^,)]+)',line.split('(',1)[1])[1])
                assert op in (6,25);x=f(abs(a) if op==6 else 1/math.sqrt(a))
            elif line.startswith('fcmp fast oge float '):
                a,b=line.removeprefix('fcmp fast oge float ').split(', ');x=v(a)>=v(b)
            elif line.startswith('select i1 '):
                m=re.fullmatch(r'select i1 (%\d+), float ([^,]+), float (.+)',line);assert m
                x=v(m[2] if v(m[1]) else m[3])
            else:raise ValueError('unmodeled actual operation '+s+': '+line)
        memo[s]=x;return x
    # Actual encoded normal outputs, not an invented normalized-vector output.
    assert 'i8 0, float %220)' in ll and 'i8 1, float %221)' in ll
    return [v('%220'),v('%221')],sorted(used)


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    sys.path.insert(0,str(a.repo/'gpu-shaders/materials'));import dxcom
    raw=(a.out/'original.dxbc').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
    ll=dxcom.dumpbin(str(a.out/'original.dxbc'));assert hashlib.sha256(dxcom.body(ll).encode()).hexdigest()==BODY
    # Orthonormal basis, identity view and positive handedness; all ordinary finite values.
    seed={f'%{i}':f(x) for i,x in {13:0,14:0,15:1,16:1,17:0,18:0,26:1,
        183:1,184:0,185:0,188:0,189:1,190:0,193:0,194:0,195:1}.items()}
    rng=random.Random(0x7d506745)
    for case in range(256):
        seed.update({f'%{i}':f(rng.uniform(.05,.95)) for i in (35,36,37)})
        original,used=evaluate(ll,seed,False);candidate,_=evaluate(ll,seed,True)
        if list(map(bits,original))!=list(map(bits,candidate)):
            result=dict(verdict='REJECT_EXACT_PICTURE_GATE',case=case,shader_sha256=SHA,body_sha256=BODY,
                sampled_normal_rgb=[seed[f'%{i}'] for i in (35,36,37)],original_encoded_normal=original,
                removed_normalization_encoded_normal=candidate,original_bits=list(map(bits,original)),
                candidate_bits=list(map(bits,candidate)),actual_output_slice_nodes=used,
                scope='Defined finite binary32 per-op model; not native AMD fast-math rounding or packed-target image proof')
            (a.out/'rejection.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2));return
    raise RuntimeError('No counterexample found; do not claim rejection')


if __name__=='__main__':main()

