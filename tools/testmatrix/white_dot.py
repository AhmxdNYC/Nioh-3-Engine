r"""white_dot.py - the walk target for a lesson/clear-gadget Proceed, read from the game's own white dot (#106000).

The BR mod logs every UI::SetCompassDestination(Index) call during a Fight now attempt:
    Rematch: white dot 76494 1270 37765 (SetCompassDestinationIndex) - player 75515 1271 39273, 18.0 m flat - Fight now row RA0A1
The harness walks to the NEWEST such line for the row. No per-Dojo coordinate table: any mission that shows a white
dot gives its own target. If the walk stalls, the fallback warp goes to the same logged point (not a stored one).

    py -I tools/testmatrix/white_dot.py <Nioh3BossRematch.log> [ROW]    -> prints "x y z" or exits 1
"""
import re
import sys

LINE = re.compile(r"Rematch: white dot (-?\d+) (-?\d+) (-?\d+) \((SetCompassDestination(?:Index)?)\)(?:.*?- Fight now row (\w+))?")


def dots(lines):
    """[(x, y, z, form, row_or_None)] in log order."""
    out = []
    for ln in lines:
        m = LINE.search(ln)
        if m:
            out.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)))
    return out


def target(lines, row=None):
    """Newest white dot for this row (or any row when row is None), else None."""
    for d in reversed(dots(lines)):
        if row is None or d[4] == row:
            return d
    return None


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    with open(argv[1], encoding="utf-8", errors="replace") as f:
        t = target(f.readlines(), argv[2] if len(argv) > 2 else None)
    if not t:
        return 1
    print("%d %d %d" % t[:3])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
