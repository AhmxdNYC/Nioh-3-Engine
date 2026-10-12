"""Build/run isolated Windows D3D12 admission. --build-only creates no device.

Requires extraction receipt. No live draw or Proton acceptance claim.
"""
import hashlib,json,pathlib,subprocess,sys
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parent.parent
OUT=HERE/'out/pipeline'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    receipt=json.loads((OUT/'binding.json').read_text())
    for n in ('original.dxbc','candidate.dxbc','material.dxbc'):
        assert sha(OUT/n)==receipt[n], n
    assert receipt['original.dxbc']=='5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc'
    assert receipt['candidate.dxbc']=='0d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be'
    assert len(receipt['records'])==8
    for i,r in enumerate(receipt['records']):
        assert r['index']==i and sha(OUT/f'desc{i}.bin')==r['desc_sha256'] and sha(OUT/f'root{i}.bin')==r['root_sha256']
    setup=ROOT/'tools/msvc/setup_x64.bat'
    cmd=f'call "{setup}" >nul && cl /nologo /W4 /WX /EHsc /std:c++17 /O2 "{HERE / "pipeline_admit.cpp"}" /Fepipeline_admit.exe /link d3d12.lib dxgi.lib'
    r=subprocess.run('cmd /d /s /c "'+cmd+'"',cwd=OUT,capture_output=True,text=True,timeout=90)
    print(r.stdout+r.stderr);assert r.returncode==0,'build failed'
    if '--build-only' in sys.argv: return
    r=subprocess.run([str(OUT/'pipeline_admit.exe')],cwd=OUT,capture_output=True,text=True,timeout=120)
    print(r.stdout+r.stderr)
    result={'scope':'WINDOWS_RECORDED_PIPELINE_COMPILER_ADMISSION_ONLY_NO_DRAW',
            'binding_sha256':sha(OUT/'binding.json'),'helper_source_sha256':sha(HERE/'pipeline_admit.cpp'),
            'helper_exe_sha256':sha(OUT/'pipeline_admit.exe'),'runner_sha256':sha(pathlib.Path(__file__)),
            'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
    (OUT/'admission.json').write_text(json.dumps(result,indent=2)+'\n')
    raise SystemExit(r.returncode)
if __name__=='__main__':main()
