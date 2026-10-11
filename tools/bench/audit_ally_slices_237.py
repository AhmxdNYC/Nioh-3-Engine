"""Bind retained measurement windows to exact contiguous source CSV rows.

This reproduces sampled-window statistics, not per-frame FPS or causality.
"""
from pathlib import Path
import csv
import hashlib
import io
import json
import math
import re
import statistics
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ARMS = ('1A', '2B', '3B', '4A', '5A', '6B')


def checked_zip(path, sha):
    assert hashlib.sha256(path.read_bytes()).hexdigest() == sha, path
    archive = zipfile.ZipFile(path)
    assert archive.testzip() is None
    return archive


def audit_windows(slices, original, slice_root, source_root):
    means, deduplicated, details = {}, {}, []
    for arm in ARMS:
        prefix = source_root + 'run/' + arm + '/'
        raw_lines = original.read(prefix + 'mangohud.csv').decode().splitlines()
        header = next(i for i, line in enumerate(raw_lines) if line.startswith('fps,frametime,'))
        columns = raw_lines[header].split(',')
        summary = original.read(prefix + 'summary.txt').decode()
        fps_means, previous_end, unique_samples = [], None, {}
        for number in (1, 2):
            name = slice_root + '/run/' + arm + '/w' + str(number) + '.csv'
            lines = slices.read(name).decode().splitlines()
            assert len(lines) == 200, (name, len(lines))
            starts = [i for i, line in enumerate(raw_lines) if line == lines[0]
                      and raw_lines[i:i + len(lines)] == lines]
            assert len(starts) == 1, ('not a unique exact contiguous source window', name)
            start = starts[0]
            assert start > header
            gap_rows = None if previous_end is None else start - previous_end
            previous_end = start + len(lines)
            rows = list(csv.DictReader(io.StringIO('\n'.join([','.join(columns)] + lines))))
            numeric = [{k: float(v) for k, v in row.items()} for row in rows]
            for offset, row in enumerate(numeric):
                unique_samples[start+offset] = row['fps']
            assert all(math.isfinite(v) for row in numeric for v in row.values())
            elapsed = [row['elapsed'] for row in numeric]
            assert all(a < b for a, b in zip(elapsed, elapsed[1:])), name
            steps = [(b-a)/1e9 for a, b in zip(elapsed, elapsed[1:])]
            assert all(0.05 < step < 0.25 for step in steps), (name, min(steps), max(steps))
            fps = statistics.mean(row['fps'] for row in numeric)
            expected = re.search(r'w' + str(number) + r' n (\d+) fps ([\d.]+)', summary)
            assert expected and int(expected[1]) == len(rows)
            assert abs(fps - float(expected[2])) <= .00501, (name, fps, expected[2])
            fps_means.append(fps)
            details.append(dict(window=name, rows=len(rows), gap_source_rows=gap_rows, source_first_line=start+1,
                                source_last_line=start+len(rows), mean_fps=fps,
                                elapsed_first_ns=int(elapsed[0]), elapsed_last_ns=int(elapsed[-1]),
                                sample_span_seconds=(elapsed[-1]-elapsed[0])/1e9,
                                mean_gpu_load=statistics.mean(row['gpu_load'] for row in numeric)))
        means[arm] = statistics.mean(fps_means)
        deduplicated[arm] = statistics.mean(unique_samples.values())
    pairs = [100*(means[b]/means[a]-1) for a, b in (('1A','2B'),('4A','3B'),('5A','6B'))]
    unique_pairs = [100*(deduplicated[b]/deduplicated[a]-1) for a, b in (('1A','2B'),('4A','3B'),('5A','6B'))]
    return dict(launch_means=means, paired_percent=pairs, mean_percent=statistics.mean(pairs),
                se_percent=statistics.stdev(pairs)/math.sqrt(len(pairs)), windows=details,
                overlapping_window_pairs=sum(w['gap_source_rows'] is not None and w['gap_source_rows'] < 0 for w in details),
                deduplicated_launch_means=deduplicated, deduplicated_mean_percent=statistics.mean(unique_pairs),
                deduplicated_se_percent=statistics.stdev(unique_pairs)/math.sqrt(len(unique_pairs)))


def main():
    with checked_zip(Path('D:/nioh3-re/ally_slices_237.zip'),
                     'ee4a116f1d47fee75334dafa9003884f3c0458901ee972a80c5fe62eebf6c315') as slices, \
         checked_zip(ROOT/'scratch/ally-default1761-file232.zip',
                     '2f3564f038c18fb99e6a336950333b0998405575a0cae28fbfce1edfc14696a7') as defaults, \
         checked_zip(ROOT/'scratch/ally-fog-native-236.zip',
                     'd0b541c858490064d78c01d5c5cbe38221542a4a6fcf2cb0850f4f752ce706c0') as fog:
        report = {'default232': audit_windows(slices, defaults, 'def1761', ''),
                  'fog236': audit_windows(slices, fog, 'fogn', 'up/'),
                  'scope': '24 exact contiguous retained windows; 100ms sampled FPS, three launch pairs; no pooled per-frame tails or universal gain'}
        output = ROOT/'scratch/ally-slices-237-audit.json'
        output.write_text(json.dumps(report, indent=2)+'\n')
        for key in ('default232', 'fog236'):
            item = report[key]
            print(key, 'exact-window PASS', len(item['windows']), 'windows',
                  'paired mean %', round(item['mean_percent'], 6), 'SE %', round(item['se_percent'], 6),
                  'overlapping pairs', item['overlapping_window_pairs'],
                  'deduplicated mean %', round(item['deduplicated_mean_percent'], 6),
                  'SE %', round(item['deduplicated_se_percent'], 6))
        print(report['scope'])


if __name__ == '__main__':
    main()
