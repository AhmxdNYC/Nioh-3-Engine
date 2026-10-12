"""Private bundle verifier/runner. No environment edits, installation or draws."""
import argparse,hashlib,json,os,pathlib,re,subprocess,time
HERE=pathlib.Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--proton',type=pathlib.Path);args=ap.parse_args()
    manifest=json.loads((HERE/'manifest.json').read_text())
    for name,digest in manifest['files'].items():
        assert pathlib.PurePosixPath(name).name==name and sha(HERE/name)==digest,'fixture mismatch: '+name
    print('Bundle hashes PASS')
    if not args.proton:return
    proton=args.proton.resolve();assert proton.is_file(),'existing Proton executable required'
    # Inherit the actual configured game environment. Never synthesize a prefix,
    # select another Proton, or change renderer/driver environment variables.
    assert os.environ.get('STEAM_COMPAT_DATA_PATH') and os.environ.get('STEAM_COMPAT_CLIENT_INSTALL_PATH'),'run from existing configured Proton context'
    receipt={'scope':'PROTON_RECORDED_PIPELINE_COMPILER_ADMISSION_ONLY_NO_DRAW',
             'manifest_sha256':sha(HERE/'manifest.json'),'proton_launcher_sha256':sha(proton),
             'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())}
    for name in ('version','toolmanifest.vdf'):
        p=proton.parent/name
        if p.is_file():receipt[name+'_sha256']=sha(p)
    try:
        run=subprocess.run([str(proton),'run',str(HERE/'pipeline_admit.exe')],cwd=HERE,capture_output=True,text=True,timeout=120)
        receipt.update(returncode=run.returncode,stdout=run.stdout,stderr=run.stderr)
        expected=[f'record={i} {stage} hr=00000000' for i in range(8) for stage in ('original','candidate')]
        lines=run.stdout.splitlines()
        receipt['accepted']=run.returncode==0 and all(lines.count(s)==1 for s in expected) and 'admission failures=0; no draws executed' in lines
    except subprocess.TimeoutExpired as e:
        receipt.update(accepted=False,error='120s timeout; root must verify Proton child process closure before any game',stdout=str(e.stdout),stderr=str(e.stderr))
    receipt['fixtures_unchanged']=all(sha(HERE/n)==h for n,h in manifest['files'].items())
    receipt['accepted']=receipt['accepted'] and receipt['fixtures_unchanged']
    (HERE/'proton-admission.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2));raise SystemExit(0 if receipt['accepted'] else 1)
if __name__=='__main__':main()
