"""Register liveness at hook sites in re\\Nioh3.dump.exe (memory layout: file offset == RVA).

Why: the game is an LTCG build. A caller may keep a volatile register live across a direct call when the compiler
knows the callee does not write it (1.7.17 LoadIsLoadEnd r11 crash). A hook that replaces such a call (or detours the
callee's entry) must preserve every register the caller still reads afterwards.

  py tools\\hook_liveness.py after  <rva> [regs]   live-in at <rva> (e.g. the return address of a hooked call)
  py tools\\hook_liveness.py callers <entry_rva>   E8/E9 references to <entry_rva> + live volatiles after each call
  py tools\\hook_liveness.py clob <func_rva>       registers a function (transitively over E8 calls) can write
  py tools\\hook_liveness.py --selftest            LoadIsLoadEnd: r11 live after LG6 (the 1.7.17 crash)

Model: direct E8 calls are transparent except rax/xmm0 (return); import / indirect calls clobber all volatiles (ABI);
ret / tail jumps read rax/xmm0 and report the still-undetermined registers as 'escape' (live in the caller's caller
when the enclosing function never writes them). 8/16-bit and merging xmm writes do not kill. Reads of rcx/rdx/r8/r9
at a following call while still undetermined are reported as 'arg?' (possible argument passed through).
"""
import struct, sys, os
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone import x86_const as X

DUMP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Nioh3.dump.exe')
data = open(DUMP, 'rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt = pe + 24
optsz = struct.unpack_from('<H', data, pe + 20)[0]
secs = []
for i in range(nsec):
    o = opt + optsz + i * 40
    name = data[o:o + 8].rstrip(b'\0').decode()
    vsz, va = struct.unpack_from('<II', data, o + 8)
    ch = struct.unpack_from('<I', data, o + 36)[0]
    secs.append((name, va, vsz, ch))
TEXT = [(va, va + vsz) for (n, va, vsz, ch) in secs if ch & 0x20000000]
pdata_rva, pdata_sz = struct.unpack_from('<II', data, opt + 112 + 3 * 8)
funcs = []
PRIMARY = set()  # function entries: .pdata ranges without UNW_FLAG_CHAININFO (cold / split parts are chained)
for i in range(pdata_sz // 12):
    b, e, u = struct.unpack_from('<III', data, pdata_rva + i * 12)
    if b:
        funcs.append((b, e))
        if u and u + 1 < len(data) and not ((data[u] >> 3) & 4): PRIMARY.add(b)
funcs.sort()
import bisect
fstarts = [b for b, e in funcs]
DECOMP = r'D:\nioh3-re\decomp'
if os.path.isdir(DECOMP):
    for n in os.listdir(DECOMP):
        if n.startswith('0x') and n.endswith('.c'):
            try: PRIMARY.add(int(n[2:-2], 16))
            except ValueError: pass

def is_text(a): return any(lo <= a < hi for lo, hi in TEXT)

def is_tail(t, here):
    """jmp target t taken from inside a function: tail call when t is another function's entry."""
    return t in PRIMARY or not is_text(t)

def func_of(a):
    """Primary function (chained unwind entries merged by walking back to the first range that starts a function)."""
    i = bisect.bisect_right(fstarts, a) - 1
    if i < 0 or not (funcs[i][0] <= a < funcs[i][1]): return None
    return funcs[i]

GPR64 = {}
for full, subs in {
    'rax': ['eax', 'ax', 'al', 'ah'], 'rbx': ['ebx', 'bx', 'bl', 'bh'], 'rcx': ['ecx', 'cx', 'cl', 'ch'],
    'rdx': ['edx', 'dx', 'dl', 'dh'], 'rsi': ['esi', 'si', 'sil'], 'rdi': ['edi', 'di', 'dil'],
    'rbp': ['ebp', 'bp', 'bpl'], 'rsp': ['esp', 'sp', 'spl']}.items():
    GPR64[full] = (full, 64)
    GPR64[subs[0]] = (full, 32)
    for s in subs[1:]: GPR64[s] = (full, 8)
for n in range(8, 16):
    GPR64['r%d' % n] = ('r%d' % n, 64)
    GPR64['r%dd' % n] = ('r%d' % n, 32)
    GPR64['r%dw' % n] = ('r%d' % n, 16)
    GPR64['r%db' % n] = ('r%d' % n, 8)

VOL = ['rcx', 'rdx', 'r8', 'r9', 'r10', 'r11', 'xmm1', 'xmm2', 'xmm3', 'xmm4', 'xmm5']
ALLVOL = ['rax', 'xmm0', 'flags'] + VOL
ARGS = ['rcx', 'rdx', 'r8', 'r9']
FULL_XMM_WRITE = {'movaps', 'movups', 'movapd', 'movupd', 'movdqa', 'movdqu', 'movq', 'movd', 'cvtps2pd', 'cvtdq2ps',
                  'cvttps2dq', 'cvtps2dq', 'pshufd', 'pmovzxbd', 'pmovsxbd', 'unpcklps', 'lddqu'}

def norm(name):
    if name.startswith(('xmm', 'ymm', 'zmm')): return 'xmm' + name[3:]
    if name in ('rflags', 'eflags', 'flags'): return 'flags'
    g = GPR64.get(name)
    return g[0] if g else name

_cache = {}
def insn(a):
    if a in _cache: return _cache[a]
    r = next(md.disasm(data[a:a + 16], a), None)
    _cache[a] = r
    return r

def rw(i):
    """(reads, kills) as normalized register names."""
    reads, kills = set(), set()
    try:
        rr, ww = i.regs_access()
    except Exception:
        rr, ww = [], []
    rnames = [i.reg_name(r) for r in rr]
    wnames = [i.reg_name(r) for r in ww]
    m = i.mnemonic
    ops = i.operands
    same = len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg
    zero_idiom = (same and m in ('xor', 'sub', 'xorps', 'xorpd', 'pxor', 'vxorps', 'vpxor')) or \
        (m in ('or', 'and') and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_IMM and
         ((m == 'or' and ops[1].imm in (-1, 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF)) or (m == 'and' and ops[1].imm == 0)))
    for n in rnames:
        if zero_idiom and n not in ('rflags', 'eflags'): continue
        reads.add(norm(n))
    # flags: capstone lists eflags in regs_access when touched; distinguish by eflags bitmask
    ef = i.eflags if hasattr(i, 'eflags') else 0
    TEST = 0
    for k in dir(X):
        pass
    for n in wnames:
        nn = norm(n)
        if nn.startswith('xmm'):
            full = m.startswith('v') or m in FULL_XMM_WRITE or zero_idiom or \
                (m in ('movss', 'movsd') and len(ops) == 2 and ops[1].type == X.X86_OP_MEM)
            if full: kills.add(nn)
            else: reads.add(nn)  # merging write: old value flows on
            continue
        g = GPR64.get(n)
        if g and g[1] < 32:
            continue  # partial write: no kill
        kills.add(nn)
    if m.startswith(('j', 'set', 'cmov', 'adc', 'sbb', 'pushf')) and m not in ('jmp',):
        if m.startswith('j') and m not in ('jrcxz', 'jecxz'): reads.add('flags')
        elif not m.startswith('j'): reads.add('flags')
    return reads, kills

def call_target(i):
    if i.mnemonic == 'call' and i.operands and i.operands[0].type == X.X86_OP_IMM:
        return i.operands[0].imm
    return None

def jmp_target(i):
    if i.operands and i.operands[0].type == X.X86_OP_IMM:
        return i.operands[0].imm
    return None

def live_after(start, regs=None, fn=None, budget=20000):
    """Paths from start; for each reg, first read before write. Returns {reg: [(rva, kind, text)]}."""
    regs = set(regs or VOL)
    fn = fn or func_of(start)
    hits = {}
    seen = set()
    stack = [(start, frozenset(regs))]
    steps = 0
    def hit(r, a, kind, txt):
        hits.setdefault(r, [])
        if len(hits[r]) < 4 and (a, kind) not in [(x[0], x[1]) for x in hits[r]]: hits[r].append((a, kind, txt))
    while stack and steps < budget:
        a, und = stack.pop()
        while und:
            if (a, und) in seen: break
            seen.add((a, und))
            steps += 1
            i = insn(a)
            if i is None: break
            txt = '%s %s' % (i.mnemonic, i.op_str)
            m = i.mnemonic
            if m in ('ret', 'retf'):
                for r in und:
                    if r in ('rax', 'xmm0'): hit(r, a, 'ret', txt)
                    else: hit(r, a, 'escape', txt)
                und = frozenset()
                break
            if m == 'int3' or m == 'ud2': break
            if m == 'call':
                t = call_target(i)
                if t is None or not is_text(t):
                    for r in und & set(ARGS): hit(r, a, 'arg?', txt)
                    und = und - set(ALLVOL)
                else:
                    for r in und & set(ARGS): hit(r, a, 'arg?', txt)
                    und = und - {'rax', 'xmm0', 'flags'}
                a += i.size
                continue
            reads, kills = rw(i)
            for r in und & reads: hit(r, a, 'read', txt)
            und = und - reads - kills
            if m == 'jmp':
                t = jmp_target(i)
                if t is not None and not is_tail(t, a):
                    a = t; continue
                # tail call or switch/indirect: possible args + escape
                for r in und & set(ARGS): hit(r, a, 'arg?', txt)
                for r in und - set(ARGS): hit(r, a, 'escape', txt)
                break
            if m.startswith('j') or m in ('loop', 'loope', 'loopne'):
                t = jmp_target(i)
                if t is not None: stack.append((t, und))
            a += i.size
    return hits

def resolve(h, fn, depth=0):
    """Drop 'escape' hits for registers the enclosing function writes anyway (its callers cannot rely on them) and
    'arg?' hits whose E8 callee writes the register before reading it."""
    out = {}
    cl = clobbers(fn[0]) if fn else set(ALLVOL)
    for r, lst in h.items():
        keep = []
        for a, k, t in lst:
            if k == 'escape' and r in cl: continue
            if k == 'arg?':
                i = insn(a)
                tg = call_target(i) if i and i.mnemonic == 'call' else (jmp_target(i) if i else None)
                if tg is not None and is_text(tg):
                    if depth >= 3: keep.append((a, 'arg?deep', t)); continue
                    sub = resolve(live_after(tg, [r]), func_of(tg), depth + 1)
                    if r not in sub: continue
                    keep.append((a, 'ARG(%s)' % sub[r][0][1], t)); continue
            keep.append((a, k, t))
        if keep: out[r] = keep
    return out

def fmt(h):
    out = []
    for r in sorted(h):
        out.append('  %-6s ' % r + '; '.join('%X %s [%s]' % (a, k, t) for a, k, t in h[r]))
    return '\n'.join(out) or '  (none)'

_clob = {}
def clobbers(f_rva, depth=0):
    """Volatile registers a function can write (body + E8 callees; ABI calls -> all)."""
    if f_rva in _clob: return _clob[f_rva]
    if depth > 10: return set(ALLVOL)
    _clob[f_rva] = set()  # recursion guard
    out = set()
    seen = set()
    work = [f_rva]
    n = 0
    while work and n < 200000:
        a = work.pop()
        while a not in seen:
            seen.add(a); n += 1
            i = insn(a)
            if i is None: break
            m = i.mnemonic
            if m == 'call':
                t = call_target(i)
                if t is None or not is_text(t): out |= set(ALLVOL)
                else: out |= clobbers(t, depth + 1)
            try:
                out |= {norm(i.reg_name(x)) for x in i.regs_access()[1]} & set(ALLVOL)
            except Exception:
                pass
            if m in ('ret', 'int3', 'ud2'): break
            if m == 'jmp':
                t = jmp_target(i)
                if t is None: out |= set(ALLVOL); break   # switch table or indirect tail call: be conservative
                if is_tail(t, a): out |= clobbers(t, depth + 1); break
                a = t; continue
            if m.startswith('j') or m.startswith('loop'):
                t = jmp_target(i)
                if t is not None: work.append(t)
            a += i.size
    _clob[f_rva] = out
    return out

_xref = None
def callers(entry):
    """Direct E8/E9 rel32 references to entry (index built once with numpy; decoded to confirm a 5-byte call/jmp)."""
    global _xref
    if _xref is None:
        import numpy as np
        _xref = {}
        for lo, hi in TEXT:
            b = np.frombuffer(data, dtype=np.uint8, count=hi - lo, offset=lo)
            pos = np.nonzero((b[:-5] == 0xE8) | (b[:-5] == 0xE9))[0]
            rel = (b[pos + 1].astype(np.int64) | (b[pos + 2].astype(np.int64) << 8) | (b[pos + 3].astype(np.int64) << 16)
                   | (b[pos + 4].astype(np.int64) << 24))
            rel = np.where(rel >= 1 << 31, rel - (1 << 32), rel)
            tgt = lo + pos + 5 + rel
            ok = (tgt >= lo) & (tgt < hi)
            for p, t in zip((lo + pos[ok]).tolist(), tgt[ok].tolist()):
                _xref.setdefault(t, []).append(p)
    res = []
    for a in _xref.get(entry, []):
        i = insn(a)
        if i and i.size == 5 and i.mnemonic in ('call', 'jmp'): res.append((a, data[a]))
    return res

def callers_slow(entry):
    res = []
    for lo, hi in TEXT:
        b = data[lo:hi]
        p = 0
        while True:
            p1 = b.find(b'\xE8', p); p2 = b.find(b'\xE9', p)
            cand = [x for x in (p1, p2) if x >= 0]
            if not cand: break
            p = min(cand)
            if lo + p + 5 <= hi:
                rel = struct.unpack_from('<i', b, p + 1)[0]
                if lo + p + 5 + rel == entry:
                    a = lo + p
                    i = insn(a)
                    if i and i.size == 5: res.append((a, b[p]))
            p += 1
    return res

def main():
    av = sys.argv[1:]
    if not av: print(__doc__); return
    if av[0] == '--selftest':
        h = resolve(live_after(0x3B34D5), func_of(0x3B34D5))
        print('LG6 return 3B34D5:\n' + fmt(h))
        assert 'r11' in h, 'expected r11 live after LG6'
        print('SELFTEST OK')
        return
    if av[0] == 'sites':
        # file lines: name ret_rva [reg,reg,...]   (no regs = the call-boundary volatiles)
        for ln in open(av[1]):
            ln = ln.split('#')[0].split()
            if not ln: continue
            name, a = ln[0], int(ln[1], 16)
            regs = ln[2].split(',') if len(ln) > 2 else None
            h = resolve(live_after(a, regs), func_of(a))
            if not regs: h = {r: v for r, v in h.items() if r not in ('rax', 'xmm0', 'flags')}
            f = func_of(a)
            ci = insn(a - 5)
            callee = call_target(ci) if ci and ci.mnemonic == 'call' and ci.size == 5 else None
            note = ''
            if callee is not None:
                cl = clobbers(callee)
                keep = sorted(r for r in h if r not in cl)
                note = ' callee %X; live AND preserved by the original callee: %s' % (callee, ','.join(keep) or '-')
            print('%s ret %X fn %s%s' % (name, a, '%X' % f[0] if f else '?', note))
            print(fmt(h))
        return
    cmd, a = av[0], int(av[1], 16)
    if cmd == 'dis':
        n = int(av[2]) if len(av) > 2 else 20
        for _ in range(n):
            i = insn(a)
            if not i: break
            print('%X  %s %s' % (a, i.mnemonic, i.op_str))
            a += i.size
        return
    if cmd == 'after':
        regs = av[2].split(',') if len(av) > 2 else None
        print(fmt(live_after(a, regs)))
    elif cmd == 'clob':
        print(sorted(clobbers(a)))
    elif cmd == 'callers':
        cs = callers(a)
        print('%d direct refs' % len(cs))
        for site, op in cs[:40]:
            f = func_of(site)
            kind = 'call' if op == 0xE8 else 'TAILJMP'
            print('%X %s in %s' % (site, kind, '%X' % f[0] if f else '?'))
            if op == 0xE8:
                h = live_after(site + 5)
                h = {r: v for r, v in h.items() if r not in ('rax', 'xmm0')}
                if h: print(fmt(h))

if __name__ == '__main__':
    main()
