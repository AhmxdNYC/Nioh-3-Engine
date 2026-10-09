"""Shared PresentMon clock-offset detection.

PresentMon 2.6 on pc2 writes CPUStartDateTime ~14 h BEHIND UTC (a timezone offset applied twice the wrong way),
so a report that treats the column as UTC finds 0 frames in the manifest windows. detect_offset() tries a set of
candidate offsets (0, +-local-UTC, +-2x(local-UTC), whole hours -14..+14) and picks the one that puts the most
frames inside the expected windows (ties -> the candidate nearest 0). The chosen offset is logged to stderr.

All times are float epoch seconds in the clock the caller parsed them with (UTC-as-naive or local-as-naive alike);
add the returned offset to the CSV times. Time-of-day users call detect_offset_tod() (modulo 24 h).
"""
import bisect
import sys
import time


def local_utc_offset_s():
    try:
        return int(time.localtime().tm_gmtoff or 0)
    except Exception:
        return 0


def candidates(local_s=None):
    L = local_utc_offset_s() if local_s is None else local_s
    out = [0]
    for v in (L, -L, 2 * L, -2 * L):
        out.append(v)
    out += [h * 3600 for h in range(-14, 15)]
    seen, res = set(), []
    for c in out:
        if c not in seen:
            seen.add(c)
            res.append(c)
    return res


def _count(ts, spans, off):
    n = 0
    for a, b in spans:
        n += bisect.bisect_right(ts, b - off) - bisect.bisect_left(ts, a - off)
    return n


def detect_offset(times, spans, label='', local_s=None, log=True, min_frames=10):
    """times: epoch seconds of CSV frames; spans: [(t0, t1)] expected windows. Returns offset seconds to ADD to times."""
    ts = sorted(times)
    if not ts or not spans:
        return 0
    best, bestn = 0, _count(ts, spans, 0)
    n0 = bestn
    for c in sorted(candidates(local_s), key=abs):
        n = _count(ts, spans, c)
        if n > bestn:
            best, bestn = c, n
    if bestn < min_frames:
        best = 0
    if log:
        print('[pm_time] %s clock offset %+.2f h chosen (%d frames in windows; offset 0 gives %d)'
              % (label or 'presentmon', best / 3600.0, bestn, n0), file=sys.stderr)
    return best


def detect_offset_tod(tods, spans, label='', log=True, min_frames=10):
    """Time-of-day variant (seconds since midnight, windows likewise); offsets are whole hours modulo 24 h."""
    best, bestn, n0 = 0, -1, 0
    for h in sorted(range(-14, 15), key=abs):
        off = h * 3600
        n = 0
        for t0, t1 in spans:
            n += sum(1 for t in tods if t0 <= (t + off) % 86400 <= t1)
        if h == 0:
            n0 = n
        if n > bestn:
            best, bestn = off, n
    if bestn < min_frames:
        best = 0
    if log:
        print('[pm_time] %s clock offset %+.2f h chosen (%d frames in windows; offset 0 gives %d)'
              % (label or 'presentmon', best / 3600.0, bestn, n0), file=sys.stderr)
    return best
