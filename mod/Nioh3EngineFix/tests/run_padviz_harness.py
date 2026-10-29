"""Offline regression of padviz.h (controller input display, Gamepad Viewer skins). No game or hooks.

Also renders preview PNGs of both skins (idle / several pressed / others pressed) from the exact sprite list the overlay draws:
  captures/padviz-skin-preview/padviz_dualsense.png, padviz_xbox.png   (needs Pillow for the TGA -> PNG step; skipped without it)
"""
import pathlib
import site
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "padviztest"
OUT.mkdir(parents=True, exist_ok=True)
PREVIEW = REPO / "captures" / "padviz-skin-preview"
PREVIEW.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat",
    pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
source = ROOT / "tests/padviz_harness.cpp"
exe = OUT / "padviz_harness.exe"
batch = OUT / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '')
    + f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- "{source}" /Fo"{OUT}\\\\" /Fe"{exe}"\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
warnings = [l for l in r.stdout.splitlines() if "warning" in l]
if r.returncode or warnings:
    print(r.stdout[-6000:], r.stderr[-2000:])
    sys.exit("padviz harness build failed or warned")
r = subprocess.run([str(exe), str(OUT)], capture_output=True, text=True, timeout=120)
print(r.stdout.strip())
if r.returncode or "ALL PASSED" not in r.stdout:
    sys.exit("padviz harness failed")
# GPU path: the real shaders / root signature / atlas upload drawn offscreen (default adapter, WARP fallback)
gpu_exe = OUT / "padviz_gpu_test.exe"
gpu_batch = OUT / "build_gpu.cmd"
gpu_batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '')
    + f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- "{ROOT / "tests/padviz_gpu_test.cpp"}" /Fo"{OUT}\\\\" /Fe"{gpu_exe}"\n'
    + 'exit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(gpu_batch)], capture_output=True, text=True)
warnings = [l for l in r.stdout.splitlines() if "warning" in l]
if r.returncode or warnings:
    print(r.stdout[-6000:], r.stderr[-2000:])
    sys.exit("padviz GPU test build failed or warned")
r = subprocess.run([str(gpu_exe), str(OUT)], capture_output=True, text=True, timeout=120)
print(r.stdout.strip())
if r.returncode or "ALL PASSED" not in r.stdout:
    sys.exit("padviz GPU test failed")
sys.path.append(site.getusersitepackages())
try:
    from PIL import Image
except ImportError:
    print("Pillow missing - previews left as TGA in", OUT)
    sys.exit(0)
for name in ("dualsense", "xbox"):
    for kind in ("padviz", "padviz_gpu"):
        dst = PREVIEW / f"{kind}_{name}.png"
        Image.open(OUT / f"{kind}_{name}.tga").save(dst, optimize=True)
        print("preview", dst)
