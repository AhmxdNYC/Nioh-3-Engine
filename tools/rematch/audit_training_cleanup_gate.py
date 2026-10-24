"""Exact retained owner disposal/event semantics; not complete producer exclusion."""
import argparse
import hashlib
import json
import struct

WINDOWS = [
    (0xF25CC, 0x17, 'fcc254fbb75b332a35747b3cdb60f53e73ea0880af22c3ae7380baf45225bfc8'),
    (0xF262C, 0x3D, 'ecc1384a308322a788a70b6c2e709057847620b1650cfb21db458f4c255fa8d2'),
    (0x1C01DC4, 9, '128a25636c724807e673655e85555bce0b416e741fa3ca7cd74b8dce2c44a6a0'),
    (0xE312A4, 0x1E, 'bf050811c5904613a3a00172c46465c3fed635549e6d4cada8d9d145b831deb4'),
    (0x1BEDD1C, 0x34, '79c1542fac5de2dd056b373c55e34c9691ad5c320a61b53f265fbfded018278f'),
    (0x27F20, 0x27, '0275f8fb6368db449f9202f6daaa4402bbdd25f65a03b4b1089094367f150a40'),
    (0x5B9E88, 0x45, 'c7a81acdfce4615bd74cb11277b82984c73b2260bef4e4a50d7bfe1d019aee8e'),
    (0x2E33A4, 0x31, '79bf1d2d4fbb873ddc6aa0fb7a30721eddf22911ad6cdbb87c69cec493ea754c'),
    (0x39E104, 0x61, '66bc739df438b13431216f13dd5b1c5c788d4e6b3d7bf6ba3a07496603d74764'),
    (0x2F6CE8, 0x22, '9259c29efd6a1a24f2805ddcbfe198b867946bcc8104083dbbb066afcf897069'),
    (0x2F6680, 0x36, 'c2d09af6eba99926f98b26a92e175e0f33b32332b7080a7fc76f3a2bf2801862'),
    (0x1BE1BF0, 0x67, '35c824f6485a60fb40a829d42dedec6c0647c25e06b35410a9fe6231252fd552'),
    (0x3A892F0, 8, 'd4cb2691eb97c08fb78dd529cd1bea269935872e837336996727f10a8e1b4fc4'),
]
EDGES = [
    (0xF25D9, 0xF262C, 0xE8), (0x1C01DC8, 0xE312A4, 0xE9),
    (0xE312BD, 0x1BEDD1C, 0xE9), (0x1BEDD34, 0x1BF9B10, 0xE8),
    (0x27F32, 0x5B9E88, 0xE8), (0x2E33BB, 0x2F6680, 0xE8),
    (0x39E15B, 0x2F6CE8, 0xE8), (0x2F6CF1, 0x48B44, 0xE8),
    (0x2F6D05, 0xB61524, 0xE9), (0x2F6689, 0xB614A8, 0xE8),
    (0x2F669D, 0xB617E8, 0xE8), (0x2F66AA, 0xB61524, 0xE9),
]


def audit(read):
    for rva, size, digest in WINDOWS:
        block = read(rva, size)
        if len(block) != size or hashlib.sha256(block).hexdigest() != digest:
            raise ValueError('window mismatch at ' + hex(rva))
    for site, target, opcode in EDGES:
        block = read(site, 5)
        if len(block) != 5 or block[0] != opcode or site + 5 + struct.unpack_from('<i', block, 1)[0] != target:
            raise ValueError('edge mismatch at ' + hex(site))
    # Retained dump VA in the typed control block vtable, not a live pointer.
    if struct.unpack('<Q', read(0x3A892F0, 8))[0] != 0x7FF74F410000 + 0x1C01DC4:
        raise ValueError('owner disposal vtable changed')
    for site in [0x27F27, 0x2E33B4, 0x39E14D]:
        if site + 7 + struct.unpack('<i', read(site + 3, 4))[0] != 0x4709AB0:
            raise ValueError('event identity changed')
    return dict(status='EXACT_TYPED_DISPOSAL_AND_MANUAL_RESET_GATE_MATCHED',
                windows=len(WINDOWS), edges=len(EDGES), gate_initial_manual_reset=True,
                gate_initial_signaled=True, typed_owner_disposal_chain_matched=True,
                destination_owner_lease_proved=False, all_event_writers_enumerated=False,
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
        sites = [0x3A892F0, 0x1BEDD35, 0x27F26, 0x2F669C, 0x2F6CFF]
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
