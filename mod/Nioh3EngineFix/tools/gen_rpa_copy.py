"""gen_rpa_copy.py - writes the RPA range copy into src/drawprep.asm (between the GENERATED rpa_copy markers).

    py -I tools/gen_rpa_copy.py [tests/rpa_ranges.txt]

Input: one "OFF LEN" (hex) line per byte range that RenderParamsAssign 247950 copies dst[OFF..OFF+LEN) = src[same]; the
ranges come from the original code itself (tests/run_w1b_rpa_harness.py --derive). Bytes outside the ranges (padding) are
never read or written. Per range: 64-byte loop blocks for long ranges, then 16-byte moves, the last one overlapping back
inside the range; ranges under 16 bytes use two overlapping 8/4/2-byte moves (or one byte). Registers: rcx = dst,
rdx = src (unchanged), rax / r8 / xmm0 / xmm1 scratch (all volatile). The harness then proves the cave equals the original.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASM = ROOT / "src" / "drawprep.asm"
BEGIN = "; ---- BEGIN GENERATED rpa_copy"
END = "; ---- END GENERATED rpa_copy ----"


def h(v):
    s = "%Xh" % v
    return ("0" + s) if s[0] in "ABCDEF" else s


def gen(ranges):
    out = []
    lab = 0
    total = 0
    for off, ln in ranges:
        total += ln
        out.append("    ; [%s, %s) %d bytes" % (h(off), h(off + ln), ln))
        pos = off
        end = off + ln
        blocks = ln // 64 if ln >= 128 else 0
        if blocks:
            lab += 1
            size = blocks * 64
            out.append("    mov     rax, -%s" % h(size))
            out.append("rpaL%d:" % lab)
            for k in range(0, 64, 32):
                a, b = off + size + k, off + size + k + 16
                out.append("    movups  xmm0, xmmword ptr [rdx+rax+%s]" % h(a))
                out.append("    movups  xmm1, xmmword ptr [rdx+rax+%s]" % h(b))
                out.append("    movups  xmmword ptr [rcx+rax+%s], xmm0" % h(a))
                out.append("    movups  xmmword ptr [rcx+rax+%s], xmm1" % h(b))
            out.append("    add     rax, 64")
            out.append("    jnz     rpaL%d" % lab)
            pos += size
        if ln >= 16:
            chunks = []
            while end - pos >= 16:
                chunks.append(pos)
                pos += 16
            if pos < end:
                chunks.append(end - 16)  # overlaps bytes already copied in this range: same values, still inside it
            for i in range(0, len(chunks), 2):
                pair = chunks[i:i + 2]
                for j, c in enumerate(pair):
                    out.append("    movups  xmm%d, xmmword ptr [rdx+%s]" % (j, h(c)))
                for j, c in enumerate(pair):
                    out.append("    movups  xmmword ptr [rcx+%s], xmm%d" % (h(c), j))
        else:
            for w, ra, rb, pt in ((8, "rax", "r8", "qword"), (4, "eax", "r8d", "dword"), (2, "ax", "r8w", "word")):
                if ln >= w:
                    out.append("    mov     %s, %s ptr [rdx+%s]" % (ra, pt, h(off)))
                    out.append("    mov     %s, %s ptr [rdx+%s]" % (rb, pt, h(end - w)))
                    out.append("    mov     %s ptr [rcx+%s], %s" % (pt, h(off), ra))
                    out.append("    mov     %s ptr [rcx+%s], %s" % (pt, h(end - w), rb))
                    break
            else:
                out.append("    mov     al, byte ptr [rdx+%s]" % h(off))
                out.append("    mov     byte ptr [rcx+%s], al" % h(off))
    return out, total


def main():
    src = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tests" / "rpa_ranges.txt"
    ranges = []
    for line in src.read_text().splitlines():
        p = line.split()
        if len(p) == 2:
            ranges.append((int(p[0], 16), int(p[1], 16)))
    if not ranges:
        sys.exit("no ranges in %s" % src)
    for (a, n), (b, _) in zip(ranges, ranges[1:]):
        if a + n >= b:
            sys.exit("ranges overlap or touch at %X" % b)
    body, total = gen(ranges)
    text = ASM.read_bytes().decode("utf-8")
    i = text.index(BEGIN)
    i = text.index("\r\n", i) + 2
    j = text.index(END)
    new = text[:i] + "".join(l + "\r\n" for l in body) + text[j:]
    ASM.write_bytes(new.encode("utf-8"))
    print("%d ranges, %d bytes (%X), %d lines -> %s" % (len(ranges), total, total, len(body), ASM))


main()
