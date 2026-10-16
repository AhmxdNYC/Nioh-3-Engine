"""Fresh isolated safe-index container and offline codegen; never installs."""
import hashlib,json,pathlib,subprocess,sys,tempfile
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;OUT=HERE/'out'/'safe'
sys.path[:0]=[str(HERE),str(HERE.parent/'materials'),str(HERE.parent/'gi')]
import full,safe,dxcom,rga_stats,native_layout_isa
from build_sfb import container
def sha(x):return hashlib.sha256(x).hexdigest()
def main():
    OUT.mkdir(exist_ok=True);tempfile.tempdir=str(OUT)
    prior,old,_=full.validate_binding()
    original=(HERE/'out/original.dxbc').read_bytes()
    new,info=safe.rewrite(old)
    candidate=container(original,dxcom.ll_to_container(new))
    path=OUT/'candidate.dxbc';path.write_bytes(candidate)
    actual=dxcom.dumpbin(str(path));assert dxcom.body(actual)==dxcom.body(new)
    (OUT/'candidate.ll').write_text(actual)
    hlsl=OUT/'root.hlsl';hlsl.write_text(native_layout_isa.ROOT_HLSL)
    root=OUT/'root.bin'
    subprocess.run([dxcom.DXC,'-T','rootsig_1_0','-E','RS',str(hlsl),'-Fo',str(root)],capture_output=True,check=True)
    isa_dir=pathlib.Path(tempfile.mkdtemp(prefix='isa-',dir=OUT))
    result=subprocess.run([rga_stats.RGA,'-s','dx12','--offline','-c','gfx1103','--cs-blob',str(path),
                           '--rs-bin',str(root),'--isa',str(isa_dir/'isa.txt')],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    matches=list(isa_dir.glob('*comp*'));assert len(matches)==1,result.stdout+result.stderr
    metrics=rga_stats.analyse(matches[0].read_text())
    receipt=dict(original_sha256=sha(original),candidate_sha256=sha(candidate),candidate_digest=candidate[4:20].hex(),
                 candidate_body_sha256=sha(dxcom.body(actual).encode()),safe_rewriter_sha256=sha(pathlib.Path(safe.__file__).read_bytes()),
                 base_rewriter_sha256=prior['rewriter_sha256'],copies=info,metrics=metrics,
                 isa_path=str(matches[0]),isa_sha256=sha(matches[0].read_bytes()),root_signature_sha256=sha(root.read_bytes()))
    (OUT/'binding.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
if __name__=='__main__':main()
