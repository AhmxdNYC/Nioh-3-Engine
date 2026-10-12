"""Build a private, hash-bound Proton admission zip. No device or send."""
import hashlib,json,pathlib,shutil,zipfile
HERE=pathlib.Path(__file__).resolve().parent
SRC=HERE/'out/pipeline'
DEST=pathlib.Path('D:/nioh3-re/candidates/pb2-native-admission-1e1f9f7')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    binding=json.loads((SRC/'binding.json').read_text());prior=json.loads((SRC/'admission.json').read_text())
    assert prior['returncode']==0 and prior['binding_sha256']==sha(SRC/'binding.json')
    assert prior['helper_exe_sha256']==sha(SRC/'pipeline_admit.exe')
    DEST.mkdir(parents=True,exist_ok=True)
    names=['pipeline_admit.exe','original.dxbc','candidate.dxbc','material.dxbc']+[f'{kind}{i}.bin' for kind in ('root','desc') for i in range(8)]
    for name in names:shutil.copyfile(SRC/name,DEST/name)
    for name in ('original.dxbc','candidate.dxbc','material.dxbc'):assert sha(DEST/name)==binding[name]
    for i,r in enumerate(binding['records']):
        for kind in ('root','desc'):assert sha(DEST/f'{kind}{i}.bin')==r[kind+'_sha256']
    shutil.copyfile(HERE/'proton_admit.py',DEST/'proton_admit.py')
    (DEST/'windows-admission.json').write_text(json.dumps(prior,indent=2)+'\n')
    (DEST/'README.txt').write_text('''PRIVATE PB2 native pipeline admission; never publish extracted shader fixtures.
No installation, game, draw, command queue, or environment change is performed.
The helper creates eight recorded root signatures and original/candidate PSOs.
Do not run alongside an active game or timing capture; root owns device lease.

1. Extract to a private scratch directory outside the game/prefix installation.
2. python3 proton_admit.py   # hash verification only; no device
3. From the EXISTING configured game Proton launch context, using its CURRENT
   Proton executable (do not change prefix, Proton version or renderer settings):
   python3 proton_admit.py --proton /actual/existing/Proton/proton
   The script inherits environment unchanged and refuses absent compatibility
   context. If launching from a plain shell, root must use the already approved
   device launch wrapper to supply that existing context; do not invent variables.
4. Retain proton-admission.json plus exact installed Proton/vkd3d/Mesa identity.
   Both original and candidate must return S_OK for all eight variants. Original
   rejection means the baseline is invalid, not a candidate-only rejection.
   A timeout is failure and requires root verification of child process closure.

RX9070 Windows receipt included: all16 PSO calls S_OK, WaveOps32..64, SM6.5.
That prior PASS is not a Proton PASS. No timing, savings, visual equivalence,
runtime descriptor validity or live material-container selection is proved here.
Follow-on: actual backend VS subgroup/nonuniform lowering evidence; root-owned
short visual/motion smoke; then matched profiler-off A/B. No shader staging
or replacement authorization follows merely from pipeline compiler acceptance.
''')
    manifest={'scope':'PRIVATE_EXACT_RECORDED_PB2_PIPELINE_ADMISSION_NO_DRAW',
              'source_checkpoint':'1e1f9f7','journal_sha256':binding['journal_sha256'],
              'binding_sha256':sha(SRC/'binding.json'),'records':binding['records'],
              'files':{p.name:sha(p) for p in sorted(DEST.iterdir()) if p.is_file() and p.name!='manifest.json'}}
    (DEST/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    archive=DEST.with_suffix('.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(DEST.iterdir()):
            if p.is_file():z.write(p,p.name)
    print(json.dumps({'directory':str(DEST),'archive':str(archive),'sha256':sha(archive),'manifest_sha256':sha(DEST/'manifest.json'),'size':archive.stat().st_size},indent=2))
if __name__=='__main__':main()
