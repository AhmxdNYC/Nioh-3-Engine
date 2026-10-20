"""Offline only: validate the generated catalogue and exercise actual move veto code."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('gen_boss_moves', ROOT / 'tools/gen_boss_moves.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
assert (ROOT / 'mod/boss_moves.h').read_text(encoding='utf-8') == generator.generate(), 'Regenerate boss_moves.h'
with tempfile.TemporaryDirectory(prefix='n3-move-tests-') as temp:
    for name in ('idle_hold_test', 'ih_alive_test', 'drill_test', 'gate_zone_test', 'move_toggle_test', 'player_hp_test', 'boss_vitals_test'):
        exe = Path(temp) / (name + '.exe')
        subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O2', '-Wall', '-fms-extensions',
                        str(Path(__file__).with_name(name + '.c')), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=30)
