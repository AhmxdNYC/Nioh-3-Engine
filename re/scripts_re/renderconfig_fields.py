r"""RenderConfig (per-frame render-settings block 'S' that ApplyGraphicsSettings 0x39A39C fills) field map.

The environment schema registers every field with
    lea rdx|rax,[name] ... mov qword ptr [rsp+28h],<offset> ... mov edx,<hash>
in two registration functions (0x25F15B0 and 0x2629194; same names -> same offsets in both). The offset is the
field's offset inside RenderConfig, so S+0x13DC = VolumetricFogResolutionX etc.

  py -I re\scripts_re\renderconfig_fields.py [out.tsv]      (default re\out\renderconfig_fields.tsv)
  py -I re\scripts_re\renderconfig_fields.py --off 13dc 107c (look up offsets)
"""
import os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, '..', 'Nioh3.dump.exe')
OUT = os.path.join(HERE, '..', 'out', 'renderconfig_fields.tsv')
REGIONS = ((0x25F15B0, 0x2614038), (0x2629194, 0x264BE24))  # the two registration functions (.pdata bounds)


def scan(f):
    rows = {}
    for lo, hi in REGIONS:
        for m in re.finditer(rb'\x48\x8d[\x15\x05](....)', f[lo:hi], re.S):
            i = lo + m.start()
            t = i + 7 + struct.unpack('<i', m.group(1))[0]
            if not 0x3800000 < t < 0x4200000:
                continue
            e = f.find(b'\0', t)
            s = f[t:e]
            if not 0 < len(s) <= 80 or not re.fullmatch(rb'[A-Za-z0-9_]+', s):
                continue
            w = f[i + 7:i + 60]
            mo = re.search(rb'\x48\xc7\x44\x24\x28(....)', w, re.S)
            if not mo:
                continue
            mh = re.search(rb'\xba(....)', w, re.S)
            off = struct.unpack('<I', mo.group(1))[0]
            h = '%08x' % struct.unpack('<I', mh.group(1))[0] if mh else ''
            rows.setdefault((off, s.decode()), (h, i))
    return rows


def main():
    f = open(EXE, 'rb').read()
    rows = scan(f)
    if len(sys.argv) > 2 and sys.argv[1] == '--off':
        want = {int(x, 16) for x in sys.argv[2:]}
        for (off, name), (h, i) in sorted(rows.items()):
            if off in want:
                print('0x%x\t%s\t%s\t0x%x' % (off, name, h, i))
        return
    out = sys.argv[1] if len(sys.argv) > 1 else OUT
    with open(out, 'w', encoding='utf-8', newline='\n') as o:
        o.write('offset\tfield\thash\treg_site\n')
        for (off, name), (h, i) in sorted(rows.items()):
            o.write('0x%x\t%s\t%s\t0x%x\n' % (off, name, h, i))
    print('%d fields -> %s' % (len(rows), out))


if __name__ == '__main__':
    main()
