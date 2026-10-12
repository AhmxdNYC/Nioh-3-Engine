"""Retained ISA rejection and exact local row-address prototype; no GPU calls."""
import argparse
import copy
import csv
import hashlib
import json
from pathlib import Path
import random
import re
import sys
sys.dont_write_bytecode = True
SHA = 'da83f00e00883f3a4824f75276461de0efd2e452c364be54b1f64265f940839c'
BODY = 'b552d22e52afdd5fdc330a4c8b47c2ed92a68980f2198296020546fc9b646695'
DIGEST = 'a11ff9e5ffd0f3356d74865e29e38228'
EXPECTED = [1049,1153,1266,1370,1467,1800,2223,2334,2472,2583,2722,2812,2922]
ADD = re.compile(r'add i32 (%\w+|-?\d+), (%\w+|-?\d+)')
MAD = re.compile(r'call i32 @dx.op.tertiary.i32\(i32 49, i32 (%\w+|-?\d+), i32 (%\w+|-?\d+), i32 (%\w+|-?\d+)\)')
def sha(x): return hashlib.sha256(x).hexdigest()

def retained_isa(repo, archive):
    mapping = repo/'re/scripts_gpu/out/rgp_top300/Nioh3_2026-10-06_19-39-58_shaders/pso_map.csv'
    rows = list(csv.DictReader(mapping.open(newline='')))
    ids = sorted({r['api_pso_hash'] for r in rows if 'pixel:'+DIGEST in r['shaders(stage:dxbc_digest=file/group#index)']})
    assert len(ids) == 7
    records = []
    for ident in ids:
        path = archive/(ident+'.txt')
        raw = path.read_bytes(); text = raw.decode()
        assert '_amdgpu_ps_main:' in text
        section = text.split('_amdgpu_ps_main:',1)[1].split('_amdgpu_gs_main:',1)[0]
        count = len(re.findall(r'^\s+[sv]_or_b32\b',section,re.M))
        assert count == 0, 'Unexpected emitted OR; reassess rejection'
        records.append(dict(pso=ident,path=str(path),sha256=sha(raw),pixel_or_b32=count))
    return dict(map_sha256=sha(mapping.read_bytes()),records=records,
                verdict='REJECT_INDEX_FOLD_NO_EMITTED_OR_TARGET_IN_RETAINED_VARIANTS')

def prototype(ll,S):
    fn = S.Fn(ll); before = copy.deepcopy(fn.blocks); users = {}
    for b,lines in before:
        for line in lines:
            for token in S.TOK.findall(line.split(' = ',1)[-1]):
                users.setdefault(token,[]).append((b,line))
    edits = []; retained = []
    for b,lines in fn.blocks:
        hits = []
        for line in lines:
            d = S.DEF.match(line); m = MAD.fullmatch(line.split(' = ',1)[-1].strip())
            if d and m: hits.append((d[1],*m.groups(),line))
        if not hits: continue
        assert len(hits)==4
        _,w,y0,x0,_ = hits[0]; x1=hits[1][3]; y1=hits[2][2]
        assert [z[1:4] for z in hits]==[(w,y0,x0),(w,y0,x1),(w,y1,x0),(w,y1,x1)]
        if any(len(users[z[0]])!=1 or users[z[0]][0][0]!=b for z in hits):
            retained.append(b); continue
        addresses=[users[z[0]][0][1] for z in hits]
        matches=[ADD.fullmatch(line.split(' = ',1)[-1].strip()) for line in addresses]
        assert all(m and (m[1]==h[0])!=(m[2]==h[0]) for m,h in zip(matches,hits))
        bases=[m[2] if m[1]==h[0] else m[1] for m,h in zip(matches,hits)]; assert len(set(bases))==1
        assert [lines.index(x) for x in addresses]==sorted(lines.index(x) for x in addresses)
        replacement={}
        for i,(h,line) in enumerate(zip(hits,addresses)):
            assert lines.index(h[4])<lines.index(line)
            row=f'%crow{b}_{i//2}'; result=S.DEF.match(line)[1]
            prefix=[] if i%2 else [f'  {row} = call i32 @dx.op.tertiary.i32(i32 49, i32 {w}, i32 {h[2]}, i32 {bases[0]})']
            replacement[line]=prefix+[f'  {result} = add i32 {row}, {h[3]}']
        removed=[h[4] for h in hits]
        edits.append(dict(block=b,removed=removed,replacements=replacement))
        lines[:]=[new for old in lines if old not in removed for new in replacement.get(old,[old])]
    assert [e['block'] for e in edits]==['b'+str(x) for x in EXPECTED]
    assert retained==['b176','b412','b594','b773']
    # Reconstruct the entire candidate from the narrow, exact edit set.
    # This does not value-number opaque resources, floating point or loop phi.
    def verify(blocks):
        expected=copy.deepcopy(before)
        for b,lines in expected:
            for e in edits:
                if e['block']==b:
                    lines[:]=[new for old in lines if old not in e['removed']
                              for new in e['replacements'].get(old,[old])]
        assert expected==blocks, 'Unexpected whole-body change'
    verify(fn.blocks)
    mutants=[]
    for label,needle,replacement in [('float','fmul fast float','fadd fast float'),
                                      ('resource','@dx.op.cbufferLoadLegacy','@dx.op.BADbufferLoadLegacy'),
                                      ('address','add i32 %crow','sub i32 %crow')]:
        bad=copy.deepcopy(fn.blocks); changed=False
        for _,lines in bad:
            for i,line in enumerate(lines):
                if needle in line and not changed:
                    lines[i]=line.replace(needle,replacement,1); changed=True
        assert changed
        try: verify(bad)
        except AssertionError: mutants.append(label)
        else: raise AssertionError('Accepted mutation '+label)
    rng=random.Random(0xa11ff9e5); mask=0xffffffff
    edge=[0,1,0x7fffffff,0x80000000,0xfffffffe,mask]; cases=0
    fixtures=[[a,b,c,d] for a in edge for b in edge for c in edge for d in edge]
    fixtures += [[rng.getrandbits(32) for _ in range(4)] for _ in range(4096)]
    for w,y,x,base in fixtures:
        assert (((w*y+x)&mask)+base)&mask==(((w*y+base)&mask)+x)&mask
        cases+=1
    return fn.emit(),dict(changed_blocks=[e['block'] for e in edits],retained_blocks=retained,
        removed_dxil_umads=26,integer_cases=cases,mutation_negatives=mutants,
        exact_edit_set=True,edits=edits,
        domain='Per-lane defined integer operands; wrapping i32 add and unsigned UMad. Identical float/resource/control instructions and order. No new read or resource-stability assumption.',
        limitations='Static identity and exact edit-set proof; no assembly, codegen, register, driver, image or frame-performance acceptance.')

def main():
    p=argparse.ArgumentParser(); p.add_argument('--repo',type=Path,required=True)
    p.add_argument('--original',type=Path,required=True); p.add_argument('--archive',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True); a=p.parse_args()
    sys.path.insert(0,str(a.repo/'gpu-shaders/materials')); import dxcom,sfb_rewrite as S
    assert sha(a.original.read_bytes())==SHA
    ll=dxcom.dumpbin(str(a.original)); assert sha(dxcom.body(ll).encode())==BODY
    isa=retained_isa(a.repo,a.archive); candidate,result=prototype(ll,S)
    a.out.mkdir(parents=True,exist_ok=True); (a.out/'local-row.ll').write_bytes(candidate.encode())
    result.update(original_sha256=SHA,original_body_sha256=BODY,candidate_body_sha256=sha(dxcom.body(candidate).encode()),
                  index_fold=isa,script_sha256=sha(Path(__file__).read_bytes()),parser_sha256=sha(Path(S.__file__).read_bytes()))
    (a.out/'static-gate.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k not in ('edits','index_fold')},indent=2))

if __name__=='__main__': main()
