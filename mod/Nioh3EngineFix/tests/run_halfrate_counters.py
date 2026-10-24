"""Production telemetry test; no game or hook installation."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/hrtest';out.mkdir(parents=True,exist_ok=True)
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
cmd=f'call "{setup}" >nul && cl /nologo /O2 /MT /W4 /WX /EHsc /std:c++17 "{root}/tests/halfrate_counters_test.cpp" /Fo"{out}/counters.obj" /Fe"{out}/counters.exe" && "{out}/counters.exe" && py "{root}/tests/run_halfrate_harness.py"'
raise SystemExit(subprocess.run('cmd /s /c "'+cmd+'"').returncode)
