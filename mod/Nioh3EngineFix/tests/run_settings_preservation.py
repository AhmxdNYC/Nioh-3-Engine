"""Offline settings isolation regression. Never loads the ASI or starts the game.

py -I tests/run_settings_preservation.py --source-only
py -I tests/run_settings_preservation.py [--setup path/to/vcvars64.bat]

Full mode first builds in this checkout, then links the real menu + existing mod
objects into a console harness. Requires MSVC/Windows SDK (same as build.bat).
Fixtures are disposable; real game files, installed binaries and saves are unused.
"""
import argparse
import hashlib
import itertools
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source_checks():
    menu = (ROOT / 'src/menu.cpp').read_text()
    dll = (ROOT / 'src/dllmain.cpp').read_text()
    header = (ROOT / 'src/menu.h').read_text()
    forbidden = ('WriteGameConfigFg', 'CallFgWrite', 'SetConfigDirty',
                 'ProtonFgEarly', 'ProtonFgExit', 'SetFgMarker', 'ChangeFg',
                 'GameConfigPath', 'gameCfg', 'fgValue', 'g_cfgXmlState',
                 'HkExitProcess', 'HkTerminateProcess')
    for name in forbidden:
        assert name not in menu + dll + header, name
    for value in ('0x1FDDFCC', '0x45C6F80', '0x4B736F0', 'config.xml', '.fgmarker'):
        assert value.lower() not in (menu + dll).lower(), value
    assert re.search(r'bool ReadOnly\(int id\).*id == kFg \|\|', menu)
    assert re.search(r'void Change\(int id, int dir\)\s*{(?:(?!\n}\n).)*?\n    if \(ReadOnly\(id\)\) return;', menu, re.S)
    assert 'FrameGenToggle' not in menu  # no legacy value can enable writes
    assert 'job->ini = g_ini;' in menu
    assert 'g_ini = h.iniPath ? h.iniPath : L"";' in menu
    assert 'WritePrivateProfileStringW' in menu and 'osd::FrameGen()' in menu
    assert 'expGroup("FGC", true' in dll
    assert 'if (!strcmp(g, "FGP")) return false;' in dll
    # No direct file I/O remains in the menu; profile writes are mod-INI only.
    for api in ('CreateFile', 'WriteFile', 'CopyFile', 'MoveFile', 'DeleteFile', 'fopen'):
        assert api not in menu, api
    print('PASS: source contracts; FG read-only, own INI retained, repair/setter paths absent')


def full_check(setup, skip_build=False):
    out = ROOT / 'build/settings-preservation'
    out.mkdir(parents=True, exist_ok=True)
    script = out / 'build-test.cmd'
    lines = ['@echo off', 'setlocal', f'cd /d "{ROOT}"']
    if setup:
        lines += [f'call "{Path(setup).resolve()}"', 'if errorlevel 1 exit /b 1']
    if not skip_build:
        lines += ['call build.bat', 'if errorlevel 1 exit /b 1']
    # build.bat uses setlocal; initialize the compiler again for this command.
    if setup:
        lines += [f'call "{Path(setup).resolve()}"']
    else:
        portable = ROOT.parents[1] / 'tools/msvc/setup_x64.bat'
        lines += [f'if exist "{portable}" call "{portable}"']
    lines += [
        'set "TEST_OBJECTS="',
        'for %%f in (build\\*.obj) do if /I not "%%~nxf"=="menu.obj" call set "TEST_OBJECTS=%%TEST_OBJECTS%% %%f"',
        'cl /nologo /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE '
        'tests\\settings_preservation_harness.cpp /Fobuild\\settings-preservation\\harness.obj '
        '/Febuild\\settings-preservation\\harness.exe /link /OPT:REF /OPT:ICF %TEST_OBJECTS% kernel32.lib user32.lib winhttp.lib',
        'exit /b %errorlevel%',
    ]
    script.write_text('\n'.join(lines) + '\n')
    result = subprocess.run(['cmd', '/c', str(script)], cwd=ROOT, capture_output=True, text=True)
    (out / 'build.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(f'BUILD BLOCKED/FAILED: see {out / "build.log"}')
    cases = 0
    for mode, wine, fg, marker in itertools.product(range(4), range(2), range(4), range(2)):
        with tempfile.TemporaryDirectory(prefix='nioh3-settings-test-') as folder:
            root = Path(folder)
            cfg = root / 'KoeiTecmo/NIOH3/config.xml'
            cfg.parent.mkdir(parents=True)
            name = ('OFF', 'DLSS', 'FSR', 'XeSS')[fg]
            initial = ('\ufeff<?xml version="1.0"?>\r\n<CONFIG>\r\n'
                       '<SCR_REZ>2560x1440</SCR_REZ><DISP_MODE>WINDOWED</DISP_MODE>\r\n'
                       f'<FRAME_GENERATION>{name}</FRAME_GENERATION><!-- keep whitespace -->\r\n'
                       '</CONFIG>\r\n').encode('utf-8')
            edited = initial.replace(b'2560x1440', b'1728x972').replace(b'WINDOWED', b'BORDERLESS')
            cfg.write_bytes(initial)
            (root / 'edited.xml').write_bytes(edited)
            ini = root / 'Nioh3EngineFix.ini'
            ini.write_text(f'[Menu]\nFrameGenToggle={mode}\n[Overlay]\nEnabled=0\n')
            stale = root / 'Nioh3EngineFix.fgmarker'
            if marker:
                stale.write_bytes(b'stale crash marker\r\n')
            backup = cfg.with_name(cfg.name + '.Nioh3EngineFix.bak')
            backup.write_bytes(b'older config that must never be restored')
            env = dict(os.environ, LOCALAPPDATA=str(root))
            run = subprocess.run([str(out / 'harness.exe'), str(root), str(wine), str(fg), str(mode)],
                                 env=env, capture_output=True, text=True, timeout=30)
            assert run.returncode == 0, run.stdout + run.stderr
            assert cfg.read_bytes() == edited, 'config changed after process exit'
            assert hashlib.sha256(cfg.read_bytes()).digest() == hashlib.sha256(edited).digest()
            assert backup.read_bytes() == b'older config that must never be restored'
            assert stale.exists() == bool(marker)
            if marker:
                assert stale.read_bytes() == b'stale crash marker\r\n'
            assert not cfg.with_name(cfg.name + '.Nioh3EngineFix.tmp').exists()
            cases += 1
    print(f'PASS: {cases} native cases; exact config bytes (resolution/window mode included), '
          'newer user edits, stale markers/backups, synthetic game memory, own-INI saves')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-only', action='store_true')
    parser.add_argument('--setup', help='MSVC x64 environment batch file')
    parser.add_argument('--skip-build', action='store_true', help='Use existing freshly built objects')
    args = parser.parse_args()
    source_checks()
    if args.source_only:
        print('Native preservation/build NOT RUN (--source-only)')
    else:
        full_check(args.setup, args.skip_build)
