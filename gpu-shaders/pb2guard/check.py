"""Actual rewritten-region integer/descriptor equivalence; not a full VS interpreter."""
import hashlib, json, pathlib, random, re, sys
sys.dont_write_bytecode = True
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from prototype import OUT, rewrite

def run(lines, env, first):
    trace=[]
    for line in lines:
        if line.strip().startswith('br '): continue
        name, rhs = line.strip().split(' = ',1)
        if '@dx.op.waveReadLaneFirst' in rhs:
            env[name]=first
        elif '@dx.op.createHandle' in rhs:
            match=re.search(r'i32 57, i8 0, i32 0, i32 (%\w+), i1 (true|false)',rhs)
            assert match, rhs
            env[name]=env[match[1]]
        elif '@dx.op.rawBufferLoad' in rhs:
            match=re.search(r'Handle (%\w+), i32 (%\w+), i32 undef, i8 15, i32 4',rhs)
            assert match,rhs
            descriptor,offset=env[match[1]],env[match[2]]
            trace.append((descriptor,offset))
            # Eight full uint32 payloads can carry arbitrary float/NaN bits unchanged.
            env[name]=tuple(int.from_bytes(hashlib.sha256(f'{descriptor}:{(offset+i*4)&0xffffffff}'.encode()).digest()[:4],'little') for i in range(4))
        elif rhs.startswith('extractvalue'):
            match=re.search(r'(%\w+), ([0-3])$',rhs)
            env[name]=env[match[1]][int(match[2])]
        else: raise AssertionError(rhs)
    return env,trace

def main():
    text=(OUT/'original.ll').read_text()
    candidate,w=rewrite(text)
    assert candidate==(OUT/'guarded.ll').read_text()
    rng=random.Random(1559714)
    cases=lanes=0; equal=mixed=0
    caught={k:False for k in ('omit_guard','invert_guard','broadcast_offset','wrong_descriptor','drop_output')}
    # Arbitrary active subsets and per-lane descriptor/address states at region entry.
    for width in (4,32,64):
      masks=[1,1<<(width-1),(1<<width)-1,sum(1<<i for i in range(0,width,2))]
      masks += [rng.getrandbits(width) or 1 for _ in range(60)]
      for mask in masks:
       active=[i for i in range(width) if mask>>i&1]
       for uniform in (True,False):
        ds={i:3 if uniform else rng.randrange(16) for i in active}
        if not uniform and len(active)>1: ds[active[-1]]=(ds[active[0]]+1)%16
        offsets={i:rng.getrandbits(30)*4 for i in active}
        first=ds[active[0]]; same=len(set(ds.values()))==1
        equal+=same; mixed+=not same; cases+=1
        for i in active:
         base={'%v256':ds[i],'%v254':offsets[i],'%v255':(offsets[i]+16)&0xffffffff}
         original,ot=run(w['original_selected'],dict(base),first)
         tag='uniform' if same else 'mixed'; actual,at=run(w[tag],dict(base),first)
         mapping=w[tag+'_map']
         expected=[original[v] for v in w['scalars']]
         assert at==ot and [actual[mapping[v]] for v in w['scalars']]==expected
         lanes+=1
         for mutation in caught:
          branch=tag; b=dict(base); f=first
          if mutation=='omit_guard': branch='uniform'
          if mutation=='invert_guard': branch='mixed' if same else 'uniform'
          if mutation=='broadcast_offset': b['%v254']=offsets[active[0]];b['%v255']=(b['%v254']+16)&0xffffffff
          if mutation=='wrong_descriptor': b['%v256']^=1;f^=1
          bad,bt=run(w[branch],b,f)
          vals=[bad[w[branch+'_map'][v]] for v in w['scalars']]
          if mutation=='drop_output': vals[-1]^=1
          caught[mutation] |= bt!=ot or vals!=expected
    assert all(caught.values()),caught
    receipt=dict(waves=cases,active_lane_cases=lanes,equal_waves=equal,mixed_waves=mixed,
                 mutations_rejected=caught,scope='actual changed region + structural unchanged remainder; not whole-VS execution')
    (OUT/'region-proof.json').write_text(json.dumps(receipt,indent=2))
    print(json.dumps(receipt))

if __name__=='__main__': main()
