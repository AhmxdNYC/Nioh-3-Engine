r"""pj_psodesc.py - print the graphics PSO state (RT formats, blend, depth/stencil, raster) of every PSO in a PJ journal
(Nioh3EngineFix_pj\pj_*.bin, format: src\psojournal.cpp header) that uses a given shader hash (psoswap / GP2 hash prefix).

Usage: py -I pj_psodesc.py <pj_*.bin> <hash prefix, e.g. c013d811> [more prefixes]
Written for notes\ally-ripple-decal.md (2026-10-09).
"""
import struct
import sys

FMT = {0: 'UNKNOWN', 2: 'R32G32B32A32_FLOAT', 10: 'R16G16B16A16_FLOAT', 11: 'R16G16B16A16_UNORM', 13: 'R16G16B16A16_SNORM',
       20: 'D32_FLOAT_S8X24_UINT', 24: 'R10G10B10A2_UNORM', 25: 'R10G10B10A2_UINT', 26: 'R11G11B10_FLOAT',
       28: 'R8G8B8A8_UNORM', 29: 'R8G8B8A8_UNORM_SRGB', 31: 'R8G8B8A8_SNORM', 34: 'R16G16_FLOAT', 35: 'R16G16_UNORM',
       37: 'R16G16_SNORM', 40: 'D32_FLOAT', 41: 'R32_FLOAT', 42: 'R32_UINT', 45: 'D24_UNORM_S8_UINT', 49: 'R8G8_UNORM',
       54: 'R16_FLOAT', 55: 'D16_UNORM', 56: 'R16_UNORM', 57: 'R16_UINT', 61: 'R8_UNORM', 62: 'R8_UINT', 67: 'R9G9B9E5',
       87: 'B8G8R8A8_UNORM', 91: 'B8G8R8A8_UNORM_SRGB'}


def chunks(pj):
    p = 48
    while p + 16 <= len(pj):
        tag, ln, _ = struct.unpack_from('<IIQ', pj, p)
        if ln == 0 or p + 16 + ln > len(pj):
            return
        yield struct.pack('<I', tag), pj[p + 16:p + 16 + ln]
        p += 16 + ln


def main():
    pj = open(sys.argv[1], 'rb').read()
    want = [h.lower() for h in sys.argv[2:]]
    psos = [pay for tag, pay in chunks(pj) if tag == b'PSO1']
    for r in psos:
        kind, _ver, nsh, _il, flen, _rso, _z = struct.unpack_from('<BBBBIII', r, 0)
        d = r[32:32 + flen]
        q = 32 + flen
        sh = []
        for _ in range(nsh):
            off, _l = struct.unpack_from('<II', r, q)
            sh.append((off, r[q + 8:q + 24].hex()))
            q += 24
        if not any(k.startswith(w) for _, k in sh for w in want):
            continue
        stage = {8: 'vs', 24: 'ps', 40: 'ds', 56: 'hs', 72: 'gs'}
        print('PSO kind %d: %s' % (kind, ' '.join('%s=%s' % (stage.get(o, o), k) for o, k in sh)))
        if kind != 1 or flen < 656:
            print('  (not a plain graphics desc; fixed bytes %d)' % flen)
            continue
        a2c, indep = struct.unpack_from('<ii', d, 120)
        n = struct.unpack_from('<i', d, 576)[0]
        rts = struct.unpack_from('<8i', d, 580)[:n]
        dsv = struct.unpack_from('<i', d, 612)[0]
        print('  RTV %s  DSV %s  samples %s  A2C %d indep %d' % ([FMT.get(x, x) for x in rts], FMT.get(dsv, dsv),
                                                                  struct.unpack_from('<ii', d, 616), a2c, indep))
        for i in range(max(n, 1)):
            b = struct.unpack_from('<iiiiiiiiiB', d, 128 + 40 * i)
            print('  RT%d blend en=%d src=%d dst=%d op=%d | a src=%d dst=%d op=%d | mask=%x' % (i, b[0], b[2], b[3], b[4],
                                                                                         b[5], b[6], b[7], b[9]))
        rs = struct.unpack_from('<11i', d, 452)
        de, dw, dfn, se, rm, wm = struct.unpack_from('<iiiiBB', d, 496)
        ff = struct.unpack_from('<4i', d, 516)
        print('  raster fill %d cull %d depthclip %d | depth en %d write %d func %d | stencil en %d read %02x write %02x '
              'front fail/dfail/pass/func %s' % (rs[0], rs[1], rs[6], de, dw, dfn, se, rm, wm, ff))


if __name__ == '__main__':
    main()
