"""Compile/run the production title predicate and reader against offline memory fixtures."""
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
repo = root.parent.parent
out = root / "build" / "titlegate-test"
out.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (repo / "tools/msvc/setup_x64.bat",
    pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
batch = out / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}"\nif errorlevel 1 exit /b 1\n' if setup else '') +
    f'cl /nologo /EHsc /std:c++17 /W4 /Fe:"{out / "test.exe"}" '
    f'/Fo:"{out / "test.obj"}" "{root / "tests/titlegate_test.cpp"}"\n'
    'if errorlevel 1 exit /b 1\n' + f'"{out / "test.exe"}"\nexit /b %errorlevel%\n')
subprocess.run(["cmd.exe", "/d", "/c", str(batch)], check=True)
