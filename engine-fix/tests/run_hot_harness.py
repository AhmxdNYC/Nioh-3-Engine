r"""run_hot_harness.py - ShaderHot (psoswap twin PSOs + gpuopt PSO-bind rebind) and LiveCtl on a real D3D12 device of this
PC. Not a game test.

    py -I tests/run_hot_harness.py

Builds tests/hot_harness.cpp with the real src/psoswap.cpp, src/gpuopt.cpp + src/gpuopt_thunks.asm (and what gpuopt links:
gimerge, gpuprof, skipfx, mlprobe, mpccheck) and src/livectl.cpp into build/hottest and runs it. Must end "ALL HOT CHECKS OK".
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "hottest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
src = ROOT / "src"
cmd = pre + (
    r'ml64 /nologo /c /W3 /Fo"{o}\gpuopt_thunks.obj" "{s}\gpuopt_thunks.asm" >nul && '
    r'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE "{t}" "{s}\gpuopt.cpp" "{s}\gimerge.cpp" "{s}\gpuprof.cpp" '
    r'"{s}\psoswap.cpp" "{s}\skipfx.cpp" "{s}\mlprobe.cpp" "{s}\mpccheck.cpp" "{s}\livectl.cpp" '
    r'"{o}\gpuopt_thunks.obj" /Fo"{o}\\" /Fe"{o}\hot_harness.exe" /link d3d12.lib d3dcompiler.lib dxgi.lib kernel32.lib user32.lib'
).format(o=OUT, s=src, t=ROOT / "tests" / "hot_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode or "warning" in r.stdout:
    print(r.stdout[-4000:], r.stderr[-2000:])
    if r.returncode:
        sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "hot_harness.exe")], timeout=120).returncode)
