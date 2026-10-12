"""Bound PS05ad1715 four-material UV hoist; no resource or sample motion."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
SHA='4173c9dce9ad919b5238426f345de1d1d648b1c47efeaf1ea2c59e01e558dd4b'
BODY='a4dcfea86c85c3ec50f02b20da99382d431fc27b807c2aa9dc8c24e41dd9e3c1'
FILE='2fd6d3c82b14be2b5cc47e76dd36f0c65a1af394833f2b17737ddfb2fab89326'
GROUPS=((666,754,852,938),(667,755,853,939),(689,777,875,961),
        (690,778,876,962),(691,779,877,963),(692,780,878,964))


def sha(x):return hashlib.sha256(x).hexdigest()


def rewrite(ll,S,dxcom):
    if sha(dxcom.body(ll).encode())!=BODY:raise ValueError('unexpected original body')
    f=S.Fn(ll);defs={S.DEF.match(x)[1]:x for _,lines in f.blocks for x in lines if S.DEF.match(x)}
    repl={'%v'+str(n):'%v'+str(g[0]) for g in GROUPS for n in g[1:]}
    selected={'%v'+str(n) for g in GROUPS for n in g}
    sub=lambda line:S.TOK.sub(lambda m:repl.get(m[0],m[0]),line)
    lifted=[]
    for g in GROUPS:
        rhs=[sub(defs['%v'+str(n)]).split(' = ',1)[1] for n in g]
        assert len(set(rhs))==1
        # Strict operation whitelist: never value-number reads/samples/handles.
        assert rhs[0].startswith(('call float @dx.op.unary.f32(i32 22,',
                                  'call float @dx.op.binary.f32(i32 35,',
                                  'call float @dx.op.binary.f32(i32 36,'))
        lifted.append(defs['%v'+str(g[0])])
    for _,lines in f.blocks:
        lines[:]=[sub(x) for x in lines if not(S.DEF.match(x) and S.DEF.match(x)[1] in selected)]
    entry=f.blocks[0][1];at=next(i for i,x in enumerate(entry) if x.startswith('  %v99 = '))+1
    entry[at:at]=lifted
    return f.emit()


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--assemble',action='store_true');a=p.parse_args()
    sys.path[:0]=[str(a.repo/'gpu-shaders/materials'),str(a.repo/'gpu-shaders')]
    import dxcom,sfb_rewrite as S
    from build_sfb import Group
    source=Path('C:/Program Files (x86)/Steam/steamapps/common/Nioh3/shader_dx12/0xbc09ad4a.file')
    raw=source.read_bytes();assert sha(raw)==FILE
    binary=Group(raw).dxbc(1);assert sha(binary)==SHA and binary[4:20].hex()=='05ad17151de4f513fb82c7428874a7f9'
    a.out.mkdir(parents=True,exist_ok=True);original=a.out/'original.dxbc';original.write_bytes(binary)
    ll=dxcom.dumpbin(str(original));candidate=rewrite(ll,S,dxcom)
    (a.out/'four-material-uv.ll').write_bytes(candidate.encode())
    r=dict(file_sha256=FILE,slot=1,original_shader_sha256=SHA,original_body_sha256=BODY,
        candidate_body_sha256=sha(dxcom.body(candidate).encode()),removed_pure_calls=18,
        changed_resource_sample_or_float_arithmetic_order=False,codegen='PENDING',native='UNADMITTED')
    if a.assemble:
        b=dxcom.ll_to_container(candidate);dst=a.out/'four-material-uv.dxbc';dst.write_bytes(b)
        assert dxcom.body(dxcom.dumpbin(str(dst)))==dxcom.body(candidate)
        op,cp=dxcom.parts(binary),dxcom.parts(b)
        assert all(op.get(k)==cp.get(k) for k in ('ISG1','OSG1','PSV0','SFI0'))
        r['candidate_shader_sha256']=sha(b)
    (a.out/'four-material-uv.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))


if __name__=='__main__':main()
