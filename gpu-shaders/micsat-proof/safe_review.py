"""Explicit address/header admission negatives; authored-candidate self-review."""
import hashlib,json,pathlib,re,struct,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import safe_check as C
F=C.full

def mutants(text):
    base=F.Fn(text);out=[]
    for name,lines in base.blocks:
        selects=[i for i,x in enumerate(lines) if ' = select i1' in x]
        if len(selects)!=6:continue
        for corner,at in enumerate(selects[::2]):
            fn=F.Fn(text);xs=dict(fn.blocks)[name]
            m=re.fullmatch(r'(\s*%\w+) = select i1 %\w+, i32 (%\w+|0), i32 (%\w+|0)',xs[at]);assert m
            idx=m[2] if m[2]!='0' else m[3]
            xs[at]=m[1]+' = add i32 '+idx+', 0'
            out.append((f'address_{name}_{corner}','normal',fn.emit()))
    assert len(out)==9
    for header in (2,1):
        targets=[name for name,xs in base.blocks if any('bufferLoad.i32' in x and 'Handle %v3,' in x and f', i32 {header}, i32 undef)' in x for x in xs)]
        assert len(targets)==3,targets
        for name in targets:
            fn=F.Fn(text);blocks=dict(fn.blocks);xs=blocks[name]
            pred=[b for b,ys in fn.blocks if name in re.findall(r'label %(\w+)',ys[-1])]
            assert len(pred)==1,pred
            at=next(i for i,x in enumerate(xs) if 'bufferLoad.i32' in x and 'Handle %v3,' in x and f', i32 {header}, i32 undef)' in x)
            pair=xs[at:at+2];assert 'extractvalue' in pair[1]
            del xs[at:at+2];blocks[pred[0]][-1:-1]=pair
            out.append((f'hoist_T{header}_{name}','width0' if header==2 else 'views0',fn.emit()))
    return out

def main():
    receipt,old,new=C.validate();F.Context=C.Tracked
    stock_scene=F.scene;mode='normal'
    def scene(seed):
        sc=stock_scene(seed)
        raw=bytearray(b''.join(struct.pack('<4I',*row) for row in sc.b[(2,0)].i))
        for off in (176,180,192,196):struct.pack_into('<f',raw,off,1.)
        for off in (184,188,200,204):struct.pack_into('<f',raw,off,0.)
        sc.b[(2,0)]=F.DI.CBuffer(raw)
        if mode=='width0':sc.b[(0,21)]=F.DI.TypedBuffer([0],isint=True)
        if mode=='views0':sc.b[(0,21)]=F.DI.TypedBuffer([4,4,0],isint=True)
        return sc
    F.scene=scene;A=F.Shader(old,'original');B=F.Shader(new,'safe');cache={};results=[]
    for name,mode,text in mutants(new):
        shader=F.Shader(text,name);caught=None
        for seed in range(64):
            key=(mode,seed)
            if key not in cache:
                a,ar,ao=C.run(A,seed,[0]*len(A.m.blocks));b,br,bo=C.run(B,seed,[0]*len(B.m.blocks))
                assert a==b and br<=ar and ao==bo,('baseline_failed',key)
                cache[key]=(a,ar)
            original,reads=cache[key]
            actual,addresses,_=C.run(shader,seed,[0]*len(shader.m.blocks))
            extra=addresses-reads
            if extra:
                assert actual==original,('mutation unexpectedly changed output',name,seed)
                caught=dict(name=name,mode=mode,seed=seed,outputs_unchanged=True,
                            new_addresses=[dict(thread=list(t),index=i) for t,i in sorted(extra)])
                break
        assert caught is not None,('address gate did not reject mutant',name)
        results.append(caught)
    result=dict(status='ADDRESS_AND_HEADER_NEGATIVES_PASS',candidate_sha256=receipt['candidate_sha256'],
                negative_cases=len(results),baseline_scene_variants=len(cache),results=results,
                harness_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                review_scope='author self-check; not independent peer review')
    (C.OUT/'address-review.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
