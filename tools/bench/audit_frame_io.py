"""Bound existing frame-I/O evidence; no game access or inferred FPS savings."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import struct

TARGETS = {0x117D6C: 'copy_fence_poll', 0x5C0D4: 'copy_submit_wait',
           0x5CEE8: 'mapped_buffer_close', 0x4A2D0: 'meshlet_request_job'}


def profile(path):
    rows = []
    seen = set()
    with path.open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if reader.fieldnames != ['tid', 'thread', 'view', 'count', 'pct', 'busy', 'function']:
            raise ValueError('unsupported profile columns')
        for row in reader:
            match = re.match(r'^Nioh3\+([0-9a-fA-F]+)(?:\s|\(|$)', row['function'])
            if not match or int(match[1], 16) not in TARGETS:
                continue
            rva = int(match[1], 16)
            count, pct = int(row['count']), float(row['pct'])
            if count < 0 or not math.isfinite(pct) or not 0 <= pct <= 100:
                raise ValueError('invalid count/percentage')
            key = (row['tid'], row['view'], rva)
            if key in seen:
                raise ValueError('duplicate attribution row; do not double count')
            seen.add(key)
            rows.append({'tid': int(row['tid']), 'thread': row['thread'], 'view': row['view'],
                         'mechanism': TARGETS[rva], 'samples': count, 'thread_sample_pct': pct})
    return {'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'observations': rows, 'fps_savings': None,
            'paired_flush_frequency': None, 'critical_path_ms': None}


def native_exports(directory):
    def body(rva):
        text = (directory / f'0x{rva:x}.c').read_text()
        return text[text.index('{'):]
    job, close, submit, wait = (body(rva) for rva in (0x4A2D0, 0x5CEE8, 0x5C0D4, 0x117D6C))
    conditions = {
        'two_normal_close_sites': job.count('GfxCmdContextCloseAndFlush(') == 2,
        'close_order_b_then_a': job.index('GfxCmdContextCloseAndFlush(local_98)') < job.index('GfxCmdContextCloseAndFlush(local_78)'),
        'worker_fanout_before_closes': job.index('MeshletRequestFanoutJobs(') < job.index('GfxCmdContextCloseAndFlush('),
        'single_copy_close': 'FUN_7ff74f46c0d4(lVar6 + 0x43e8,&local_60,1)' in close,
        'staging_release_after_submit_wait': close.index('FUN_7ff74f46c0d4(') < close.index('GpuAllocRelease2('),
        'submit_then_wait_then_return_context': submit.index('CmdListRingCloseAndSubmit(') < submit.index('GfxFencePollWorker(') < submit.index('FUN_7ff74f46c1a4('),
        'wait_retires_allocator_after_completion': wait.index('if (uVar1 <= uVar3) break;') < wait.index('*(ulonglong *)(param_1 + 0x30) = uVar1'),
        'both_request_maps_mode0': all(value in job for value in (
            'FUN_7ff74f46b3ac(local_78,&local_80,0,0,', 'FUN_7ff74f46b3ac(local_98,&local_a0,0,0,')),
        'read_copy_helper_precedes_cpu_map': body(0x62E680).index('FUN_7ff74fa3e9b4(') < body(0x62E680).index('FUN_7ff74f45d9c4('),
        'read_copy_single_record': 'FUN_7ff74f46c0d4(param_1,param_2,1,param_3,0,param_5)' in body(0x1A807F8),
    }
    if not all(conditions.values()):
        raise ValueError(f'native export contract changed: {conditions}')
    return {'export_shape_checks': conditions,
            'scope': 'decompiler export shape only; not ABI, map-mode, alias or full native proof',
            'hashes': {f'{rva:x}': hashlib.sha256((directory/f'0x{rva:x}.c').read_bytes()).hexdigest()
                       for rva in (0x4A2D0, 0x5CEE8, 0x5C0D4, 0x117D6C, 0x62E680, 0x1A807F8)}}


def map_modes(path):
    with path.open('rb') as stream:
        header = stream.read(4096)
        pe = struct.unpack_from('<I', header, 0x3c)[0]
        if header[pe:pe+4] != b'PE\0\0':
            raise ValueError('not PE')
        count = struct.unpack_from('<H', header, pe+6)[0]
        optional_size = struct.unpack_from('<H', header, pe+20)[0]
        table = pe+24+optional_size
        address = 0x3BE29D0
        matched = False
        for index in range(count):
            _, virtual_size, rva, raw_size, raw = struct.unpack_from('<8sIIII', header, table+index*40)
            if rva <= address and address+10 <= rva+min(virtual_size, raw_size):
                if raw != rva:
                    raise ValueError('expected flat mapped dump')
                matched = True
        if not matched:
            raise ValueError('map table not inside a raw-backed section')
        stream.seek(address)
        modes = stream.read(10)
    if modes[:2] != bytes([1, 0]):
        raise ValueError('mode0 assumptions changed')
    return {'rva': hex(address), 'sha256': hashlib.sha256(modes).hexdigest(),
            'mode0_read_path': True, 'mode0_close_copyback': False,
            'paired_close_optimization': 'REJECTED: mode0 close has no copyback',
            'next_candidate': 'pair read-side preparation before CPU consumption; retain feedback epoch'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    parser.add_argument('--decomp', type=Path)
    parser.add_argument('--dump', type=Path)
    args = parser.parse_args()
    reports = [profile(p) for p in args.profiles]
    waits = [row['thread_sample_pct'] for report in reports for row in report['observations']
             if row['mechanism'] == 'copy_fence_poll' and row['view'] == 'attr']
    output = {'profiles': reports, 'attributed_worker_wait_pct_range': [min(waits), max(waits)] if waits else None,
              'limits': ['Thread-local sample percentages are not frame wall time.',
                         'Self/attr/incl views overlap; never sum them.',
                         'No target row means unobserved, not zero.',
                         'Generic copy wait includes callers besides paired meshlet closes.']}
    if args.decomp:
        output['native'] = native_exports(args.decomp)
    if args.dump:
        output['map_mode_evidence'] = map_modes(args.dump)
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    main()
