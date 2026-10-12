"""Bound signed UV+row-base composition, offline codegen and private package."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import random
import re
import sys
import tempfile
sys.dont_write_bytecode=True
import address_probe as A
import native_pack as N
import uv_probe as U


def sha(b):return hashlib.sha256(b).hexdigest()


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();sys.path[:0]=[str(a.repo/'gpu-shaders/materials'),str(a.repo/'gpu-shaders/pb2guard'),str(a.repo/'gpu-shaders')]
    import dxcom,sfb_rewrite as S,rga_ps,pack_check,idrk
    from build_sfb import Group
    original=(a.out/'original.dxbc').read_bytes();uv=(a.out/'uv-hoist.dxbc').read_bytes()
    assert sha(original)==N.PS and sha(uv)==N.CAND
    oll=dxcom.dumpbin(str(a.out/'original.dxbc'));ull=dxcom.dumpbin(str(a.out/'uv-hoist.dxbc'))
    assert dxcom.body(ull)==dxcom.body(U.rewrite(oll,S,dxcom))
    text=A.rewrite(ull,S,dxcom,'8cd2f2b9e1ac1a2422d27665ec90454b7cd23845fb5a96e6c879a40c8ee36138')
    baseline=A.certificate(oll,S,True)
    assert baseline==A.certificate(ull,S,True)==A.certificate(text,S,True)
    mutants={
        'address_base':text.replace(', 3\n',', 4\n',1),
        'uv_component':text.replace('i32 22, float %5','i32 22, float %6',1),
        'clamp_endpoint':text.replace('float 1.000000e+00','float 5.000000e-01',1),
        'resource_identity':text.replace('i32 0, i32 3, i1 false','i32 0, i32 4, i1 false',1),
    }
    for name,bad in mutants.items():
        assert bad!=text and A.certificate(bad,S,True)!=baseline,name
    # Direct wraparound fixtures, with no bounded-width assumption for coords.
    edge=[0,1,2,255,256,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]
    cases=list(itertools.product(edge,repeat=4));rng=random.Random(493)
    cases += [tuple(rng.getrandbits(32) for _ in range(4)) for _ in range(4096)]
    mask=0xffffffff
    for w,y,x,b in cases:
        assert (((w*y+x)&mask)+((b+3)&mask))&mask == (((w*y+((b+3)&mask))&mask)+x)&mask
    # Float operands remain opaque exact expressions, including raw +/-0,
    # infinities, subnormals and NaN payloads. No host float calculation used.
    floatbits=[0,0x80000000,0x7f800000,0xff800000,0x7fc00001,0x7fa12345,
               0xffc12345,1,0x007fffff,0x7f7fffff,0x3f800000,0xbf800000]
    def raw_uv(module,u,v):
        for col,bits in enumerate((u,v)):
            pattern=rf'(  %\d+ = )call float @dx.op.loadInput.f32\(i32 4, i32 5, i32 0, i8 {col}, i32 undef\)'
            module,n=re.subn(pattern,lambda m:m[1]+f'bitcast i32 {bits if bits<0x80000000 else bits-0x100000000} to float',module)
            assert n==1
        return module
    for u,v in itertools.product(floatbits,repeat=2):
        assert A.certificate(raw_uv(oll,u,v),S,True)==A.certificate(raw_uv(text,u,v),S,True)
    candidate=dxcom.ll_to_container(text);dst=a.out/'combined.dxbc';dst.write_bytes(candidate)
    actual=dxcom.dumpbin(str(dst));assert dxcom.body(actual)==dxcom.body(text)
    assert A.certificate(actual,S,True)==baseline
    po,pn=dxcom.parts(original),dxcom.parts(candidate)
    assert all(po.get(k)==pn.get(k) for k in ('ISG1','OSG1','PSV0','SFI0'))
    (a.out/'combined.ll').write_bytes(actual.encode())
    tempfile.tempdir=str(a.out.resolve())
    vs=a.repo/'gpu-shaders/meshvs/work/1559714c.dxbc';assert sha(vs.read_bytes())==N.VS
    stats,isa=rga_ps.stats(str(vs),str(dst),'gfx1103');(a.out/'combined.gfx1103.isa.txt').write_bytes(isa.encode())
    # Compare to bound retained baseline receipts, not arithmetically added gains.
    before=json.loads((a.out/'binding.json').read_text());prior=json.loads((a.out/'uv-hoist.json').read_text())
    assert before['ps_sha256']==N.PS and prior['candidate_sha256']==N.CAND
    assert before['isa_sha256']==sha((a.out/'original.gfx1103.isa.txt').read_bytes())
    assert prior['isa_sha256']==sha((a.out/'uv-hoist.gfx1103.isa.txt').read_bytes())
    assert before['rga_sha256']==prior['rga_sha256']==sha(Path(rga_ps.RGA).read_bytes())
    root_sha=sha(rga_ps.root_sig(actual,dxcom.dumpbin(str(vs))).encode())
    gpso_sha=sha(rga_ps.gpso(dxcom.dumpbin(str(vs))).encode())
    assert root_sha==before['generated_root_sha256']==prior['generated_root_sha256']
    assert gpso_sha==before['generated_gpso_sha256']==prior['generated_gpso_sha256']
    good=stats['instructions']<prior['stats']['instructions'] and all(stats[k]<=prior['stats'][k] for k in ['vgprs','sgprs','scratch','buffer_loads','samples','branches'])
    result=dict(candidate_sha256=sha(candidate),body_sha256=sha(dxcom.body(actual).encode()),isa_sha256=sha(isa.encode()),
        original_stats=before['stats'],uv_stats=prior['stats'],combined_stats=stats,
        retained_for_native_gates=good,integer_cases=len(cases),float_rawbit_structural_cases=len(floatbits)**2,semantic_mutants=list(mutants),
        whole_body_certificate=True,opaque_float_semantics=True,compiler_sha256=before['rga_sha256'],
        root_sha256=root_sha,gpso_sha256=gpso_sha,script_sha256=sha(Path(__file__).read_bytes()),
        address_tool_sha256=sha(Path(A.__file__).read_bytes()))
    if good:
        game=Path(r'C:/Program Files (x86)/Steam/steamapps/common/Nioh3/shader_dx12/0xb4751b0f.file')
        raw=game.read_bytes();assert sha(raw)==N.FILE
        pack_check.ORIGINAL=original[4:20].hex();pack_check.ORIGINAL_SHA=N.PS
        pack_check.CANDIDATE=candidate[4:20].hex();pack_check.CANDIDATE_SHA=sha(candidate)
        pack=pack_check.prove(raw,candidate,1);g=Group(raw);packed=idrk.pack(g.replace({1:candidate}),raw)
        assert sha(packed)==pack['hypothetical_packed_sha256'] and Group(packed).replace({1:original})==g.p
        for blob,slot in [(candidate,0),(candidate[:-1]+bytes([candidate[-1]^1]),1)]:
            try:pack_check.prove(raw,blob,slot)
            except AssertionError:pass
            else:raise AssertionError('pack mutation accepted')
        package=a.out/'combined-package';package.mkdir(exist_ok=True)
        (package/'candidate.file').write_bytes(packed);(package/'original.file').write_bytes(raw)
        manifest=dict(schema=1,target_relative='shader_dx12/0xb4751b0f.file',slot=1,
            original_sha256=sha(raw),candidate_sha256=sha(packed),original_ps_sha256=N.PS,candidate_ps_sha256=sha(candidate),
            companion_vs_sha256=N.VS,pack=pack,
            activation_rule='Device idle and lead lease; current target must equal original_sha256. Fresh current-entry backup before mutation; recheck target immediately before atomic publish.',
            restoration_rule='Restore only while idle and target still equals candidate_sha256. Otherwise retain backup/receipt and refuse overwrite. Verify original hash afterward.',
            gates={'structural_offline':True,'single_record_pack':True,'gfx1103_codegen':True,
                   'combined_captured_windows_pso':False,'native_draw_image':False,'proton':False,'short_ab':False},
            no_installation_performed=True,not_public=True)
        (package/'manifest.json').write_text(json.dumps(manifest,indent=2))
        assert game.read_bytes()==raw
        result['package_manifest_sha256']=sha((package/'manifest.json').read_bytes())
    (a.out/'combined.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))


if __name__=='__main__':main()
