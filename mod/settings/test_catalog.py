"""Light offline coverage, default and protected-source checks; no compiler or game."""
from collections import Counter
import configparser
import json
import re
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

# #97604: baseline = last public release (v1.8.27 = installer-pc/rc-1827-125580 37ba8a0a, RC 56a68936; was v1.8.26 030b9b61 = installer-pc/rc-1826-125285, RC 58fae5d4; was v1.8.25 c600620c = installer-pc/rc-1825-121820 427ca129, RC 5859c227; was v1.8.24 852642c3, v1.8.23 daa61c1d, v1.8.22 = main 3e6e730f = installer-pc/rc-1822on dd381ed2, regression #110443; was v1.8.21 29d49561, was v1.8.17 47505850, was v1.8.15 2e9efc04, v1.8.14 667edbe8). The old v1.7.67 pins (8fcb81b / 0b25b2b) predate
# every default flip and version bump since; bump RELEASE when a new release ships.
RELEASE = '37ba8a0a'
# v1.8.22 loading fix ON by default (PC2 #109923, Ally #110220; LoadFixDefaultV migration in EF dllmain)
INTENDED_FLIPS = {}  # RC 0->2 flip shipped in v1.8.22 (now baseline)

def protected(engine, addon):
    return {'engine_enum': section(engine, 'enum Id {', '#include "settings_catalog.h"'),
        'engine_order': section(engine, 'const OrderEnt kOrder[] = {', 'constexpr int kOrderN'),
        'exp_keys': section(engine, 'kExpIniKey[kNumExp] = {', 'struct ItemDef'),
        'remote': section(engine, 'int FindItem(', 'void ProcessRemote()'),
        'addon_prefix': addon.split('// ---- UI model', 1)[0],
        'addon_prepare': section(addon, 'bool Prepare() {', 'std::atomic<int> g_configSaveState'),
        'addon_commands': addon.split('// ---- pipe commands', 1)[1]}

# regression #103159: the item enum and the page order may GROW (a new menu row adds an Id and a kOrder entry; Remote looks
# rows up by tech key, not by number, and no Id number is saved), but every release entry must stay, in the same order,
# with the same text. Removed, renamed or reordered entries still fail. kCount must stay last. Remote: see remote_ok.
ADDITIVE = {'engine_enum', 'engine_order'}

def entries(text):
    # comments dropped; kOrder rows are '{kId, "Group" or nullptr}', enum rows are bare kId names
    code = re.sub(r'//[^\n]*', '', text)
    rows = re.findall(r'\{\s*(k\w+)\s*,\s*([^}]*?)\s*\}', code)
    return [f'{i}, {g}' for i, g in rows] if rows else re.findall(r'\bk\w+\b', code)

def additive_ok(original, actual):
    old, new = entries(original), entries(actual)
    it = iter(new)
    if not all(any(o == n for n in it) for o in old):  # old is an ordered subsequence of new
        return False
    if 'kCount' in old and new[-1:] != old[-1:]:
        return False
    return True

def code_lines(text):
    return [l for l in (re.sub(r'//.*', '', x).strip() for x in text.split('\n')) if l]

def remote_ok(original, actual, new_ids):
    # Remote (FindItem..ProcessRemote) may only gain code for NEW rows: every release line stays, in order (comments
    # ignored), and every block of added lines names one of the Ids that are new since the release.
    old, new = code_lines(original), code_lines(actual)
    added, block, i = [], [], 0
    for line in new:
        if i < len(old) and line == old[i]:
            i += 1
            if block:
                added.append(block)
                block = []
        else:
            block.append(line)
    if block:
        added.append(block)
    if i != len(old):
        return False
    pat = re.compile(r'\b(' + '|'.join(map(re.escape, new_ids)) + r')\b') if new_ids else None
    return all(pat and any(pat.search(l) for l in b) for b in added)

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
        # INTENDED_FLIPS: deliberate default changes since RELEASE (key -> (old, new)); clear when RELEASE is bumped
        flips = INTENDED_FLIPS.get(mod, {})
        assert all(values[key] == (flips[key][1] if key in flips and flips[key][0] == value else value) for key, value in baseline.items()), (mod, 'changed shipped default')
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
    new_ids = [i for i in entries(actual['engine_enum']) if i not in entries(original['engine_enum'])]
    for key, expected in original.items():
        if key in ADDITIVE:
            assert additive_ok(expected, actual[key]), ('protected source drift', key)
        elif key == 'remote':
            assert remote_ok(expected, actual[key], new_ids), ('protected source drift', key)
        else:
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
