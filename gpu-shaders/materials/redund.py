r"""redund.py - look for STRICTLY redundant work in a DXIL module (dumpbin text):
  dom-dup   : an instruction identical to one in a dominating block (pure op / readonly load) - any compiler CSEs these
  sib-dup   : identical pure instructions in blocks that do not dominate each other (only hoisting would merge them)
  dead      : defined values without users (excluding void calls)
  clamp0    : FMax(x, 0) / FMin(x, 1) / saturate where x is provably already in range (Frc/Sqrt/FAbs/uitofp/Saturate/FMax(.,0))
  same-mem  : two sample/load calls with identical operands (in any blocks)
    py -I redund.py X.ll [...]"""
import re, sys, collections as C
PURE = re.compile(r'@dx\.op\.(binary|unary|tertiary|dot[234]|cbufferLoadLegacy|bufferLoad|rawBufferLoad|textureLoad|sample\w*|'
                  r'createHandle|bitcast\w*|legacy\w*|quaternary)\.')
def parse(t):
    body = t.split('define void @main()', 1)[1].split('\n}\n', 1)[0]
    blocks, cur, name = C.OrderedDict(), [], '0'
    for l in body.splitlines():
        m = re.match(r'^; <label>:(\d+)', l)
        if m:
            blocks[name] = cur; cur, name = [], m.group(1); continue
        s = l.split('  ;')[0].rstrip()
        if s.strip() and not s.startswith(';') and s.strip() != '{':
            cur.append(s.strip())
    blocks[name] = cur
    return blocks
def analyse(fn):
    t = open(fn, encoding='utf-8').read()
    B = parse(t); names = list(B)
    succ = {b: re.findall(r'label %(\d+)', B[b][-1]) if B[b] else [] for b in names}
    pred = C.defaultdict(list)
    for b in names:
        for s in succ[b]: pred[s].append(b)
    dom = {b: set(names) for b in names}; dom['0'] = {'0'}
    ch = True
    while ch:
        ch = False
        for b in names[1:]:
            ps = [dom[p] for p in pred[b]]
            nd = ({b} | set.intersection(*ps)) if ps else {b}
            if nd != dom[b]: dom[b] = nd; ch = True
    defs, where, rhs_of = {}, {}, {}
    uses = C.Counter()
    for b in names:
        for x in B[b]:
            m = re.match(r'^(%[\w.]+) = (.*)$', x)
            r = m.group(2) if m else x
            for u in re.findall(r'%[\w.]+', r): uses[u] += 1
            if m: defs[m.group(1)] = r; where[m.group(1)] = b
    out = C.defaultdict(list)
    seen = C.defaultdict(list)
    for b in names:
        for x in B[b]:
            m = re.match(r'^(%[\w.]+) = (.*)$', x)
            if not m: continue
            d, r = m.groups()
            if r.startswith('phi') or r.startswith('alloca') or 'loadInput' in r or r.startswith('load ') or 'getelementptr' in r: continue
            if ' call ' in ' ' + r and not PURE.search(r): continue
            for (d0, b0) in seen[r]:
                if b0 in dom[b]: out['dom-dup'].append((d0, d, r[:90])); break
            else:
                if seen[r]: out['sib-dup'].append((seen[r][0][0], d, r[:90]))
                if re.search(r'@dx\.op\.(sample|textureLoad|bufferLoad|rawBufferLoad)', r) and seen[r]: out['same-mem'].append((seen[r][0][0], d))
            seen[r].append((d, b))
            if uses[d] == 0: out['dead'].append((d, r[:80]))
    def nonneg(v):
        r = defs.get(v, '')
        return bool(re.search(r'unary\.f32\(i32 (22|24|6|7),', r) or r.startswith('uitofp') or
                    re.search(r'binary\.f32\(i32 35, float %[\w.]+, float 0\.0', r))
    def le1(v):
        r = defs.get(v, '')
        return bool(re.search(r'unary\.f32\(i32 (22|7),', r) or re.search(r'binary\.f32\(i32 36, float %[\w.]+, float 1\.0', r))
    for d, r in defs.items():
        m = re.search(r'binary\.f32\(i32 35, float (%[\w.]+), float 0\.000000e\+00\)', r)
        if m and nonneg(m.group(1)): out['clamp0'].append((d, r[:80]))
        m = re.search(r'binary\.f32\(i32 36, float (%[\w.]+), float 1\.000000e\+00\)', r)
        if m and le1(m.group(1)): out['clamp0'].append((d, r[:80]))
        m = re.search(r'unary\.f32\(i32 7, float (%[\w.]+)\)', r)
        if m and nonneg(m.group(1)) and le1(m.group(1)): out['clamp0'].append((d, r[:80]))
    return out, len(names)
for fn in sys.argv[1:]:
    o, nb = analyse(fn)
    print('==', fn, 'blocks', nb, ' '.join('%s=%d' % (k, len(v)) for k, v in o.items()))
    for k in ('dom-dup', 'dead', 'clamp0', 'same-mem'):
        for x in o.get(k, [])[:6]: print('  ', k, x)
