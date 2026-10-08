r"""build_dedupe.py - LOAD-DEDUPE builder: apply dedupe_loads.rewrite() to every DXIL record (any stage) of the given
shader_dx12 files, prove each rewritten record, and stage the repacked file (the game folder is only read).

    py -I gpu-shaders\materials\build_dedupe.py --files 0x52c39db0.file[,...] [--scan] [--max-dist 64] [--pixels 400 --scenes 6]

Per record: dxc -dumpbin -> rewrite (records without a repeated load stay byte-identical) -> dxcom assemble; ISG1/OSG1/
PSV0/SFI0 must equal the original's; dumpbin of the result must equal the rewritten text; dx.op call counts may only drop
by the removed loads; psim interpreter: outputs bit-identical on random scenes. Any failure keeps the record original.
--scan only reports how many repeated loads each record has. Output: gpu-shaders\replacement\<file>,
gpu-shaders\materials\out\dedupe_build.csv, gpu-shaders\materials\dedupe_manifest.tsv.
"""
import collections, csv, os, re, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import build_sfb, dedupe_loads, dxcom, verify_sfb  # noqa: E402

a = sys.argv[1:]
files = a[a.index('--files') + 1].split(',')
NPX = int(a[a.index('--pixels') + 1]) if '--pixels' in a else 400
NSC = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 6
MAXD = int(a[a.index('--max-dist') + 1]) if '--max-dist' in a else 64   # same block, <= N instructions apart: the kept
# load's value lives only a little longer (a far repeat would stretch live ranges if a driver had not CSE'd it anyway)
OPS = re.compile(r'@dx\.op\.(\w+)\.')
rows, manifest = [], []
for fname in files:
    g = build_sfb.Group(open(os.path.join(build_sfb.SD, fname), 'rb').read())
    new = {}
    for k in range(len(g.ents)):
        dx = g.dxbc(k)
        tmp = tempfile.mkdtemp(prefix='dd_')
        try:
            fp = os.path.join(tmp, 'r.dxbc'); open(fp, 'wb').write(dx)
            ll = dxcom.dumpbin(fp)
            nt, n = dedupe_loads.rewrite(ll, MAXD)
            row = dict(file=fname, rec=k, removed=n, status='no-pattern' if not n else '')
            if n and '--scan' not in a:
                try:
                    c = build_sfb.container(dx, dxcom.ll_to_container(nt))
                    fq = os.path.join(tmp, 'n.dxbc'); open(fq, 'wb').write(c)
                    if dxcom.body(dxcom.dumpbin(fq)) != dxcom.body(nt):
                        raise ValueError('assembled module != rewritten text')
                    co = collections.Counter(OPS.findall(ll.split('define void @main', 1)[1]))
                    cn = collections.Counter(OPS.findall(nt.split('define void @main', 1)[1]))
                    d = co - cn
                    if cn - co or sum(d.values()) != n or set(d) - {'rawBufferLoad', 'bufferLoad', 'textureLoad'}:
                        raise ValueError('dx.op counts: %r' % dict(d))
                    ok, msg, cov = verify_sfb.verify(ll, nt, NPX, NSC)
                    if not ok:
                        raise ValueError('interp: ' + msg)
                    new[k] = c
                    row.update(status='proven', blocks_run='%d/%d' % (sum(1 for x in cov if x), len(cov)),
                               old_size=len(dx), new_size=len(c))
                except Exception as e:  # noqa: BLE001
                    row['status'] = 'FAILED: ' + str(e)[:200]
            elif n:
                row['status'] = 'scan'
            rows.append(row)
            if n:
                print(fname, k, n, row['status'], row.get('blocks_run', ''), flush=True)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    if new and '--scan' not in a:
        ho, hn = build_sfb.repack(fname, new)
        manifest.append('dedupe %s %s %s' % (fname, ho, hn))
        print(fname, 'staged', len(new), 'records; sha256', ho[:12], '->', hn[:12])
if '--scan' not in a:
    with open(os.path.join(HERE, 'out', 'dedupe_build.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, ['file', 'rec', 'removed', 'status', 'blocks_run', 'old_size', 'new_size'], extrasaction='ignore')
        w.writeheader(); [w.writerow(r) for r in rows if r['removed']]
    open(os.path.join(HERE, 'dedupe_manifest.tsv'), 'w', newline='\n').write(''.join(m + '\n' for m in manifest))
