r"""Offline check for notes/load-memory.md (LM sweeper: MEM_RESET of dead payload of free arena-heap chunks).
Part 1: static facts against re/Nioh3.dump.exe.  Part 2: model of the heap (layout from the decompile of
HeapFree 130D0C / HeapAlloc 31242C / HeapReset 9C0A94) run twice on identical random op streams, once with the
sweeper zeroing free payload pages; asserts identical addresses/headers and intact live payloads.
Usage: py -I re\scripts_stutter\loadmem_check.py"""
import struct, random, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
d = open(os.path.join(HERE, '..', 'Nioh3.dump.exe'), 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]; n = struct.unpack_from('<H', d, pe+6)[0]; so = struct.unpack_from('<H', d, pe+20)[0]
secs = [struct.unpack_from('<IIII', d, pe+24+so+8+i*40) for i in range(n)]
def rd(rva, l):
    for vs, va, rs, rp in secs:
        if va <= rva < va+max(vs, rs): return d[rp+rva-va:rp+rva-va+l]
BASE = 0x7ff74f410000
def q(rva): return struct.unpack('<Q', rd(rva, 8))[0]
ok = True
def chk(name, cond):
    global ok; ok &= bool(cond); print(('OK   ' if cond else 'FAIL ') + name)
# --- part 1: static
vt = [q(0x408f940+8*i)-BASE for i in range(14)]
chk('heap vtable 408F940 slot 11 (+58h) == HeapFree 130D0C', vt[11] == 0x130d0c)
chk('heap vtable slot 7 (+38h) == HeapAlloc 3123B4', vt[7] == 0x3123b4)
chk('heap vtable2 408FB78 present (shares the chunk layout)', q(0x408fb78) > BASE)
free = rd(0x130d0c, 0x190)
chk('HeapFree prologue (mov [rsp+8],rbx; mov [rsp+10h],rsi; push rdi)', free[:11].hex() == '48895c240848897424105 7'.replace(' ', ''))
chk('HeapFree ends in jmp _Mtx_unlock at 130E8C (E9 rel32 -> B61524)', rd(0x130e8c, 1) == b'\xe9' and 0x130e8c+5+struct.unpack('<i', rd(0x130e8d, 4))[0] == 0xb61524)
chk('HeapFree locks via call B614A8 at 130D25', rd(0x130d25, 1) == b'\xe8' and 0x130d25+5+struct.unpack('<i', rd(0x130d26, 4))[0] == 0xb614a8)
chk('HeapReset 9C0A94 prologue (then rax=[rcx+28h] chunk0, [rcx+58h]=0)', rd(0x9c0a94, 14).hex() == '48895c2408574883ec20488b4128')
chk('HeapFree footprint: size sign test (js after [rcx+8]) at 130EDC', rd(0x130edc, 2) == b'\x78\x5b')
# --- part 2: model
HDR = 0x20; PAGE = 0x1000
class Heap:
    def __init__(s, size, sweep):
        s.m = bytearray(size); s.size = size; s.sweep = sweep
        s.c0 = 0x90; s.end = size & ~0xf
        s.w(s.c0, 0); s.w(s.c0+8, s.end-s.c0-HDR); s.w(s.c0+16, 0); s.w(s.c0+24, 0)
        s.bins = {}  # model of bin table lives outside payloads (region head, 0x80 bytes)
        s.bins[0] = [s.c0]
        s.used = 0
    def w(s, a, v): struct.pack_into('<q', s.m, a, v)
    def r(s, a): return struct.unpack_from('<q', s.m, a)[0]
    def freelist(s):
        out = []; c = s.c0
        while c < s.end:
            S = s.r(c+8)
            if S > 0: out.append(c)
            c += HDR + abs(S)
        return out
    def alloc(s, req):
        req = (req + 15) & ~15 or 16
        best = None
        for c in s.freelist():          # reads headers only
            S = s.r(c+8)
            if S >= req + HDR + 16 or S == req:
                if best is None or S < s.r(best+8): best = c
        if best is None: return 0
        c = best; S = s.r(c+8)
        if S >= req + HDR + 16:
            rem = c + HDR + req
            s.w(rem, c); s.w(rem+8, S-HDR-req); s.w(rem+16, 0); s.w(rem+24, 0)   # all 4 header fields written
            nxt = rem + HDR + (S-HDR-req)
            if nxt < s.end: s.w(nxt, rem)
            s.w(c+8, -req)
        else: s.w(c+8, -S)
        return c + HDR
    def free(s, p):
        c = p - HDR; S = -s.r(c+8); s.w(c+8, S)
        prv = s.r(c)
        if c != s.c0 and s.r(prv+8) > 0:        # merge prev (free)
            s.w(prv+8, s.r(prv+8) + HDR + S); c = prv; S = s.r(c+8)
        nxt = c + HDR + S
        if nxt < s.end and s.r(nxt+8) > 0:       # merge next
            S = S + HDR + s.r(nxt+8); s.w(c+8, S)
        nn = c + HDR + S
        if nn < s.end: s.w(nn, c)
    def do_sweep(s, minS):
        cnt = 0
        for c in s.freelist():
            S = s.r(c+8)
            if S >= minS:
                a = (c + HDR + PAGE - 1) & ~(PAGE-1); b = (c + HDR + S) & ~(PAGE-1)
                if b > a:
                    s.m[a:b] = bytes(b-a); cnt += b-a
                    assert a >= c+HDR and b <= c+HDR+S            # never header, never allocated, never next header
        return cnt
def run(seed, sweep):
    rnd = random.Random(seed); h = Heap(8 << 20, sweep); live = {}; trace = []; dropped = 0
    for i in range(6000):
        op = rnd.random()
        if op < 0.55 or not live:
            req = rnd.choice([64, 200, 4096, 30000, 90000, 300000, 700000]) if rnd.random() < .6 else rnd.randrange(16, 5000)
            p = h.alloc(req); trace.append(p)
            if p:
                n = -h.r(p-HDR+8); data = bytes(rnd.randrange(256) for _ in range(max(0, min(n-1, 63)))) * 1
                h.m[p:p+len(data)] = data; h.m[p+n-1] = (i & 0xff) | 1; live[p] = (n, data, (i & 0xff) | 1)
        else:
            p = rnd.choice(list(live)); del live[p]; h.free(p); trace.append(-p)
        if sweep and i % 50 == 0: dropped += h.do_sweep(256 << 10)
        for p, (n, data, tag) in list(live.items())[:4]:   # live payloads intact
            assert h.m[p:p+len(data)] == data and h.m[p+n-1] == tag
    return trace, bytes(h.m), h, dropped
bad = 0; tot = 0
for seed in range(20):
    ta, ma, ha, _ = run(seed, False); tb, mb, hb, dr = run(seed, True); tot += dr
    # headers of every chunk must be byte-identical; only free payload may differ
    same = ta == tb
    c = ha.c0
    while same and c < ha.end:
        S = ha.r(c+8); same &= (ma[c:c+HDR] == mb[c:c+HDR]) and hb.r(c+8) == S
        c += HDR + abs(S)
    bad += not same
chk('model: 20 seeds x 6000 ops, same alloc addresses + identical chunk headers with sweeper (zeroed %.1f MiB total)' % (tot/2**20), bad == 0 and tot > 0)
print('ALL LOADMEM CHECKS OK' if ok else 'LOADMEM CHECKS FAILED'); sys.exit(0 if ok else 1)
