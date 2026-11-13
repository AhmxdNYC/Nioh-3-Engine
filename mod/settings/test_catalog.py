"""Light offline coverage, default and protected-source checks; no compiler or game."""
from collections import Counter
import configparser
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
CATALOG = json.loads((HERE / 'catalog.json').read_text())
def ini_values(text):
    config = configparser.ConfigParser(interpolation=None, strict=True)
    config.optionxform = str
    config.read_string(text)
    return {(s, k): v for s in config.sections() for k, v in config[s].items()}

def content(ref, path):
    return subprocess.check_output(['git', 'show', ref + ':' + path], cwd=ROOT).decode().replace('\r\n', '\n')

def section(source, start, end):
    return source.split(start, 1)[1].split(end, 1)[0]

# #97604: baseline = last public release (v1.8.14, main 667edbe8). The old v1.7.67 pins (8fcb81b / 0b25b2b) predate
# every default flip and version bump since; bump RELEASE when a new release ships.
RELEASE = '667edbe8'

def protected(engine, addon):
    return {'engine_enum': section(engine, 'enum Id {', '#include "settings_catalog.h"'),
        'engine_order': section(engine, 'const OrderEnt kOrder[] = {', 'constexpr int kOrderN'),
        'exp_keys': section(engine, 'kExpIniKey[kNumExp] = {', 'struct ItemDef'),
        'remote': section(engine, 'int FindItem(', 'void ProcessRemote()'),
        'addon_prefix': addon.split('// ---- UI model', 1)[0],
        'addon_prepare': section(addon, 'bool Prepare() {', 'std::atomic<int> g_configSaveState'),
        'addon_commands': addon.split('// ---- pipe commands', 1)[1]}

def tree(path):
    return (ROOT / path).read_text(encoding='utf-8').replace(chr(13) + chr(10), chr(10))

def main():
    subprocess.run([sys.executable, '-I', str(HERE / 'generate_catalog.py'), '--check'], cwd=ROOT, check=True)
    for mod, data in CATALOG.items():
        values = ini_values((ROOT / 'mod' / mod / (mod + '.ini')).read_text())
        rows = data['settings']
        ids = [(r['section'], r['key']) for r in rows]
        assert len(set(ids)) == len(ids), (mod, 'duplicate catalog keys')
        # iniOnly: shipped ini keys edited by hand only (reports, save files, VRS, crash/debug); no generated menu row (#97604)
        hidden = {tuple(k.split('.', 1)) for k in data.get('iniOnly', [])}
        assert not hidden & set(ids), (mod, 'iniOnly key also has a menu row', hidden & set(ids))
        assert set(values) <= set(ids) | hidden, (mod, 'uncovered ini keys', set(values) - set(ids) - hidden)
        baseline = ini_values(content(RELEASE, 'mod/' + mod + '/' + mod + '.ini'))
        assert all(values[key] == value for key, value in baseline.items()), (mod, 'changed shipped default')
        # codeDefault: rows whose shipped ini value differs from the ASI's missing-key default; the menu shows the code
        # default (what actually runs when a user ini lacks the key), so these must differ from the shipped ini (#97660)
        coded = {tuple(k.split('.', 1)) for k in data.get('codeDefault', [])}
        assert coded <= set(ids), (mod, 'codeDefault key without a menu row', coded - set(ids))
        for r in rows:
            key = (r['section'], r['key'])
            if key in coded:
                assert key in values and r['default'] != values[key], (mod, key, 'codeDefault no longer differs from shipped ini')
                continue
            assert r['default'] == values.get(key, r['default']), (mod, key, 'catalog default drift')
            assert data['groups'][r['group']] == r['section']
            assert r['help'] and r['label']
            if r['kind'] == 'number':
                assert r['lo'] <= float(r['default']) <= r['hi'] and r['step'] > 0, (mod, key, 'bad bounds')
            elif r['kind'] == 'choice':
                choices = r['choices'].split('|')
                assert len(choices) == len(set(choices)) and r['default'] in choices, (mod, key, 'bad choices')
            else:
                assert r['kind'] == 'manual' and not r['live'], (mod, key, 'manual setter')
        count = Counter(r['kind'] for r in rows)
        print(f'{mod}: {len(rows)} catalog keys / {len(values)} shipped keys; ' + ', '.join(f'{k}={v}' for k, v in count.items()))
    # The supplemental UI must not replace the native action, pump, request or observer implementation.
    original = protected(content(RELEASE, 'mod/Nioh3EngineFix/src/menu.cpp'), content(RELEASE, 'mod/Nioh3BossRematch/src/rematch.cpp'))
    actual = protected(tree('mod/Nioh3EngineFix/src/menu.cpp'), tree('mod/Nioh3BossRematch/src/rematch.cpp'))
    for key, expected in original.items():
        assert actual[key] == expected, ('protected source drift', key)
    assert tree('mod/Nioh3BossRematch/src/training.cpp') == content(RELEASE, 'mod/Nioh3BossRematch/src/training.cpp')
    for path, macro in (('mod/Nioh3BossRematch/src/addon.h', '#define ADDON_VERSION '), ('mod/Nioh3EngineFix/src/dllmain.cpp', '#define MOD_VERSION ')):
        line = next(l for l in content(RELEASE, path).splitlines() if l.startswith(macro))
        assert line in tree(path), (path, 'version differs from release', line)
    reserved = next(r for r in CATALOG['Nioh3BossRematch']['settings'] if (r['section'], r['key']) == ('Training', 'Enable'))
    assert reserved['kind'] == 'manual' and reserved['default'] == '0'
    print('PASS: catalog/defaults, enum/order/keys/Remote semantics, native v4 routes/commands, and default-off training preserved')

if __name__ == '__main__':
    main()
