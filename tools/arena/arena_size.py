"""Boss arena sizes for the draw-distance 'arena size' stop (pc #102806, human #102651).
Per catalogue row with arena data: points in XZ = player stand (arena_x/z, else warp), boss_pos, 4 corners of arena_boxgeo
(centre x y z, yaw deg, FULL size x y z; cm). radius = minimum enclosing circle of those points (m); span = max pairwise distance (m)
= what a player at one edge must see. Stats over rows with a 'confirmed' date (catalogue confirmed column). Universal: no per-boss numbers."""
import csv, math, random, statistics, sys

def mec(pts):  # Welzl, small n
    pts = pts[:]; random.Random(1).shuffle(pts)
    def circ2(a, b):
        c = ((a[0]+b[0])/2, (a[1]+b[1])/2); return c, math.dist(a, c)
    def circ3(a, b, c):
        ax, ay = a; bx, by = b; cx, cy = c
        d = 2*(ax*(by-cy)+bx*(cy-ay)+cx*(ay-by))
        if abs(d) < 1e-9:
            return max((circ2(p, q) for p, q in ((a, b), (a, c), (b, c))), key=lambda t: t[1])
        ux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/d
        uy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/d
        return (ux, uy), math.dist((ux, uy), a)
    inside = lambda c, p: math.dist(c[0], p) <= c[1] + 1e-6
    c = (pts[0], 0)
    for i, p in enumerate(pts):
        if inside(c, p): continue
        c = (p, 0)
        for j in range(i):
            q = pts[j]
            if inside(c, q): continue
            c = circ2(p, q)
            for k in range(j):
                if not inside(c, pts[k]): c = circ3(p, q, pts[k])
    return c

def f(s):
    try: return float(s)
    except Exception: return None

def main(path, out_tsv, arenas=None):
    caps = {}
    if arenas:
        for r in csv.reader(open(arenas, encoding="utf-8"), delimiter="	"):
            if len(r) >= 5 and r[0].lower().startswith("0x") and f(r[2]) is not None:
                caps[r[0].lower()] = (f(r[2]), f(r[4]))   # last capture wins
    rows = [r for r in csv.reader(open(path, encoding="utf-8"), delimiter="\t") if r and not r[0].startswith("#")]
    hdr, rows = rows[0], rows[1:]
    ix = {h: i for i, h in enumerate(hdr)}
    g = lambda r, k: r[ix[k]] if k in ix and ix[k] < len(r) else ""
    res, missing = [], []
    for r in rows:
        confd = g(r, "confirmed") not in ("", "-")
        ok = confd
        pts, have = [], []
        sx, sz = f(g(r, "arena_x")), f(g(r, "arena_z"))
        if sx is None and g(r, "replay_mission_id").lower() in caps:   # in-game capture: player pos at BossBattleMode start
            sx, sz = caps[g(r, "replay_mission_id").lower()]
        if sx is not None: pts.append((sx, sz)); have.append("stand")
        bp = g(r, "boss_pos").split("_")
        if len(bp) == 3 and f(bp[0]) is not None: pts.append((f(bp[0]), f(bp[2]))); have.append("boss")
        geo = [f(v) for v in g(r, "arena_boxgeo").split("_")]
        if len(geo) == 7 and None not in geo:
            cx, cy, cz, yaw, ex, ey, ez = geo; a = math.radians(yaw)
            for sxn, szn in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
                lx, lz = sxn*ex/2, szn*ez/2   # local -> world, inverse of pick_arena's rotation
                pts.append((cx + lx*math.cos(a) + lz*math.sin(a), cz - lx*math.sin(a) + lz*math.cos(a)))
            have.append("box")
        lack = [k for k in ("stand", "boss", "box") if k not in have]
        if lack: missing.append((g(r, "id"), g(r, "name"), g(r, "route"), ok, ",".join(lack)))
        if len(pts) < 2: continue
        (_, rad) = mec(pts)
        span = max(math.dist(p, q) for p in pts for q in pts)
        res.append(dict(id=g(r, "id"), name=g(r, "name"), route=g(r, "route"), confirmed=int(ok), have="+".join(have),
                        radius_m=rad/100, span_m=span/100, stand_boss_m=(math.dist(pts[0], pts[1])/100 if have[:2] == ["stand", "boss"] else None)))
    with open(out_tsv, "w", newline="", encoding="utf-8") as o:
        w = csv.writer(o, delimiter="\t", lineterminator="\n")
        w.writerow(["id", "name", "route", "confirmed", "have", "radius_m", "span_m", "stand_boss_m"])
        for d in sorted(res, key=lambda d: -d["radius_m"]):
            w.writerow([d["id"], d["name"], d["route"], d["confirmed"], d["have"], "%.1f" % d["radius_m"], "%.1f" % d["span_m"],
                        "" if d["stand_boss_m"] is None else "%.1f" % d["stand_boss_m"]])
    def stats(sel, key):
        v = sorted(d[key] for d in sel)
        if not v: return "n=0"
        q = lambda p: v[min(len(v)-1, int(math.ceil(p*len(v)))-1)]
        return "n=%d median %.1f p75 %.1f p90 %.1f max %.1f" % (len(v), statistics.median(v), q(.75), q(.9), v[-1])
    for lab, sel in (("confirmed, full (stand+boss+box)", [d for d in res if d["confirmed"] and d["have"] == "stand+boss+box"]),
                     ("confirmed, any", [d for d in res if d["confirmed"]]), ("all rows", res)):
        print(lab, "| radius", stats(sel, "radius_m"), "| span", stats(sel, "span_m"))
    print("missing data:", len(missing))
    for m in missing: print("  ", *m)

if __name__ == "__main__":
    main(*sys.argv[1:4])
