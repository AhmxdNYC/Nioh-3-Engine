r"""build_msv.py - MSV builder: scalarize the meshlet-slot load chain of every PB2/SM/TREE2/DEPTH meshlet vertex shader
(scalarize.py), prove every rewritten record, and stage the repacked shader_dx12 files (the game folder is only read).

    py -I gpu-shaders\meshvs\build_msv.py [--set hot|all] [--jobs N] [--verts 300 --scenes 4]

--set hot : the meshlet VS digests that rank in the pc2 RDNA2 GP2 budget (1559714c, 70d0ee9f, f311c8e2) -> 7 files
--set all : every meshlet VS ('Mshlt' group, kind vs) -> 1616 unique records in 187 files (default)
Per unique VS (cached by container digest):
  1. dxc -dumpbin -> scalarize.rewrite(hoist=True) (pattern asserts: slot phi, single-entry region, no side effects,
     no other per-lane input, all exits through the join);
  2. dxcom: text -> bitcode -> container (validated + signed by dxil.dll); dumpbin of the result == the rewritten text;
  3. container: original part order; DXIL/HASH/SFI0 from the new one, everything else from the original; ISG1/OSG1/PSV0
     must be byte-identical; SFI0 may differ ONLY by the WaveOps feature bit (0x4000);
  4. dx.op call multiset: identical except +1 waveReadLaneFirst and the createHandle nonUniform flags;
  5. verify_msv.verify(): interpreter, shipped vs rewrite, honest/decoy/decoy2 waterfall paths, direct+meshlet paths.
Any failure keeps that record original. Output: gpu-shaders\replacement\msv_<set>\<file> (git-ignored), manifest
gpu-shaders\meshvs\msv_manifest_<set>.tsv (sha256 original -> replacement), log gpu-shaders\meshvs\out\msv_build_<set>.csv.
"""
import collections, csv, hashlib, os, re, struct, sys, tempfile, shutil
from multiprocessing import Pool
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, '..', 'materials'), os.path.join(HERE, '..')]
import build_sfb, dxcom, scalarize, verify_msv  # noqa: E402

HOT = ['1559714c', '70d0ee9f', 'f311c8e2']
OPS = re.compile(r'@dx\.op\.(\w+)\.')
ARGS = {'verts': 300, 'scenes': 4}


def container(orig, new):
    po, pn = dxcom.parts(orig), dxcom.parts(new)
    for k in ('ISG1', 'OSG1', 'PSV0'):
        if po.get(k) != pn.get(k):
            raise ValueError('interface part %s differs' % k)
    fo, fn = struct.unpack('<Q', po['SFI0'])[0], struct.unpack('<Q', pn['SFI0'])[0]
    if fo ^ fn != 0x4000:
        raise ValueError('SFI0 %x -> %x (expected only WaveOps 0x4000)' % (fo, fn))
    n = struct.unpack_from('<I', orig, 28)[0]
    order = [orig[struct.unpack_from('<I', orig, 32 + 4 * i)[0]:][:4].decode('latin1') for i in range(n)]
    parts = [(k, pn[k] if k in ('DXIL', 'HASH', 'SFI0') else po[k]) for k in order]
    hdr = 32 + 4 * len(parts)
    offs, body = [], b''
    for k, v in parts:
        offs.append(hdr + len(body)); body += k.encode('latin1') + struct.pack('<I', len(v)) + v
    c = b'DXBC' + bytes(16) + struct.pack('<III', 1, hdr + len(body), len(parts)) + b''.join(struct.pack('<I', o) for o in offs) + body
    return dxcom.validate_sign(c)


CACHE = os.path.join(HERE, 'work', 'msv_cache')   # proven records survive a restart (keyed by original digest + tool sources)


def _tools_key():
    h = hashlib.sha256()
    for f in ('scalarize.py', 'verify_msv.py', 'build_msv.py'):
        h.update(open(os.path.join(HERE, f), 'rb').read())
    return h.hexdigest()[:12]


def do_digest(task):
    dg, dx = task
    cp = os.path.join(CACHE, _tools_key(), dg)
    if os.path.exists(cp + '.dxbc'):
        import json
        return dg, open(cp + '.dxbc', 'rb').read(), json.load(open(cp + '.json'))
    r = _do_digest(task)
    if r[1]:
        import json
        os.makedirs(os.path.dirname(cp), exist_ok=True)
        open(cp + '.dxbc', 'wb').write(r[1]); json.dump(r[2], open(cp + '.json', 'w'))
    return r


def _do_digest(task):
    dg, dx = task
    tmp = tempfile.mkdtemp(prefix='msv_')
    row = dict(digest=dg, status='')
    try:
        fp = os.path.join(tmp, 'r.dxbc'); open(fp, 'wb').write(dx)
        ll = dxcom.dumpbin(fp)
        nt, info = scalarize.rewrite(ll, hoist=True)
        row.update(region=info['region'], phis=info['phis'], hoisted=info['hoisted'], nonuni=info['nonuniform_cleared'])
        c = container(dx, dxcom.ll_to_container(nt))
        fq = os.path.join(tmp, 'n.dxbc'); open(fq, 'wb').write(c)
        if dxcom.body(dxcom.dumpbin(fq)) != dxcom.body(nt):
            raise ValueError('assembled module != rewritten text')
        co = collections.Counter(OPS.findall(ll.split('define void @main', 1)[1].split('\n}', 1)[0]))
        cn = collections.Counter(OPS.findall(nt.split('define void @main', 1)[1].split('\n}', 1)[0]))
        if cn - co != collections.Counter({'waveReadLaneFirst': 1}) or co - cn:
            raise ValueError('dx.op counts changed: +%r -%r' % (dict(cn - co), dict(co - cn)))
        ok, msg = verify_msv.verify(ll, nt, ARGS['verts'], ARGS['scenes'])
        if not ok:
            raise ValueError('interp: ' + msg)
        row.update(status='proven', old_size=len(dx), new_size=len(c), new_digest=c[4:20].hex())
        return dg, c, row
    except Exception as e:  # noqa: BLE001
        row['status'] = 'FAILED: ' + str(e)[:200]
        return dg, None, row
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    a = sys.argv[1:]
    which = a[a.index('--set') + 1] if '--set' in a else 'all'
    jobs = int(a[a.index('--jobs') + 1]) if '--jobs' in a else max(1, (os.cpu_count() or 2) - 4)
    for k in ARGS:
        if '--' + k in a:
            ARGS[k] = int(a[a.index('--' + k) + 1])
    rows = list(csv.DictReader(open(os.path.join(HERE, '..', 'dxil_index.csv'))))
    by = collections.defaultdict(list)
    for r in rows:
        by[r['file']].append(r)
    targets = {}          # file -> [record index]
    first = {}            # digest -> (file, k)
    for f, rs in by.items():
        for k, r in enumerate(rs):
            if r['kind'] != 'vs' or 'Mshlt' not in r['group']:
                continue
            if which == 'hot' and r['container_digest'][:8] not in HOT:
                continue
            targets.setdefault(f, []).append(k)
            first.setdefault(r['container_digest'], (f, k))
    print('%s: %d files, %d records, %d unique VS, %d jobs' % (which, len(targets), sum(map(len, targets.values())), len(first), jobs), flush=True)
    tasks = []
    for dg, (f, k) in first.items():
        g = build_sfb._group(f)
        dx = g.dxbc(k)
        assert dx[4:20].hex() == dg, (f, k)
        tasks.append((dg, dx))
    done, log = {}, []
    with Pool(jobs) as pool:
        for i, (dg, c, row) in enumerate(pool.imap_unordered(do_digest, tasks)):
            log.append(row)
            if c:
                done[dg] = c
            if row['status'] != 'proven' or i % 100 == 0:
                print(i, dg[:8], row['status'], flush=True)
    man = []
    build_sfb.REPL = os.path.join(HERE, '..', 'replacement', 'msv_' + which)
    for f in sorted(targets):
        g = build_sfb.Group(open(os.path.join(build_sfb.SD, f), 'rb').read())
        new = {}
        for k in targets[f]:
            dg = g.dxbc(k)[4:20].hex()
            if dg in done:
                new[k] = done[dg]
        if new:
            so, sn = build_sfb.repack(f, new)
            man.append((f, so, sn, len(new)))
    os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
    with open(os.path.join(HERE, 'out', 'msv_build_%s.csv' % which), 'w', newline='') as fo:
        w = csv.DictWriter(fo, ['digest', 'status', 'region', 'phis', 'hoisted', 'nonuni', 'old_size', 'new_size', 'new_digest'])
        w.writeheader(); w.writerows(log)
    with open(os.path.join(HERE, 'msv_manifest_%s.tsv' % which), 'w', newline='\n') as fo:
        fo.write('# file\tsha256_original_v2.02\tsha256_replacement\trecords_rewritten\n')
        for m in man:
            fo.write('%s\t%s\t%s\t%d\n' % m)
    import zipfile      # transport pack for other machines: splice_msv.py rebuilds the exact files from the game's originals
    with zipfile.ZipFile(os.path.join(HERE, 'out', 'msv_%s_records.zip' % which), 'w', zipfile.ZIP_DEFLATED) as z:
        for dg, c in sorted(done.items()):
            z.writestr(dg + '.dxbc', c)
        z.write(os.path.join(HERE, 'msv_manifest_%s.tsv' % which), 'manifest.tsv')
    st = collections.Counter(r['status'].split(':')[0] for r in log)
    print('done: %r; %d files staged in %s' % (dict(st), len(man), build_sfb.REPL))


if __name__ == '__main__':
    main()
