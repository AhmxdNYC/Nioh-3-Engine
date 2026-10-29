"""Offline ABI and actual addon provider callbacks; no game, window or input."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SETUP = Path(r'C:/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-menu-bridge-') as temp:
    out = Path(temp)
    for source in (ROOT/'tests/rematch_menu_bridge_test.cpp', ROOT/'Nioh3BossRematch/tests/menu_bridge_harness.cpp', ROOT/'Nioh3BossRematch/tests/hosted_input_harness.cpp'):
        exe = out/(source.stem+'.exe')
        command = f'call "{SETUP}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /DUNICODE /D_UNICODE /Fo"{out / (source.stem + ".obj")}" /Fe"{exe}" "{source}" /link kernel32.lib user32.lib gdi32.lib'
        result = subprocess.run('cmd /d /s /c "'+command+'"', capture_output=True, text=True)
        assert result.returncode == 0, result.stdout+result.stderr
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        print(result.stdout.strip())
        assert result.returncode == 0, result.stderr
