"""Offline block35 prototype; no shader staging."""
import hashlib,json,pathlib,re,sys,tempfile
sys.dont_write_bytecode=True
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from prototype import OUT,ROOT,SHA,DIGEST,dxcom,container,Fn,DEF,TOK

def rewrite(text):
    fn=Fn(text);before=[(b,list(xs)) for b,xs in fn.blocks]
    at=next(i for i,(b,_) in enumerate(fn.blocks) if b=='b35')
    ins=fn.blocks[at][1];start=next(i for i,x in enumerate(ins) if x.startswith('  %v73 ='))
    region=ins[start:-1]
    assert len(region)==72 and ins[-1].strip()=='br label %b176'
    defs=[DEF.match(x)[1] for x in region]
    live=['%v'+str(i) for i in (119,121,124,129,131,137,142,143,144,96,97,98,99)]
    def clone(tag):
        mapping={v:'%multi_'+tag+'_'+v[2:] for v in defs}
        body=[TOK.sub(lambda m:mapping.get(m[0],m[0]),x) for x in region]
        if tag=='u':
            assert 'i32 %v72, i1 true)' in body[0]
            body[0]=body[0].replace('i32 %v72, i1 true)','i32 %multi_first, i1 false)')
            body.insert(0,'  %multi_first = call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32 %v72)')
        return body+['  br label %multi_join'],mapping
    u,um=clone('u');m,mm=clone('m')
    guard=ins[:start]+['  %multi_equal = call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32 %v72)',
                     '  br i1 %multi_equal, label %multi_u, label %multi_m']
    joined=['  '+v+' = phi '+('float' if v in ['%v96','%v97','%v98','%v99'] else 'i32')+
            ' [ '+um[v]+', %multi_u ], [ '+mm[v]+', %multi_m ]' for v in live]+['  br label %b176']
    fn.blocks[at:at+1]=[['b35',guard],['multi_u',u],['multi_m',m],['multi_join',joined]]
    for b,xs in fn.blocks:
        if b=='b176': xs[:]=[x.replace(', %b35 ]',', %multi_join ]') for x in xs]
    for b,xs in before:
        if b!='b35':
            assert dict(fn.blocks)[b]==([x.replace(', %b35 ]',', %multi_join ]') for x in xs] if b=='b176' else xs)
    # Exact clone bijection, no extra operand broadcast or changed arithmetic.
    inverse={v:k for k,v in mm.items()}
    assert [TOK.sub(lambda z:inverse.get(z[0],z[0]),x) for x in m[:-1]]==region
    inverse={v:k for k,v in um.items()}
    back=[TOK.sub(lambda z:inverse.get(z[0],z[0]),x) for x in u[1:-1]]
    back[0]=back[0].replace('i32 %multi_first, i1 false)','i32 %v72, i1 true)')
    assert back==region
    out=fn.emit()
    decl='\ndeclare i1 @dx.op.waveActiveAllEqual.i32(i32, i32) #1\ndeclare i32 @dx.op.waveReadLaneFirst.i32(i32, i32) #1\n'
    out=out.replace('\nattributes #0',decl+'\nattributes #0',1)
    ep=re.search(r'!dx.entryPoints = !\{!(\d+)\}',out)[1]
    flag=re.search(r'^!'+ep+r' = !\{void \(\)\* @main, !"main", (?:!\d+|null), (?:!\d+|null), !(\d+)\}',out,re.M)[1]
    match=re.search(r'^!'+flag+r' = !\{i32 0, i64 (\d+)(.*)\}$',out,re.M)
    out=out.replace(match[0],'!'+flag+' = !{i32 0, i64 '+str(int(match[1])|(1<<19))+match[2]+'}',1)
    return out,dict(original=region,uniform=u,mixed=m,uniform_map=um,mixed_map=mm,live_out=live)

def main():
    tempfile.tempdir=str(OUT)
    src=ROOT/'meshvs/work/1559714c.dxbc';raw=src.read_bytes()
    assert hashlib.sha256(raw).hexdigest()==SHA and raw[4:20].hex()==DIGEST
    original=dxcom.dumpbin(str(src))
    assert dxcom.body(original)==dxcom.body((OUT/'original.ll').read_text())
    text,witness=rewrite(original);(OUT/'multi.ll').write_text(text)
    binary=container(raw,dxcom.ll_to_container(text));target=OUT/'multi.dxbc';target.write_bytes(binary)
    assert dxcom.body(dxcom.dumpbin(str(target)))==dxcom.body(text)
    report=dict(status='OFFLINE_BOUND_VALIDATED',digest=binary[4:20].hex(),sha256=hashlib.sha256(binary).hexdigest(),witness=witness)
    (OUT/'multi-binding.json').write_text(json.dumps(report,indent=2))
    print(report['status'],report['digest'])
    sys.path.insert(0,str(ROOT/'meshvs'))
    import rga_vs
    isa=rga_vs.compile_vs(str(target),'gfx1103');(OUT/'multi.gfx1103.isa.txt').write_text(isa)
    stats=rga_vs.summary(isa);(OUT/'multi-isa.json').write_text(json.dumps(stats,indent=2));print(json.dumps(stats))

if __name__=='__main__':main()
