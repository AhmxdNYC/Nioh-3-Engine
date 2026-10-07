r"""find_precise.py - find which locals of a decompiled shader need `precise` to stop dxc's fast-math reassociation.

    py -I find_precise.py rp121 --seeds 0:20 [--plain] [--rounds 12]

Loop: build (mkfast.py) -> run original and rebuild on the scenes -> for each mismatching output thread, trace both
programs (every float SSA value) and take the ROOT divergences: original values that the rebuild never produces
although every float operand of the instruction is produced by both (= dxc combined that op differently). Their
locals are marked precise and the loop repeats until all writes are bit-exact; --prune then removes every mark
that is not needed. Prints the final list for mkfast.ITEMS.
"""
import argparse, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dxil_interp as D, scenes, mkfast  # noqa: E402


def scene(kind, text, seed):
    return scenes.refprobe_scene(text, seed) if kind == "refprobe" else scenes.vfscat_scene(text, seed)


def trace(sh, kind, otext, seed, tid):
    sc, _ = scene(kind, otext, seed)
    nt = sh.numthreads
    X = D.Ctx(sc)
    lds = {n: ([0] * c if t != "float" else [0.0] * c) for n, (c, t) in sh.lds_globals.items()}
    F = sh._build(X, sh.consts, lds, lambda p: None)
    X.gid = tuple(tid[i] // nt[i] for i in range(3)); X.gtid = tuple(tid[i] % nt[i] for i in range(3))
    X.tid = tuple(tid); X.flat = X.gtid[0] + nt[0] * (X.gtid[1] + nt[1] * X.gtid[2]); X.who = X.tid
    R = [None] * sh.nregs
    for r, n, t in sh.alloca_init:
        R[r] = [0.0] * n if t == "float" else [0] * n
    out, b = [], 0
    while b >= 0:
        blk = sh.m.blocks[b]
        b2 = F[b](R)
        for ins in blk.instrs:
            if ins.dest is not None and isinstance(R[sh.regs[ins.dest]], float):
                out.append((R[sh.regs[ins.dest]], ins))
        b = b2
    return out


def mismatches(so, sn, kind, otext, seed):
    s1, groups = scene(kind, otext, seed)
    s2, _ = scene(kind, otext, seed)
    so.dispatch(s1, groups); sn.dispatch(s2, groups)
    u1, u2 = D.uav_state(s1), D.uav_state(s2)
    bad = []
    for k in u1:
        w2 = {}
        for w in u2[k][1]:
            w2.setdefault(w[0], []).append(w[-2])
        for w in u1[k][1]:
            vs = w2.get(w[0], [])
            if not any(all(D.ulp_diff(a, b) == 0 for a, b in zip(w[-2], v)) for v in vs):
                bad.append(w[0] if len(w[0]) == 3 else None)
    return [b for b in bad if b is not None]


def roots(A, B):
    bits = lambda v: D.f2bits(v) if v == v else -1
    setB = set(bits(v) for v, _ in B) | set(bits(-v) for v, _ in B)   # -x: dxc folds negations exactly
    val = {}
    for v, ins in A:
        val[ins.dest] = v
    out = []
    for v, ins in A:
        if bits(v) in setB or ins.op == "phi":
            continue
        ok = True
        for a in ins.args:
            if getattr(a, "kind", None) == "reg" and isinstance(val.get(a.v), float) and bits(val[a.v]) not in setB:
                ok = False
        if ok:
            out.append(ins.dest.lstrip("%"))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("item"); ap.add_argument("--seeds", default="0:20"); ap.add_argument("--plain", action="store_true")
    ap.add_argument("--rounds", type=int, default=12); ap.add_argument("--start", default=None)
    ap.add_argument("--prune", action="store_true", help="then drop every precise local that is not needed (ddmin)")
    a = ap.parse_args()
    it = mkfast.ITEMS[a.item]
    kind = "vfscat" if a.item.startswith("vf") else "refprobe"
    opath = os.path.join(mkfast.ORIG, it["orig"])
    otext = open(opath).read()
    so = D.Shader(otext, "orig")
    lo, hi = map(int, a.seeds.split(":"))
    pre = [x for x in (a.start if a.start is not None else it["precise"]).split(",") if x]
    for rnd in range(a.rounds):
        stem = mkfast.build(a.item, plain=a.plain, precise=",".join(pre), tag="fp")
        sn = D.Shader(open(stem + ".ll").read(), "new")
        new = []
        nbad = 0
        for s in range(lo, hi):
            bad = mismatches(so, sn, kind, otext, s)
            nbad += len(bad)
            for tid in bad[:2]:
                for r in roots(trace(so, kind, otext, s, tid), trace(sn, kind, otext, s, tid)):
                    v = "v" + r
                    if v not in pre and v not in new:
                        new.append(v)
        print("round %d: %d mismatching writes, new precise: %s" % (rnd, nbad, ",".join(new)), flush=True)
        if not nbad or not new:
            break
        pre += new
    if a.prune:
        def bad_count(lst):
            stem = mkfast.build(a.item, plain=a.plain, precise=",".join(lst), tag="fp")
            sn = D.Shader(open(stem + ".ll").read(), "new")
            return sum(len(mismatches(so, sn, kind, otext, s)) for s in range(lo, hi))
        n = max(1, len(pre) // 2)
        while n >= 1:
            i = 0
            while i < len(pre):
                trial = pre[:i] + pre[i + n:]
                if bad_count(trial) == 0:
                    pre = trial
                else:
                    i += n
            print("prune chunk %d: %d left" % (n, len(pre)), flush=True)
            n //= 2
    print("PRECISE=" + ",".join(pre))


if __name__ == "__main__":
    main()
