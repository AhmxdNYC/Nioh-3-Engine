"""Bind existing completed artifacts into a private Windows live-test packet."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import zipfile
sys.dont_write_bytecode=True
from live_switch import ORIGINAL,CANDIDATE,sha


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True)
    p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--game',type=Path,required=True);a=p.parse_args()
    sys.path.insert(0,str(a.repo/'gpu-shaders/materials'));import dxcom
    root=a.artifacts;native=root/'combined-native';out=a.destination
    original=(root/'original.dxbc').read_bytes();candidate=(root/'combined.dxbc').read_bytes()
    assert hashlib.sha256(candidate).hexdigest()=='a062c69ff48f8fcb0bd8822d17f3a5dcf0ab2b66d6375af9ecfcddfa8bcd4b38'
    assert hashlib.sha256(original).hexdigest()=='0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268'
    assert dxcom.validate_sign(candidate)==candidate
    op,cp=dxcom.parts(original),dxcom.parts(candidate)
    assert all(op.get(k)==cp.get(k) for k in ('ISG1','OSG1','PSV0','SFI0'))
    assert sha(a.game/'shader_dx12/0xb4751b0f.file')==ORIGINAL
    assert sha(root/'combined-package/original.file')==ORIGINAL
    assert sha(root/'combined-package/candidate.file')==CANDIDATE
    gate=json.loads((native/'admission.json').read_text())
    assert gate['scope']=='COMBINED_EXACT_CAPTURED_WINDOWS_PSO_NO_DRAW' and gate['returncode']==0
    assert gate['inputs']['candidate.dxbc']==hashlib.sha256(candidate).hexdigest()
    for name,digest in gate['inputs'].items():assert sha(native/name)==digest,name
    helper=root/'native-pipeline/pixel_admit.exe'
    assert sha(helper)==gate['helper_exe_sha256']
    out.mkdir(parents=True,exist_ok=False);(out/'native').mkdir()
    for name in gate['inputs']:shutil.copy2(native/name,out/'native'/name)
    shutil.copy2(helper,out/'native/pixel_admit.exe')
    shutil.copy2(native/'admission.json',out/'native/admission.json')
    for name in ('candidate.file','original.file'):shutil.copy2(root/'combined-package'/name,out/name)
    shutil.copy2(root/'combined-package/manifest.json',out/'composition-manifest.json')
    for name in ('live_switch.py','capture_window.ps1','exercise_log.py','LIVE.md'):shutil.copy2(Path(__file__).parent/name,out/name)
    guide=out/'LIVE.md'
    guide.write_text(guide.read_text().replace('D:\\nioh3-re\\deliveries\\pb2-combined-live-20261012',str(out)))
    shutil.copy2(a.repo/'tools/PresentMon.exe',out/'PresentMon.exe')
    result=dict(target='shader_dx12/0xb4751b0f.file',record=1,original_sha256=ORIGINAL,candidate_sha256=CANDIDATE,
        candidate_shader_sha256=hashlib.sha256(candidate).hexdigest(),candidate_shader_digest=candidate[4:20].hex(),
        current_local_original_verified=str(a.game),signed_interface_resource_metadata_unchanged=True,
        combined_windows_recorded_pso='PASS_NO_DRAW',real_game_exercise='UNTESTED',native_visual='UNTESTED',
        native_performance='UNTESTED',proton='UNTESTED',
        composition_manifest_note='Historical creation-time gate flags retained; native/admission.json is the new combined gate.',
        files={str(f.relative_to(out)).replace('\\','/'):sha(f) for f in sorted(out.rglob('*')) if f.is_file()})
    (out/'delivery.json').write_text(json.dumps(result,indent=2))
    archive=out.with_suffix('.zip')
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for f in sorted(out.rglob('*')):
            if f.is_file():z.write(f,str(f.relative_to(out)))
    print(json.dumps({'packet':str(out),'zip':str(archive),'sha256':sha(archive),'candidate_digest':candidate[4:20].hex()},indent=2))


if __name__=='__main__':main()
