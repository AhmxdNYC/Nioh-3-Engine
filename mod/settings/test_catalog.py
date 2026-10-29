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

def protected(ref):
    engine = content(ref if ref == '8fcb81b' else '8fcb81b', 'mod/Nioh3EngineFix/src/menu.cpp')
    addon = content('0b25b2b', 'mod/Nioh3BossRematch/src/rematch.cpp')
    return {'engine_enum': section(engine, 'enum Id {', 'static_assert'),
        'engine_order': section(engine, 'const OrderEnt kOrder[] = {', 'constexpr int kOrderN'),
        'exp_keys': section(engine, 'kExpIniKey[kNumExp] = {', 'struct ItemDef'),
        'remote': section(engine, 'int FindItem(', 'void ProcessRemote()'),
        'addon_prefix': addon.split('// ---- UI model', 1)[0],
        'addon_prepare': section(addon, 'bool Prepare() {', 'void UiOpen() {'),
        'addon_commands': addon.split('// ---- pipe commands', 1)[1]}

def main():
    subprocess.run([sys.executable, '-I', str(HERE / 'generate_catalog.py'), '--check'], cwd=ROOT, check=True)
    for mod, data in CATALOG.items():
        values = ini_values((ROOT / 'mod' / mod / (mod + '.ini')).read_text())
        rows = data['settings']
        ids = [(r['section'], r['key']) for r in rows]
        assert len(set(ids)) == len(ids), (mod, 'duplicate catalog keys')
        assert set(values) <= set(ids), (mod, 'uncovered ini keys', set(values) - set(ids))
        baseline = ini_values(content('8fcb81b' if mod == 'Nioh3EngineFix' else '0b25b2b', 'mod/' + mod + '/' + mod + '.ini'))
        assert all(values[key] == value for key, value in baseline.items()), (mod, 'changed shipped default')
        for r in rows:
            key = (r['section'], r['key'])
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
    original = protected('8fcb81b')
    engine = (ROOT / 'mod/Nioh3EngineFix/src/menu.cpp').read_text()
    addon = (ROOT / 'mod/Nioh3BossRematch/src/rematch.cpp').read_text()
    actual = {'engine_enum': section(engine, 'enum Id {', '#include "settings_catalog.h"'),
        'engine_order': section(engine, 'const OrderEnt kOrder[] = {', 'constexpr int kOrderN'),
        'exp_keys': section(engine, 'kExpIniKey[kNumExp] = {', 'struct ItemDef'),
        'remote': section(engine, 'int FindItem(', 'void ProcessRemote()').replace('        SyncConfigFromLive(id);\n', ''),
        'addon_prefix': addon.split('// ---- UI model', 1)[0].replace('#include "../../menu_navigation.h"\n', '').replace('#include "../../settings/store.h"\n', '').replace('#include <new>\n', ''),
        'addon_prepare': section(addon, 'bool Prepare() {', 'std::atomic<int> g_configSaveState'),
        'addon_commands': addon.split('// ---- pipe commands', 1)[1]}
    for key, expected in original.items():
        assert actual[key] == expected, ('protected source drift', key)
    assert (ROOT / 'mod/Nioh3BossRematch/src/training.cpp').read_text() == content('0b25b2b', 'mod/Nioh3BossRematch/src/training.cpp')
    assert '#define ADDON_VERSION "1.4.7"' in (ROOT / 'mod/Nioh3BossRematch/src/addon.h').read_text()
    assert '#define MOD_VERSION "1.7.67"' in (ROOT / 'mod/Nioh3EngineFix/src/dllmain.cpp').read_text()
    reserved = next(r for r in CATALOG['Nioh3BossRematch']['settings'] if (r['section'], r['key']) == ('Training', 'Enable'))
    assert reserved['kind'] == 'manual' and reserved['default'] == '0'
    print('PASS: catalog/defaults, enum/order/keys/Remote semantics, native v4 routes/commands, and default-off training preserved')

if __name__ == '__main__':
    main()
