r"""hook_patchable_offline.py - every hook site in src/sites_gen.h can be written by hooks.cpp Patch().

    py -I tests/hook_patchable_offline.py [path\to\sites_gen.h]

Patch() = PatchAtomic (the whole head inside one aligned 8- or 16-byte window, one cmpxchg) or PatchSpin (a 2-byte `jmp $`
parked atomically, then the tail, then the head). A head whose first 2 bytes straddle a 16-byte line (RVA ends in F with
no 8-byte fit) fails both: the hook logs "atomic write failed" at run time and never fires. Module bases are 64 KB aligned,
so RVA alignment = runtime alignment. Regression test for #111552 (ScProbe_HoldTimerDec 0x70B3AF head 8).
Ends with "ALL N HOOK HEADS PATCHABLE" or the failures (exit 1).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "src" / "sites_gen.h"


def fits(a, n):  # mirrors PatchAtomic's fit8 / fit16
    return (a & ~7) + 8 >= a + n or (a & ~15) + 16 >= a + n


def main():
    rows = re.findall(r'\{"([^"]+)", "([A-Z_0-9]+)", kHook, 0x([0-9A-Fa-f]+), (\d+),', SRC.read_text(encoding="utf-8"))
    bad = []
    for name, group, rva, head in rows:
        a, n = int(rva, 16), int(head)
        if n < 5:
            bad.append("%s (%s) %X: head %d < 5 (no room for jmp rel32)" % (name, group, a, n))
        elif not fits(a, n) and not fits(a, 2):
            bad.append("%s (%s) %X head %d: not atomic and the 2-byte spin straddles a 16-byte line" % (name, group, a, n))
    if not rows:
        bad.append("no kHook rows parsed from %s" % SRC)
    for b in bad:
        print("FAIL", b)
    if bad:
        return 1
    print("ALL %d HOOK HEADS PATCHABLE" % len(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
