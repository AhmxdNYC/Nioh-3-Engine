"""PS05ad local row preparation, with ordered opaque-operation identities."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import re
import sys
import tempfile
sys.dont_write_bytecode=True
SHA='4173c9dce9ad919b5238426f345de1d1d648b1c47efeaf1ea2c59e01e558dd4b'
BODY='a4dcfea86c85c3ec50f02b20da99382d431fc27b807c2aa9dc8c24e41dd9e3c1'
MASK=0xffffffff
ADD=re.compile(r'add i32 (%\w+|-?\d+), (%\w+|-?\d+)')
MAD=re.compile(r'call i32 @dx.op.tertiary.i32\(i32 49, i32 (%\w+|-?\d+), i32 (%\w+|-?\d+), i32 (%\w+|-?\d+)\)')
def sha(x):return hashlib.sha256(x).hexdigest()


def ordered_certificate(ll,S):
    """Normalize ONLY wrapping add/UMad; preserve every other instruction site.

    Each opaque definition has its own ordinal, even repeated samples/loads.
    No resource stability, float equality or pure-call inference is made here.
    """
    f=S.Fn(ll);labels={'%'+b:'BLOCK'+str(i) for i,(b,_) in enumerate(f.blocks)}
    values={};ordinal=0
    for _,lines in f.blocks:
        for line in lines:
            d=S.DEF.match(line);rhs=line.split(' = ',1)[-1].strip()
            if d and not ADD.fullmatch(rhs) and not MAD.fullmatch(rhs):
                values[d[1]]={(f'opaque{ordinal}',):1};ordinal+=1
    def scalar(t):return values[t] if t.startswith('%') else {():int(t)&MASK}
    def plus(a,b):
        r=dict(a)
        for k,v in b.items():r[k]=(r.get(k,0)+v)&MASK
        return {k:v for k,v in r.items() if v}
    def times(a,b):
        r={}
        for x,u in a.items():
            for y,v in b.items():
                k=tuple(sorted(x+y));r[k]=(r.get(k,0)+u*v)&MASK
        return {k:v for k,v in r.items() if v}
    def tok(m):
        t=m[0]
        if t in labels:return labels[t]
        if t in values:return repr(sorted(values[t].items()))
        if t.startswith('%dx.'):return t
        raise ValueError('unresolved actual token '+t)
    trace=[]
    for b,lines in f.blocks:
        block=[]
        for line in lines:
            d=S.DEF.match(line);rhs=line.split(' = ',1)[-1].strip();a=ADD.fullmatch(rhs);m=MAD.fullmatch(rhs)
            if d and a:values[d[1]]=plus(scalar(a[1]),scalar(a[2]))
            elif d and m:values[d[1]]=plus(times(scalar(m[1]),scalar(m[2])),scalar(m[3]))
            else:block.append(S.TOK.sub(tok,rhs))
        trace.append(block)
    return trace


def rewrite(ll,S,dxcom):
    assert sha(dxcom.body(ll).encode())==BODY
    f=S.Fn(ll);changed=[];retained=[]
    # Snapshot original users before any mutation, including cross-block consumers.
    users={}
    for b,lines in f.blocks:
        for line in lines:
            for t in S.TOK.findall(line.split(' = ',1)[-1]):users.setdefault(t,[]).append((b,line))
    for b,lines in f.blocks:
        hits=[]
        for line in lines:
            d=S.DEF.match(line);m=MAD.fullmatch(line.split(' = ',1)[-1].strip())
            if d and m:hits.append((d[1],*m.groups()))
        if not hits:continue
        assert len(hits)==4
        d,w,y0,x0=hits[0];x1=hits[1][3];y1=hits[2][2]
        assert [z[1:] for z in hits]==[(w,y0,x0),(w,y0,x1),(w,y1,x0),(w,y1,x1)]
        if any(len(users[z[0]])!=1 or users[z[0]][0][0]!=b for z in hits):
            retained.append(b);continue
        addresses=[];bases=[]
        for d,_,_,_ in hits:
            line=users[d][0][1];m=ADD.fullmatch(line.split(' = ',1)[-1].strip());assert m and m[2]==d
            addresses.append(line);bases.append(m[1])
        assert len(set(bases))==1;base=bases[0]
        remove={z[0] for z in hits};replacement={};rows=[f'%row{b}a',f'%row{b}b']
        for i,(line,(_,_,y,x)) in enumerate(zip(addresses,hits)):
            out=S.DEF.match(line)[1];new=[];row=rows[i//2]
            if i%2==0:new.append(f'  {row} = call i32 @dx.op.tertiary.i32(i32 49, i32 {w}, i32 {y}, i32 {base})')
            new.append(f'  {out} = add i32 {row}, {x}');replacement[line]=new
        rewritten=[]
        for line in lines:
            d=S.DEF.match(line)
            if d and d[1] in remove:continue
            rewritten.extend(replacement.get(line,[line]))
        lines[:]=rewritten;changed.append(b)
    assert changed==['b225','b387','b540','b709','b797','b895','b981','b1141','b1346']
    assert retained==['b309','b463']
    return f.emit(),changed,retained


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--original',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--assemble',action='store_true');p.add_argument('--isa',action='store_true');a=p.parse_args()
    sys.path.insert(0,str(a.repo/'gpu-shaders/materials'));import dxcom,sfb_rewrite as S
    original=a.original.read_bytes();assert sha(original)==SHA
    ll=dxcom.dumpbin(str(a.original));candidate,groups,retained=rewrite(ll,S,dxcom)
    reference=ordered_certificate(ll,S);assert reference==ordered_certificate(candidate,S)
    # Actual-body mutations must not disappear through repeated-load identity folding.
    mutants={
      'base':candidate.replace(', 3\n',', 4\n',1),
      'resource':candidate.replace('i32 1, i32 31, i1 false','i32 1, i32 30, i1 false',1),
      'float_operand':candidate.replace('fmul fast float','fadd fast float',1),
    }
    # Equal-looking repeated cbuffer reads must retain separate result identity.
    badfn=S.Fn(candidate);seen={};pair=None
    for _,lines in badfn.blocks:
        for line in lines:
            d=S.DEF.match(line);rhs=line.split(' = ',1)[-1]
            if d and '@dx.op.cbufferLoadLegacy.' in rhs:
                if rhs in seen:pair=(seen[rhs],d[1]);break
                seen[rhs]=d[1]
        if pair:break
    assert pair
    for _,lines in badfn.blocks:
        for i,line in enumerate(lines):
            pieces=line.split(' = ',1)
            pieces[-1]=S.TOK.sub(lambda m:pair[0] if m[0]==pair[1] else m[0],pieces[-1])
            lines[i]=' = '.join(pieces)
    mutants['distinct_read_identity']=badfn.emit()
    for name,bad in mutants.items():
        assert bad!=candidate,name
        assert ordered_certificate(bad,S)!=reference,name
    rng=random.Random(0x05ad1715)
    for _ in range(4096):
        w,y,x,base=[rng.getrandbits(32) for _ in range(4)]
        assert (((w*y+x)&MASK)+base)&MASK==(((w*y+base)&MASK)+x)&MASK
    a.out.mkdir(parents=True,exist_ok=True);(a.out/'local-row.ll').write_bytes(candidate.encode())
    result=dict(original_shader_sha256=SHA,original_body_sha256=BODY,candidate_body_sha256=sha(dxcom.body(candidate).encode()),
        changed_blocks=groups,retained_multiple_consumer_blocks=retained,removed_umads=18,integer_overflow_cases=4096,
        mutation_negatives=list(mutants),ordered_whole_body_match=True,native='UNADMITTED',codegen='PENDING',
        dxc_sha256=sha(Path(dxcom.DXC).read_bytes()),script_sha256=sha(Path(__file__).read_bytes()),
        dxcom_sha256=sha(Path(dxcom.__file__).read_bytes()),parser_sha256=sha(Path(S.__file__).read_bytes()))
    if a.assemble or a.isa:
        binary=dxcom.ll_to_container(candidate);dst=a.out/'local-row.dxbc';dst.write_bytes(binary)
        actual=dxcom.dumpbin(str(dst));assert dxcom.body(actual)==dxcom.body(candidate)
        assert ordered_certificate(actual,S)==reference
        op,cp=dxcom.parts(original),dxcom.parts(binary)
        assert all(op.get(k)==cp.get(k) for k in ('ISG1','OSG1','PSV0','SFI0'))
        result['candidate_shader_sha256']=sha(binary)
    if a.isa:
        import rga_ps
        tempfile.tempdir=str(a.out.resolve());vs=a.repo/'gpu-shaders/meshvs/work/70d0ee9f.dxbc'
        assert sha(vs.read_bytes())=='83d64677954ab9c2368b032ea022659353a701bf6a1daf07c37eeaf93a6d55bf'
        vll=dxcom.dumpbin(str(vs));root=rga_ps.root_sig(ll,vll)
        assert root==rga_ps.root_sig(actual,vll)
        result.update(vs_sha256=sha(vs.read_bytes()),generated_root_sha256=sha(root.encode()),
            generated_gpso_sha256=sha(rga_ps.gpso(vll).encode()),rga_helper_sha256=sha(Path(rga_ps.__file__).read_bytes()))
        for label,path in [('original',a.original),('candidate',dst)]:
            st,isa=rga_ps.stats(str(vs),str(path),'gfx1103');(a.out/(label+'.isa.txt')).write_bytes(isa.encode())
            result[label+'_stats']=st;result[label+'_isa_sha256']=sha(isa.encode());print(label,st,flush=True)
        before,after=result['original_stats'],result['candidate_stats']
        good=after['instructions']<before['instructions'] and all(after[k]<=before[k] for k in ('vgprs','sgprs','scratch','buffer_loads','samples','branches'))
        result['codegen']='PROMISING_OFFLINE_ONLY' if good else 'REJECT_RESOURCE_OR_INSTRUCTION_GATE'
        result['scope']='Synthetic RGA root/eight RGBA16F targets, no native performance claim'
        result['rga_sha256']=sha(Path(rga_ps.RGA).read_bytes())
    (a.out/'local-row.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))


if __name__=='__main__':main()
