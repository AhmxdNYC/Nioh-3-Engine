"""leaf_ranges.py - map the code that has NO .pdata entry to its real function (writes re\\out\\leaf_ranges.tsv).

usage: py -I re\\scripts_re\\leaf_ranges.py [--check 0xRVA ...]      (needs capstone; ~1 min)

Why: x64 .pdata covers every function that touches the stack, but not
  - leaf functions (no prolog: getters, setters, small math, tail-call thunks), and
  - cold blocks split off a leaf function or out of a .pdata function's hot range (only a branch leads there).
Profiler samples landing there used to fold to themselves (perf_coverage.fold), so they were given bogus rows such as
Unknown_NotAFunction_<rva> / Unknown_MidFunction_<rva>. Ghidra's function table misses most of them too, and sometimes
stretches a .pdata function over the leaf that follows it.

Method, per gap between consecutive .pdata ranges inside .text:
  linear disassembly; int3 runs are padding; a block ends after an unconditional ret/jmp/int3/ud2.
  Owner of a block, first rule that applies:
    1. target of a direct call (E8), or of an absolute pointer in .rdata/.data/_RDATA (vtable, callback) -> function start
    2. target of a jcc/jmp from X, and either packed (not right after padding) or, when right after padding, the piece
       branches back into owner(X)'s body                                                 -> owner(X)  (cold piece)
    3. right after padding / at the gap start (a jcc/jmp target there that never returns into the jumper is a
       (conditional) tail call)                                                           -> function start (leaf)
    4. otherwise (packed after a ret/jmp, nothing branches to it)                         -> owner of the block before it
  X is either a .pdata function (folded through chained unwind info) or another gap block (resolved recursively).
Output rows (hex, no 0x): start size owner   (owner empty = the range is its own leaf function; else a cold piece of owner, which is a
.pdata function or another leaf). Consecutive blocks with the same owner are merged.
"""
import bisect, os, re, struct, sys

from capstone import CS_ARCH_X86, CS_MODE_64, Cs

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DUMP = os.path.join(ROOT, 're', 'Nioh3.dump.exe')
OUT = os.path.join(ROOT, 're', 'out', 'leaf_ranges.tsv')
BASE = 0x7FF74F410000

B = open(DUMP, 'rb').read()
_e = struct.unpack_from('<I', B, 0x3c)[0]
_ns, _oh = struct.unpack_from('<H', B, _e + 6)[0], struct.unpack_from('<H', B, _e + 20)[0]
SEC = {}
for i in range(_ns):
    n, vs, va = struct.unpack_from('<8sII', B, _e + 24 + _oh + 40 * i)
    SEC[n.rstrip(b'\0').decode()] = (va, va + vs)
TXT = SEC['.text']
_rva, _sz = struct.unpack_from('<II', B, _e + 24 + 112 + 3 * 8)
RF = sorted(struct.unpack_from('<III', B, _rva + 12 * i) for i in range(_sz // 12))
ST = [r[0] for r in RF]


def primary(x):
    i = bisect.bisect_right(ST, x) - 1
    if i < 0 or not (RF[i][0] <= x < RF[i][1]):
        return None
    r = RF[i]
    for _ in range(10):
        ui = r[2] & ~1
        flags, cnt = B[ui] >> 3, B[ui + 2]
        if not flags & 4:
            break
        r = struct.unpack_from('<III', B, ui + 4 + ((cnt + 1) & ~1) * 2)
    return r[0]


def data_ptrs():
    """code addresses stored as absolute pointers in .rdata/.data/_RDATA (vtables, callback tables)."""
    out = set()
    for s in ('.rdata', '.data', '_RDATA'):
        lo, hi = SEC[s]
        for (v,) in struct.iter_unpack('<Q', B[lo:hi - (hi - lo) % 8]):
            if TXT[0] <= v - BASE < TXT[1]:
                out.add(v - BASE)
    return sorted(out)


def blocks(anc_all):
    """yield (start, end, after_pad, [(kind, target)]) for every straight-line block in the .pdata gaps.
    Pointer-referenced addresses (anc_all, sorted) always start a block and resync the sweep (data inside a gap)."""
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.skipdata = True
    for i in range(len(RF) - 1):
        lo, hi = RF[i][1], RF[i + 1][0]
        if hi <= lo or not B[lo:hi].strip(b'\xcc') or not (TXT[0] <= lo < TXT[1]):
            continue
        anc = anc_all[bisect.bisect_left(anc_all, lo):bisect.bisect_left(anc_all, hi)]
        ai = 0
        a, pad, cur = lo, True, None
        while a < hi:
            while ai < len(anc) and anc[ai] < a:
                ai += 1
            if ai < len(anc) and anc[ai] == a and cur:
                yield cur
                cur, pad = None, True
            if B[a] == 0xcc:
                if cur:
                    yield cur
                    cur = None
                while a < hi and B[a] == 0xcc:
                    a += 1
                pad = True
                continue
            ins = next(md.disasm(B[a:min(a + 16, hi)], a), None)
            if ins is None or ins.mnemonic == '.byte':
                a += 1
                continue
            if ai < len(anc) and a < anc[ai] < a + ins.size:  # decoding ran across a pointer target: resync there
                if cur:
                    yield cur
                cur, pad, a = None, True, anc[ai]
                continue
            if cur is None:
                cur = [a, a, pad, []]
                pad = False
            m = ins.mnemonic
            if (m.startswith('j') or m == 'call') and ins.op_str.startswith('0x'):
                cur[3].append(('call' if m == 'call' else 'jmp' if m == 'jmp' else 'jcc', int(ins.op_str, 16)))
            a += ins.size
            cur[1] = a
            if m in ('ret', 'jmp', 'ud2', 'int3') or m.startswith('ret'):
                yield cur
                cur = None
        if cur:
            yield cur


def main():
    dp = data_ptrs()
    blk = list(blocks(dp))
    starts = {b[0]: k for k, b in enumerate(blk)}
    print(f'{len(blk)} blocks in .pdata gaps', file=sys.stderr)
    # references into block starts from the whole image
    fstart, jcc_from, jmp_from = set(), {}, {}
    for k, b in enumerate(blk):  # branches inside gap code (decoded, exact)
        for kind, t in b[3]:
            if t in starts:
                if kind == 'call':
                    fstart.add(t)
                else:
                    (jcc_from if kind == 'jcc' else jmp_from).setdefault(t, []).append(('g', k))
    tx = B[TXT[0]:TXT[1]]
    for m in re.finditer(rb'\xe8|\xe9|\x0f[\x80-\x8f]', tx):  # rel32 branches from .pdata code (byte scan, exact-start filter)
        a = TXT[0] + m.start()
        n = m.end() - m.start()
        t = a + n + 4 + struct.unpack_from('<i', B, a + n)[0]
        if t not in starts or primary(a) is None:
            continue
        if B[a] == 0xe8:
            fstart.add(t)
        else:
            (jmp_from if B[a] == 0xe9 else jcc_from).setdefault(t, []).append(('p', a))
    fstart.update(x for x in dp if x in starts)  # absolute pointers (vtables, callback tables)
    owner = {}
    bs = [b[0] for b in blk]

    def src_owner(src, depth):
        return primary(src[1]) if src[0] == 'p' else own(src[1], depth + 1)

    def returns_into(k, o, depth):
        """does the piece starting at block k (k and the packed blocks after it) branch into o's body (not o's start)?"""
        j = k
        while j < len(blk) and (j == k or (not blk[j][2] and blk[j - 1][1] == blk[j][0])) and j < k + 16:
            for _, t in blk[j][3]:
                if t == o:
                    continue
                p = primary(t)
                if p is not None:
                    if p == o:
                        return True
                    continue
                i = bisect.bisect_right(bs, t) - 1
                if i >= 0 and t < blk[i][1] and not (k <= i <= j) and own(i, depth + 1) == o:
                    return True
            j += 1
        return False

    def own(k, depth=0):
        if k in owner:
            return owner[k]
        b = blk[k]
        s = b[0]
        if depth > 50:
            return s
        owner[k] = s  # cycle guard
        # branches from inside the block itself (loops) say nothing about the owner
        srcs = [x for x in jcc_from.get(s, []) + jmp_from.get(s, []) if x != ('g', k)]
        r = None
        if s in fstart:
            r = s
        elif srcs and not b[2]:
            for src in srcs:  # first source that does not resolve back to this block (rotated loops)
                o = src_owner(src, depth)
                if o != s:
                    r = o
                    break
            if r is None:  # only reached from blocks that resolve to it: continue the previous block's function
                r = own(k - 1, depth + 1) if k and blk[k - 1][1] == s else s
        elif srcs:
            r = s
            for src in srcs:
                o = src_owner(src, depth)
                if o != s and returns_into(k, o, depth):
                    r = o
                    break
        elif b[2]:
            r = s
        else:  # packed after an unconditional transfer, unreferenced: continue the previous block's function
            r = own(k - 1, depth + 1) if k and blk[k - 1][1] == s else s
        owner[k] = r
        return r

    rows = []
    for k in range(len(blk)):
        o = own(k)
        s, e = blk[k][0], blk[k][1]
        if rows and rows[-1][2] == o and rows[-1][1] == s:
            rows[-1][1] = e
        else:
            rows.append([s, e, o])
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('# generated by re/scripts_re/leaf_ranges.py: code without .pdata -> owning function (see the script docstring)\n')
        f.write('start\tsize\towner\n')
        for s, e, o in rows:
            f.write(f'{s:x}\t{e - s:x}\t{"" if o == s else "%x" % o}\n')
    print(f'{len(rows)} ranges -> {OUT}; leaf functions {sum(1 for r in rows if r[2] == r[0])}, '
          f'cold pieces {sum(1 for r in rows if r[2] != r[0])}', file=sys.stderr)
    if '--check' in sys.argv:
        ss = [r[0] for r in rows]
        for h in sys.argv[sys.argv.index('--check') + 1:]:
            x = int(h, 16)
            i = bisect.bisect_right(ss, x) - 1
            r = rows[i] if i >= 0 and rows[i][0] <= x < rows[i][1] else None
            print(f'{x:#x} -> {"%#x %s" % (r[2], "leaf" if r[2] == r[0] else "cold piece") if r else "none"}')


if __name__ == '__main__':
    main()
