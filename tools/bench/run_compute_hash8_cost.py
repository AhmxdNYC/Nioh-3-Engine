"""Bounded offline native-body microbenchmark. No game/ASI/files touched."""
import hashlib,json,pathlib,statistics,subprocess,sys,tempfile
HERE=pathlib.Path(__file__).resolve().parent
PIN='91feb6ed7555a21750613844e53e01158565fb1d0a9081d7803dd207fad0d24e'
source=pathlib.Path(sys.argv[1]);setup=pathlib.Path(sys.argv[2])
subprocess.run([sys.executable,'-I','-B',str(HERE/'audit_compute_hash8.py'),str(source)],check=True,stdout=subprocess.DEVNULL,timeout=10)
with source.open('rb') as f:f.seek(0x1199a4);body=f.read(164)
assert hashlib.sha256(body).hexdigest()==PIN
out=pathlib.Path(tempfile.mkdtemp(prefix='nioh3-hash8-cost-'))
raw=out/'native.bin';raw.write_bytes(body)
asm=HERE/'compute_hash8_ordered.asm';cpp=HERE/'compute_hash8_cost.cpp'
batch=out/'build.cmd';obj=out/'ordered.obj';exe=out/'cost.exe'
batch.write_text(f'@echo off\ncall "{setup}" >nul\nif errorlevel 1 exit /b 1\n'
                 f'ml64 /nologo /c /Fo"{obj}" "{asm}"\nif errorlevel 1 exit /b 1\n'
                 f'cl /nologo /O2 /W4 /WX /EHsc /std:c++17 "{cpp}" "{obj}" /Fo"{out / "cost.obj"}" /Fe"{exe}"\nexit /b %errorlevel%\n')
subprocess.run(['cmd.exe','/d','/c',str(batch)],check=True,timeout=30)
result=subprocess.run([str(exe),str(raw)],check=True,capture_output=True,text=True,timeout=30)
(out/'samples.csv').write_text(result.stdout)
assert result.stdout.splitlines()[0]=='checks,65536'
groups={}
cpu=result.stdout.splitlines()[1];assert cpu.startswith('cpu,')
for line in result.stdout.splitlines()[2:]:
    tag,mode,mask,sample,a,b=line.split(',');assert tag=='sample'
    groups.setdefault((int(mode),int(mask)),[]).append((float(a),float(b)))
summary=[]
for (mode,mask),rows in groups.items():
    assert len(rows)==9
    summary.append({'mode':['XOR','CRC','alternating-contexts-stress'][mode], 'keys':mask+1,
                    'native_median_ns':statistics.median(x[0] for x in rows),
                    'ordered_median_ns':statistics.median(x[1] for x in rows),
                    'paired_saving_median_ns':statistics.median(a-b for a,b in rows),
                    'paired_saving_range_ns':[min(a-b for a,b in rows),max(a-b for a,b in rows)]})
assert hashlib.sha256(raw.read_bytes()).hexdigest()==PIN
receipt={'native_body_sha256':PIN,'native_vs_helper_checks':65536,'cpu':cpu[4:],'calls_per_sample':5000000,'pairs_per_case':9,'samples':summary,
         'scope':'Isolated indirect hash call ns; process not affinity/frequency pinned; no FPS or native caller sampling',
         'hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (asm,cpp,obj,exe,HERE/'audit_compute_hash8.py',pathlib.Path(__file__))}}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2));print('Artifacts:',out)
