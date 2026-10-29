"""Offline analyser of the #35411 empty-area i-frame probe (Nioh3Training [Debug] IFrameProbe = 1, notes/iframes.md "Empty-area probe").

  py -I tools/iframe_probe_summary.py <Nioh3Training.log> [--events notes/player-action-events.tsv] [--min-share 0.6] [--control 2000,2001]

Reads two line kinds (nothing else in the log is needed, no boss, no hit):
  IFRAMEPR t <ms> act <id> f <frame> m <hex mask> <names> [| ...]   one per game tick of a non-looping player action
  IFRAMEWN t <start ms> act <id> frames <n> noisy a/c/s: <entries>  one per action run; entries <blk><off>=<base>><new>@<f1>-<f2|"">
For every action id it prints:
  - the predicate window: per predicate bit, in how many runs it was on and the median first / last frame it was on
    (the stripe bits = the bits that mean "a hit would do no damage / not connect");
  - window candidates from the field scan: fields (c = ctrl, s = status [actor+0x3A0], a = actor) that turned on after the start and back
    before the end in at least --min-share of the runs of this action, and in less than 20 % of the runs of looping / idle actions (the control);
  - the action's events and end frame from the event table, so a window can be compared with the event frames.
"""
import csv
import re
import statistics
import sys

BITS = ['cannot0', 'cannot4', 'cannot9', 'r29ae0', 'a8b20', 'nodmgact', 's48', 's2dc8b31', 'ea0', 'ea1', 'eae', 'x15d8', 'isnodmg', 'nohit',
        'cond4', 'armor', 'guardx', 'hurtoff']
STRIPE = {'cannot0', 'nodmgact', 'nohit', 'hurtoff', 'ea0', 'ea1', 's2dc8b31', 'x15d8', 'a8b20'}  # = FMI_STRIPE_MASK in frame_meter_iframe.h
PR = re.compile(r'IFRAMEPR t (-?[\d.]+) act (\d+) f (\d+) m ([0-9A-Fa-f]+)')
WN = re.compile(r'IFRAMEWN t (-?[\d.]+) act (\d+) frames (\d+) noisy (-?\d+)/(-?\d+)/(-?\d+):(.*)$')
ENT = re.compile(r'([acs])([0-9A-F]+)=([0-9A-F]+)>([0-9A-F]+)@(\d+)-(\d*)')


def parse_pr(lines):
    """IFRAMEPR lines -> list of runs {act, t0, frames: {frame: mask}}. A new run starts when the act changes or the frame goes back."""
    runs, cur = [], None
    for ln in lines:
        m = PR.search(ln)
        if not m:
            continue
        t, act, f, mask = float(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4), 16)
        if cur is None or cur['act'] != act or f < cur['last']:
            cur = {'act': act, 't0': t, 'frames': {}, 'last': -1}
            runs.append(cur)
        cur['frames'][f] = cur['frames'].get(f, 0) | mask
        cur['last'] = f
    return runs


def window(frames, bits):
    """(first, last) frame where any bit of `bits` (a mask) is on, or None."""
    on = sorted(f for f, m in frames.items() if m & bits)
    return (on[0], on[-1]) if on else None


def mask_of(names):
    return sum(1 << BITS.index(n) for n in names)


def parse_wn(lines):
    """IFRAMEWN lines -> list of {act, t0, frames, fields: {key: (kind, f1, f2)}} with kind 'w' (window) or 'o' (open)."""
    out = []
    for ln in lines:
        m = WN.search(ln)
        if not m:
            continue
        fields = {}
        for e in ENT.finditer(m.group(7)):
            key = '%s+%s=%s>%s' % (e.group(1), e.group(2), e.group(3), e.group(4))
            fields[key] = ('w', int(e.group(5)), int(e.group(6))) if e.group(6) else ('o', int(e.group(5)), None)
        out.append({'act': int(m.group(2)), 't0': float(m.group(1)), 'frames': int(m.group(3)), 'fields': fields})
    return out


def field_key(k):
    """'c+EA0=0>1' -> 'c+EA0' (same field across runs even if the values differ)."""
    return k.split('=', 1)[0]


def candidates(wn_runs, act, control_acts, min_share=0.6, max_control=0.2):
    """Fields that window in >= min_share of the runs of `act` and in < max_control of the control runs. Sorted by share."""
    mine = [r for r in wn_runs if r['act'] == act]
    ctrl = [r for r in wn_runs if r['act'] in control_acts]
    if not mine:
        return []
    def per_field(runs):
        d = {}
        for r in runs:
            seen = set()
            for k, v in r['fields'].items():
                fk = field_key(k)
                if fk in seen:
                    continue
                seen.add(fk)
                d.setdefault(fk, []).append(v)
        return d
    pm, pc = per_field(mine), per_field(ctrl)
    out = []
    for fk, vs in pm.items():
        share = len(vs) / len(mine)
        cshare = len(pc.get(fk, [])) / len(ctrl) if ctrl else 0.0
        if share < min_share or cshare >= max_control:
            continue
        w = [v for v in vs if v[0] == 'w']
        f1 = statistics.median(v[1] for v in vs)
        f2 = statistics.median(v[2] for v in w) if w else None
        out.append({'field': fk, 'share': share, 'n': len(vs), 'runs': len(mine), 'kind': 'w' if len(w) * 2 >= len(vs) else 'o',
                    'f1': f1, 'f2': f2, 'control': cshare})
    out.sort(key=lambda c: (-c['share'], c['kind'] != 'w', c['field']))
    return out


def load_events(path):
    ev = {}
    try:
        with open(path, newline='', encoding='utf-8') as fh:
            for r in csv.DictReader(fh, delimiter='\t'):
                ev.setdefault(int(r['action']), r)
    except OSError:
        pass
    return ev


def summarize(lines, events=None, min_share=0.6, control_acts=None):
    events = events or {}
    pr, wn = parse_pr(lines), parse_wn(lines)
    looping = {a for a, r in events.items() if r.get('loops') == '1'}
    pr_acts = {r['act'] for r in pr}
    # control = --control ids, else the looping actions of the event table, else the actions with window-scan lines but no IFRAMEPR
    # (the probe skips looping actions)
    control = set(control_acts) if control_acts else looping if looping else ({r['act'] for r in wn} - pr_acts if pr_acts else set())
    acts = sorted(pr_acts | {r['act'] for r in wn if r['act'] not in control})
    out = []
    smask = mask_of(STRIPE)
    for act in acts:
        runs = [r for r in pr if r['act'] == act]
        res = {'act': act, 'runs': len(runs), 'bits': {}, 'stripe': None, 'cands': candidates(wn, act, control, min_share), 'event': events.get(act)}
        for b, name in enumerate(BITS):
            ws = [window(r['frames'], 1 << b) for r in runs]
            ws = [w for w in ws if w]
            if ws:
                res['bits'][name] = (len(ws), statistics.median(w[0] for w in ws), statistics.median(w[1] for w in ws),
                                     statistics.median(max(r['frames']) for r in runs))
        sw = [window(r['frames'], smask) for r in runs]
        sw = [w for w in sw if w]
        if sw:
            res['stripe'] = (len(sw), min(w[0] for w in sw), statistics.median(w[0] for w in sw), statistics.median(w[1] for w in sw), max(w[1] for w in sw))
        out.append(res)
    return out


def render(res):
    lines = []
    for r in res:
        e = r['event']
        head = 'act %d: %d IFRAMEPR runs' % (r['act'], r['runs'])
        if e:
            head += ' | events %s | end frame %s | next %s' % (e.get('events') or '-', e.get('end_frame') or '-', e.get('next') or '-')
        lines.append(head)
        if r['stripe']:
            n, lo, m1, m2, hi = r['stripe']
            lines.append('  NO-DAMAGE WINDOW (stripe bits): %d/%d runs, frames %s-%s (median), earliest %s, latest %s' % (n, r['runs'], m1, m2, lo, hi))
        else:
            lines.append('  no stripe bit was on in any run')
        for name, (n, f1, f2, last) in r['bits'].items():
            always = ' (whole run: also on at frame 0 / end, likely not a window)' if f1 == 0 and f2 >= last else ''
            lines.append('    %-9s %s on in %d/%d runs, frames %s-%s%s' % (name, '*' if name in STRIPE else ' ', n, r['runs'], f1, f2, always))
        for c in r['cands'][:12]:
            lines.append('    scan %-8s %s in %d/%d runs (control %.0f%%): frames %s-%s' % (c['field'], 'window' if c['kind'] == 'w' else 'open  ',
                         c['n'], c['runs'], 100 * c['control'], c['f1'], c['f2'] if c['f2'] is not None else 'end'))
    return '\n'.join(lines)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    evp, share = 'notes/player-action-events.tsv', 0.6
    if '--events' in argv:
        evp = argv[argv.index('--events') + 1]
    if '--min-share' in argv:
        share = float(argv[argv.index('--min-share') + 1])
    with open(argv[1], encoding='utf-8', errors='replace') as fh:
        lines = fh.readlines()
    ctl = [int(x) for x in argv[argv.index('--control') + 1].split(',')] if '--control' in argv else None
    print(render(summarize(lines, load_events(evp), share, ctl)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
