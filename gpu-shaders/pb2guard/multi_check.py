"""Changed 72-instruction region interpreter; bit-exact integers/bitcasts."""
import hashlib,json,pathlib,random,re,sys
sys.dont_write_bytecode=True
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from multi import OUT,rewrite,TOK,DEF,Fn
MASK=(1<<32)-1
def signed(x):return x if x<1<<31 else x-(1<<32)
def execute(lines,inputs,first,seed):
    env=dict(inputs);trace=[]
    def val(x):return env[x] if x.startswith('%') else int(x)
    def memory(key):return int.from_bytes(hashlib.sha256((str(seed)+':'+str(key)).encode()).digest()[:4],'little')
    for line in lines:
        if line.strip().startswith('br '):continue
        dest,rhs=line.strip().split(' = ',1)
        if 'waveReadLaneFirst' in rhs:result=first
        elif 'createHandle' in rhs:
            result=val(re.search(r'i32 57, i8 0, i32 0, i32 (%\w+)',rhs)[1])
        elif 'rawBufferLoad' in rhs:
            m=re.search(r'Handle (%\w+), i32 (%\w+), i32 (undef|\d+), i8 (\d+), i32 4',rhs);assert m,rhs
            h,index,off,mask=val(m[1]),val(m[2]),m[3],int(m[4]);trace.append((h,index,off,mask))
            result=tuple(memory((h,index,off,c)) if mask>>c&1 else None for c in range(4))
        elif 'cbufferLoadLegacy' in rhs:
            m=re.search(r'Handle (%\w+), i32 (\d+)',rhs);trace.append(('cbuffer',val(m[1]),int(m[2])))
            result=(seed&65535,0,0,0) # ensures the preserved nsw add is defined
        elif rhs.startswith('extractvalue'):
            m=re.search(r'(%\w+), (\d+)$',rhs);result=val(m[1])[int(m[2])];assert result is not None
        elif rhs.startswith('bitcast'):
            m=re.fullmatch(r'bitcast i32 (%\w+) to float',rhs);assert m
            result=('float_bits',val(m[1]))
        elif rhs.startswith('icmp ne'):
            m=re.fullmatch(r'icmp ne i32 (%\w+), (\d+)',rhs);result=val(m[1])!=val(m[2])
        elif rhs.startswith('select'):
            m=re.fullmatch(r'select i1 (%\w+), i32 (%\w+), i32 (%\w+)',rhs);result=val(m[2]) if val(m[1]) else val(m[3])
        else:
            m=re.fullmatch(r'(add|mul|shl|lshr|and) ((?:(?:nuw|nsw) )*)i32 (%\w+|\d+), (%\w+|\d+)',rhs);assert m,rhs
            op,flags=m[1],m[2];a,b=val(m[3]),val(m[4])
            result={'add':lambda:a+b,'mul':lambda:a*b,'shl':lambda:a<<b,'lshr':lambda:a>>b,'and':lambda:a&b}[op]()
            if op in ('shl','lshr'):assert b<32
            if 'nuw' in flags:assert 0<=result<=MASK
            if 'nsw' in flags:
                s={'add':lambda:signed(a)+signed(b),'mul':lambda:signed(a)*signed(b),'shl':lambda:signed(a)<<b}[op]()
                assert -(1<<31)<=s<(1<<31)
            result&=MASK
        env[dest]=result
    return env,trace

def inspect_cfg(text, original, witness):
    """Validate emitted SSA/edges and return actual suffixes with stable names.

    Do not trust witness clone instructions or maps: derive mappings from the
    emitted definitions, then check every clone operand and every escaping use.
    """
    old=Fn(original).blocks;new=Fn(text).blocks
    at=next(i for i,(b,_) in enumerate(old) if b=='b35')
    assert len(new)==len(old)+3
    guard,uniform,mixed,join=new[at:at+4]
    suffix=witness['original'];defs=[DEF.match(x)[1] for x in suffix]
    outside=[x for i,(_,xs) in enumerate(old) for x in (xs[:-len(suffix)-1] if i==at else xs)]
    escaping=set(defs)&{t for x in outside for t in TOK.findall(x)}
    live=witness['live_out'];assert len(live)==len(set(live)) and set(live)==escaping
    assert len(join[1])==len(live)+1
    # Map every original block/value to its emitted identity, including joins.
    paired=list(zip(old[:at+1],new[:at+1]))+list(zip(old[at+1:],new[at+4:]))
    rename={'%'+a[0]:'%'+b[0] for a,b in paired}
    for i,(a,b) in enumerate(paired):
        ax=a[1][:-len(suffix)-1] if i==at else a[1]
        bx=b[1][:-2] if i==at else b[1]
        assert len(ax)==len(bx)
        for x,y in zip(ax,bx):
            dx,dy=DEF.match(x),DEF.match(y)
            assert bool(dx)==bool(dy)
            if dx:rename[dx[1]]=dy[1]
    for v,line in zip(live,join[1][:-1]):
        match=DEF.match(line);assert match
        rename[v]=match[1]
    inverse={v:k for k,v in rename.items()}
    assert len(inverse)==len(rename)
    # Only successor phi predecessor b35 changes to the inserted join.
    for i,(a,b) in enumerate(paired):
        ax=a[1][:-len(suffix)-1] if i==at else a[1]
        bx=b[1][:-2] if i==at else b[1]
        expected=[TOK.sub(lambda m:rename.get(m[0],m[0]),x) for x in ax]
        if a[0]=='b176':expected=[x.replace(', '+rename['%b35']+' ]',', %'+join[0]+' ]') for x in expected]
        assert expected==bx,'changed outside region'
    eq=re.fullmatch(r'  (%\w+) = call i1 @dx.op.waveActiveAllEqual.i32\(i32 115, i32 (%\w+)\)',guard[1][-2]);assert eq
    assert eq[2]==rename.get('%v72','%v72')
    assert guard[1][-1]=='  br i1 '+eq[1]+', label %'+uniform[0]+', label %'+mixed[0]
    assert join[1][-1]=='  br label '+rename['%b176']
    branches={};maps={}
    for tag,block in (('uniform',uniform),('mixed',mixed)):
        xs=block[1];assert xs[-1]=='  br label %'+join[0]
        body=xs[1:-1] if tag=='uniform' else xs[:-1]
        assert len(body)==len(suffix)
        mapping={v:DEF.match(x)[1] for v,x in zip(defs,body)}
        back=dict(inverse);back.update({v:k for k,v in mapping.items()})
        if tag=='uniform':
            first=re.fullmatch(r'  (%\w+) = call i32 @dx.op.waveReadLaneFirst.i32\(i32 118, i32 (%\w+)\)',xs[0]);assert first
            assert first[2]==eq[2];back[first[1]]='%multi_first'
        normalized=[TOK.sub(lambda m:back.get(m[0],m[0]),x) for x in body]
        expected=list(suffix)
        if tag=='uniform':expected[0]=expected[0].replace('i32 %v72, i1 true)','i32 %multi_first, i1 false)')
        assert normalized==expected,'clone operand/instruction changed'
        branches[tag]=([TOK.sub(lambda m:back.get(m[0],m[0]),xs[0])] if tag=='uniform' else [])+normalized
        maps[tag]=mapping
    for v,line in zip(live,join[1][:-1]):
        kind='float' if v in ('%v96','%v97','%v98','%v99') else 'i32'
        expected='  '+rename[v]+' = phi '+kind+' [ '+maps['uniform'][v]+', %'+uniform[0]+' ], [ '+maps['mixed'][v]+', %'+mixed[0]+' ]'
        assert line==expected,'joined value/type/predecessor changed'
    return branches

def cfg_faults(text):
    """Mutate actual emitted source, never the interpreted result vector."""
    base=Fn(text);at=next(i for i,(_,xs) in enumerate(base.blocks) if any('waveActiveAllEqual' in x for x in xs))
    faults={}
    def save(name,edit):
        fn=Fn(text);edit(fn.blocks);bad=fn.emit();assert bad!=text;faults[name]=bad
    def replace_line(block,line,change):
        return lambda bs:bs[block][1].__setitem__(line,change(bs[block][1][line]))
    def swap_targets(line):
        a,b=re.findall(r'label (%\w+)',line)
        return line.replace(a,'TEMP').replace(b,a).replace('TEMP',b)
    save('invert_emitted_guard',replace_line(at,-1,swap_targets))
    save('omit_emitted_guard',replace_line(at,-1,lambda x:'  br label %'+base.blocks[at+1][0]))
    save('wrong_guard_operand',replace_line(at,-2,lambda x:x.replace('i32 %v72)','i32 %v71)')))
    save('wrong_first_lane_operand',replace_line(at+1,0,lambda x:x.replace('i32 %v72)','i32 %v71)')))
    save('wrong_nonuniform_flag',replace_line(at+2,0,lambda x:x.replace('i1 true)','i1 false)')))
    save('bypass_join',replace_line(at+1,-1,lambda x:'  br label %'+base.blocks[at+4][0]))
    save('wrong_successor',replace_line(at+3,-1,lambda x:'  br label %'+base.blocks[at][0]))
    for n in range(13):
        def swap_phi(line):
            a,b=re.findall(r'\[ (%\w+),',line)
            return line.replace(a,'TEMP').replace(b,a).replace('TEMP',b)
        save('swap_phi_'+str(n),replace_line(at+3,n,swap_phi))
    save('missing_phi',lambda bs:bs[at+3][1].pop(0))
    save('wrong_phi_type',replace_line(at+3,0,lambda x:x.replace('phi i32','phi float')))
    save('wrong_phi_predecessor',replace_line(at+3,0,lambda x:x.replace('%'+base.blocks[at+1][0]+' ]','%'+base.blocks[at][0]+' ]')))
    # Unchanged consumers are part of the compositional boundary, too.
    consumer=next((i,j) for i,(_,xs) in enumerate(base.blocks) for j,x in enumerate(xs) if 'storeOutput' in x)
    save('changed_downstream_output',replace_line(*consumer,lambda x:x.replace('i32 5,','i32 6,',1)))
    return faults

def main():
    original=(OUT/'original.ll').read_text()
    text,w=rewrite(original);assert text==(OUT/'multi.ll').read_text()
    branches=inspect_cfg(text,original,w)
    cfg_mutants={}
    for name,bad in cfg_faults(text).items():
        try:inspect_cfg(bad,original,w)
        except (AssertionError,KeyError,AttributeError):cfg_mutants[name]=True
        else:raise AssertionError('accepted emitted-source fault: '+name)
    defined={DEF.match(x)[1] for x in w['original']}
    needed=set()
    for x in w['original']:needed|={t for t in TOK.findall(x.split(' = ',1)[1]) if t.startswith('%v')}-defined
    rng=random.Random(351559);count=waves=0;equal=mixed=0
    mutants={k:False for k in ('omit_guard','invert_guard','broadcast_address','wrong_descriptor','bad_metadata','drop_join')}
    for width in (4,32,64):
      masks=[1,1<<(width-1),(1<<width)-1,sum(1<<i for i in range(0,width,2))]+[rng.getrandbits(width) or 1 for _ in range(28)]
      for mask in masks:
       active=[i for i in range(width) if mask>>i&1]
       for same in (False,True):
        ds={i:3 if same else rng.randrange(16) for i in active}
        if not same and len(active)>1:ds[active[-1]]=(ds[active[0]]+1)%16
        same=len(set(ds.values()))==1;equal+=same;mixed+=not same;waves+=1;first=ds[active[0]]
        entries={i:{v:rng.getrandbits(32) for v in needed} for i in active}
        for i in active:
         b=entries[i];b['%v72']=ds[i];b['%v47']=rng.randrange(1<<20)
         for num in (1,5,6,7,10):b['%v'+str(num)]=100+num
         orig,ot=execute(w['original'],b,first,waves)
         # inspect_cfg proved these exact guard/edge semantics and every phi
         # mapping against emitted source; now interpret that source's suffix.
         branch='uniform' if same else 'mixed';actual,at=execute(branches[branch],b,first,waves)
         expected=[orig[v] for v in w['live_out']]
         assert ot==at and expected==[actual[v] for v in w['live_out']]
         count+=1
         for mut in mutants:
          tag=branch;bad=dict(b);f=first;lines=None
          if mut=='omit_guard':tag='uniform'
          if mut=='invert_guard':tag='mixed' if same else 'uniform'
          if mut=='broadcast_address':bad['%v71']=entries[active[0]]['%v71']
          if mut=='wrong_descriptor':bad['%v72']^=1;f^=1
          lines=w[tag]
          if mut=='bad_metadata':lines=[x.replace('lshr i32','shl i32',1) if 'lshr i32' in x and x.endswith(', 17') else x for x in lines]
          try:
           wrong,wt=execute(lines,bad,f,waves);values=[wrong[w[tag+'_map'][v]] for v in w['live_out']]
           if mut=='drop_join':values[0]^=1
           mutants[mut]|=values!=expected or wt!=ot
          except AssertionError:mutants[mut]=True
    assert all(mutants.values()),mutants
    result=dict(waves=waves,lanes=count,equal=equal,mixed=mixed,mutants=mutants,emitted_cfg_mutants=cfg_mutants,
                original_sha256=hashlib.sha256((OUT/'original.dxbc').read_bytes()).hexdigest(),
                candidate_sha256=hashlib.sha256((OUT/'multi.dxbc').read_bytes()).hexdigest(),
                interpreter_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                scope='emitted guard/edges/all 13 phi operands and outside-region SSA structurally validated; actual emitted suffixes interpreted, 13 live-outs and ordered reads; no whole-VS execution')
    (OUT/'multi-proof.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
