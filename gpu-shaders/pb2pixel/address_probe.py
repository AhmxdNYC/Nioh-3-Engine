"""Bounded integer row-base factorization probe; no installation/draws."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import random
import re
import sys
import tempfile
sys.dont_write_bytecode = True


def sha(x): return hashlib.sha256(x).hexdigest()


def certificate(ll, S, allow_uv_dedup=False):
    """Exact modular integer polynomial normalization; other ops opaque/exact.

    All resource calls, float calls, branches and outputs remain in their block
    and order. Integer add/UMad alone normalize in Z/(2^32); no float algebra.
    """
    fn=S.Fn(ll);values={};labels={'%'+b:'BLOCK'+str(i) for i,(b,_) in enumerate(fn.blocks)}
    mask=(1<<32)-1
    def scalar(s):
        if s in values:return values[s]
        return {():int(s)&mask}
    def add(a,b):
        d=dict(a)
        for k,v in b.items():d[k]=(d.get(k,0)+v)&mask
        return {k:v for k,v in d.items() if v}
    def mul(a,b):
        d={}
        for x,u in a.items():
            for y,v in b.items():
                k=tuple(sorted(x+y));d[k]=(d.get(k,0)+u*v)&mask
        return {k:v for k,v in d.items() if v}
    def show(d):return repr(sorted(d.items()))
    def token(t):
        k=t[0]
        if k in labels:return labels[k]
        if k in values:return show(values[k])
        if k.startswith('%dx.'):return k
        raise ValueError('undefined '+k)
    traces=[]
    for _,ins in fn.blocks:
        trace=[]
        for line in ins:
            definition=S.DEF.match(line);rhs=line.split(' = ',1)[-1].strip()
            a=re.fullmatch(r'add i32 (%\w+|-?\d+), (%\w+|-?\d+)',rhs)
            m=re.fullmatch(r'call i32 @dx.op.tertiary.i32\(i32 49, i32 (%\w+|-?\d+), i32 (%\w+|-?\d+), i32 (%\w+|-?\d+)\)',rhs)
            expr=S.TOK.sub(token,rhs)
            if definition:
                if a:values[definition[1]]=add(scalar(a[1]),scalar(a[2]))
                elif m:values[definition[1]]=add(mul(scalar(m[1]),scalar(m[2])),scalar(m[3]))
                else:values[definition[1]]={(sha(expr.encode()),):1}
            pure_uv = allow_uv_dedup and re.search(r'@dx\.op\.(?:unary\.f32\(i32 22,|binary\.f32\(i32 (?:35|36),)', rhs)
            if not definition or ('call ' in rhs and not m and not pure_uv):trace.append(expr)
        traces.append(trace)
    return traces


def rewrite(ll, S, dxcom, expected_body="f01abd800dbbe594ffe931f3f373db599abb749695e031b0064a9825ceeeeabd"):
    assert expected_body in ("f01abd800dbbe594ffe931f3f373db599abb749695e031b0064a9825ceeeeabd",
                             "8cd2f2b9e1ac1a2422d27665ec90454b7cd23845fb5a96e6c879a40c8ee36138")
    assert sha(dxcom.body(ll).encode()) == expected_body
    fn = S.Fn(ll)
    changed = 0
    mad = re.compile(r"  (%v\d+) = call i32 @dx.op.tertiary.i32\(i32 49, i32 (%v\d+), i32 (%v\d+), i32 (%v\d+)\)")
    for _, ins in fn.blocks:
        hits = [(i, mad.fullmatch(x)) for i, x in enumerate(ins) if mad.fullmatch(x)]
        if not hits: continue
        assert len(hits) == 4
        at = hits[0][0]
        assert [i for i, _ in hits] == list(range(at, at + 4))
        vals = [m.groups() for _, m in hits]
        d, w, y0, x0 = vals[0]
        x1, y1 = vals[1][3], vals[2][2]
        assert [v[1:] for v in vals] == [(w,y0,x0),(w,y0,x1),(w,y1,x0),(w,y1,x1)]
        base = re.fullmatch(r"  (%v\d+) = add i32 (%v\d+), 3", ins[at+4])
        assert base
        row0, row1 = f"%row{changed}a", f"%row{changed}b"
        # Every deleted MAD must have exactly one use, in its own address add.
        mapping = {}
        for (v, _, _, x), row in zip(vals, (row0,row0,row1,row1)):
            users = [(b, i) for b, body in fn.blocks for i, s in enumerate(body)
                     if v in S.TOK.findall(s.split(' = ', 1)[-1])]
            assert len(users) == 1
            source = f"add i32 {base[1]}, {v}"
            assert any(s.endswith(source) for s in ins)
            mapping[source] = f"add i32 {row}, {x}"
        for i, line in enumerate(ins):
            for before, after in mapping.items():
                if line.endswith(before): ins[i] = line[:-len(before)] + after
        ins[at:at+5] = [ins[at+4],
            f"  {row0} = call i32 @dx.op.tertiary.i32(i32 49, i32 {w}, i32 {y0}, i32 {base[1]})",
            f"  {row1} = call i32 @dx.op.tertiary.i32(i32 49, i32 {w}, i32 {y1}, i32 {base[1]})"]
        changed += 1
    assert changed == 3
    return fn.emit()


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();sys.path.insert(0,str(a.repo/'gpu-shaders/materials'))
    import dxcom,sfb_rewrite as S,rga_ps
    original=a.out/'original.dxbc'
    assert sha(original.read_bytes()) == '0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268'
    ll=dxcom.dumpbin(str(original));new=rewrite(ll,S,dxcom)
    reference=certificate(ll,S)
    assert reference==certificate(new,S),'whole-body structural mismatch'
    mutant=new.replace(', 3\n', ', 4\n', 1)
    assert mutant!=new and reference!=certificate(mutant,S),'address mutation escaped'
    # Ring Z/(2^32): ((w*y+x)+(base+3)) == ((w*y+(base+3))+x).
    # No nsw/nuw added, no division, no read movement. Finite overflow checks
    # supplement this algebraic identity; no float/sample equivalence inferred.
    rng=random.Random(4203);mask=(1<<32)-1
    for _ in range(4096):
        w,y,x,b=[rng.getrandbits(32) for _ in range(4)]
        assert (((w*y+x)&mask)+((b+3)&mask))&mask == (((w*y+((b+3)&mask))&mask)+x)&mask
    candidate=a.out/'row-base.dxbc';candidate.write_bytes(dxcom.ll_to_container(new))
    actual=dxcom.dumpbin(str(candidate));assert dxcom.body(actual)==dxcom.body(new)
    assert certificate(actual,S)==reference
    po,pn=dxcom.parts(original.read_bytes()),dxcom.parts(candidate.read_bytes())
    assert all(po.get(k)==pn.get(k) for k in ['ISG1','OSG1','PSV0','SFI0'])
    (a.out/'row-base.ll').write_bytes(actual.encode())
    tempfile.tempdir=str(a.out.resolve())
    st,isa=rga_ps.stats(str(a.repo/'gpu-shaders/meshvs/work/1559714c.dxbc'),str(candidate),'gfx1103')
    (a.out/'row-base.gfx1103.isa.txt').write_bytes(isa.encode())
    result=dict(candidate_sha256=sha(candidate.read_bytes()),body_sha256=sha(dxcom.body(actual).encode()),
                isa_sha256=sha(isa.encode()),stats=st,groups=3,overflow_checks=4096,native_admitted=False,
                whole_body_integer_ring_certificate=True,address_mutation_rejected=True,
                scope='Synthetic RGA compiler diagnostic, no draw/performance claim')
    (a.out/'row-base.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))


if __name__=='__main__':main()
