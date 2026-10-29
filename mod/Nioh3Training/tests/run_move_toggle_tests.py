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
spec = importlib.util.spec_from_file_location('gen_move_motion', ROOT / 'tools/gen_move_motion.py')
motion = importlib.util.module_from_spec(spec)
spec.loader.exec_module(motion)
assert (ROOT / 'mod/Nioh3Training/src/move_motion.h').read_text(encoding='utf-8') == motion.generate(), 'Regenerate move_motion.h (py tools/gen_move_motion.py --write)'
spec = importlib.util.spec_from_file_location('gen_summon_rows', ROOT / 'tools/gen_summon_rows.py')
summon = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summon)
arenas = (ROOT / 'mod/Nioh3Training/Nioh3Training_arenas.tsv').read_text(encoding='utf-8')
assert arenas == summon.generate(), 'Regenerate the summon rows (py tools/gen_summon_rows.py)'  # user #22984
summon.validate(arenas)
assert '	summon	S724394	Kajiwara Kagetoki' in arenas, 'Kajiwara Kagetoki must be summonable (user #22984)'
with tempfile.TemporaryDirectory(prefix='n3-move-tests-') as temp:
    for name in ('idle_hold_test', 'ih_alive_test', 'drill_test', 'gate_zone_test', 'move_toggle_test', 'player_hp_test', 'slot_hold_test', 'boss_vitals_test', 'aggro_test', 'practice_stats_test', 'negki_meter_test', 'auto_stagger_test', 'autobind_test', 'auto_moves_test'):
        exe = Path(temp) / (name + '.exe')
        subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O2', '-Wall', '-fms-extensions',
                        str(Path(__file__).with_name(name + '.c')), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=300)
