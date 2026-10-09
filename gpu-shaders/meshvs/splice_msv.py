r"""splice_msv.py - rebuild the MSV replacement files on another machine from the transport pack (no dxc / RGA needed).

    py -I gpu-shaders\meshvs\splice_msv.py <msv_SET_records.zip> [--set hot|all] [--game "C:\...\Nioh3"] [--flat]

The pack holds the proven, signed DXBC of every rewritten meshlet VS (named by the ORIGINAL container digest) and the
build manifest (file, sha256 original v2.02, sha256 replacement). For every manifest file: the game's original is read
(sha256 must equal the manifest's original), every record whose digest is in the pack is replaced (gpu-shaders\materials\
build_sfb.Group, the same splicer the builder uses), the file is repacked, and the result must hash to the manifest's
replacement sha256 - byte-identical to what the builder staged - or nothing is written for that file.
Output: gpu-shaders\replacement\msv_<set>\<file>; then install with gpu-shaders\apply_msv.ps1 -Set <set>.
--flat: write to gpu-shaders\replacement\<file> instead, for pc_gpu_session -Items msv / ally_shaders.sh 'msv' rows.
That overwrites the trvs/pbvs replacements of 0x52c39db0 / 0xfd4f418f on that machine: msv and trvs/pbvs (same
files, ISA-neutral load dedupe) cannot be installed together; msv supersedes them.
"""
import hashlib, os, sys, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(HERE, '..', 'materials'), os.path.join(HERE, '..')]
import build_sfb  # noqa: E402

a = sys.argv[1:]
pack = a[0]
which = a[a.index('--set') + 1] if '--set' in a else ('hot' if '_hot_' in os.path.basename(pack) else 'all')
game = a[a.index('--game') + 1] if '--game' in a else r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
build_sfb.SD = os.path.join(game, 'shader_dx12')
build_sfb.REPL = os.path.join(HERE, '..', 'replacement', 'msv_' + which)
if '--flat' in a:   # straight into gpu-shaders\replacement\ (pc_gpu_session / ally_shaders.sh layout, item 'msv'); this
    build_sfb.REPL = os.path.join(HERE, '..', 'replacement')   # overwrites the trvs/pbvs files of that dir (same names)
z =zipfile.ZipFile(pack)
recs = {n[:-5]: z.read(n) for n in z.namelist() if n.endswith('.dxbc')}
man = [l.split('\t') for l in z.read('manifest.tsv').decode().splitlines() if l and not l.startswith('#')]
ok = bad = 0
for f, so, sn, n in man:
    fb = open(os.path.join(build_sfb.SD, f), 'rb').read()
    if hashlib.sha256(fb).hexdigest() != so:
        print('SKIP %s: game file is not the v2.02 original' % f); bad += 1; continue
    g = build_sfb.Group(fb)
    new = {k: recs[g.dxbc(k)[4:20].hex()] for k in range(len(g.ents)) if g.dxbc(k)[4:20].hex() in recs}
    if len(new) != int(n):
        print('SKIP %s: %d records matched, manifest says %s' % (f, len(new), n)); bad += 1; continue
    o, r = build_sfb.repack(f, new)
    if r != sn:
        os.remove(os.path.join(build_sfb.REPL, f))
        print('FAIL %s: rebuilt sha256 %s != manifest %s (removed)' % (f, r, sn)); bad += 1; continue
    ok += 1
print('%d files rebuilt byte-identical in %s, %d problems' % (ok, os.path.abspath(build_sfb.REPL), bad))
sys.exit(1 if bad else 0)
