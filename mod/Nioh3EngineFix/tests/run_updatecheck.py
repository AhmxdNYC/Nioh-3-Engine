"""Offline parser, WinHTTP-failure, publication and endpoint-contract checks (no network/game)."""
from pathlib import Path
import json
import re
import subprocess
root = Path(__file__).resolve().parents[1]
repo = root.parent.parent
out = root / 'build/updatecheck-test'
out.mkdir(parents=True, exist_ok=True)
setup = repo / 'tools/msvc/setup_x64.bat'
exe = out / 'updatecheck_test.exe'
batch = out / 'build.cmd'
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\n' if setup.exists() else '') +
    f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX "{root / "tests/updatecheck_test.cpp"}" '
    f'/Fo"{out}/updatecheck_test.obj" /Fe"{exe}" /link winhttp.lib\nexit /b %errorlevel%\n')
subprocess.run(['cmd', '/d', '/c', str(batch)], check=True)
subprocess.run([str(exe)], check=True, timeout=20)
version = re.search(r'#define MOD_VERSION "([0-9]+\.[0-9]+\.[0-9]+)', (root/'src/dllmain.cpp').read_text())[1]
manifest = json.loads((repo/'site/version.json').read_text())
assert manifest['version'] == version, 'Prepared manifest must advertise current built numeric release, not a fictional update'
assert set(manifest) == {'version','url','notes'}
print('PASS prepared site/version.json matches the built-in numeric release')
