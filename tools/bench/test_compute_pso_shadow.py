"""Build/run the offline shadow core. No game, hooks, installation or cached hits."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
setup=ROOT/'tools/msvc/setup_x64.bat'
with tempfile.TemporaryDirectory(prefix='nioh3-compute-shadow-') as tmp:
    out=Path(tmp)
    batch=out/'build.cmd'
    source=ROOT/'tools/bench/compute_pso_shadow_test.cpp'
    batch.write_text('@echo off\ncall "'+str(setup)+'" >nul\nif errorlevel 1 exit /b 1\n'
        'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX "'+str(source)+'" /Fo"'+str(out/'test.obj')+'" /Fe"'+str(out/'test.exe')+'"\nexit /b %errorlevel%\n')
    build=subprocess.run(['cmd','/d','/c',str(batch)],capture_output=True,text=True)
    if build.returncode:
        raise SystemExit(build.stdout+build.stderr)
    run=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,timeout=20)
    print(run.stdout.strip())
    if run.returncode or not run.stdout.startswith('PASS '):
        raise SystemExit(run.returncode or 1)
