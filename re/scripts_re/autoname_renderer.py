r"""autoname_renderer.py - deterministic heuristic naming of unnamed renderer functions.

usage: py -I re\scripts_re\autoname_renderer.py [--merge]
Inputs : D:\nioh3-re\decomp (C export, index.tsv, graph.tsv), re\scripts_renderer\partition.tsv, re\symbols\symbols.tsv.
Output : re\symbols\autoname_renderer.tsv   (rva, name, confidence, heuristic, reason)
--merge: replace the block "# renderer/autoname (heuristic, high confidence)" in symbols.tsv with the high rows only.
Never touches a function that already has a real name (progress_treemap.status rule); never reuses an existing name.

Heuristics (id in the heuristic column):
 a  D3D12Wrap_<Method>_<rva>   body = one ID3D12GraphicsCommandList vcall through SynCtx+0x580 (vtable offset -> method)
 b1 <Str>_GetInstance          singleton getter registering the name string "Str"
 b2 Str_<Str>_<rva>            function whose only string literal is an identifier (marker / debug name)  [med]
 c1 <Callee>_Thunk[_n]         body forwards its params to one named callee
 c2 <Caller>_Sub_<n>           only caller is a named function (high: small, not address-taken; else low)
 d  <Handler>_Helper_<n>       only caller is one of the 185 named KTGL token handlers
 e1 ScalarDeletingDtor / VtableCtor  (named after the vtable symbol when known)
 e2 Getter/Setter_Field0x..    single field load / store
 e3 MemSet/Hash                memset-only wrapper, FNV/murmur-constant hash
 f  <Pkg>_<region>_<rva>       per-package prefix from partition.tsv [low]
"""
import collections, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import progress_treemap as pt  # noqa: E402
import perf_coverage as pc  # noqa: E402

DEC = r'D:\nioh3-re\decomp'
PART = os.path.join(ROOT, 're', 'scripts_renderer', 'partition.tsv')
SYMS = os.path.join(ROOT, 're', 'symbols', 'symbols.tsv')
OUT = os.path.join(ROOT, 're', 'symbols', 'autoname_renderer.tsv')
BACKEND_MD = os.path.join(ROOT, 'notes', 'renderer', 'backend.md')
BLOCK = '# renderer/autoname (heuristic, high confidence)'
BLOCK_END = '# END renderer/autoname'
IMG = 0x7ff74f410000
PKG = {'PRESENT': 'Post', 'POST': 'Post', 'UI': 'Ui', 'VOLNAT': 'Vol', 'LIGHT': 'Light', 'FX': 'Fx', 'DRAW': 'Draw',
       'SCENE': 'Meshlet', 'PSO': 'Mat', 'RESOURCE': 'Res', 'BACKEND': 'Backend', 'FRAME': 'FrameGraph'}
# ID3D12GraphicsCommandList vtable (slot*8 = offset)
CL = ('QueryInterface AddRef Release GetPrivateData SetPrivateData SetPrivateDataInterface SetName GetDevice GetType Close '
      'Reset ClearState DrawInstanced DrawIndexedInstanced Dispatch CopyBufferRegion CopyTextureRegion CopyResource CopyTiles '
      'ResolveSubresource IASetPrimitiveTopology RSSetViewports RSSetScissorRects OMSetBlendFactor OMSetStencilRef '
      'SetPipelineState ResourceBarrier ExecuteBundle SetDescriptorHeaps SetComputeRootSignature SetGraphicsRootSignature '
      'SetComputeRootDescriptorTable SetGraphicsRootDescriptorTable SetComputeRoot32BitConstant SetGraphicsRoot32BitConstant '
      'SetComputeRoot32BitConstants SetGraphicsRoot32BitConstants SetComputeRootConstantBufferView '
      'SetGraphicsRootConstantBufferView SetComputeRootShaderResourceView SetGraphicsRootShaderResourceView '
      'SetComputeRootUnorderedAccessView SetGraphicsRootUnorderedAccessView IASetIndexBuffer IASetVertexBuffers SOSetTargets '
      'OMSetRenderTargets ClearDepthStencilView ClearRenderTargetView ClearUnorderedAccessViewUint '
      'ClearUnorderedAccessViewFloat DiscardResource BeginQuery EndQuery ResolveQueryData SetPredication SetMarker BeginEvent '
      'EndEvent ExecuteIndirect').split()
CLM = {i * 8: n for i, n in enumerate(CL)}
CLM_OK = {k: v for k, v in CLM.items() if k >= 9 * 8}   # skip IUnknown/ID3D12Object slots
NAME_RE = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)*$')
DECL = re.compile(r'^\s*(?!returnb)(const\s+)?[A-Za-z_][\w ]*?[ \*]+\**\w+(\[\w+\])?;\s*$')
HASHC = ('0x1000193', '0x811c9dc5', '0x100000001b3', '0xcbf29ce484222325', '0xcc9e2d51', '0x1b873593', '0x85ebca6b',
         '0xc2b2ae35', '0x9e3779b1', '0x9e3779b9', '0x9e3779b97f4a7c15', '0xff51afd7ed558ccd', '0xc4ceb9fe1a85ec53')


def load_all_names():
    s = set()
    for l in open(SYMS, encoding='utf-8'):
        f = l.split('\t')
        if len(f) > 2 and not l.startswith('#'):
            s.add(f[2])
    return s


def load_vtables():
    vt = {}
    for l in open(SYMS, encoding='utf-8'):
        f = l.split('\t')
        if len(f) > 2 and f[0].startswith('0x') and f[1] == 'vtable':
            n = f[2]
            for suf in ('_vftable', 'Vtbl', '_vtbl'):
                if n.endswith(suf):
                    n = n[:-len(suf)]
            vt[int(f[0], 16)] = n
    return vt


def load_handlers(names):
    h = {}
    for l in open(BACKEND_MD, encoding='utf-8'):
        m = re.match(r'^\| [0-9A-F]{2} \| ([0-9A-F]+) \| (\w+) \|', l)
        if m:
            h[int(m.group(1), 16)] = m.group(2)
    return h


def parse(rva):
    p = os.path.join(DEC, '0x%x.c' % rva)
    try:
        t = open(p, encoding='utf-8', errors='replace').read()
    except OSError:
        return None
    i = t.find('\n{')
    if i < 0:
        return None
    body = t[i + 2:]
    body = body[:body.rfind('}')]
    lines = [x for x in body.split('\n') if x.strip() and not DECL.match(x) and 'WARNING' not in x]
    return t[i:], ' '.join(x.strip() for x in lines), lines


def main():
    merge = '--merge' in sys.argv
    syms = pt.load_syms()
    allnames = load_all_names()
    vtabs = load_vtables()
    handlers = load_handlers(syms)
    part = {}
    for l in open(PART, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) >= 5 and f[0].startswith('0x') and f[1] in PKG:
            r = int(f[0], 16)
            part.setdefault(r, (f[1], int(f[3]), f[4]))
    idx = {}
    for l in open(os.path.join(DEC, 'index.tsv'), encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if f[0].startswith('0x'):
            idx[int(f[0], 16)] = f
    graph = {}
    for l in open(os.path.join(DEC, 'graph.tsv'), encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if f[0].startswith('0x') and len(f) >= 6:
            graph[int(f[0], 16)] = ([int(x, 16) for x in f[4].split(',') if x], [int(x, 16) for x in f[5].split(',') if x])

    def real(r):
        n = syms.get(pc.fold(r))
        return n if n and pt.status(syms, pc.fold(r)) == 1.0 else None

    unnamed = sorted(r for r in part if real(r) is None)
    print('renderer functions %d, unnamed %d' % (len(part), len(unnamed)))
    # address-taken FUN_ tokens (referenced other than by a direct call) -> not safe to assume "only caller"
    taken = set()
    tok = re.compile(r'(?:FUN|thunk_FUN)_7ff7([0-9a-f]{8})\b(?!\()')
    cache = {}
    for r in sorted(part):
        res = parse(r)
        cache[r] = res
        if res:
            for m in tok.finditer(res[0]):
                taken.add(int(m.group(1), 16) - 0x4f410000 + 0x0)  # FUN_7ff7XXXXXXXX == IMG + rva ; IMG low32 = 0x4f410000
    prop = {}   # rva -> (name, conf, heur, reason)
    used = set(allnames)

    def uniq(n):
        if n not in used:
            used.add(n)
            return n
        k = 2
        while '%s_%d' % (n, k) in used:
            k += 1
        used.add('%s_%d' % (n, k))
        return '%s_%d' % (n, k)

    subcount = collections.Counter()
    thunkc = collections.Counter()
    for r in unnamed:
        pk, size, region = part[r]
        res = cache.get(r)
        ix = idx.get(r)
        callers, callees = graph.get(r, ([], []))
        h = None
        if res:
            txt, flat, lines = res
            ncode = flat.count('(code **)')
            stm = flat.count(';')
            # (a) command-list vcall wrapper
            m = re.search(r'\(\*\*\(code \*\*\)\(\*\*\(longlong \*\*\)\(param_1 \+ 0x580\) \+ (0x[0-9a-f]+)\)\)', flat)
            if m and ncode == 1 and size <= 260 and 'FUN_' not in flat:
                off = int(m.group(1), 16)
                if off in CLM_OK:
                    h = ('D3D12Wrap_%s_%X' % (CLM_OK[off], r), 'high', 'a',
                         'sole vcall via SynCtx+0x580 (cmdlist) at vtable +0x%x = ID3D12GraphicsCommandList::%s' % (off, CLM_OK[off]))
            # (b1) singleton getter
            if not h:
                m = re.match(r'^FUN_\w+\(&DAT_(\w+),"([A-Za-z_]\w{2,})"\); _DAT_\1 = &?\w+; return &DAT_\1;$', flat)
                if m:
                    h = ('%s_GetInstance' % m.group(2), 'high', 'b1', 'registers singleton object with name string "%s" and returns it' % m.group(2))
            # (e1) scalar deleting dtor / dtor / ref-counted store / throw / static init (shape rules on normalised body)
            nf = re.sub(r'(?:FUN|thunk_FUN|DAT|PTR_FUN|PTR|LAB)_7ff7[0-9a-f]{8}', lambda m_: m_.group(0).split('_7ff7')[0] + '_X', flat)
            nf = re.sub(r'0x[0-9a-f]+|(?<!\w)\d+(?!\w)', 'N', nf)
            nf = re.sub(r'\b(?:local|uVar|lVar|iVar|pl?Var|puVar|cVar|bVar|auVar)\w*', 'V', nf)
            if not h and re.match(r'^(?:\*param_1 = &\w+_X; |FUN_X\(\); |FUN_X\(param_1\); |FUN_X\(param_1 \+ N\); )?'
                                  r'if \(\(param_2 & N\) (?:!=|==) N\) \{ (?:thunk_)?FUN_X\(param_1,N\); \}(?: return param_1;)?$', nf):
                h = ('ScalarDeletingDtor_%X' % r, 'high', 'e1', 'frees `this` when (param_2 & flag) (scalar deleting destructor shape)')
            if not h and re.match(r'^\*param_1 = &\w+_X; (?:FUN_X\([^;]*\); )+\*param_1 = &\w+_X; return;?$', nf) and size <= 160:
                h = ('Dtor_%X' % r, 'high', 'e1', 'destructor shape: stores derived vtable, destroys members, resets base vtable')
            if not h and re.match(r'^if \((?:\*param_1|\*\(\w+ \*\)\(param_1 \+ N\)) != N\) \{ FUN_X\(\); \} return;$', nf):
                h = ('CallIfSet_%X' % r, 'high', 'e1', 'calls one function only if a pointer field of param_1 is non-null')
            if not h and re.match(r'^V = \*param_1; \*param_1 = param_2; if \(V != N\) \{ FUN_X\(V\); \} return;$', nf):
                h = ('ReplaceOwnedPtr_%X' % r, 'high', 'e1', 'swaps *param_1 for param_2 and frees the old pointer')
            m = re.search(r'\*\(\w+ \*\)\(param_1 \+ (0x[0-9a-f]+)\) = param_2;(?: return;)?$', flat)
            if not h and m and 'LOCK();' in flat and 'UNLOCK();' in flat and size <= 120:
                h = ('RefCountedSet_Field%s_%X' % (m.group(1), r), 'high', 'e1',
                     'interlocked refcount increment on param_2, releases old, stores param_2 at +%s' % m.group(1))
            if not h and stm <= 3 and flat.rstrip(' ;').endswith(')') and '_CxxThrowException(' in flat and size <= 60:
                h = ('Throw_%X' % r, 'high', 'e1', 'ends in _CxxThrowException (throw helper)')
            if not h and 'atexit(' in flat and stm <= 3 and size <= 80:
                h = ('StaticInit_%X' % r, 'high', 'e1', 'CRT static initializer: init object then atexit(dtor)')
            m = re.match(r'^(?:return )?\(\*\*\(code \*\*\)\(\*param_1 \+ (0x[0-9a-f]+)\)\)\(param_1(?:,\w+)*\);(?: return;)?$', flat)
            if not h and m:
                h = ('VCallThunk_Slot%s_%X' % (m.group(1), r), 'high', 'e1', 'forwards to this->vtable[+%s](this, ...)' % m.group(1))
            if not h and len(callees) == 1 and stm <= 2 and size <= 80 and ncode == 0 and                     re.match(r'^(?:return )?(?:thunk_)?FUN_X\((?:param_\d(?: \+ N)?(?:,param_\d(?: \+ N)?)*)?\);(?: return;)?$', nf):
                cal = real(callees[0])
                if cal and NAME_RE.match(cal):
                    h = ('%s_Thunk' % cal, 'high', 'c1', 'forwards its arguments to %s' % cal)
                else:
                    h = ('Fwd_%X_%X' % (callees[0], r), 'high', 'c1', 'forwards its arguments to 0x%x (unnamed)' % callees[0])
            # (c1) thunk to a named function
            if not h and callees and len(callees) == 1 and stm <= 2 and size <= 80:
                m = re.match(r'^(?:return )?(?:FUN|thunk_FUN)_7ff7([0-9a-f]{8})\(((?:param_\d(?:, ?|,)?)*)\);(?: return;)?$', flat)
                if not m:
                    m = re.match(r'^(?:return )?([A-Za-z_]\w*)\(((?:param_\d(?:, ?|,)?)*)\);$', flat)
                    callee = real(callees[0]) if m else None
                else:
                    callee = real(callees[0])
                if m and callee and NAME_RE.match(callee) and not callee.startswith('D3D12Wrap_'):
                    thunkc[callee] += 1
                    h = ('%s_Thunk' % callee, 'high', 'c1', 'forwards its arguments unchanged to %s' % callee)
            # (e3) memset wrapper / hash
            if not h and stm <= 4 and re.match(r'^(?:return )?memset\(', flat) and ncode == 0 and size <= 60:
                h = ('MemSetWrap_%X' % r, 'high', 'e3', 'body is a single memset call')
            if not h and size <= 300 and any(c in flat.lower() for c in HASHC) and ('*' in flat) and '^' in flat:
                strong = any(c in flat.lower() for c in HASHC[:5])
                h = ('Hash_%X' % r, 'high' if strong else 'med', 'e3', 'multiply/xor loop with hash constant')
            # (e2) getter / setter
            if not h and size <= 24:
                m = re.match(r'^return \*\(\w[\w ]*\*\)\(param_1 \+ (0x[0-9a-f]+|\d+)\);$', flat)
                if m:
                    h = ('Getter_Field%s_%X' % (m.group(1), r), 'high', 'e2', 'returns *(param_1 + %s)' % m.group(1))
                m = re.match(r'^\*\(\w[\w ]*\*\)\(param_1 \+ (0x[0-9a-f]+|\d+)\) = param_2;(?: return;)?$', flat)
                if m:
                    h = ('Setter_Field%s_%X' % (m.group(1), r), 'high', 'e2', 'stores param_2 to *(param_1 + %s)' % m.group(1))
            # (b2) identifier string
            if not h and ix and ix[7]:
                strs = [s for s in ix[7].split(' | ') if s]
                ids = [s for s in strs if re.match(r'^[A-Za-z][A-Za-z0-9_]{5,40}$', s)]
                if len(strs) == 1 and len(ids) == 1 and size <= 600:
                    h = ('Str_%s_%X' % (ids[0], r), 'med', 'b2', 'only string literal "%s"' % ids[0])
        # (d)/(c2) caller propagation
        if not h and len(callers) == 1 and ix and int(ix[3]) == 1:
            c = callers[0]
            cn = real(c)
            small_ok = size <= 400 and r not in taken
            if c in handlers and cn:
                subcount[cn] += 1
                h = ('%s_Helper_%d' % (cn, subcount[cn]), 'high' if small_ok else 'med', 'd',
                     'only caller is KTGL token handler %s%s' % (cn, '' if small_ok else ' (large or address-taken)'))
            elif cn and NAME_RE.match(cn) and not cn.startswith(('Str_', 'Getter_', 'Setter_')):
                subcount[cn] += 1
                h = ('%s_Sub_%d' % (cn, subcount[cn]), 'high' if (small_ok and size <= 200) else 'med', 'c2',
                     'only caller is %s%s' % (cn, '' if small_ok else ' (large or address-taken)'))
        if not h:
            reg = re.sub(r'\W', '', region) or 'x'
            h = ('%s_%s_%X' % (PKG[pk if False else [k for k in PKG if PKG[k] == PKG[pk]][0]], reg, r), 'low', 'f',
                 'package %s region %s' % (pk, region))
        if h[1] != 'low' and h[2] not in ('d', 'c2') and False:
            pass
        name = h[0]
        if h[2] == 'c1':
            n = thunkc[name[:-6]]
            name = uniq(name if n == 1 and name not in used else '%s_%d' % (name, n))
        else:
            name = uniq(name)
        if not NAME_RE.match(name):
            h = (name, 'low', h[2], h[3] + ' (invalid identifier, demoted)')
        prop[r] = (name, h[1], h[2], h[3])

    conf = collections.Counter(v[1] for v in prop.values())
    byh = collections.Counter((v[2], v[1]) for v in prop.values())
    print('proposals:', dict(conf))
    for k in sorted(byh):
        print('  ', k, byh[k])
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('rva\tname\tconfidence\theuristic\treason\n')
        for r in sorted(prop):
            v = prop[r]
            f.write('0x%x\t%s\t%s\t%s\t%s\n' % (r, v[0], v[1], v[2], v[3]))
    print('wrote', OUT)
    if merge:
        do_merge(prop)


def do_merge(prop):
    lines = open(SYMS, encoding='utf-8').read().split('\n')
    out, skip = [], False
    for l in lines:
        if l == BLOCK:
            skip = True
            continue
        if skip:
            if l == BLOCK_END:
                skip = False
            continue
        out.append(l)
    while out and out[-1] == '':
        out.pop()
    out.append(BLOCK)
    n = 0
    for r in sorted(prop):
        nm, cf, hh, why = prop[r]
        if cf != 'high':
            continue
        out.append('0x%x\tfunction\t%s\t\t%s\thigh\tre/scripts_re/autoname_renderer.py (%s)' % (r, nm, why.replace('\t', ' '), hh))
        n += 1
    out.append(BLOCK_END)
    out.append('')
    open(SYMS, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print('merged %d high rows' % n)


if __name__ == '__main__':
    main()
