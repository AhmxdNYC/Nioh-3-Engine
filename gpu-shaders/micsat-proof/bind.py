"""Fresh exact MIC record binding/assembly/ISA, isolated outputs only."""
import hashlib,json,pathlib,sys,tempfile,subprocess
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parent;OUT=HERE/'out'
sys.path[:0]=[str(ROOT),str(ROOT/'materials'),str(ROOT/'meshcull'),str(ROOT/'gi')]
import idrk,dxcom,micsat,rga_stats,rga_ps
from build_sfb import container
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
    OUT.mkdir(exist_ok=True);tempfile.tempdir=str(OUT)
    source=pathlib.Path(r'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12\0x17ad25bb.file')
    before=source.read_bytes();payload=idrk.unpack(before)
    matches=[(off,size) for off,size,digest,*_ in idrk.iter_dxbc(payload) if digest.hex()=='e31c389f6d4a38de7b1b784b87c710d2']
    assert len(matches)==1,matches
    offset,size=matches[0];original=payload[offset:offset+size]
    op=OUT/'original.dxbc';op.write_bytes(original)
    body=dxcom.dumpbin(str(op));(OUT/'original.ll').write_text(body)
    assert dxcom.body(body)==dxcom.body((ROOT/'meshvs/work/mic/mic0.ll').read_text())
    rewritten,info=micsat.rewrite(body);assert len(info)==3
    (OUT/'current.ll').write_text(rewritten)
    candidate=container(original,dxcom.ll_to_container(rewritten));cp=OUT/'current.dxbc';cp.write_bytes(candidate)
    assert dxcom.body(dxcom.dumpbin(str(cp)))==dxcom.body(rewritten)
    records={}
    for name,p in [('original',op),('current',cp)]:
        work=OUT/(name+'-compile');work.mkdir(exist_ok=True)
        (work/'root.hlsl').write_text(rga_ps.root_sig(dxcom.dumpbin(str(p))))
        result=subprocess.run([rga_stats.RGA,'-s','dx12','--offline','-c','gfx1103','--cs-blob',str(p),
                              '--rs-hlsl',str(work/'root.hlsl'),'--rs-macro','RS','--isa',str(work/'isa.txt')],capture_output=True,text=True)
        artifacts=list(work.glob('*comp*'))
        if not artifacts:raise RuntimeError(result.stdout[-1600:]+result.stderr[-400:])
        isa=artifacts[0].read_text();(OUT/(name+'.gfx1103.isa')).write_text(isa)
        records[name]={'gfx1103':rga_stats.analyse(isa)}
        print(name,json.dumps(records[name]),flush=True)
    assert source.read_bytes()==before
    receipt=dict(source_file=str(source),source_file_sha256=sha(before),record_offset=offset,record_bytes=size,
                 original_digest=original[4:20].hex(),original_sha256=sha(original),
                 candidate_digest=candidate[4:20].hex(),candidate_sha256=sha(candidate),
                 rewriter_sha256=sha(pathlib.Path(micsat.__file__).read_bytes()),
                 original_body_sha256=sha(dxcom.body(body).encode()),candidate_body_sha256=sha(dxcom.body(rewritten).encode()),
                 retained_body_bound=True,candidate_body_bound=True,source_unchanged=True,metrics=records)
    (OUT/'binding.json').write_text(json.dumps(receipt,indent=2));print('FRESH_BOUND_CURRENT_THREE_COPY_ISA_PASS')
if __name__=='__main__':main()
