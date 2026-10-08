r"""kids property / RenderConfig field hash:  h = sum(c[i] * 31**(i+1)) mod 2**32  (bytes of the ASCII name, i from 0).

Matches 1,560 of the 1,567 RenderConfig schema fields (re\out\renderconfig_fields.tsv; the 7 misses are extractor
mis-pairs) and kassent_property_hashes.csv ('G' = 0x899). NOT the hash of shader-object/render-node TYPE names
(SSFog 0x63BA44B4, StaticNature 0x197E61C6), nor of material params (h*27+c, MaterialFindParam 0x3C768C).

  py -I re\scripts_re\kidshash.py Name1 Name2 ...         print hashes
  py -I re\scripts_re\kidshash.py --find 93bce2c1 ...      hash every identifier in Nioh3.dump.exe and report matches
"""
import os, re, sys

EXE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Nioh3.dump.exe')


def kh(b):
    h, p = 0, 31
    for c in b:
        h = (h + c * p) & 0xFFFFFFFF
        p = (p * 31) & 0xFFFFFFFF
    return h


def main(a):
    if a[:1] == ['--find']:
        want = {int(x, 16) for x in a[1:]}
        seen = set()
        for m in re.finditer(rb'[A-Za-z_][A-Za-z0-9_]{1,120}', open(EXE, 'rb').read()):
            s = m.group()
            if s not in seen:
                seen.add(s)
                if kh(s) in want:
                    print('%08x\t%s\t0x%x' % (kh(s), s.decode(), m.start()))
        print('%d identifiers hashed' % len(seen))
    else:
        for n in a:
            print('%08x\t%s' % (kh(n.encode()), n))


if __name__ == '__main__':
    main(sys.argv[1:])
