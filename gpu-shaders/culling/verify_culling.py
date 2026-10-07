r"""verify_culling.py - prove that the replacement MPC prefix-sum chain computes exactly what the shipped chain computes.

    py -I verify_culling.py            (needs sweep\build\mpc_<k>_orig.ll / _fast.ll: run sweep\build_sweep.py mpc first)
    py -I verify_culling.py quick      (skip the two large-count scenes)
    py -I verify_culling.py selftest   (the chain checker must catch broken chains)
    py -I verify_culling.py mshlod [selftest]   MshLod#3 (item "mshlod"; needs sweep\build\mshlod_orig.ll / _fast.ll)

Both DXIL programs of every record (the shipped DXBC from the game file and our compiled DXIL, as `dxc -dumpbin`
text) are executed by the reference interpreter (vendor\sim.py: per-IR-op semantics, typed-buffer loads/stores with
robust out-of-bounds behaviour, groupshared memory with barriers and an LDS race detector) on the same synthetic
memory, dispatched exactly as MeshletPreculling 5301FC records them:

    #23 (ceil(cnt/1024),1,1) | #28 (((cnt+63)>>6)+511)>>9 | #29 (1,1,1) | #30 as #28 | #31 (ceil(cnt/1024),1,1) | #26 (1,1,1)

with a UAV barrier between dispatches (the interpreter runs them one after the other). Checks per scene:
  A  file-only install:   orig #23 #28 #29 #30 #31 #26  ==  fast #23 #28 #29 #30 #31 + orig #26   (all UAV contents)
  B  engine skip (ASI):   orig chain                     ==  fast #23 [+ fast #28-#31 only if cnt > 1024]   (no #26)
  W  write sets:          every address the fast chain writes is one the shipped chain writes
  R  per record, cnt > 1024: fast #k alone == orig #k alone on the same memory (contents and write log)
Scenes: cnt around every 64/1024 boundary, random u32 contents (full range: sums wrap mod 2^32), several
nMshltPrm2.x offsets, u9[0] (the culled-meshlet count read by #26) across the 2048 rounding boundaries.
"""
import os, random, struct, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "vendor"))
import sim  # noqa: E402

BUILD = os.path.join(HERE, "..", "sweep", "build")       # the combined 0x29ee0902.file is built by build_sweep.py "mpc"
CHAIN = [23, 28, 29, 30, 31]


def ll26():
    """shipped MPC#26 (not replaced; run after the chain) -> build/mpc26_orig.ll, from gpu-shaders/backup (read-only)."""
    out = os.path.join(HERE, "build", "mpc26_orig.ll")
    if not os.path.exists(out):
        import subprocess, s2g
        os.makedirs(os.path.dirname(out), exist_ok=True)
        g = s2g.Group(open(os.path.join(HERE, "..", "backup", "0x29ee0902.file"), "rb").read())
        d = g.dxbc(26)
        assert d[4:20].hex() == "5f610a3efc8e62dc02a4135c7d1b6d33", "MPC#26 digest - game updated?"
        open(out[:-3] + ".dxbc", "wb").write(d)
        dxc = os.path.join(HERE, "..", "..", "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12",
                           "dxc", "dxc.exe")
        open(out, "w", encoding="utf-8", newline="\n").write(
            subprocess.run([dxc, "-dumpbin", out[:-3] + ".dxbc"], capture_output=True, text=True, check=True).stdout)
    return out


def load(k, kind):
    p = ll26() if k == 26 else os.path.join(BUILD, "mpc_%d_%s.ll" % (k, kind))
    return sim.BShader(open(p, encoding="utf-8").read(), "mpc%d_%s" % (k, kind))


def groups(k, cnt):
    if k in (23, 31):
        n = (cnt + 1023) >> 10
    elif k in (28, 30):
        n = (((cnt + 63) >> 6) + 511) >> 9
    else:
        n = 1
    return [(g, 0, 0) for g in range(n)]


class Mem:
    def __init__(self, seed, cnt, off):
        rng = random.Random(seed)
        hi = rng.choice([1 << 32, 1 << 8, 1 << 31])
        n = cnt + 64
        self.cnt, self.off = cnt, off
        u9_0 = rng.choice([0, 1, 2047, 2048, 2049, rng.randrange(1 << 20), (1 << 32) - 1, (1 << 32) - 2048])
        self.u2 = sim.TypedBuffer([rng.randrange(hi) for _ in range(off + 4 * n + 16)], isint=True)
        self.u9 = sim.TypedBuffer([u9_0] + [rng.randrange(1 << 32) for _ in range(9 * n + 32)], isint=True)
        # u11: 3 per draw + the L1 scratch after it (2 per 64-block) + slack
        self.u11 = sim.TypedBuffer([rng.randrange(hi) for _ in range(3 * n + 2 * (n // 64 + 2) + 16)], isint=True)
        self.cb23 = sim.CBuffer(struct.pack("<4i4I", off, rng.randrange(-5, 5), 0, 0, rng.randrange(1 << 32), cnt,
                                            rng.randrange(1 << 32), 0))
        self.cbv = sim.CBuffer(struct.pack("<4I", rng.randrange(1 << 32), cnt, rng.randrange(1 << 32), 0))

    def scene(self, k):
        cb = self.cb23 if k == 23 else self.cbv
        return sim.Scene({(2, 0): cb, (1, 2): self.u2, (1, 9): self.u9, (1, 11): self.u11})

    def state(self):
        return [x[0] for x in self.u2.data], [x[0] for x in self.u9.data], [x[0] for x in self.u11.data]

    def written(self):
        return {(r, w[1]) for r, b in (("u2", self.u2), ("u9", self.u9), ("u11", self.u11)) for w in b.writes}


def run(mem, prog, seq):
    for k in seq:
        prog[k].dispatch(mem.scene(k), groups(k, mem.cnt))


def diff(a, b):
    for name, x, y in zip(("u2", "u9", "u11"), a, b):
        if x != y:
            i = next(j for j in range(len(x)) if x[j] != y[j])
            return "%s[%d]: orig %d fast %d" % (name, i, x[i], y[i])
    return None


def main():
    quick = "quick" in sys.argv[1:]
    orig = {k: load(k, "orig") for k in CHAIN + [26]}
    fast = {k: load(k, "fast") for k in CHAIN}
    rng = random.Random(2310)
    counts = [1, 2, 3, 15, 16, 17, 63, 64, 65, 66, 127, 128, 129, 130, 191, 192, 193, 500, 959, 960, 961, 1000, 1022,
              1023, 1024, 1025, 1026, 1087, 1088, 1089, 2047, 2048, 2049, 3000]
    if not quick:
        counts += [33000]
    n = bad = 0
    t0 = time.time()
    for cnt in counts:
        reps = 3 if cnt <= 1100 else 1
        for rep in range(reps):
            off = rng.choice([0, 1, 7, 100])
            seed = rng.randrange(1 << 30)
            # A: file-only
            mo, mf = Mem(seed, cnt, off), Mem(seed, cnt, off)
            run(mo, orig, CHAIN + [26])
            run(mf, fast, CHAIN)
            run(mf, orig, [26])
            so = mo.state()
            d = diff(so, mf.state())
            wo = mo.written()
            extra = mf.written() - wo
            # B: engine skip (ASI): #28-#31 not dispatched when cnt <= 1024, #26 never dispatched
            mb = Mem(seed, cnt, off)
            run(mb, fast, [23] if cnt <= 1024 else CHAIN)
            db = diff(so, mb.state())
            extra_b = mb.written() - wo
            n += 1
            msg = []
            if d: msg.append("A " + d)
            if db: msg.append("B " + db)
            if extra: msg.append("W extra writes %s" % sorted(extra)[:4])
            if extra_b: msg.append("W(B) extra writes %s" % sorted(extra_b)[:4])
            # R: per record for cnt > 1024 (same memory before each record)
            if cnt > 1024 and rep == 0:
                for k in CHAIN:
                    m1, m2 = Mem(seed, cnt, off), Mem(seed, cnt, off)
                    pre = CHAIN[:CHAIN.index(k)]
                    run(m1, orig, pre)
                    run(m2, orig, pre)
                    m1.u2.writes, m1.u9.writes, m1.u11.writes = [], [], []
                    m2.u2.writes, m2.u9.writes, m2.u11.writes = [], [], []
                    run(m1, orig, [k])
                    run(m2, fast, [k])
                    dk = diff(m1.state(), m2.state())
                    if k == 23:
                        # the fast #23 additionally writes u9[1] (= what #26 writes later in the chain)
                        w1 = sorted(w[1:] for b in (m1.u9, m1.u11) for w in b.writes)
                        w2 = sorted(w[1:] for b in (m2.u9, m2.u11) for w in b.writes if not (b is m2.u9 and w[1] == 1))
                        if dk and dk.startswith("u9[1]:"):
                            dk = None
                    else:
                        w1 = sorted(w[1:] for b in (m1.u9, m1.u11) for w in b.writes)
                        w2 = sorted(w[1:] for b in (m2.u9, m2.u11) for w in b.writes)
                    if dk or w1 != w2:
                        msg.append("R #%d %s" % (k, dk or "write log differs"))
            if msg:
                bad += 1
                print("  FAIL cnt=%d off=%d seed=%d: %s" % (cnt, off, seed, "; ".join(msg)))
        print("  cnt=%5d ok so far (%d scenes, %d failures, %.0f s)" % (cnt, n, bad, time.time() - t0))
    print("MPC chain #23/#28/#29/#30/#31 (+#26): %d scenes, %d failures -> %s" % (n, bad, "PASS" if bad == 0 else "FAIL"))
    return bad == 0


def selftest():
    """the checker must catch wrong chains: (1) shipped #23 alone instead of the chain, (2) shipped chain without #30,
    (3) fast chain with the shipped #23 (block-local values, later stages skipped), (4) fast chain without #26."""
    orig = {k: load(k, "orig") for k in CHAIN + [26]}
    fast = {k: load(k, "fast") for k in CHAIN}
    mixed = dict(fast); mixed[23] = orig[23]; mixed[26] = orig[26]
    cases = [("orig #23 alone", orig, [23], 200), ("orig chain without #30", orig, [23, 28, 29, 31, 26], 3000),
             ("fast chain with orig #23", mixed, CHAIN + [26], 300), ("fast chain, no #26 (file-only)", fast, CHAIN, 1500)]
    ok = True
    for name, prog, seq, cnt in cases:
        mo, mx = Mem(5, cnt, 3), Mem(5, cnt, 3)
        run(mo, orig, CHAIN + [26])
        run(mx, prog, seq)
        d = diff(mo.state(), mx.state())
        if 26 not in seq and prog is fast:
            d = d if (d is None or not d.startswith("u9[1]")) else None    # fast #23 writes u9[1] itself
            caught = d is None                                                # expected: still identical
            print("  selftest %-32s cnt=%d -> %s (expected identical)" % (name, cnt, "identical" if caught else d))
        else:
            caught = d is not None
            print("  selftest %-32s cnt=%d -> %s" % (name, cnt, d if d else "NOT DETECTED"))
        ok &= caught
    print("selftest:", "PASS" if ok else "FAIL")
    return ok


# ------------------------------------------------------------------------------------------------- MshLod#3 (item mshlod)
M32 = 0xFFFFFFFF


class LodMem:
    def __init__(self, seed, n, x, nmax, poff, aoff):
        rng = random.Random(seed)
        self.n, self.x, self.nmax, self.poff, self.aoff = n, x, nmax, poff, aoff
        self.u1 = sim.TypedBuffer([rng.randrange(1 << 32) for _ in range(aoff + 4 * 64 + 16)], isint=True)
        self.u3 = sim.TypedBuffer([x] + [rng.randrange(1 << 32) for _ in range(3)], isint=True)
        self.u4 = sim.DI.StructBuffer(bytes(rng.randrange(256) for _ in range(8 * 600)), 8)
        self.cb = sim.CBuffer(struct.pack("<IIiI", n & M32, aoff, poff, nmax & M32))

    def scene(self):
        return sim.Scene({(2, 0): self.cb, (1, 1): self.u1, (1, 3): self.u3, (1, 4): self.u4})

    def state(self):
        return [v[0] for v in self.u1.data], [v[0] for v in self.u3.data], bytes(self.u4.b)

    def written(self):
        return ({("u1", w[1] & M32) for w in self.u1.writes} | {("u3", w[1] & M32) for w in self.u3.writes}
                | {("u4", w[1] & M32, w[2]) for w in self.u4.writes})


def lod_ref(m, ngroups):
    """the shipped MshLod#3 with GPU semantics (one wave per group: every active lane loads u3[0] before any lane
    stores it), transcribed from the DXIL; groups in dispatch order. Checked against the interpreter (n <= 1)."""
    for g in range(ngroups):
        act = [t for t in range(32 * g, 32 * g + 32) if t < m.n]
        for t in act:
            for j, v in enumerate((0, 1, 1, 0)):
                m.u1.store((m.aoff + 4 * t + j) & M32, None, (v, v, v, v), 15, ("ref", t))
        if not act:
            continue
        c = min(m.u3.data[0][0], m.nmax)
        r = ((c + 31) & M32) >> 5
        m.u3.store(0, None, (r, r, r, r), 15, ("ref", act[0]))
        k, cnt = 0, ((r << 5) - c) & M32
        while k < cnt:                     # shipped loop, entered iff cnt != 0
            p = (c + k) & M32
            if p < m.nmax:
                idx = (m.poff + p) & M32
                m.u4.store(idx, 0, (0, 0, 0, 0), 1, ("ref", act[0]), True)
                m.u4.store(idx, 4, (M32, 0, 0, 0), 1, ("ref", act[0]), True)
            k += 1


def mshlod_selftest():
    """the MshLod#3 check must catch: a stray payload store, a pad without the nMax test, u3[0] read after another
    active lane's store (the interpreter's thread-serial order applied to the shipped code with 2 active threads)."""
    fast = sim.BShader(open(os.path.join(BUILD, "mshlod_fast.ll"), encoding="utf-8").read(), "mshlod3_fast")
    orig = sim.BShader(open(os.path.join(BUILD, "mshlod_orig.ll"), encoding="utf-8").read(), "mshlod3_orig")
    ok = True
    for name, x, nmax, nn in (("stray u4 store", 37, 4096, 1), ("no nMax test", 37, 40, 1), ("serial u3 reads", 37, 4096, 2)):
        me, mx = LodMem(1, nn, x, nmax, 3, 0), LodMem(1, nn, x, nmax, 3, 0)
        lod_ref(me, 1)
        if name == "serial u3 reads":
            orig.dispatch(mx.scene(), [(0, 0, 0)])
        else:
            fast.dispatch(mx.scene(), [(0, 0, 0)])
            c = min(x, nmax) if name == "stray u4 store" else nmax
            mx.u4.store(3 + c + (26 if name == "stray u4 store" else 0), 0, (123, 0, 0, 0), 1, "mut", True)
        caught = me.state() != mx.state()
        print("  selftest %-18s -> %s" % (name, "detected" if caught else "NOT DETECTED"))
        ok &= caught
    print("mshlod selftest:", "PASS" if ok else "FAIL")
    return ok


def mshlod_main():
    orig = sim.BShader(open(os.path.join(BUILD, "mshlod_orig.ll"), encoding="utf-8").read(), "mshlod3_orig")
    fast = sim.BShader(open(os.path.join(BUILD, "mshlod_fast.ll"), encoding="utf-8").read(), "mshlod3_fast")
    rng = random.Random(3003)
    xs = [0, 1, 2, 31, 32, 33, 63, 64, 65, 500, 511, 512, 513, M32, M32 - 30, M32 - 31, M32 - 32, (1 << 31) - 1, 1 << 31]
    nmaxs = [0, 1, 31, 32, 33, 100, 512, 520, 4096, (1 << 31) - 1, 1 << 31, M32, M32 - 5, M32 - 31]
    ns = [0, 1, 2, 3, 16, 31, 32, 33, 40, 64, M32]
    n = bad = refchk = 0
    for x in xs:
        for nmax in nmaxs:
            for nn in ns:
                seed = rng.randrange(1 << 30)
                poff = rng.choice([0, 3, 100, -2, -40, 0x7FFFFFF0, -0x80000000])
                aoff = rng.choice([0, 5, 64])
                groups = [(0, 0, 0)] if nn <= 32 or rng.random() < 0.5 else [(0, 0, 0), (1, 0, 0)]
                me, mf = LodMem(seed, nn, x, nmax, poff, aoff), LodMem(seed, nn, x, nmax, poff, aoff)
                lod_ref(me, len(groups))
                fast.dispatch(mf.scene(), groups)
                msg = []
                if me.state() != mf.state():
                    msg.append("fast != ref")
                if mf.written() != me.written():
                    msg.append("write sets differ: %s" % sorted(mf.written() ^ me.written())[:4])
                if nn <= 1:
                    mo = LodMem(seed, nn, x, nmax, poff, aoff)
                    orig.dispatch(mo.scene(), groups)
                    refchk += 1
                    if mo.state() != me.state() or mo.written() != me.written():
                        msg.append("ref != shipped DXIL (interpreter)")
                n += 1
                if msg:
                    bad += 1
                    print("  FAIL x=%d nmax=%d n=%d poff=%d aoff=%d groups=%d: %s" % (x, nmax, nn, poff, aoff, len(groups),
                                                                                  "; ".join(msg)))
    print("MshLod#3: %d scenes (%d also vs the shipped DXIL), %d failures -> %s" % (n, refchk, bad,
                                                                                   "PASS" if bad == 0 else "FAIL"))
    return bad == 0


if __name__ == "__main__":
    if "mshlod" in sys.argv[1:]:
        sys.exit(0 if (mshlod_selftest() if "selftest" in sys.argv[1:] else mshlod_main()) else 1)
    if "selftest" in sys.argv[1:]:
        sys.exit(0 if selftest() else 1)
    sys.exit(0 if main() else 1)
