"""Windows CPU/GDI regression and bitmap preview; never accesses the game."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
out=Path(tempfile.gettempdir())/'nioh3-gdi-stroke-tests'
out.mkdir(exist_ok=True)
setup=next((p for p in [root/'tools/msvc/setup_x64.bat',Path.home()/'nioh3-final3-39541/tools/msvc/setup_x64.bat',Path.home()/'nioh3-engine/tools/msvc/setup_x64.bat'] if p.exists()),None)
lines=['@echo off']
if setup: lines += [f'call "{setup}" >nul','if not "%errorlevel%"=="0" exit /b 1']
for src,name in [('mod/shared/tests/gdi_stroke_test.c','stroke'),('mod/Nioh3Training/tests/frame_meter_count_test.c','count')]:
    lines += [f'cl /nologo /O2 /W4 "{root/src}" /Fo"{out/name}.obj" /Fe"{out/name}.exe" gdi32.lib user32.lib','if not "%errorlevel%"=="0" exit /b 1',f'"{out/name}.exe" "{out}/preview.bmp"','if not "%errorlevel%"=="0" exit /b 1']
batch=out/'run.cmd';batch.write_text('\n'.join(lines)+'\n')
subprocess.run(['cmd','/d','/c',str(batch)],check=True,cwd=root)
print(f'Preview: {out / "preview.bmp"}')
