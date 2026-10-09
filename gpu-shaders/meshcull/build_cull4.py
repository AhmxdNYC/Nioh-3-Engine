r"""build_cull4.py - round 4 builder (notes/gpu-rewrite-round4.md): MshLod#2 wave-uniform fast path (lodfast.py), proven
(verify_lod.py), repacked into shader_dx12\0x8d9282e6.file (the game folder is only read).

    py -I gpu-shaders\meshcull\build_cull4.py [--scenes 60]

Steps (any failure stops the build; nothing is staged):
  1. record 2 of 0x8d9282e6.file must be MshLod#2, container digest 63c07c21f09fbf65ed3486e04da80c00 (Steam v2.02);
  2. dxc -dumpbin -> lodfast.rewrite() (structural asserts: one row index, rows only used as component 0, the fast copy
     uses no slow-copy value);
  3. dxcom: text -> bitcode -> container; original part order, ISG1/OSG1/PSV0/SFI0 byte-identical (the kernel already
     declares WaveOps); validated + signed by dxil.dll; dumpbin of the result == the rewritten text;
  4. side-effect census: every bufferStore / atomicBinOp / waveActiveOp / wavePrefixOp / waveIsFirstLane exactly twice
     (slow copy = shipped, fast copy = clone); +1 waveActiveAllEqual, +1 waveReadLaneFirst beyond the clone;
  5. verify_lod.verify(): event logs + UAV state identical in all guard modes.
Output: gpu-shaders\replacement\cull4\0x8d9282e6.file (git-ignored), manifest gpu-shaders\meshcull\cull4_manifest.tsv,
transport pack gpu-shaders\meshcull\out\cull4_records.zip (rebuild on another PC with meshvs\splice_msv.py <pack> --flat).
"""
import collections, os, re, sys, tempfile, zipfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, '..', 'materials'), os.path.join(HERE, '..')]
import build_sfb, dxcom, lodfast, verify_lod  # noqa: E402

FILE, REC, DIGEST = '0x8d9282e6.file', 2, '63c07c21f09fbf65ed3486e04da80c00'
OPS = re.compile(r'@dx\.op\.(\w+)\.')
SIDE = ('bufferStore', 'atomicBinOp', 'waveActiveOp', 'wavePrefixOp', 'waveIsFirstLane')


def main():
    a = sys.argv[1:]
    nsc = int(a[a.index('--scenes') + 1]) if '--scenes' in a else 60
    g = build_sfb._group(FILE)
    dx = g.dxbc(REC)
    assert dx[4:20].hex() == DIGEST, 'record %d is %s, not MshLod#2 v2.02' % (REC, dx[4:20].hex())
    tmp = tempfile.mkdtemp(prefix='cull4_')
    try:
        fp = os.path.join(tmp, 'o.dxbc'); open(fp, 'wb').write(dx)
        ll = dxcom.dumpbin(fp)
        nt, info = lodfast.rewrite(ll)
        print('rewrite:', info)
        c = build_sfb.container(dx, dxcom.ll_to_container(nt))
        fq = os.path.join(tmp, 'n.dxbc'); open(fq, 'wb').write(c)
        assert dxcom.body(dxcom.dumpbin(fq)) == dxcom.body(nt), 'assembled module != rewritten text'
        body = lambda t: t.split('define void @main', 1)[1].split('\n}', 1)[0]   # noqa: E731
        co, cn = collections.Counter(OPS.findall(body(ll))), collections.Counter(OPS.findall(body(nt)))
        for k in SIDE:
            assert cn[k] == 2 * co[k], 'side effect census %s: %d -> %d' % (k, co[k], cn[k])
        assert cn['waveActiveAllEqual'] == 1 and cn['waveReadLaneFirst'] == 2 * co['waveReadLaneFirst'] + 1
        print('census: side effects x2 (slow copy + fast copy), +1 WaveActiveAllEqual +1 WaveReadLaneFirst')
        ok, msg, _ = verify_lod.verify(ll, nt, nsc)
        print('verify:', 'EQUIVALENT' if ok else 'MISMATCH', msg)
        assert ok
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    build_sfb.REPL = os.path.join(HERE, '..', 'replacement', 'cull4')
    so, sn = build_sfb.repack(FILE, {REC: c})
    man = os.path.join(HERE, 'cull4_manifest.tsv')
    with open(man, 'w', newline='\n') as fo:
        fo.write('# item lodu: MshLod#2 (63c07c21) wave-uniform fast path; file\tsha256_original_v2.02\tsha256_replacement\trecords_rewritten\n')
        fo.write('%s\t%s\t%s\t1\n' % (FILE, so, sn))
    os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
    with zipfile.ZipFile(os.path.join(HERE, 'out', 'cull4_records.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(DIGEST + '.dxbc', c)
        z.write(man, 'manifest.tsv')
    print('staged %s: %s -> %s (new record digest %s)' % (FILE, so, sn, c[4:20].hex()))


if __name__ == '__main__':
    main()
