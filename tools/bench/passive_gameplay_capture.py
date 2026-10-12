#!/usr/bin/env python3
"""Passive Linux/SteamOS collector. Never starts logging, a game, or an upload."""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import statistics
import stat as statmod
import time
import uuid
import zipfile
from datetime import datetime, timezone

MAX_BYTES = 8 * 1024 * 1024
MAX_LINE = 256 * 1024


def utc():
    return datetime.now(timezone.utc).isoformat()


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(65536), b''):
            h.update(block)
    return h.hexdigest()


def identity(pid, proc=Path('/proc')):
    p = proc / str(pid)
    stat = (p / 'stat').read_text()
    fields = stat[stat.rindex(')') + 2:].split()
    if fields[0] in ('Z', 'X'):
        raise RuntimeError('process_exited')
    return {'pid': pid, 'start_ticks': int(fields[19]),
            'exe': os.readlink(p / 'exe'), 'boot_id': (proc / 'sys/kernel/random/boot_id').read_text().strip()}


class Tail:
    def __init__(self, path, kind):
        self.path, self.kind = Path(path).resolve(), kind
        if not statmod.S_ISREG(self.path.stat().st_mode):
            raise ValueError('input_not_regular')
        self.f = self.path.open('rb')
        st = os.fstat(self.f.fileno())
        if not self.path.is_file():
            raise ValueError('input_not_regular')
        self.key = (st.st_dev, st.st_ino)
        self.initial = self.offset = st.st_size
        self.pending = b''
        self.total = 0
        self.partial_initial = False
        self.schema = b''
        self.f.seek(0)
        prefix = self.f.read(min(st.st_size, MAX_LINE))
        if kind == 'mangohud':
            for line in prefix.splitlines(keepends=True):
                if line.lstrip(b'\xef\xbb\xbf').startswith(b'fps,') and line.endswith(b'\n'):
                    self.schema = line
                    break
            if not self.schema:
                self.f.close()
                raise ValueError('mangohud_schema_missing_in_initial_prefix')
        # Preserve schema/prelude separately; never summarize historical rows.
        self.prefix = prefix[:prefix.find(self.schema)+len(self.schema)] if self.schema else b''.join(
            line for line in prefix.splitlines(keepends=True)
            if line.endswith(b'\n') and (b'schema' in line.lower() or b'header' in line.lower()))
        self.f.seek(max(0, st.st_size - 1))
        self.partial_initial = st.st_size > 0 and self.f.read(1) != b'\n'
        # Prefix/tail fingerprints detect same-inode truncate-and-regrow cases.
        self.f.seek(max(0, self.initial - 4096))
        self.anchor_pos = self.f.tell()
        self.anchor = self.f.read(self.initial - self.anchor_pos)
        self.f.seek(self.offset)

    def poll(self):
        st = self.path.stat()
        if (st.st_dev, st.st_ino) != self.key:
            raise RuntimeError('input_rotated')
        if st.st_size < self.offset:
            raise RuntimeError('input_truncated')
        self.f.seek(self.anchor_pos)
        if self.f.read(len(self.anchor)) != self.anchor:
            raise RuntimeError('input_prefix_replaced')
        self.f.seek(self.offset)
        remaining = MAX_BYTES - self.total
        block = self.f.read(min(65536, remaining + 1))
        if len(block) > remaining:
            raise RuntimeError('input_byte_limit')
        self.total += len(block)
        self.offset += len(block)
        self.pending += block
        cut = self.pending.rfind(b'\n')
        if cut < 0:
            if len(self.pending) > MAX_LINE:
                raise RuntimeError('input_line_limit')
            return b''
        complete, self.pending = self.pending[:cut+1], self.pending[cut+1:]
        if self.partial_initial:
            complete = complete[complete.find(b'\n')+1:]
            self.partial_initial = False
        if any(len(line) > MAX_LINE for line in complete.splitlines()):
            raise RuntimeError('input_line_limit')
        return complete

    def close(self):
        self.f.close()


def summarize(schema, appended):
    rows = csv.DictReader(io.StringIO((schema + appended).decode('utf-8-sig', errors='replace')))
    values, elapsed, invalid = [], [], 0
    for r in rows:
        try:
            v, e = float(r['frametime']), float(r['elapsed'])
            if not math.isfinite(v) or v <= 0 or not math.isfinite(e) or e < 0:
                raise ValueError()
            if elapsed and e <= elapsed[-1]:
                raise ValueError()
        except (KeyError, ValueError, TypeError):
            invalid += 1
            continue
        values.append(v)
        elapsed.append(e)
    result = {'frametime_rows': len(values), 'invalid_rows': invalid,
              'source': 'appended MangoHud frametime column (ms)',
              'sampling_limit': 'Rows are logger samples; not necessarily every presented frame.',
              'gpu_busy_ms': None, 'cpu_busy_ms': None}
    if values:
        s = sorted(values)
        result.update(mean_sample_frametime_ms=statistics.mean(s),
                      p50_sample_frametime_ms=s[math.ceil(.50*len(s))-1],
                      p99_sample_frametime_ms=s[math.ceil(.99*len(s))-1],
                      elapsed_span_seconds=(elapsed[-1]-elapsed[0])/1e9)
    return result


def collect(args, get_identity=identity, clock=time.monotonic, sleep=time.sleep):
    if not 0 < args.seconds <= 60:
        raise ValueError('seconds must be >0 and <=60')
    if len(args.hash_file) > 16:
        raise ValueError('at most16 hash files')
    expected = get_identity(args.pid)
    if expected['start_ticks'] != args.start_ticks or expected['exe'] != args.exe:
        raise ValueError('process_identity_mismatch')
    inputs = [(p, k) for p, k in [(args.engine_log, 'engine'), (args.mangohud, 'mangohud')] if p]
    if not inputs:
        raise ValueError('explicit input log required')
    parent = Path(args.output).resolve()
    parent.mkdir(parents=True, exist_ok=True)
    out = parent / ('passive-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ-') + uuid.uuid4().hex[:12])
    out.mkdir()
    report = {'status': 'CAPTURING', 'utc_start': utc(), 'process': expected,
              'requested_seconds': args.seconds, 'inputs': [], 'hashes': [],
              'limits': {'bytes_per_input': MAX_BYTES, 'line_bytes': MAX_LINE},
              'gp2': 'Raw diagnostic buckets only; no FPS inference.',
              'schema_limit': 'Only initial first256KiB inspected for schema; Engine header/schema lines retained when present.',
              'process_log_attribution': 'Explicit paths supplied by operator; no proof that PID wrote these files.'}
    tails, chunks = [], {}
    start = clock()
    try:
        for name in args.hash_file:
            p = Path(name).resolve()
            if (not statmod.S_ISREG(p.stat().st_mode) or
                    p.suffix.lower() not in ('.asi', '.ini', '.cfg', '.conf', '.xml') or p.stat().st_size > 32*1024*1024):
                raise ValueError('hash_file_type_or_size_rejected')
            report['hashes'].append({'path': str(p), 'sha256': digest(p), 'size': p.stat().st_size})
        for path, kind in inputs:
            t = Tail(path, kind)
            tails.append(t)
            chunks[kind] = bytearray()
            report['inputs'].append({'kind': kind, 'path': str(t.path), 'initial_offset': t.initial,
                                     'device_inode': list(t.key), 'initial_partial_line_skipped': t.partial_initial})
            if t.prefix:
                (out / (kind + '-schema.txt')).write_bytes(t.prefix)
        if get_identity(args.pid) != expected:
            raise RuntimeError('process_replaced')
        while True:
            if get_identity(args.pid) != expected:
                raise RuntimeError('process_replaced')
            for t in tails:
                chunks[t.kind].extend(t.poll())
            if clock() - start >= args.seconds:
                break
            sleep(min(.1, max(0, args.seconds-(clock()-start))))
        for item in report['hashes']:
            item['end_sha256'] = digest(item['path'])
            if item['end_sha256'] != item['sha256']:
                raise RuntimeError('hashed_file_changed')
        if get_identity(args.pid) != expected:
            raise RuntimeError('process_replaced')
        report['status'] = 'CAPTURE_COMPLETE'
    except (OSError, RuntimeError, ValueError) as exc:
        report['status'] = 'CAPTURE_FAILED'
        report['failure'] = type(exc).__name__ + ': ' + str(exc)
    finally:
        for t in tails:
            t.close()
            (out / (t.kind + '-appended.log')).write_bytes(chunks[t.kind])
            for item in report['inputs']:
                if item['kind'] == t.kind:
                    item.update(schema_captured=bool(t.prefix), final_offset=t.offset, captured_complete_bytes=len(chunks[t.kind]), incomplete_tail_bytes=len(t.pending))
            if t.kind == 'mangohud':
                report['mangohud'] = summarize(t.schema, bytes(chunks[t.kind]))
        report['utc_end'], report['actual_seconds'] = utc(), clock()-start
        (out / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    archive = out.with_suffix('.zip')
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(out.iterdir()):
            z.write(p, arcname=p.name)
    sha = digest(archive)
    archive.with_suffix('.zip.sha256').write_text(sha + '  ' + archive.name + '\n', encoding='ascii')
    return out, report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pid', type=int, required=True)
    p.add_argument('--start-ticks', type=int, required=True, help='/proc/PID/stat field22, not wall-clock seconds')
    p.add_argument('--exe', required=True, help='exact /proc/PID/exe link target; may be Wine host on Proton')
    p.add_argument('--engine-log')
    p.add_argument('--mangohud')
    p.add_argument('--hash-file', action='append', default=[])
    p.add_argument('--seconds', type=float, default=25)
    p.add_argument('--output', required=True)
    args = p.parse_args()
    out, report = collect(args)
    print(json.dumps({'output': str(out), 'status': report['status']}))
    return 0 if report['status'] == 'CAPTURE_COMPLETE' else 2


if __name__ == '__main__':
    raise SystemExit(main())
