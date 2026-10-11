"""Independent adversarial checks of MIC proof admission and observable events.

All mutations are in memory or a temporary private proof directory. Never
changes bound shader artifacts, rewrite source, installed files or devices.
"""
import hashlib,json,pathlib,re,sys,tempfile
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import full as F

def main():
    sys.argv=['review_check.py']
    names=('original.ll','current.ll','original.dxbc','current.dxbc','binding.json')
    snapshots={n:(F.OUT/n).read_bytes() for n in names}
    receipt,old,new=F.validate_binding()
    refused=[]
    for n in (0,-1):
        try:F.validate_count(n)
        except ValueError:refused.append('scene_count_'+str(n))
        else:raise AssertionError('empty scene suite accepted')
    F.validate_count(1)
    import dxcom
    # Deliberately exercise coherent stale LL+bodyhash as well as simple damage.
    for case in ('bad_candidate_sha','bad_candidate_digest','bad_original_sha',
                 'corrupt_candidate','corrupt_original','stale_ll_and_bodyhash','stale_rewriter'):
        with tempfile.TemporaryDirectory(prefix='mic-review-',dir=F.OUT) as tmp:
            out=pathlib.Path(tmp)
            for n,data in snapshots.items():(out/n).write_bytes(data)
            meta=json.loads((out/'binding.json').read_text())
            if case.startswith('bad_'):
                key=case[4:];key=key.replace('_sha','_sha256')
                meta[key]='0'*len(meta[key])
            elif case.startswith('corrupt_'):
                name='current.dxbc' if case.endswith('candidate') else 'original.dxbc'
                data=bytearray((out/name).read_bytes());data[-1]^=1;(out/name).write_bytes(data)
            elif case=='stale_ll_and_bodyhash':
                text=new.replace('i32 93, i32 0)','i32 93, i32 1)',1);assert text!=new
                (out/'current.ll').write_text(text)
                meta['candidate_body_sha256']=hashlib.sha256(dxcom.body(text).encode()).hexdigest()
            else:meta['rewriter_sha256']='0'*64
            (out/'binding.json').write_text(json.dumps(meta))
            try:F.validate_binding(out)
            except AssertionError:refused.append(case)
            else:raise AssertionError('accepted '+case)

    A=F.Shader(old,'review_original');B=F.Shader(new,'review_candidate')
    def run(sh,seed):return F.run(sh,seed,[0]*len(sh.m.blocks))
    baselines={seed:run(A,seed) for seed in range(64)}
    for seed in baselines:assert run(B,seed)==baselines[seed]
    lines=new.splitlines();si=next(i for i,x in enumerate(lines) if '= call ' in x and '@dx.op.sampleLevel.' in x)
    ai=next(i for i,x in enumerate(lines) if '= call ' in x and '@dx.op.atomicBinOp.' in x)
    mutations={}
    def save(name,at,line):
        altered=list(lines);assert line!=altered[at];altered[at]=line;mutations[name]='\n'.join(altered)
    save('sample_texture_identity',si,lines[si].replace('Handle %5,','Handle %9,',1))
    save('sample_sampler_identity',si,lines[si].replace('Handle %9,','Handle %5,',1))
    save('sample_lod',si,re.sub(r'float %\w+\)$','float 0.000000e+00)',lines[si]))
    save('dropped_atomic',ai,lines[ai].split(' = ',1)[0]+' = add i32 0, 0')
    caught={}
    for name,text in mutations.items():
        sh=F.Shader(text,name)
        for seed,expected in baselines.items():
            if run(sh,seed)!=expected:caught[name]=seed;break
        assert name in caught,'observable source mutation escaped: '+name

    # These fixtures test the stated zero-return bounds MODEL, not native
    # descriptor validity. The original may suppress reads newly speculated.
    make_scene=F.scene;sparse={}
    try:
        for name,data in (('empty',[]),('width_zero',[0]),('zero_views',[4,4,0]),('header_only',[4,4,4])):
            def fixture(seed,data=data):
                sc=make_scene(seed);sc.b[(0,21)]=F.DI.TypedBuffer(data,isint=True);return sc
            F.scene=fixture
            for seed in range(64):assert run(A,seed)==run(B,seed),(name,seed)
            sparse[name]=64
    finally:F.scene=make_scene
    for name,data in snapshots.items():assert (F.OUT/name).read_bytes()==data
    result=dict(status='INDEPENDENT_MIC_REVIEW_CHECK_PASS',admission_faults_refused=refused,
                actual_source_mutants=caught,sparse_model_scenes=sparse,normal_scenes=64,
                candidate_sha256=receipt['candidate_sha256'],bound_artifacts_unchanged=True,
                review_sha256=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                full_sha256=hashlib.sha256(pathlib.Path(F.__file__).read_bytes()).hexdigest(),
                limits='finite synthetic oracle/zero-return OOB model; no native validity or arbitrary scheduling proof')
    (F.OUT/'review-check.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
