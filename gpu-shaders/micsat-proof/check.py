"""Actual three-corner CFG/select check. Deliberately not full MIC proof."""
import hashlib,json,pathlib,random,re,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parent
sys.path[:0]=[str(ROOT/'meshcull'),str(ROOT/'materials')]
import micsat
from sfb_rewrite import Fn,DEF,TOK

def signed(x):return x if x<0x80000000 else x-0x100000000
class Capture(Fn):
    captured=None
    def emit(self):
        Capture.captured={b:list(xs) for b,xs in self.blocks}
        return super().emit()

def execute(blocks,start,env,buffer,mode):
    env=dict(env);cur=start;pred=None;reads=[]
    for _ in range(20):
        for line in blocks[cur]:
            x=line.strip()
            if x.startswith('br '):
                if re.match(r'br i1',x):
                    m=re.fullmatch(r'br i1 (%\w+), label %(\w+), label %(\w+)',x)
                    # Final hit comparison branches out of the selected region.
                    if m[2] not in blocks and m[3] not in blocks:return env[m[1]],reads
                    pred,cur=cur,m[2] if env[m[1]] else m[3]
                else:pred,cur=cur,re.search(r'label %(\w+)',x)[1]
                break
            dest,rhs=x.split(' = ',1)
            def v(t):return env[t] if t.startswith('%') else int(t)&0xffffffff
            if 'bufferLoad' in rhs:
                idx=v(re.search(r'Handle %\w+, i32 (%\w+|\d+), i32 undef',rhs)[1]);reads.append(idx)
                if idx>=len(buffer):
                    if mode=='strict':raise IndexError(idx)
                    value=0 if mode=='zero' else 0xdeadbeef
                else:value=buffer[idx]
                result=(value,)
            elif rhs.startswith('extractvalue'):result=v(re.search(r'(%\w+), 0$',rhs)[1])[0]
            elif rhs.startswith('phi'):
                opts=re.findall(r'\[ (%\w+|0), %(\w+) \]',rhs);result=v(next(a for a,b in opts if b==pred))
            elif rhs.startswith('select'):
                m=re.fullmatch(r'select i1 (%\w+), i32 (%\w+|0), i32 (%\w+|0)',rhs);result=v(m[2]) if v(m[1]) else v(m[3])
            elif rhs.startswith('icmp'):
                m=re.fullmatch(r'icmp (slt|sge|eq|ne) i32 (%\w+|\d+), (%\w+|\d+)',rhs);a,b=v(m[2]),v(m[3])
                result={'slt':lambda:signed(a)<signed(b),'sge':lambda:signed(a)>=signed(b),'eq':lambda:a==b,'ne':lambda:a!=b}[m[1]]()
            else:
                m=re.fullmatch(r'(add|sub|xor|srem) i32 (%\w+|-?\d+), (%\w+|-?\d+)',rhs);assert m,rhs
                a,b=v(m[2]),v(m[3]);op=m[1]
                if op=='srem':
                    aa,bb=signed(a),signed(b);assert bb and not(aa==-2147483648 and bb==-1)
                    q=abs(aa)//abs(bb);q=-q if (aa<0)!=(bb<0) else q;result=aa-q*bb
                else:result={'add':lambda:a+b,'sub':lambda:a-b,'xor':lambda:a^b}[op]()
                result&=0xffffffff
            env[dest]=result
    raise AssertionError('region did not exit')

def main():
    source=ROOT/'meshvs/work/mic/mic0.ll';text=source.read_text();original=dict(Fn(text).blocks)
    binding_path=HERE/'out/binding.json'
    if binding_path.exists():
        import dxcom
        binding=json.loads(binding_path.read_text())
        assert dxcom.body(text)==dxcom.body((HERE/'out/original.ll').read_text())
        assert hashlib.sha256(pathlib.Path(micsat.__file__).read_bytes()).hexdigest()==binding['rewriter_sha256']
    saved=micsat.Fn;micsat.Fn=Capture
    try:rewritten,info=micsat.rewrite(text)
    finally:micsat.Fn=saved
    transformed=Capture.captured;rng=random.Random(310);cases=0;witness=None;polarity_mutant=False
    if binding_path.exists():
        assert dxcom.body(rewritten)==dxcom.body((HERE/'out/current.ll').read_text())
    for _,head,_ in info:
        # Region begins at original first corner condition. Prefix math/header is outside this proof.
        prefix=original[head][:-1]
        start_rewrite=next(i for i,x in enumerate(transformed[head]) if x not in prefix and not ('bufferLoad' in x and 'i32 1,' in x))
        # The first moved corner instruction is xor width,-1 in all three copies.
        start_rewrite=next(i for i,x in enumerate(transformed[head]) if '= xor i32' in x and x.endswith(', -1'))
        new={head:transformed[head][start_rewrite:]}
        mutant={head:list(new[head])}
        si=next(i for i,x in enumerate(mutant[head]) if '= select i1' in x)
        sm=re.fullmatch(r'(.*select i1 %\w+, i32 )(%\w+|0)(, i32 )(%\w+|0)',mutant[head][si])
        mutant[head][si]=sm[1]+sm[4]+sm[3]+sm[2]
        old={head:[original[head][-1]]};todo=re.findall(r'label %(\w+)',old[head][-1])
        while todo:
            b=todo.pop()
            if b in old:continue
            xs=original[b]
            # Last corner block contains the final SAT subtraction comparison.
            if any(' = sub i32 0,' in x for x in xs):old[b]=xs;continue
            old[b]=xs;todo+=re.findall(r'label %(\w+)',xs[-1])
        defs={DEF.match(x)[1] for xs in old.values() for x in xs if DEF.match(x)}
        used={t for xs in old.values() for x in xs for t in TOK.findall(x.split(' = ',1)[-1]) if t.startswith('%v')}
        inputs=used-defs
        width=re.search(r'= xor i32 (%\w+), -1',new[head][0])[1]
        condition=re.search(r'br i1 (%\w+)',old[head][0])[1]
        for k in range(1000):
            env={v:rng.getrandbits(32) for v in inputs};env[width]=1+rng.randrange(16);env[condition]=bool(k%2)
            buffer=[rng.getrandbits(32) for _ in range(32)]
            for mode in ('zero','arbitrary'):
                a,ar=execute(old,head,env,buffer,mode);b,br=execute(new,head,env,buffer,mode);assert a==b
                bad,_=execute(mutant,head,env,buffer,mode);polarity_mutant |= bad!=a
                cases+=1
        if head=='b393':
            env={v:0 for v in inputs};env.update({'%v387':4,'%v413':3,'%v417':False})
            buf=[4,4,1]+[0]*16
            a,ar=execute(old,head,env,buf,'strict')
            try:execute(new,head,env,buf,'strict');raise AssertionError('missing bounds witness')
            except IndexError as e:witness=dict(width=4,height=4,view=0,original_reads=ar,candidate_new_oob=e.args[0])
    assert witness and polarity_mutant
    out=HERE/'out';out.mkdir(exist_ok=True)
    result=dict(corner_comparisons=cases,copies=len(info),bounds_witness=witness,polarity_mutant_rejected=polarity_mutant,
                source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                scope='actual corner CFG vs actual rewritten selects; header/float/full-CS not proved')
    (out/'receipt.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
