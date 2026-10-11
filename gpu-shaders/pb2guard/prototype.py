"""Isolated offline one-record descriptor guard. Never stages game files."""
import hashlib, json, pathlib, re, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.dont_write_bytecode = True
sys.path[:0] = [str(ROOT / 'materials'), str(ROOT / 'meshvs')]
import dxcom
from sfb_rewrite import Fn, DEF, TOK
from build_msv import container

DIGEST = '1559714ca496fffe6ddd3c827e1d962e'
SHA = '5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc'
OUT = HERE / 'out'

def rewrite(text):
    fn = Fn(text)
    before = [(b, list(x)) for b, x in fn.blocks]
    pos = next(i for i, (b, _) in enumerate(fn.blocks) if b == 'b250')
    ins = fn.blocks[pos][1]
    start = next(i for i,x in enumerate(ins) if x.startswith('  %v257 ='))
    assert '%v256, i1 true)' in ins[start]
    selected = [x for x in ins[start:] if DEF.match(x) and DEF.match(x).group(1) in
                {'%v257','%v258','%v259','%v260','%v261','%v262','%v283','%v284','%v285','%v286','%v287'}]
    assert len(selected) == 11
    assert sum('@dx.op.rawBufferLoad' in x for x in selected) == 2
    assert all('i8 15, i32 4)' in x for x in selected if 'rawBufferLoad' in x)
    scalars = ['%v'+str(n) for n in (259,260,261,262,284,285,286,287)]
    defs = [DEF.match(x).group(1) for x in selected]
    def clone(tag):
        mp = {d: '%pg_'+tag+'_'+d[2:] for d in defs}
        lines = [TOK.sub(lambda m: mp.get(m.group(),m.group()),x) for x in selected]
        if tag == 'u':
            lines[0] = lines[0].replace('i32 %v256, i1 true)', 'i32 %pg_first, i1 false)')
            lines.insert(0, '  %pg_first = call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32 %v256)')
        return lines + ['  br label %pg_join'], mp
    uni, um = clone('u'); mixed, mm = clone('m')
    tail = [x for x in ins[start:] if x not in selected]
    join = ['  '+v+' = phi i32 [ '+um[v]+', %pg_uniform ], [ '+mm[v]+', %pg_mixed ]' for v in scalars] + tail
    guard = ins[:start] + ['  %pg_equal = call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32 %v256)',
                          '  br i1 %pg_equal, label %pg_uniform, label %pg_mixed']
    fn.blocks[pos:pos+1] = [['b250',guard],['pg_uniform',uni],['pg_mixed',mixed],['pg_join',join]]
    for b, lines in fn.blocks:
        if b == 'b296':
            lines[:] = [x.replace(', %b250 ]', ', %pg_join ]') for x in lines]
    # Every non-selected instruction is preserved; successor phi source labels alone change.
    assert tail == [x for x in before[pos][1][start:] if x not in selected]
    for b, lines in before:
        if b != 'b250':
            actual = next(x for name,x in fn.blocks if name == b)
            expected = [x.replace(', %b250 ]', ', %pg_join ]') for x in lines] if b == 'b296' else lines
            assert actual == expected
    out = fn.emit()
    decl = '\ndeclare i1 @dx.op.waveActiveAllEqual.i32(i32, i32) #1\ndeclare i32 @dx.op.waveReadLaneFirst.i32(i32, i32) #1\n'
    out = out.replace('\nattributes #0', decl+'\nattributes #0',1)
    ep = re.search(r'!dx.entryPoints = !\{!(\d+)\}',out).group(1)
    fl = re.search(r'^!'+ep+r' = !\{void \(\)\* @main, !"main", (?:!\d+|null), (?:!\d+|null), !(\d+)\}',out,re.M).group(1)
    m = re.search(r'^!'+fl+r' = !\{i32 0, i64 (\d+)(.*)\}$',out,re.M)
    out = out.replace(m.group(), '!'+fl+' = !{i32 0, i64 '+str(int(m.group(1))|(1<<19))+m.group(2)+'}',1)
    return out, dict(original_selected=selected, uniform=uni, mixed=mixed, scalars=scalars,
                     uniform_map=um,mixed_map=mm, original_blocks=len(before), unchanged_outputs=25)

def main():
    OUT.mkdir(exist_ok=True)
    tempfile.tempdir = str(OUT)
    source = ROOT/'meshvs/work/1559714c.dxbc'
    raw = source.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == SHA and raw[4:20].hex() == DIGEST
    fresh = dxcom.dumpbin(str(source))
    retained = (ROOT/'meshvs/work/1559714c.ll').read_text()
    assert dxcom.body(fresh) == dxcom.body(retained), 'retained body does not bind to container'
    (OUT/'original.dxbc').write_bytes(raw)
    (OUT/'original.ll').write_text(fresh)
    transformed, witness = rewrite(fresh)
    (OUT/'guarded.ll').write_text(transformed)
    binary = container(raw, dxcom.ll_to_container(transformed))
    target = OUT/'guarded.dxbc'; target.write_bytes(binary)
    assert dxcom.body(dxcom.dumpbin(str(target))) == dxcom.body(transformed)
    receipt = dict(original_sha256=SHA, original_digest=DIGEST,
                   candidate_sha256=hashlib.sha256(binary).hexdigest(), candidate_digest=binary[4:20].hex(),
                   fresh_body_sha256=hashlib.sha256(dxcom.body(fresh).encode()).hexdigest(),
                   status='BOUND_AND_VALIDATED_OFFLINE_ONLY', witness=witness)
    (OUT/'binding.json').write_text(json.dumps(receipt,indent=2))
    print(receipt['status'], receipt['candidate_digest'])

if __name__ == '__main__':
    main()
