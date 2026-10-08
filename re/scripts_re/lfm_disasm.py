#!/usr/bin/env python3
"""Disassembler for Nioh 3 mission script bytecode (data/extract/lfm/archive_09/..).
stdlib only.  Format (see notes/boss-fights.md s7, notes/boss-duelflow-script.md):
  u8 len + date | u16 nNames | nNames x {u16 len,u32 hash,name,\0} | u16 nSyms |
  nSyms x {u16 nameIdx,u16 id,u16 kind} | misc tables | string pool | code (u16 ops).
Call operands (0x1b native, 0x1f script fn) index the symbol table.
Usage: lfm_disasm.py FILE [--calls] [--grep NAME]   (prefix match on file name ok)
Library: load(path) -> Script ; Script.insns (list of Insn) ; Script.native_calls()
"""
import struct, sys, glob, os
from collections import namedtuple

VALID = set(list(range(1, 0x14)) + [0x15, 0x16, 0x19, 0x1b, 0x1c, 0x1d, 0x1f] +
            list(range(0x22, 0x3f + 1)) + [0x40, 0x41, 0x43, 0x44, 0x49, 0x4a, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x52])
# operand layout per opcode: 'H' u16, 'I' u32, 'i' rel32 jump, 'HH' pair ; others none
OPER = {0x1b: 'H', 0x1f: 'H', 0x29: 'I', 0x3e: 'HH', 0x3d: 'HH', 0x3b: 'H', 0x3a: 'H', 0x11: 'I', 0x2d: 'I',
        0x22: 'i', 0x23: 'i', 0x24: 'i', 0x30: 'i', 0x31: 'i', 0x05: 'i',
        0x01: 'H', 0x2f: 'H', 0x50: 'H', 0x51: 'H', 0x35: 'T8', 0x2c: 'T4', 0x2e: 'T4'}
NAME = {0x1b: 'CALL_NATIVE', 0x1f: 'CALL_FN', 0x29: 'PUSH_I32', 0x3e: 'LOAD', 0x3d: 'STORE',
        0x3b: 'FN_ENTRY', 0x3a: 'OP3A', 0x22: 'JMP22', 0x23: 'JMP23', 0x24: 'JMP24', 0x30: 'JMP30', 0x31: 'JMP31',
        0x05: 'JMP05', 0x35: 'SWITCH', 0x52: 'END', 0x2c: 'WORDS2C', 0x2e: 'WORDS2E'}
Insn = namedtuple('Insn', 'off op args')


class Script:
    pass


def _decode(b, start, nsym):
    """Linear decode b[start:]; return insn list or None if invalid."""
    out = []
    p = start
    n = len(b)
    while p < n:
        if p + 2 > n:
            return None
        op = struct.unpack_from('<H', b, p)[0]
        if op not in VALID:
            return None
        o = p
        p += 2
        k = OPER.get(op, '')
        if k == 'H' or k == 'HH':
            fmt = '<' + k
            sz = struct.calcsize(fmt)
            if p + sz > n:
                return None
            a = struct.unpack_from(fmt, b, p)
            p += sz
        elif k == 'I':
            if p + 4 > n:
                return None
            a = struct.unpack_from('<I', b, p)
            p += 4
        elif k == 'i':
            if p + 4 > n:
                return None
            a = struct.unpack_from('<i', b, p)
            p += 4
        elif k in ('T8', 'T4'):
            if p + 2 > n:
                return None
            c = struct.unpack_from('<H', b, p)[0]
            p += 2
            w = 8 if k == 'T8' else 4
            if p + c * w > n:
                return None
            a = (c,) + struct.unpack_from('<%dI' % (c * w // 4), b, p)
            p += c * w
        else:
            a = ()
        if op in (0x1b, 0x1f) and a[0] >= nsym:
            return None
        out.append(Insn(o, op, a))
    return out


def load(path):
    b = open(path, 'rb').read()
    s = Script()
    s.path, s.data = path, b
    p = 1 + b[0]
    s.date = b[1:p].decode('latin1')
    cnt = struct.unpack_from('<H', b, p)[0]
    p += 2
    s.names, s.hashes = [], []
    for _ in range(cnt):
        l, h = struct.unpack_from('<HI', b, p)
        s.names.append(b[p + 6:p + 6 + l].decode('latin1'))
        s.hashes.append(h)
        p += 7 + l
    n = struct.unpack_from('<H', b, p)[0]
    p += 2
    s.syms = [struct.unpack_from('<HHH', b, p + 6 * i) for i in range(n)]
    p += 6 * n
    s.after_syms = p
    # string pool: utf-8 NUL separated text; code start = first even offset that decodes to EOF
    s.code_start, s.insns = None, None
    # code begins with the function table: op 0x35, u16 = nSyms+1 (found by signature; the size u32 before it is unreliable)
    sig = bytes([0x35, 0]) + struct.pack('<H', n + 1)
    cands, q = [], b.find(sig, p)
    while q >= 0:
        cands.append(q)
        q = b.find(sig, q + 1)
    for off in cands:
        ins = _decode(b, off, n)
        if ins is not None and len(ins) > 20:
            s.code_start, s.insns = off, ins
            break
    s.pool = b[p:s.code_start] if s.code_start else b''
    return s


def symname(s, idx):
    return s.names[s.syms[idx][0]] if 0 <= idx < len(s.syms) else '?%d' % idx


def insn_name(s, i):
    """Target name of a call/marker insn.  NOTE (verified, see notes/boss-duelflow-script.md):
    CALL_NATIVE (0x1b) operand is the 1-based sym *id* (entry = operand-1); CALL_FN (0x1f) and FN_ENTRY (0x3b)
    operands are 0-based indexes into the sym table."""
    return symname(s, i.args[0] - 1 if i.op == 0x1b else i.args[0])


def fnames(s):
    """map code offset -> user fn name from 0x3b markers (name = sym name)."""
    return {i.off: symname(s, i.args[0]) for i in s.insns if i.op == 0x3b}


def native_calls(s):
    """yield (insn_index, name, [const int args immediately pushed before the call], enclosing fn)
    Args are the maximal run of PUSH_I32 / LOAD directly before the call: ints or ('var',k,i)."""
    cur = '<top>'
    res = []
    for k, i in enumerate(s.insns):
        if i.op == 0x3b:
            cur = symname(s, i.args[0])
        if i.op in (0x1b, 0x1f):
            res.append((k, insn_name(s, i), call_args(s, k), cur, i.op == 0x1b))
    return res


def tok(s, i):
    """compact token for an argument-ish instruction"""
    o = i.op
    if o == 0x29:
        v = i.args[0]
        return str(v - (1 << 32) if v >= 0x80000000 else v)
    if o == 0x2f:
        return 'u%d' % i.args[0]
    if o == 0x2d:
        return 'f%g' % struct.unpack('<f', struct.pack('<I', i.args[0]))[0]
    if o in (0x3e, 0x3d):
        return ('v%d.%d' if o == 0x3e else 'set%d.%d') % tuple(i.args)
    if o in (0x2c, 0x2e):
        return '[' + ','.join('%08x' % w for w in i.args[1:]) + ']'
    if o == 0x15:
        return 'T'
    if o == 0x16:
        return 'F'
    return 'op%02x' % o


def statements(s):
    """Split code into statements started by op03.  Each: (start_off, fn, [insns]).
    Observed convention: `op03 <arg pushes...> CALL`; the pushes between op03 and the call are its args
    (nested calls / expressions show up as extra insns, see notes)."""
    out, cur, fn = [], None, '<top>'
    for i in s.insns:
        if i.op == 0x3b:
            fn = symname(s, i.args[0])
        if i.op == 0x03:
            cur = (i.off, fn, [])
            out.append(cur)
        elif cur is not None:
            cur[2].append(i)
    return out


def call_args(s, k):
    """tokens of the arg-ish insns immediately before insns[k] back to the previous op03 or non-arg op"""
    toks = []
    j = k - 1
    while j >= 0 and s.insns[j].op in (0x29, 0x2f, 0x2d, 0x3e, 0x15, 0x16, 0x2b, 0x2a, 0x2c, 0x2e, 0x0a, 0x0c):
        toks.append(tok(s, s.insns[j]))
        j -= 1
    toks.reverse()
    return toks


def fmt(s, i):
    nm = NAME.get(i.op, 'op%02x' % i.op)
    if i.op in (0x1b, 0x1f, 0x3b):
        return '%-11s %s' % (nm, insn_name(s, i))
    if i.op == 0x29:
        v = i.args[0]
        return '%-11s %d%s' % (nm, v if v < 0x80000000 else v - (1 << 32), '  (0x%x)' % v if v > 9 else '')
    if i.op in (0x3e, 0x3d):
        return '%-11s kind=%d idx=%d' % (nm, *i.args)
    if i.op in (0x22, 0x23, 0x24, 0x30, 0x31, 0x05):
        return '%-11s -> %x (rel %d)' % (nm, i.off + 6 + i.args[0], i.args[0])
    return '%-11s %s' % (nm, ' '.join(str(a) for a in i.args))


def L_fmt(s, i):
    if i.op in (0x1b, 0x1f):
        return ('%s' if i.op == 0x1b else '%s()') % insn_name(s, i)
    if i.op == 0x3b:
        return '<fn %s>' % symname(s, i.args[0])
    if i.op in (0x22, 0x23, 0x24, 0x30, 0x31, 0x05):
        return '%s->%x' % (NAME.get(i.op), i.off + 6 + i.args[0])
    return tok(s, i) if i.op in (0x29, 0x2f, 0x2d, 0x3e, 0x3d, 0x15, 0x16) else NAME.get(i.op, 'op%02x' % i.op)


def resolve(arg):
    c = glob.glob(arg)
    if c:
        return c[0]
    c = glob.glob(os.path.join(os.path.dirname(__file__), '..', '..', 'data', 'extract', 'lfm', 'archive_09', '*', arg + '*'))
    if not c:
        raise SystemExit('no file ' + arg)
    return c[0]


if __name__ == '__main__':
    a = sys.argv[1:]
    if not a:
        raise SystemExit(__doc__)
    s = load(resolve(a[0]))
    if not s.insns:
        raise SystemExit('code start not found')
    print('; %s date=%s names=%d syms=%d code@%x insns=%d' % (os.path.basename(s.path)[:12], s.date, len(s.names), len(s.syms), s.code_start, len(s.insns)))
    if '--stmts' in a:
        for off, fn, ins in statements(s):
            print('%6x %-28s %s' % (off, fn[:28], ' '.join(L_fmt(s, i) for i in ins)))
    elif '--calls' in a:
        for k, nm, args, fn, nat in native_calls(s):
            print('%6x %-30s %s(%s)' % (s.insns[k].off, fn, nm, ', '.join(str(x) for x in args)))
    else:
        for i in s.insns:
            if i.op == 0x3b:
                print()
            print('%6x  %s' % (i.off, fmt(s, i)))
