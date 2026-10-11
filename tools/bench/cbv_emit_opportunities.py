"""Validate existing CBV replay traces; never infer emitter savings from them.

Production Record/LoadTrace write bare CALL records, despite a stale comment
advertising a CBUTRACE header. This tool accepts only that observed format.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct

HEADER = struct.Struct('<6I3Q')
ENTRY = struct.Struct('<2I')
MISSING = [
    'emitter_call_id_and_instruction_site', 'emitter_context_and_generation',
    'stream_segment_and_token_offset', 'emitter_to_replay_correlation',
    'source_mutation_and_lifetime', 'staging_identity',
    'post_call_list_cut_and_redo', 'capture_loss_and_concurrency_receipt',
]


class TraceError(ValueError):
    pass


def records(data):
    """Yield fully bounded, copied records from the production bare-CALL format."""
    if not data:
        raise TraceError('empty trace: no recording coverage')
    offset = 0
    while offset < len(data):
        begin = offset
        if len(data) - offset < HEADER.size:
            raise TraceError(f'truncated CALL header at {begin}')
        magic, stage, mask, count, n, root, ctx, tag, cmdlist = HEADER.unpack_from(data, offset)
        offset += HEADER.size
        if magic != 0x4C4C4143:
            raise TraceError(f'expected bare CALL record at {begin}')
        if stage >= 7 or count > 4096:
            raise TraceError(f'unsupported stage/count at {begin}')
        if count * ENTRY.size > len(data) - offset:
            raise TraceError(f'truncated layout at {begin}')
        layout = tuple(ENTRY.unpack_from(data, offset + i * ENTRY.size) for i in range(count))
        offset += count * ENTRY.size
        if any(slot >= 14 for _, slot in layout):
            raise TraceError(f'unsupported slot at {begin}')
        if len({slot for _, slot in layout}) != count:
            raise TraceError(f'ambiguous duplicate layout slot at {begin}')
        payloads = []
        for size, slot in layout:
            if mask & (1 << slot):
                if size > len(data) - offset:
                    raise TraceError(f'truncated payload at {begin}')
                payloads.append((slot, bytes(data[offset:offset + size])))
                offset += size
        yield stage, n, root, ctx, tag, cmdlist, layout, payloads


def analyze(data):
    calls = uploads = payload_bytes = equal_calls = equal_bytes = transitions = 0
    contexts, prior, epochs = set(), {}, {}
    for stage, n, root, ctx, tag, cmdlist, layout, payloads in records(data):
        calls += 1
        contexts.add(ctx)
        identity = tag, cmdlist
        if ctx in epochs and epochs[ctx] != identity:
            transitions += 1
            prior[ctx] = {}
        epochs[ctx] = identity
        cache = prior.setdefault(ctx, {})
        for slot, payload in payloads:
            uploads += 1
            payload_bytes += len(payload)
            key = stage, slot
            observation = n, root, layout, payload
            if key in cache and cache[key] == observation:
                equal_calls += 1
                equal_bytes += len(payload)
            cache[key] = observation
    return {
        'schema': 'production_bare_CALL', 'sha256': hashlib.sha256(data).hexdigest(),
        'file_bytes': len(data), 'calls': calls, 'contexts': len(contexts),
        'dirty_payload_records': uploads, 'dirty_payload_bytes': payload_bytes,
        'observed_tag_or_list_transitions': transitions,
        'equal_prior_payload_records_same_observed_identity': equal_calls,
        'equal_prior_payload_bytes_same_observed_identity': equal_bytes,
        'emitter_correlated_calls': 0, 'emitter_correlated_fraction': 0.0,
        'candidate_eliminated_emitter_copies': None,
        'candidate_eliminated_emitter_bytes': None,
        'missing_fields': MISSING,
        'verdict': 'insufficient_for_emitter_elimination',
        'limits': 'Equality is replay observation, not CBN eligibility, command-list lifetime proof, emitter savings or FPS. No loss marker exists; valid syntax does not prove complete capture.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path, nargs='+')
    args = parser.parse_args()
    failed = False
    for path in args.trace:
        try:
            result = analyze(path.read_bytes())
        except (OSError, TraceError) as error:
            result = {'trace': str(path), 'error': str(error)}
            failed = True
        print(json.dumps(result, sort_keys=True))
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
