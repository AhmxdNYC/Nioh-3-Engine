"""Read-only MIC dependency census; absence is NOT D3D12 admission."""
import collections
import hashlib
import json
import pathlib
import struct
import sys

MIC = 'e31c389f6d4a38de7b1b784b87c710d2'


def scan(path):
    data = path.read_bytes()
    if data[:4] != b'N3PJ' or len(data) < 48:
        raise ValueError('not a complete journal header')
    if struct.unpack_from('<I', data, 4)[0] not in (1, 2):
        raise ValueError('unknown journal version')
    pos = 48
    counts = collections.Counter()
    kinds = collections.Counter()
    shaders = []
    matches = []
    while pos < len(data):
        if pos + 16 > len(data):
            raise ValueError('partial chunk header')
        tag, size, _checksum = struct.unpack_from('<4sIQ', data, pos)
        if not 0 < size <= 16 * 1024 * 1024 or pos + 16 + size > len(data):
            raise ValueError('invalid or partial chunk')
        body = data[pos + 16:pos + 16 + size]
        counts[tag.decode('ascii')] += 1
        if tag == b'SHDR' and body[:16].hex() == MIC:
            shaders.append(pos)
        if tag == b'PSO1':
            if len(body) < 32:
                raise ValueError('short PSO')
            kind, ver, ns, il, fixed, ro, zero = struct.unpack_from('<BBBBIII', body)
            if ver != 1 or kind not in (1, 2, 3) or 32 + fixed + 24 * ns > len(body):
                raise ValueError('unsupported PSO structure')
            kinds[str(kind)] += 1
            refs = [body[40 + fixed + 24*i:56 + fixed + 24*i].hex() for i in range(ns)]
            if MIC in refs:
                matches.append({'offset': pos, 'kind': kind, 'root_key': body[16:32].hex(),
                                'fixed_size': fixed, 'shader_refs': refs})
        pos += 16 + size
    if path.read_bytes() != data:
        raise ValueError('journal changed during scan')
    return {'source': str(path), 'sha256': hashlib.sha256(data).hexdigest(),
            'bytes': len(data), 'chunk_counts': dict(counts), 'pso_kinds': dict(kinds),
            'mic_digest': MIC, 'shader_offsets': shaders, 'pso_matches': matches,
            'status': 'DEPENDENCIES_REQUIRE_VALIDATION' if matches else 'MISSING_NATIVE_MIC_PSO',
            'scope': 'Structural absence census only; no chunk checksum, dependency or device admission claim'}


if __name__ == '__main__':
    print(json.dumps(scan(pathlib.Path(sys.argv[1])), indent=2))
