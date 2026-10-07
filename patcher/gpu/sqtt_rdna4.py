"""Stdlib-only decoder for RDNA3/RDNA4 SQ thread-trace (SQTT) token streams.

Port of tinygrad's tinygrad/renderer/amd/sqtt.py (MIT) which is itself derived from
AMD's open-sourced rocprof-trace-decoder packet definitions. Only the packet *framing*
and the fields we need (timestamps, REG/userdata, wave start/end, events) are kept.

The stream is nibble-based; the first token is LAYOUT_HEADER whose `layout` field
selects RDNA3 (3) or RDNA4 (4) packet layouts.

decode(data) yields tuples (kind:str, time:int, raw:int) where `raw` is the 64-bit
window starting at the packet and `time` is the accumulated delta (SQTT clock cycles).
"""
from __future__ import annotations


def _f(hi, lo):
    return (lo, (1 << (hi - lo + 1)) - 1)


# name: (enc_hi, enc_value, size_nibbles, delta(lo,mask)|None, special)
# special: 1 = TS_DELTA_OR_MARK (marker -> delta 0), 2 = TS_DELTA_SHORT (+4)
def _types(rdna4: bool):
    t = {
        "VALUINST":       (2, 0b011,       3, _f(5, 3), 0),
        "VMEMEXEC":       (3, 0b1111,      2, _f(5, 4), 0),
        "ALUEXEC":        (3, 0b1110,      2, _f(5, 4), 0),
        "IMMEDIATE":      (3, 0b1101,      3, _f(6, 4), 0),
        "IMMEDIATE_MASK": (4, 0b00100,     6, _f(7, 5), 0),
        "WAVERDY":        (4, 0b10100,     6, _f(7, 5), 0),
        "TS_DELTA_S8_W3": (6, 0b0100001,  18, _f(10, 8), 0),
        "WAVEEND":        (4, 0b10101,     5, _f(7, 5), 0),
        "WAVESTART":      (4, 0b01100,     8, _f(6, 5), 0),
        "TS_DELTA_S5_W2": (4, 0b11100,    12, _f(6, 5), 0),
        "WAVEALLOC":      (4, 0b00101,     5, _f(7, 5), 0),
        "TS_DELTA_S5_W3": (4, 0b00110,    13, _f(7, 5), 0),
        "PERF":           (4, 0b10110,     7, _f(7, 5), 0),
        "UTILCTR":        (6, 0b0110001,  12, _f(8, 7), 0),
        "TS_DELTA_SHORT": (3, 0b1000,      2, _f(7, 4), 2),
        "NOP":            (3, 0b0000,      1, None, 0),
        "TS_WAVE_STATE":  (6, 0b1010001,   6, _f(15, 7), 0),
        "EVENT":          (7, 0b01100001,  6, _f(10, 8), 0),
        "EVENT_BIG":      (7, 0b11100001,  8, _f(10, 8), 0),
        "REG":            (3, 0b1001,     16, _f(6, 4), 0),
        "SNAPSHOT":       (6, 0b1110001,  16, _f(9, 7), 0),
        "TS_DELTA_OR_MARK": (6, 0b0000001, 12, _f(47, 12), 1),
        "LAYOUT_HEADER":  (6, 0b0010001,  16, None, 0),
        "INST":           (2, 0b010,       5, _f(6, 4), 0),
    }
    if rdna4:
        t.update({
            "WAVEEND":        (4, 0b10101,     5, _f(7, 5), 0),
            "WAVESTART":      (4, 0b01100,     8, _f(6, 5), 0),
            "TS_DELTA_S5_W2": (4, 0b11100,    10, _f(6, 5), 0),
            "WAVEALLOC":      (4, 0b00101,     6, _f(7, 5), 0),
            "TS_DELTA_S5_W3": (4, 0b00110,    14, _f(9, 7), 0),
            "PERF":           (4, 0b10110,     8, _f(9, 7), 0),
            "TS_DELTA_OR_MARK": (6, 0b0000001, 16, _f(63, 12), 1),
            "INST":           (2, 0b010,       5, _f(5, 3), 0),
        })
    return t


def _build(rdna4: bool):
    types = _types(rdna4)
    # more specific encodings first, NOP last
    order = sorted(types.items(), key=lambda kv: (-(kv[1][0] + 1), kv[0] == "NOP"))
    table = []
    for b in range(256):
        for name, (hi, val, *_rest) in order:
            if (b & ((1 << (hi + 1)) - 1)) == val:
                table.append(name)
                break
        else:
            table.append("NOP")
    info = {n: (v[2], v[3][0] if v[3] else 0, v[3][1] if v[3] else 0, v[4]) for n, v in types.items()}
    return [(n,) + info[n] for n in table]


_TAB3 = _build(False)
_TAB4 = _build(True)

M64 = (1 << 64) - 1


def decode(data: bytes, want: set[str] | None = None):
    """Yield (kind, time, raw64). If `want` is given only those kinds are yielded
    (all packets are still parsed to keep framing/time)."""
    n = len(data)
    reg = 0
    pos = 0
    nib_off = 0
    nib_count = 16
    time = 0
    tab = _TAB3
    first = True
    while pos + ((nib_count + nib_off + 1) >> 1) <= n:
        need = nib_count - nib_off
        if nib_off:
            reg = (reg >> 4) | ((data[pos] >> 4) << 60)
            pos += 1
        bc = need >> 1
        if bc:
            rb = 8 if bc > 8 else bc
            chunk = int.from_bytes(data[pos:pos + rb], "little")
            reg = (reg >> (rb * 8)) | (chunk << (64 - rb * 8))
            pos += bc
        nib_off = need & 1
        if nib_off:
            reg = (reg >> 4) | ((data[pos] & 0xF) << 60)
        reg &= M64
        name, nib_count, dlo, dmask, special = tab[reg & 0xFF]
        delta = (reg >> dlo) & dmask
        if special == 1:
            rt = (reg >> (7 if tab is _TAB4 else 9)) & 1
            pl = (reg >> 8) & 1
            if rt and not pl:
                delta = 0
        elif special == 2:
            delta += 4
        time += delta
        if first and name == "LAYOUT_HEADER":
            layout = (reg >> 7) & 0x3F
            if layout == 4:
                tab = _TAB4
            elif layout != 3:
                raise ValueError(f"unsupported SQTT layout {layout} (CDNA / pre-RDNA3?)")
            first = False
        if want is None or name in want:
            yield name, time, reg


# ---- field helpers -------------------------------------------------------
def reg_fields(raw: int):
    """REG packet -> (slot, hi_byte, subop/regaddr, val32)."""
    return (raw >> 7) & 7, (raw >> 8) & 0xFF, (raw >> 16) & 0xFFFF, (raw >> 32) & 0xFFFFFFFF


def wavestart4(raw):
    return dict(sa=(raw >> 7) & 1, simd=(raw >> 8) & 3, wgp=(raw >> 10) & 0xF, wave=(raw >> 15) & 0x1F, id=(raw >> 20) & 0xFFF)


def waveend4(raw):
    return dict(sa=(raw >> 8) & 1, simd=(raw >> 9) & 3, wgp=(raw >> 11) & 0xF, wave=(raw >> 15) & 0x1F)


def event_fields(raw, big=False):
    return (raw >> 11) & ((1 << (21 if big else 13)) - 1)
