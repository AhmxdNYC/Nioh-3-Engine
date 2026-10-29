"""Per-action event table of the player's action records (static, read-only) for the i-frame analysis (#35411, notes/iframes.md).

  py -I tools/player_action_events.py [--out out.tsv] <archive_01 file> [<archive_01 file> ...]

The player's actions (dodges 10014-10045, steps, attacks) are in archive_01 `88/2229E56A...` (787 records), movement / idle
(2000/2001/2051/2060) in `E5/95C355A8...` (235 records); notes/player-action-events.tsv holds both (column source). The parser is
tools/chr_action_table.py (record layout there). Columns: action (decimal), motion, next action, loops (next == self), kind_by_motion,
nodmg_bit35 (record qword +0 bit 35 = the 326C94 "no damage" action flag), r18_bit29 (record dword +0x18 bit 29, read by 481290),
end_frame (+0xEA, the frame where the end / cancel bit appears in the PC logs), events (`id@frame+len`, len empty = instant),
local_events (sub-table 5 class, chr_action_table GUESSED), sched_a / sched_b.
The game is not touched; the archive file must be an extracted copy (data/extract, not in git).
"""
import csv
import importlib.util
import os
import sys


def _cat():
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'chr_action_table.py')
    spec = importlib.util.spec_from_file_location('chr_action_table', p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


COLS = ['source', 'action', 'motion', 'next', 'loops', 'kind_by_motion', 'nodmg_bit35', 'r18_bit29', 'end_frame', 'events', 'local_events',
        'sched_a', 'sched_b']


def rows(path):
    for r in _cat().full(path):
        aid = int(r['action_id'], 16)
        nxt = int(r['next_action'], 16) if r['next_action'] else ''
        ev = ' '.join(e.split(':', 1)[1] for e in r['events'].split()) if r['events'] else ''
        yield {'source': os.path.basename(os.path.dirname(path)) + '/' + os.path.basename(path)[:8], 'action': aid, 'motion': r['motion'], 'next': nxt, 'loops': r['loops'], 'kind_by_motion': r['kind_by_motion'],
               'nodmg_bit35': int(int(r['flags04'], 16) >> 3 & 1), 'r18_bit29': int(int(r['flags1a'], 16) >> 13 & 1),
               'end_frame': r['atk_paramEA'], 'events': ev, 'local_events': r['local_event_decode'],
               'sched_a': r['sched_a'], 'sched_b': r['sched_b']}


def main(argv):
    args = argv[1:]
    out = sys.stdout
    if '--out' in args:
        i = args.index('--out')
        out = open(args[i + 1], 'w', newline='', encoding='utf-8')
        del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    w = csv.DictWriter(out, COLS, delimiter='\t', lineterminator='\n')
    w.writeheader()
    for r in sorted((r for a in args for r in rows(a)), key=lambda r: (r['action'], r['source'])):
        w.writerow(r)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
