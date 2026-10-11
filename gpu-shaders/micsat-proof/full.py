"""Bounded full-body MIC execution with sample/atomic/UAV event comparison."""
import collections,hashlib,json,pathlib,random,re,struct,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parent;OUT=HERE/'out'
sys.path[:0]=[str(ROOT/'materials'),str(ROOT/'meshcull')]
import psim,dxil_interp as DI
from sfb_rewrite import Fn

class Shader(DI.Shader):
    def _gen_call(self,ins,d,a):
        fn=ins.extra['fn']
        if fn.startswith('dx.op.sampleLevel.'):
            return [d+' = X.sample('+', '.join(a)+')']
        if fn.startswith('dx.op.atomicBinOp.'):
            return [d+' = X.atomic('+', '.join(a[1:])+')']
        return super()._gen_call(ins,d,a)

class Context(DI.Ctx):
    def __init__(self,scene):super().__init__(scene);self.events=[]
    def sample(self,*args):
        self.events.append(('sample',tuple(psim._bits(x) for x in args)))
        return psim.oracle(*args)
    def atomic(self,h,op,a,b,c,value):
        assert op==2,op # MIC InterlockedOr, not lodu's InterlockedAdd
        assert isinstance(h,DI.TypedBuffer)
        old=h.data[a][0] if 0<=a<len(h.data) else 0
        self.events.append(('atomic_or',a,b,c,value,old))
        if 0<=a<len(h.data):h.data[a][0]=(old|value)&0xffffffff
        return old
    def bload(self,h,idx,off,isint):
        result=super().bload(h,idx,off,isint)
        if h in (self.scene.b[(1,0)],self.scene.b[(1,1)]):self.events.append(('uav_read',h.name,idx,off,tuple(result)))
        return result
    def bstore(self,h,idx,off,vals,mask,isint):
        self.events.append(('store',h.name,idx,off,tuple(vals),mask,isint))
        super().bstore(h,idx,off,vals,mask,isint)

def scene(seed):
    rng=random.Random(seed);b={};raw=bytearray(320)
    def i(off,x):struct.pack_into('<I',raw,off,x&0xffffffff)
    def f(off,x):struct.pack_into('<f',raw,off,x)
    i(0,rng.randrange(8));i(4,(seed//8)%2);f(8,.05)
    for start in (16,240):
        for n in range(16):f(start+n*4,1. if n in (0,5,10,15) else 0.)
    for n in range(6):f(80+n*16+12,100.)
    for off in (176,180,192,196):f(off,64.)
    for off in (184,188,200,204):f(off,1/64)
    i(208,(seed//16)%2);i(212,(seed%8)|512);i(216,8);i(220,seed%4);i(232,128);i(236,seed%2);i(304,1)
    b[(2,0)]=DI.CBuffer(raw)
    b[(3,0)]=psim.Res('sampler0');b[(0,0)]=psim.Res('hiz0')
    root=bytearray(16384)
    for n in range(32):
        centre=[rng.uniform(-.7,.7),rng.uniform(-.7,.7),rng.uniform(.1,.8)]
        struct.pack_into('<10fI',root,n*44,*centre,.2,*(x-.15 for x in centre),*(x+.15 for x in centre),0)
    # Matrix-set indirection points to palette offset0, outside bounding records.
    struct.pack_into('<I',root,8192,0)
    for n in range(1,17):b[(0,n)]=DI.StructBuffer(root,1)
    g1m=bytearray(48*32)
    for n in range(32):struct.pack_into('<12I',g1m,n*48,0,0,0,0,0,0,0,0,0,0,0,8192)
    b[(0,17)]=DI.StructBuffer(g1m,48)
    palette=bytearray(16*96)
    for n in range(96):struct.pack_into('<4f',palette,n*16,*(1. if c==n%3 else 0. for c in range(4)))
    b[(0,18)]=DI.StructBuffer(palette,16)
    b[(0,19)]=DI.StructBuffer(bytes(16*32),16)
    b[(0,20)]=DI.StructBuffer(b''.join(struct.pack('<3I',0,n,n%2 if '--alias-threads' in sys.argv else n) for n in range(8)),12)
    w,h,views=4,4,4
    sat=[w,h,views]+[rng.randrange(4) if seed%3 else 0 for _ in range(w*h*views)]
    if '--valid-sat' in sys.argv:
        w,h,views=(1,2,4,8)[seed%4],(1,2,4,8)[(seed//4)%4],4
        sat=[w,h,views]
        for view in range(views):
            grid=[[rng.randrange(2) if seed%3 else 0 for x in range(w)] for y in range(h)]
            for y in range(h):
                for x in range(w):
                    grid[y][x]+=(grid[y-1][x] if y else 0)+(grid[y][x-1] if x else 0)-(grid[y-1][x-1] if x and y else 0)
            sat.extend(v for row in grid for v in row)
    b[(0,21)]=DI.TypedBuffer(sat,isint=True)
    initial=(5<<(seed%4)) if seed%8==2 else 0
    b[(1,0)]=DI.StructBuffer(struct.pack('<I',initial)*256,4);b[(1,0)].name='cull'
    b[(1,1)]=DI.TypedBuffer([0]*256,isint=True);b[(1,1)].name='vis'
    return DI.Scene(b)

def run(sh,seed,cov):
    sc=scene(seed);ctx=Context(sc);F=sh._build(ctx,{k:psim.Arr(v) for k,v in sh.consts.items()},{},lambda p:None)
    order=list(range(8))
    if '--reverse-threads' in sys.argv:order.reverse()
    if '--shuffle-threads' in sys.argv:random.Random(seed^0x734a).shuffle(order)
    for tid in order:
        ctx.tid=(tid,0,0);ctx.who=ctx.tid;R=[None]*sh.nregs
        for rr,n,ty in sh.alloca_init:R[rr]=psim.Arr([0.]*n if ty=='float' else [0]*n)
        at=0;steps=0
        while at>=0:
            cov[at]+=1;at=F[at](R);steps+=1
            if steps>20000:raise RuntimeError('unbounded shader path')
    return ctx.events,DI.uav_state(sc)

def mutants(text):
    fn=Fn(text);targets=[]
    for name,lines in fn.blocks:
        if sum('bufferLoad.i32' in x and 'Handle %v3,' in x for x in lines)==4 and sum(' = select i1' in x for x in lines)>=3:
            targets.append(name)
    assert len(targets)==3,targets
    out={}
    for name in targets:
        f=Fn(text);lines=dict(f.blocks)[name]
        at=next(i for i,x in enumerate(lines) if ' = select i1' in x)
        m=re.fullmatch(r'(.*select i1 %\w+, i32 )(%\w+|0)(, i32 )(%\w+|0)',lines[at]);assert m
        lines[at]=m[1]+m[4]+m[3]+m[2];out['SAT_'+name]=f.emit()
    out['drop_store']='\n'.join(x for x in text.split('\n') if not ('call void @dx.op.bufferStore.' in x))
    lines=text.split('\n');at=next(i for i,x in enumerate(lines) if '= call ' in x and '@dx.op.sampleLevel.' in x)
    assert ', i32 0,' in lines[at];lines[at]=lines[at].replace(', i32 0,',', i32 1,',1)
    out['sample_offset']='\n'.join(lines)
    lines=text.split('\n');at=next(i for i,x in enumerate(lines) if '= call ' in x and '@dx.op.atomicBinOp.' in x)
    lines[at]=re.sub(r'i32 %\w+\)$','i32 0)',lines[at]);out['atomic_value_zero']='\n'.join(lines)
    return out

def validate_count(n):
    if n<=0:raise ValueError('scene count must be positive')

def validate_binding(out=OUT):
    receipt=json.loads((out/'binding.json').read_text())
    old=(out/'original.ll').read_text();new=(out/'current.ll').read_text()
    import dxcom,micsat
    for txt,name,key in ((old,'original','original'),(new,'current','candidate')):
        blob=(out/(name+'.dxbc')).read_bytes()
        assert blob[:4]==b'DXBC'
        assert hashlib.sha256(blob).hexdigest()==receipt[key+'_sha256']
        assert blob[4:20].hex()==receipt[key+'_digest']
        assert hashlib.sha256(dxcom.body(txt).encode()).hexdigest()==receipt[key+'_body_sha256']
        assert dxcom.body(dxcom.dumpbin(str(out/(name+'.dxbc'))))==dxcom.body(txt)
    assert receipt['original_digest']=='e31c389f6d4a38de7b1b784b87c710d2'
    assert hashlib.sha256(pathlib.Path(micsat.__file__).read_bytes()).hexdigest()==receipt['rewriter_sha256']
    rewritten,info=micsat.rewrite(old)
    assert len(info)==3 and dxcom.body(rewritten)==dxcom.body(new)
    return receipt,old,new

def main():
    n=int(sys.argv[1]) if len(sys.argv)>1 else 32;validate_count(n)
    receipt,old,new=validate_binding()
    A=Shader(old,'MICoriginal');B=Shader(new,'MICcurrent');ca=[0]*len(A.m.blocks);cb=[0]*len(B.m.blocks)
    totals=collections.Counter()
    start=int(sys.argv[sys.argv.index('--start')+1]) if '--start' in sys.argv else 0
    cached={}
    for seed in range(start,start+n):
        a=run(A,seed,ca);b=run(B,seed,cb);assert a==b,('mismatch',seed)
        cached[seed]=a
        totals.update(x[0] for x in a[0])
    mutation_results={}
    if '--mutants' in sys.argv:
        for name,text in mutants(new).items():
            mutant=Shader(text,name);caught=None
            for seed in range(start,start+n):
                actual=run(mutant,seed,[0]*len(mutant.m.blocks))
                if actual!=cached[seed]:caught=seed;break
            mutation_results[name]=caught
        assert all(v is not None for v in mutation_results.values()),mutation_results
    result=dict(scenes=n,seed_start=start,sat_mode='cumulative' if '--valid-sat' in sys.argv else 'arbitrary',
                alias_threads='--alias-threads' in sys.argv,thread_order='reverse' if '--reverse-threads' in sys.argv else 'shuffled' if '--shuffle-threads' in sys.argv else 'ascending',
                threads=n*8,events=dict(totals),original_blocks=sum(bool(x) for x in ca),original_total=len(ca),
                candidate_blocks=sum(bool(x) for x in cb),candidate_total=len(cb),
                missing_original=[x.name for i,x in enumerate(A.m.blocks) if not ca[i]],
                original_sha256=receipt['original_sha256'],candidate_sha256=receipt['candidate_sha256'],
                mutation_first_detected_seed=mutation_results,
                harness_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                interpreter_sha256=hashlib.sha256(pathlib.Path(DI.__file__).read_bytes()).hexdigest(),
                oracle_sha256=hashlib.sha256(pathlib.Path(psim.__file__).read_bytes()).hexdigest(),
                mutations_tested=bool(mutation_results),
                status='FINITE_FULL_COVERAGE_MODEL_PASS' if all(ca) and all(cb) else 'FINITE_PARTIAL_COVERAGE_MODEL_PASS')
    suffix=('cumulative' if '--valid-sat' in sys.argv else 'arbitrary')+('-aliased' if '--alias-threads' in sys.argv else '')+('-reversed' if '--reverse-threads' in sys.argv else '-shuffled' if '--shuffle-threads' in sys.argv else '')
    (OUT/f'full-{suffix}-{start}.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
