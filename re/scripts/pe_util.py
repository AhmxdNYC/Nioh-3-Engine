"""Small offline helpers over Nioh3.dump.exe (raw offset == RVA).
  py pe_util.py func 0x5af50a [...]   -> primary function start/end from .pdata (chained unwind resolved)
  py pe_util.py imports               -> library!name  IAT-RVA  (from the import directory's name table)

"""
import struct, sys, os
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Nioh3.dump.exe')
f = open(P, 'rb').read()
pe = struct.unpack_from('<I', f, 0x3c)[0]; opt = pe + 24
def ddir(i): return struct.unpack_from('<II', f, opt + 112 + 8 * i)
u32 = lambda o: struct.unpack_from('<I', f, o)[0]
PD, PDS = ddir(3)
N = PDS // 12
begins = [u32(PD + 12 * i) for i in range(N)]
import bisect
def func(r):
    i = bisect.bisect_right(begins, r) - 1
    if i < 0: return None
    b, e, u = struct.unpack_from('<III', f, PD + 12 * i)
    if not (b <= r < e): return None
    frag = (b, e)
    for _ in range(32):
        if u & 1: b, e, u = struct.unpack_from('<III', f, u & ~1); continue
        if not (f[u] >> 3) & 4: break
        cnt = f[u + 2]; c = u + 4 + ((cnt + 1) & ~1) * 2
        b, e, u = struct.unpack_from('<III', f, c)
    return b, e, frag
def imports():
    IR, _ = ddir(1); o = IR
    while True:
        ilt, ts, fc, name, iat = struct.unpack_from('<IIIII', f, o)
        if not iat: break
        lib = f[name:f.index(b'\0', name)].decode()
        t = ilt or iat; k = 0
        while True:
            v = struct.unpack_from('<Q', f, t + 8 * k)[0]
            if not v: break
            if v >> 63: nm = '#%d' % (v & 0xffff)
            elif v < len(f): nm = f[v + 2:f.index(b'\0', v + 2)].decode()
            else: nm = '?resolved:%x' % v
            yield lib, nm, iat + 8 * k; k += 1
        o += 20
if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'func':
        for a in sys.argv[2:]:
            r = int(a, 16); x = func(r)
            print(a, '->', 'none' if not x else 'start=0x%x end=0x%x (fragment 0x%x-0x%x)' % (x[0], x[1], x[2][0], x[2][1]))
    elif cmd == 'imports':
        for lib, nm, iat in imports(): print('%s\t%s\t0x%x' % (lib, nm, iat))
