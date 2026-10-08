r"""extract_ps.py - pull material shader records out of the game's shader_dx12 files (read-only) and disassemble them.
  py -I gpu-shaders\materials\extract_ps.py 0x08c2f248.file:3 [more file:index ...]   (or --rank N: top N of out\material_ps_rank.csv)
Writes gpu-shaders\materials\work\<file>_<idx>.dxbc / .ll (work\ is git-ignored: derived from game data)."""
import csv, os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'sweep'))
sys.path.insert(0, os.path.join(HERE, '..'))
import idrk  # noqa: E402
SD = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12"
DXC = os.path.join(HERE, '..', '..', 'tools', 'rdts', 'RadeonDeveloperToolSuite-2026-05-28-1806', 'utils', 'dx12', 'dxc', 'dxc.exe')
W = os.path.join(HERE, 'work'); os.makedirs(W, exist_ok=True)
items = []
args = sys.argv[1:]
if args and args[0] == '--rank':
    for r in list(csv.DictReader(open(os.path.join(HERE, 'out', 'material_ps_rank.csv'))))[:int(args[1])]:
        if r['file']: items.append((r['file'], int(r['index']), r['ps_digest']))
else:
    for a in args:
        f, _, i = a.partition(':'); items.append((f, int(i), None))
IDX = list(csv.DictReader(open(os.path.join(HERE, '..', 'dxil_index.csv'))))
cache = {}
for f, i, dg in items:
    base = os.path.join(W, '%s_%d' % (f.replace('.file', ''), i))
    if os.path.exists(base + '.ll'): print('have', base + '.ll'); continue
    rows = [r for r in IDX if r['file'] == f]
    r = next((r for r in rows if dg in (r['hash_digest'], r['container_digest'])), None) if dg else rows[i]
    p = cache.get(f) or idrk.unpack(open(os.path.join(SD, f), 'rb').read()); cache[f] = p
    o, n = int(r['dxbc_off']), int(r['dxbc_size'])
    open(base + '.dxbc', 'wb').write(p[o:o + n])
    ll = subprocess.run([DXC, '-dumpbin', base + '.dxbc'], capture_output=True, text=True).stdout
    open(base + '.ll', 'w', encoding='utf-8').write(ll); print('wrote', base + '.ll', len(ll.splitlines()), 'lines')
