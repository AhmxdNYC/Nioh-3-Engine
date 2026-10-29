#!/usr/bin/env python3
"""Recover committed DXGI diagnostic records after a process crash (stdlib only)."""
import argparse
import json
import struct
from pathlib import Path

EVENT = struct.Struct('<6Q10IQ')
RECORD = struct.Struct('<Q96sQ')
HEADER = struct.Struct('<8sII8Q')
METHODS = ['unknown', 'Present', 'Present1', 'SetColorSpace1', 'InstallHooks',
           'dummy-factory', 'dummy-device', 'dummy-queue', 'dummy-chain', 'hook-slot']
STAGES = ['unknown', 'enter', 'overlay', 'filter', 'original', 'return', 'retry',
          'finish', 'setup', 'CPU-fault', 'removed-reason', 'queue']
SIZE = HEADER.size + 4096 + (4 + 64 + 256) * RECORD.size


def record(raw):
    commit, payload, checksum = RECORD.unpack(raw)
    h = 14695981039346656037
    for b in payload:
        h = ((h ^ b) * 1099511628211) & ((1 << 64) - 1)
    e = dict(zip(['seq', 'call', 'qpc', 'chain', 'target', 'caller', 'tid', 'depth',
                  'method', 'stage', 'sync', 'flags', 'dirty', 'scroll', 'hr', 'hasHr', 'extra'], EVENT.unpack(payload)))
    return e if commit and commit == e['seq'] and checksum == h else None


def describe(e):
    if not e:
        return 'none/unavailable'
    method = METHODS[e['method']] if e['method'] < len(METHODS) else 'unknown'
    stage = STAGES[e['stage']] if e['stage'] < len(STAGES) else 'unknown'
    hr = f"{e['hr']:08X}" if e['hasHr'] else 'unavailable'
    return (f"seq={e['seq']} call={e['call']} {method}/{stage} tid={e['tid']} depth={e['depth']} "
            f"chain={e['chain']:X} SI={e['sync']} flags={e['flags']:08X} hr={hr}")


def recover(data):
    if len(data) != SIZE:
        raise ValueError(f'Wrong size: {len(data)}; expected {SIZE}. Truncated/unsupported file.')
    magic, schema, size, *counts = HEADER.unpack_from(data)
    if magic != b'N3DXGI1\0' or schema != 1 or size != SIZE:
        raise ValueError('Unsupported DXGI diagnostic schema')
    def text(offset):
        return data[offset:offset + 2048].split(b'\0')[0].decode('utf-8', errors='replace')
    header, metadata = text(HEADER.size), text(HEADER.size + 2048)
    offset = HEADER.size + 4096
    records = [record(data[i:i + RECORD.size]) for i in range(offset, SIZE, RECORD.size)]
    entry, returned, failure, removed = records[:4]
    pending = [r for r in records[4:68] if r]
    ring = sorted((r for r in records[68:] if r), key=lambda e: e['seq'])
    newest = max(pending, key=lambda e: e['seq']) if pending else None
    lines = [header, 'Modules=see flushed live report/detail; offline addresses require module map',
             metadata or 'GPU/chain metadata=unavailable\nFirst chain=unavailable',
             'Creation=unobserved; generation=unknown; lifecycle-hook coverage=none',
             f'Calls={counts[2]} failed-original-returns={counts[3]} nested={counts[4]} suppressed={counts[5]} overwritten={counts[6]} pendingLost={counts[7]} dropped=unavailable',
             'Last entry: ' + describe(entry), 'Last return: ' + describe(returned),
             'Last failure: ' + describe(failure), 'DeviceRemovedReason: ' + describe(removed),
             f'Pending snapshot={len(pending)}; newest: ' + describe(newest),
             f'Committed rolling records={len(ring)}; checksum-invalid/uncommitted slots ignored',
             'Capture=partial/crash recovery; counters/header are best effort; pending has NO fabricated HRESULT',
             'Trigger=<startup/FG toggle/resize/gameplay>; exact error=<paste here>']
    return '\n'.join(lines) + '\n', ring


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture', type=Path)
    p.add_argument('--detail', type=Path, help='Write committed rolling records as JSON')
    args = p.parse_args()
    try:
        report, events = recover(args.capture.read_bytes())
    except (OSError, ValueError) as e:
        p.error(str(e))
    print(report, end='')
    if args.detail:
        args.detail.write_text(json.dumps(events, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
