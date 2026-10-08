r"""build_wdo14.py - STAGE (not install) a replacement shader_dx12\0x1667d3a0.file ("WDO-PtSrbSsmVsm") whose record
#14 (sun-shadow resolve, RGP PSO 4001317a, container digest 8a013bc73a4cdabbc561121050579e73, 120 us PC / 0.7-1.1 ms
Ally) is swapped for WDO14_fast: the same DXIL with the VSM page-flag loads de-duplicated (gpu-shaders\vsm\flag_dedupe.py,
notes\gpu-wdo14-flag-dedupe.md). Reads the game file read-only; writes re\scripts_gpu\out\wdo14\ and
gpu-shaders\replacement\0x1667d3a0.file (git-ignored), which gpu-shaders\apply_wdo14.ps1 / ally_shaders.sh install.

    py -I re\scripts_gpu\build_wdo14.py [--pixels 600 --scenes 120]

Checks (any failure -> nothing staged):
  1. record #14 is the v2.02 original (container digest), its dumpbin equals the committed re\scripts_gpu\out\WDO_14.ll;
  2. dxcom: rewritten text -> bitcode -> container, validated + signed by dxil.dll; every container part except
     DXIL/HASH (ISG1, OSG1, PSV0, SFI0, RTS0 if any, STAT, ILDN...) byte-identical to the original; dumpbin of the result
     equals the rewritten module text;
  3. verify_vsm.lint: same dx.op call multiset, every guarded load matches the template;
  4. verify_vsm.verify: psim interpreter, original vs rewrite on VSM-driving scenes, every pixel bit-identical; every
     guarded load both hit and missed;
  5. repack: all 115 records but #14 byte-identical, header unchanged, idrk round trip.
"""
import hashlib, os, struct, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'gpu-shaders', 'materials'))
sys.path.insert(0, os.path.join(ROOT, 'gpu-shaders', 'vsm'))
sys.path.insert(0, os.path.join(ROOT, 'gpu-shaders'))
import build_sfb, dxcom, flag_dedupe, verify_vsm  # noqa: E402

FILE, REC = '0x1667d3a0.file', 14
ORIG_DIGEST = '8a013bc73a4cdabbc561121050579e73'
OUT = os.path.join(HERE, 'out', 'wdo14'); os.makedirs(OUT, exist_ok=True)
a = sys.argv[1:]
NPX = int(a[a.index('--pixels') + 1]) if '--pixels' in a else 600
NSC = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 120

g = build_sfb.Group(open(os.path.join(build_sfb.SD, FILE), 'rb').read())
dx = g.dxbc(REC)
assert dx[:4] == b'DXBC' and dx[4:20].hex() == ORIG_DIGEST, 'record #14 mismatch - game updated?'
tmp = tempfile.mkdtemp(prefix='wdo14_')
try:
    fp = os.path.join(tmp, 'o.dxbc'); open(fp, 'wb').write(dx)
    ll = dxcom.dumpbin(fp)
    ref = open(os.path.join(HERE, 'out', 'WDO_14.ll'), encoding='utf-8-sig').read()
    assert dxcom.body(ll) == dxcom.body(ref), 'dumpbin differs from out/WDO_14.ll'
    nt, sites = flag_dedupe.rewrite(ll)
    open(os.path.join(OUT, 'WDO14_fast.ll'), 'w', encoding='utf-8', newline='\n').write(nt)
    print('rewrite: %d page-flag loads guarded (%d chains)' % (len(sites), len({s['chain'] for s in sites})))
    c = build_sfb.container(dx, dxcom.ll_to_container(nt))
    po, pn = dxcom.parts(dx), dxcom.parts(c)
    assert list(po) == list(pn), 'part list differs'
    same = [k for k in po if po[k] == pn[k]]
    assert set(po) - set(same) <= {'DXIL', 'HASH'}, 'parts differ: %r' % (set(po) - set(same))
    print('container: parts', ' '.join(po), '| identical:', ' '.join(same))
    fq = os.path.join(tmp, 'n.dxbc'); open(fq, 'wb').write(c)
    assert dxcom.body(dxcom.dumpbin(fq)) == dxcom.body(nt), 'assembled module != rewritten text'
    open(os.path.join(OUT, 'WDO14_fast.dxbc'), 'wb').write(c)
    ok, msg = verify_vsm.lint(ll, nt, len(sites))
    print('lint:', msg); assert ok
    ok, msg, cov = verify_vsm.verify(ll, nt, NPX, NSC)
    print('psim (%d scenes x %d pixels):' % (NSC, NPX), 'BIT-EXACT' if ok else 'MISMATCH ' + msg); assert ok
    sc = verify_vsm.site_coverage(sites, cov)
    print('per guarded load (hit = load skipped / miss = load run):', ' '.join('%s:%d/%d' % x for x in sc))
    th, tm = sum(x[1] for x in sc), sum(x[2] for x in sc)
    print('total hit %d, miss %d (hit rate %.1f %% on these random scenes)' % (th, tm, 100.0 * th / max(1, th + tm)))
    assert all(h and m for _, h, m in sc), 'a guarded load was not both hit and missed - widen the scenes'
finally:
    shutil.rmtree(tmp, ignore_errors=True)
ho, hn = build_sfb.repack(FILE, {REC: c})
print('new container digest:', c[4:20].hex(), ' dxbc %d -> %d bytes' % (len(dx), len(c)))
print('sha256 original    ', ho)
print('sha256 replacement ', hn)
open(os.path.join(OUT, 'manifest.tsv'), 'w', newline='\n').write('wdo14 %s %s %s\n' % (FILE, ho, hn))
print('STAGED (not installed):', os.path.join(build_sfb.REPL, FILE))
