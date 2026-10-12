"""Bound the native manager's owner-pair insertion and signal-before-work edge."""
import argparse
import hashlib
import json
import struct

WINDOWS = [
    (0x702CC0, 0x22, 'a759179782c4d7e3552768fa18fbbc8d6ff38e506de30bc700ca220f1fe2ae61'),
    (0x1A01226, 0x14, 'a84c3d620e366c4bab14ec6365b7f8961b3f74f6be26c43a1feae5b1adcc09a1'),
    (0x1C07FAC, 0x16, '2451a924fc695209159b096a54be7feb83808e77e3052e9c54aa30dffe39b436'),
    (0x1C07F0C, 0x9E, 'ba4c0f11474d1694f9e961df9e8b6f403fe983165e989c5a120890180ce0abb0'),
    (0xE2AAA0, 0x60, '2f9534ea958bbc96fc48ba4dca8ed8f239b6d5457d85a2f03b1b9b490dd26451'),
    (0x1BE1998, 0x3B, '73c271ee5e4a650f0271577868002a6a402aa931d2efe6f20d5cc6bad1b629a4'),
    (0x1BE1BF0, 0x67, '35c824f6485a60fb40a829d42dedec6c0647c25e06b35410a9fe6231252fd552'),
    (0x2E33A4, 0x31, '79bf1d2d4fbb873ddc6aa0fb7a30721eddf22911ad6cdbb87c69cec493ea754c'),
    (0x683F74, 0x32, 'a12b041a45609b3ad2126d8f9670779114249d75ec79560371ab03e33711e53d'),
    (0x1A81B28, 0x1E, 'ce23a72ff5e9f54525408b296a28eeecec1875f48d76fa307a83340634370b21'),
    (0x9DCF54, 0x18, 'e1ecb04b916a5510681a744c1bcfd057386028140f069bbc8223b3776ae6430c'),
]
EDGES = [
    (0x1A01235, 0x1C07FAC, 0xE9), (0x1C07FB8, 0x1C07F0C, 0xE8),
    (0x1C07F2A, 0xE307B8, 0xE8), (0x1C07F3C, 0x1C2608C, 0xE8),
    (0x1C07F8E, 0xE2AAA0, 0xE8), (0x1C07F98, 0x1BF9784, 0xE8),
    (0xE2AAD5, 0x1BE1998, 0xE8), (0x1BE19C0, 0x1BE1BF0, 0xE8),
    (0x2E33BB, 0x2F6680, 0xE8), (0x2E33D0, 0x2E33D8, 0xE9),
    (0x683F93, 0x9DCE70, 0xE8), (0x1A81B38, 0x9DCE70, 0xE8),
    (0x9DCF5E, 0x9DCF6C, 0xE8),
]


def audit(read):
    for rva, size, digest in WINDOWS:
        data = read(rva, size)
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('window mismatch: ' + hex(rva))
    for site, target, opcode in EDGES:
        data = read(site, 5)
        if len(data) != 5 or data[0] != opcode or site + 5 + struct.unpack_from('<i', data, 1)[0] != target:
            raise ValueError('edge mismatch: ' + hex(site))
    # Same manager wrapper as the mission-thread update, not merely same layout.
    for site in [0x702CC4, 0xE64745]:
        if site + 7 + struct.unpack('<i', read(site + 3, 4))[0] != 0x45B9E40:
            raise ValueError('manager identity mismatch')
    if read(0x1C07F82, 4) != bytes.fromhex('488d4d10'):
        raise ValueError('manager owner-list offset changed')
    if struct.unpack('<Q', read(0x4035810, 8))[0] != 0x7FF74F410000 + 0x2E33A4:
        raise ValueError('scene task vtable identity changed')
    return dict(status='EXACT_MANAGER_OWNER_INSERTION_MATCHED', windows=len(WINDOWS),
                edges=len(EDGES), typed_pair_inserted_at_manager_10=True,
                mission_gate_signaled_before_scene_task_body=True,
                callback_retained_owner_lease=False, all_cleanup_paths_enumerated=False,
                all_producers_excluded=False, native_mutation_admitted=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dump', required=True)
    args = parser.parse_args()
    with open(args.dump, 'rb') as source:
        def read(rva, size):
            source.seek(rva)
            return source.read(size)
        result = audit(read)
        sites = [0x702CC7, 0x1C07F85, 0x1BE19C1, 0x2E33BC, 0x4035810, 0x1A81B39]
        for site in sites:
            def changed(rva, size):
                data = bytearray(read(rva, size))
                if rva <= site < rva + size:
                    data[site - rva] ^= 1
                return bytes(data)
            try:
                audit(changed)
            except ValueError:
                pass
            else:
                raise AssertionError('mutation accepted')
        result['mutation_negatives_passed'] = len(sites)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
