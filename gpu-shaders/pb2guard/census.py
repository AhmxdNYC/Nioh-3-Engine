"""Read-only SSA/control-flow and retained native-waterfall census. No rewrite."""
import hashlib,json,pathlib,re,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from prototype import OUT,ROOT,DIGEST,SHA
from sfb_rewrite import Fn,DEF,TOK
import dxcom

def main():
    original=ROOT/'meshvs/work/1559714c.dxbc'
    raw=original.read_bytes()
    assert raw[4:20].hex()==DIGEST and hashlib.sha256(raw).hexdigest()==SHA
    fresh=dxcom.dumpbin(str(original))
    assert dxcom.body(fresh)==dxcom.body((OUT/'original.ll').read_text())
    fn=Fn(fresh)
    blocks=dict(fn.blocks)
    successors={b:re.findall(r'label %(\w+)',xs[-1]) for b,xs in fn.blocks}
    predecessors={b:[] for b in blocks}
    for b,ss in successors.items():
        for s in ss: predecessors[s].append(b)
    block=blocks['b35']
    first=next(i for i,x in enumerate(block) if x.startswith('  %v73 ='))
    region=block[first:-1]
    assert block[-1].strip()=='br label %b176'
    assert region[0].endswith('i32 %v72, i1 true)')
    assert all(not re.match(r'\s*(br|switch|store|ret|atomic|fence)',x) for x in region)
    calls=[re.search(r'@([\w.]+)',x)[1] for x in region if ' call ' in x]
    assert set(calls)<={'dx.op.createHandle','dx.op.rawBufferLoad.i32','dx.op.cbufferLoadLegacy.i32'}
    # Build transitive integer/address dependency witness in source order.
    deps={};loads=[]
    for x in region:
        m=DEF.match(x);assert m,x
        name=m[1];rhs=x.split(' = ',1)[1]
        uses=TOK.findall(rhs)
        inherited=set()
        for u in uses: inherited|=deps.get(u,set())
        if '@dx.op.rawBufferLoad' in rhs and 'Handle %v73,' in rhs:
            addr=re.search(r'Handle %v73, i32 (%\w+),',rhs)[1]
            loads.append(dict(value=name,address=addr,depends_on_region_loads=sorted(deps.get(addr,set()))))
            inherited.add(name)
        deps[name]=inherited
    assert [x['value'] for x in loads]==['%v74','%v83','%v86','%v91','%v103','%v112','%v141']
    defs=set(deps)
    outside=[]
    for b,xs in fn.blocks:
        if b!='b35': outside.extend(xs)
        else: outside.extend(block[:first]);outside.append(block[-1])
    external_uses=set()
    for x in outside:
        rhs=x.split(' = ',1)[1] if DEF.match(x) else x
        external_uses|=set(TOK.findall(rhs))&defs
    isa_path=OUT/'original.gfx1103.isa.txt'
    isa=isa_path.read_text()
    groups=[]
    for label,nloads in [('01B4',1),('0214',1),('02A0',5)]:
        start=isa.index('label_'+label+':')
        end=isa.index('s_cbranch_execnz  label_'+label,start)
        native=isa[start:end]
        assert re.search(r'v_readfirstlane_b32\s+s40, v4\b',native)
        assert re.search(r'v_cmpx_eq_u32\s+exec, s40, v4\b',native)
        assert re.search(r's_load_b128\s+s\[(?:40:43|36:39)\], s\[6:7\], s40\b',native)
        count=len(re.findall(r'^\s+buffer_load_\w+',native,re.M))
        assert count==nloads
        groups.append(dict(label=label,descriptor_index_register='v4',native_buffer_loads=count))
    report=dict(status='CONCRETE_MULTI_WATERFALL_REGION_FOUND_NO_REWRITE',source_sha256=SHA,
                isa_sha256=hashlib.sha256(isa_path.read_bytes()).hexdigest(),
                block='b35',source_predecessors=predecessors['b35'],source_successors=successors['b35'],
                guard_point='after %v72, before %v73',descriptor_index='%v72',
                region_instructions=len(region),calls=calls,raw_loads=loads,
                live_out=sorted(external_uses),native_groups=groups,
                caveat='native grouping supported by matching shared descriptor and load dependency shape; not instruction debug mapping')
    (OUT/'census.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({k:report[k] for k in ('status','region_instructions','live_out','native_groups')}))

if __name__=='__main__': main()
