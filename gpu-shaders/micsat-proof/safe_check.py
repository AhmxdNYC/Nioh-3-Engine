"""Bound safe candidate: full outputs and no-new-t21-address finite gate."""
import collections,hashlib,json,pathlib,re,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import full,safe,dxcom
OUT=HERE/'out'/'safe'
def sha(x):return hashlib.sha256(x).hexdigest()

class Tracked(full.Context):
    last=None
    def __init__(self,scene):
        super().__init__(scene);self.sat=[];self.other=[];Tracked.last=self
    def bload(self,h,idx,off,isint):
        if h is self.scene.b[(0,21)]:self.sat.append((self.tid,idx))
        else:
            slot=next(k for k,v in self.scene.b.items() if v is h)
            self.other.append((self.tid,slot,idx,off,isint))
        return super().bload(h,idx,off,isint)

def validate():
    prior,old,_=full.validate_binding()
    receipt=json.loads((OUT/'binding.json').read_text());blob=(OUT/'candidate.dxbc').read_bytes()
    assert sha(blob)==receipt['candidate_sha256'] and blob[4:20].hex()==receipt['candidate_digest']
    assert receipt['original_sha256']==prior['original_sha256']
    assert receipt['base_rewriter_sha256']==prior['rewriter_sha256']
    assert sha(pathlib.Path(safe.__file__).read_bytes())==receipt['safe_rewriter_sha256']
    text=(OUT/'candidate.ll').read_text()
    assert sha(dxcom.body(text).encode())==receipt['candidate_body_sha256']
    assert dxcom.body(dxcom.dumpbin(str(OUT/'candidate.dxbc')))==dxcom.body(text)
    fresh,info=safe.rewrite(old);assert len(info)==3 and dxcom.body(fresh)==dxcom.body(text)
    return receipt,old,text

def run(shader,seed,cov):
    output=full.run(shader,seed,cov);ctx=Tracked.last
    return output,set(ctx.sat),ctx.other

def mutants(text):
    targets=[name for name,lines in full.Fn(text).blocks if sum(' = select i1' in x for x in lines)==6]
    assert len(targets)==3,targets
    out={}
    for name in targets:
        f=full.Fn(text);lines=dict(f.blocks)[name]
        at=[i for i,x in enumerate(lines) if ' = select i1' in x][1]
        m=re.fullmatch(r'(.*select i1 %\w+, i32 )(%\w+|0)(, i32 )(%\w+|0)',lines[at]);assert m
        lines[at]=m[1]+m[4]+m[3]+m[2];out['SAT_'+name]=f.emit()
    out['drop_store']='\n'.join(x for x in text.split('\n') if 'call void @dx.op.bufferStore.' not in x)
    lines=text.split('\n');at=next(i for i,x in enumerate(lines) if '= call ' in x and '@dx.op.sampleLevel.' in x)
    lines[at]=lines[at].replace(', i32 0,',', i32 1,',1);out['sample_offset']='\n'.join(lines)
    lines=text.split('\n');at=next(i for i,x in enumerate(lines) if '= call ' in x and '@dx.op.atomicBinOp.' in x)
    lines[at],changed=re.subn(r'i32 %\w+\)(?=\s*(;.*)?$)','i32 0)',lines[at]);assert changed==1
    out['atomic_value_zero']='\n'.join(lines)
    return out

def main():
    n=int(sys.argv[1]) if len(sys.argv)>1 else 64;full.validate_count(n)
    receipt,old,new=validate();full.Context=Tracked
    A=full.Shader(old,'original');B=full.Shader(new,'safe')
    ca=[0]*len(A.m.blocks);cb=[0]*len(B.m.blocks)
    start=int(sys.argv[sys.argv.index('--start')+1]) if '--start' in sys.argv else 0
    baseline={};totals=collections.Counter();extra_addresses=set()
    for seed in range(start,start+n):
        a,ar,ao=run(A,seed,ca);b,br,bo=run(B,seed,cb)
        assert a==b,('output',seed);assert ao==bo,('other_resource_read',seed)
        assert br<=ar,('new_t21_address',seed,br-ar)
        baseline[seed]=a;totals.update(x[0] for x in a[0])
    mutations={}
    if '--mutants' in sys.argv:
        for name,text in mutants(new).items():
            sh=full.Shader(text,name);caught=None
            for seed in range(start,start+n):
                actual,_,_=run(sh,seed,[0]*len(sh.m.blocks))
                if actual!=baseline[seed]:caught=seed;break
            mutations[name]=caught
        assert all(x is not None for x in mutations.values()),mutations
    result=dict(scenes=n,threads=n*8,seed_start=start,arguments=sys.argv[1:],events=dict(totals),
                original_blocks=sum(bool(x) for x in ca),original_total=len(ca),candidate_blocks=sum(bool(x) for x in cb),candidate_total=len(cb),
                candidate_sha256=receipt['candidate_sha256'],no_new_t21_addresses=True,other_resource_reads_equal=True,
                mutations=mutations,harness_sha256=sha(pathlib.Path(__file__).read_bytes()),
                base_harness_sha256=sha(pathlib.Path(full.__file__).read_bytes()),
                status='FINITE_FULL_COVERAGE_MODEL_PASS' if all(ca) and all(cb) else 'FINITE_PARTIAL_COVERAGE_MODEL_PASS')
    (OUT/f'proof-{start}.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
