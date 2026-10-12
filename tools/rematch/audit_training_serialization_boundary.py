"""Read-only retained-dump proof of local order; never authorizes native mutation."""
import argparse
import hashlib
import json
import struct

WINDOWS = [
    ("early_unlock", 0xE6323A, 0xC, "846bc36d0d480273a99fe21e35595f5884793667391bac4d9cfdf6a138d5bb46"),
    ("second_unlock", 0xE632BD, 0xC, "63446e9520935cb8760789e60cd5a351937aa15676173f64dcbacdc5fca546e6"),
    ("gate_release_and_dispatch", 0xE632C9, 0x69, "f3c3562be7ffa624c466299ef387c6f343105f2a3a3cd0b96c67df1cc701ce3b"),
    ("manager_dispatch", 0xE64700, 0x51, "f42acd9212938294a59f34c5874ecd72da4a61b137660884ba46a271c3fb02bf"),
    ("manager_update", 0x379620, 0x305, "5fd6a8808ae6314195fc2819322ed1c87811abea9c790dbd3f218dbc844728a0"),
    ("owner_compaction", 0xE33840, 0xD2, "2caf9662ae2f72e313566f2c410f90881342910ee4ccf263e8aa2f3f2fe867b4"),
    ("cleanup_child_branch", 0xE22D10, 0xCE, "a03289bb3dfbd6f6ba238fc8925d3c876fb767fd7ffb49f610b2d520dd55dc6d"),
    ("owner_erase_cold", 0x19227FC, 0x29, "e91d4013a00dd4141bd94ef823026a897bf9a2b42c372a690f54291ddaea6382"),
    ("owner_node_destroy", 0x1BE5B6C, 0x1F, "cadfe3bde7cb334affbb72492e6fefc9b59ce08c608d2e22cc742f1d2557549a"),
]
EDGES = [
    (0xE63241, 0xB61524, 0xE8), (0xE632C4, 0xB61524, 0xE8),
    (0xE632D0, 0xB614A8, 0xE8), (0xE632EF, 0xB618E8, 0xE8),
    (0xE63311, 0xB61524, 0xE8), (0xE6332D, 0xE64700, 0xE9),
    (0xE6474C, 0x379620, 0xE8), (0x379803, 0x37A434, 0xE8),
    (0x3798A6, 0x89A328, 0xE8), (0x3798D9, 0xE33840, 0xE8),
    (0x3798E8, 0x8F4A20, 0xE8), (0xE33870, 0xE22D10, 0xE8),
    (0xE338A7, 0xE22D10, 0xE8), (0xE338D1, 0xF262C, 0xE8),
    (0xE22DB2, 0x94B04, 0xE8), (0x192280C, 0x1BE5B6C, 0xE8),
    (0x1BE5B79, 0xF25CC, 0xE8),
]


def audit(read):
    for name, rva, size, digest in WINDOWS:
        block = read(rva, size)
        if len(block) != size or hashlib.sha256(block).hexdigest() != digest:
            raise ValueError("native window mismatch: " + name)
    for site, target, opcode in EDGES:
        block = read(site, 5)
        if len(block) != 5 or block[0] != opcode or site + 5 + struct.unpack_from("<i", block, 1)[0] != target:
            raise ValueError("native edge mismatch: " + hex(site))
    return {
        "status": "EXACT_LOCAL_SERIALIZATION_BOUNDARY_MATCHED",
        "window_count": len(WINDOWS), "edge_count": len(EDGES),
        "mission_gate_mutex_released_before_update": True,
        "local_owner_pruning_after_script_loop": True,
        "owner_retention_proved": False, "all_producers_excluded": False,
        "callback_reentrancy_excluded": False, "native_mutation_admitted": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dump", required=True)
    args = parser.parse_args()
    with open(args.dump, "rb") as source:
        def read(rva, size):
            source.seek(rva)
            return source.read(size)
        result = audit(read)
        # In-memory negatives: dispatch before unlocking, another callback,
        # different cleanup-child insertion, and different owner-node disposal.
        sites = [0xE63312, 0x379804, 0xE22DB3, 0x192280D]
        for site in sites:
            def altered(rva, size):
                block = bytearray(read(rva, size))
                if rva <= site < rva + size:
                    block[site - rva] ^= 1
                return bytes(block)
            try:
                audit(altered)
            except ValueError:
                pass
            else:
                raise AssertionError("mutated edge accepted")
        result["mutation_negatives_passed"] = len(sites)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
