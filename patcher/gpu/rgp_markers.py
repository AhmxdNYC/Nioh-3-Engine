"""Extract the RGP SQTT instrumentation markers (written by the driver via the
SQ_THREAD_TRACE_USERDATA_2/3 registers) and wave/timestamp info from an RDF .rgp.

Marker layouts: Mesa src/amd/common/ac_sqtt.h ("RGP SQTT instrumentation spec").
On gfx11/gfx12 the userdata registers show up as REG tokens with reg addr
0xC342 (USERDATA_2) / 0xC343 (USERDATA_3).

Time base: SQTT deltas are in shader-clock cycles. TS_DELTA_OR_MARK tokens with
rt=1,pl=0 carry an absolute 100 MHz GPU timestamp in bits [63:12]; we use them to
convert cycles -> ns (robust against the occasional mis-framed delta).
"""
from __future__ import annotations
import struct
from dataclasses import dataclass, field
from rdf import RDF
from sqtt_rdna4 import decode

USERDATA_REGS = (0xC342, 0xC343)   # USERDATA_2/3 carry the RGP marker stream (USERDATA_7 = other driver data)

MARKER_NAMES = {0: "EVENT", 1: "CB_START", 2: "CB_END", 3: "BARRIER_START", 4: "BARRIER_END",
                5: "USER_EVENT", 6: "GENERAL_API", 7: "SYNC", 8: "PRESENT", 9: "LAYOUT_TRANSITION",
                10: "RENDER_PASS", 12: "BIND_PIPELINE"}
FIXED_DW = {1: 4, 2: 3, 3: 2, 4: 2, 6: 1, 9: 2, 12: 3}


@dataclass
class Marker:
    t: int                 # SQTT cycle time of first dword
    kind: str
    dw: list[int]
    ns: float = 0.0
    text: str = ""
    utype: int = -1        # user event type (0 trigger, 1 pop, 2 push, 3 objname)
    src: int = 0           # userdata writer id (REG bits [15:7])


@dataclass
class SEStream:
    se: int
    markers: list[Marker] = field(default_factory=list)   # all sources, time-sorted
    sources: dict = field(default_factory=dict)           # source id -> list[Marker]
    waves: "WaveList" = None                                      # (start_cycle, end_cycle) pairs
    ts_pairs: list[tuple[int, int]] = field(default_factory=list)  # (cycle, gpu_ts_100MHz)
    bad: int = 0
    last_cycle: int = 0


class WaveList:
    """Compact storage for millions of (start, end) cycle pairs (16 bytes per wave)."""
    def __init__(self):
        from array import array
        self.s, self.e = array("q"), array("q")

    def append(self, pair):
        self.s.append(pair[0]); self.e.append(pair[1])

    def __len__(self):
        return len(self.s)

    def __iter__(self):
        return zip(self.s, self.e)


def _parse_markers(ud: list[tuple[int, int]], se: SEStream):
    i, n = 0, len(ud)
    cur_cb = None
    while i < n:
        t, d0 = ud[i]
        ident = d0 & 0xF
        if ident == 0:
            ln = 6 if (d0 >> 31) & 1 else 3
        elif ident == 5:
            ut = (d0 >> 12) & 0xFF
            if ut in (0, 2, 3):
                if i + 1 >= n:
                    break
                slen = ud[i + 1][1]
                if slen > 4096:
                    se.bad += 1; i += 1; continue
                ln = 2 + (slen + 3) // 4
            else:
                ln = 1
        elif ident in FIXED_DW:
            ln = FIXED_DW[ident]
        else:
            se.bad += 1; i += 1; continue
        dws = [v for _, v in ud[i:i + ln]]
        # validate against the current command-buffer id to reject junk / lost-sync dwords
        if ident == 1:
            cur_cb = (d0 >> 7) & 0xFFFFF
        elif cur_cb is not None:
            cb = None
            if ident in (2, 3, 4):
                cb = (d0 >> 7) & 0xFFFFF
            elif ident == 0 and len(dws) > 1:
                cb = dws[1] & 0xFFFFF
            elif ident == 12:
                cb = (d0 >> 9) & 0xFFFFF
            elif ident == 5 and ((d0 >> 4) & 0xFF or d0 >> 20):
                cb = -1   # reserved bits set -> not a real user event
            if cb is not None and cb != cur_cb:
                se.bad += 1; i += 1; continue
        m = Marker(t, MARKER_NAMES.get(ident, f"ID{ident}"), dws)
        if ident == 5:
            m.utype = (d0 >> 12) & 0xFF
            if m.utype in (0, 2, 3) and len(dws) >= 2:
                raw = b"".join(struct.pack("<I", x) for x in dws[2:])[:dws[1]]
                m.text = raw.decode("utf-8", "replace").rstrip("\0")
        se.markers.append(m)
        i += ln


def read_se(data: bytes, se_index: int, want_waves=True) -> SEStream:
    se = SEStream(se_index)
    se.waves = WaveList()
    ud: dict[int, list[tuple[int, int]]] = {}
    open_w: dict[tuple, int] = {}
    want = {"REG", "TS_DELTA_OR_MARK"} | ({"WAVESTART", "WAVEEND"} if want_waves else set())
    MAXD = 1 << 24   # deltas bigger than this are mis-framed garbage; ignore
    corr = 0
    last_t = 0
    for kind, t, raw in decode(data, want):
        # guard against absurd jumps (mis-framed TS_DELTA tokens)
        if t - corr - last_t > MAXD:
            corr += (t - corr - last_t)
        t -= corr
        last_t = t
        if kind == "REG":
            reg = (raw >> 16) & 0xFFFF
            if reg in USERDATA_REGS:
                # bits [9:7] ("slot") identify the writing pipe: 0 = gfx ME, others = compute pipes
                ud.setdefault((raw >> 7) & 0x7, []).append((t, (raw >> 32) & 0xFFFFFFFF))
        elif kind == "TS_DELTA_OR_MARK":
            if (raw & 0x3FF) == 0x081:          # rt=1 pl=0 tl=0 -> absolute timestamp marker
                se.ts_pairs.append((t, raw >> 12))
        elif kind == "WAVESTART":
            key = ((raw >> 7) & 1, (raw >> 10) & 0xF, (raw >> 8) & 3, (raw >> 15) & 0x1F)
            open_w[key] = t
        elif kind == "WAVEEND":
            key = ((raw >> 8) & 1, (raw >> 11) & 0xF, (raw >> 9) & 3, (raw >> 15) & 0x1F)
            s = open_w.pop(key, None)
            if s is not None and t >= s:
                se.waves.append((s, t))
    se.last_cycle = last_t
    for src, lst in ud.items():
        tmp = SEStream(se_index)
        _parse_markers(lst, tmp)
        for m in tmp.markers:
            m.src = src
        se.sources[src] = tmp.markers
        se.bad += tmp.bad
        se.markers.extend(tmp.markers)
    se.markers.sort(key=lambda m: m.t)
    return se


def fit_clock(pairs: list[tuple[int, int]]):
    """Robust fit cycles -> ns using absolute 100MHz timestamps.
    Returns (f_cycles_per_tick, function(cycle)->ns relative to first anchor)."""
    if len(pairs) < 2:
        return None
    LO, HI = 10.0, 50.0   # plausible shader-clock / 100 MHz refclk ratios (1.0 .. 5.0 GHz)

    def ok(a, b):
        dc, dt = b[0] - a[0], b[1] - a[1]
        return dt > 0 and dc > 0 and dt < 2_000_000 and LO < dc / dt < HI

    # start from the first point that agrees with its successor
    start = next((i for i in range(len(pairs) - 1) if ok(pairs[i], pairs[i + 1])), None)
    if start is None:
        return None
    good = [pairs[start]]
    for p in pairs[start + 1:]:
        if ok(good[-1], p):
            good.append(p)
    if len(good) < 2:
        return None
    ratio = (good[-1][0] - good[0][0]) / (good[-1][1] - good[0][1])
    import bisect
    cs = [g[0] for g in good]
    base_ts = good[0][1]

    def to_ns(c):
        """cycle -> absolute GPU time in ns (100 MHz refclk * 10), interpolated between anchors."""
        j = bisect.bisect_right(cs, c) - 1
        if j < 0:
            gc, gts = good[0]
            return (gts + (c - gc) / ratio) * 10.0
        if j >= len(good) - 1:
            gc, gts = good[-1]
            return (gts + (c - gc) / ratio) * 10.0
        (c0, t0), (c1, t1) = good[j], good[j + 1]
        return (t0 + (c - c0) * (t1 - t0) / (c1 - c0)) * 10.0
    return ratio, to_ns, base_ts, good


def load(path: str, ses: list[int] | None = None, want_waves=True):
    r = RDF(path)
    out = []
    for c in r.find("SqttData"):
        hdr = r.header(c)
        se_idx = struct.unpack_from("<I", hdr, 4)[0] if len(hdr) >= 8 else c.index
        if ses is not None and c.index not in ses:
            continue
        s = read_se(r.data(c), se_idx, want_waves)
        out.append(s)
    return r, out
