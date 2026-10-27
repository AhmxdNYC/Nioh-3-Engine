"""Offline production DIB checks and CPU draw timing. Never opens or accesses the game."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / 'build/ring-test'
out.mkdir(parents=True, exist_ok=True)
sources = ('frame_meter_test', 'frame_meter_ki_test', 'frame_meter_kipress_test',
           'frame_meter_position_test', 'frame_meter_count_test', 'practice_stats_test', 'frame_meter_draw_test')
for name in sources:
    exe = out / (name + '.exe')
    result = subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-target', 'x86_64-windows-gnu',
        '-O2', '-fms-extensions', '-Wall', str(root / 'tests' / (name + '.c')),
        '-o', str(exe), '-lgdi32', '-lmsimg32'], text=True, capture_output=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        sys.exit(result.returncode)
    args = ['--check-only'] if name == 'frame_meter_draw_test' and '--measure' not in sys.argv else []
    result = subprocess.run([str(exe), *args], cwd=out, text=True, capture_output=True, timeout=180)
    print(result.stdout, end='', flush=True)
    (out / (name + '.txt')).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stderr)
        sys.exit(result.returncode)

# Rendering remains independent of menu visibility and must repaint with a frozen sampler.
game = (root / 'src/frame_meter_game.h').read_text()
assert 'const int want = scope && FmPanels() && gameOk && fg && !loading;' in game
assert 'memcmp(&lastCfg, &g_pst, sizeof lastCfg)' in game
assert 'const int hh = FmDrawHeight(cr.right,cr.bottom)' in game
keys = (root / 'src/practice_stats_game.h').read_text()
assert 'if (what == PS_FRAME_STYLE) return "FrameMeterStyle";' in keys
assert 'PstKey(i), PstToIni(i, PstDefault(i))' in keys
assert 'WritePrivateProfileStringA("Practice", PstKey(i), v, ini)' in keys
assert '\nFrameMeterStyle = 0\n' in (root / 'Nioh3Training.ini').read_text()
print('ALL PASSED: frame state, Ki, counts, position, settings, renderer, persistence wiring and menu visibility')
