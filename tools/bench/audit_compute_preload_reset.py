"""Read-only pinned preload joins and native scheduler-pause evidence."""
import argparse
import mmap
from pathlib import Path
from audit_compute_cache_lifetime import verify
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

WINDOWS = [
    (0x34835d4, 0x10c, "bce94a6c5f5ab457a66b1bb776ffb1d7e4af6018ff71ac24e1dd03b2e1727487"),
    (0x3483e04, 0x78d, "3dfbb5c720f7489da8a8a787f3b12f9dcea724d07475a2a752d611d3a2d7593a"),
    (0x3e7cc, 0x77, "1a0e46a778eea40ee16534efa108b8f0c4b58a59a9918a48d539334c8c8a95b3"),
    (0x1101b90, 0x1ee, "5dc0e469568e058a54ad3eff5786f48fc8e61ad906baa7c79cdc03c228aec0f9"),
    (0x3484594, 0x82, "dda01f8646f0cc47753756b859a0daa28921db6e56967a7b71b9361a22eeb393"),
    (0x226d140, 0x222, "163cc50bcbf338916b90c2257ef058b282a4b6cd4c60050398eb42f445b3431a"),
    (0x3242cb8, 0xab, "4aca66c66a2e164ff14b3261d3e43139ee23bd75403a0606c4d6a64576994bb1"),
    (0x8fe5fc, 0x92, "20ef756dc2a21af82a2dba9e44e9f7f360bc43f962f52bf5d7ab51838762fd43"),
    (0x1a37bc7, 0xb2, "da9fd3da2b606181f59b99a789532b1a5d3fe51e46055edfc1c4a47e8f987dbd"),
    (0x8fe690, 0x40, "787c3832139f11673a8f2d695d5dc136207950c04522109dc632f5a2e8cb4f80"),
    (0x18cbcb8, 0xf2, "2fbff999372c8009ad63131c4297ffc5dc4eacbe4477a6804bd2442038def4e0"),
    (0x3b2970, 0x29, "1dd07faa04c2934f8ef58fae321d5bb9b287d1dd3a4be973efdc30d0f4a0508a"),
]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("dump", type=Path)
    a = p.parse_args()
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    decoded = {}
    with a.dump.open("rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as b:
        for rva, size, sha in WINDOWS:
            body = b[rva:rva + size]
            verify(body, sha)
            mutant = bytearray(body)
            mutant[0] ^= 1
            try:
                verify(mutant, sha)
            except ValueError:
                pass
            else:
                raise AssertionError("changed native window admitted")
            decoded.update({i.address: (i.mnemonic, i.op_str) for i in md.disasm(body, rva)})
    checks = {
        0x3483631: ("call", "0x2efe1cc"),
        0x3484444: ("call", "0x3fad8"),
        0x348445f: ("call", "0x3e7cc"),
        0x3484514: ("call", "0x2efe1cc"),
        0x1101c61: ("call", "0x3483e04"),
        0x34845ee: ("call", "0x3483e04"),
        0x226d1a8: ("call", "0x3242cb8"),
        0x226d30e: ("call", "0x2eea690"),
        0x226d331: ("call", "0x18cbcb8"),
        0x8fe670: ("jb", "0x1a37bc7"),
        0x1a37c19: ("lock cmpxchg", "dword ptr [rbx + 0x30], r14d"),
        0x1a37c1f: ("cmp", "edi, eax"),
        0x1a37c21: ("jne", "0x1a37beb"),
        0x1a37c23: ("mov", "edx, 0x20000000"),
        0x1a37c2e: ("call", "0x42050"),
    }
    for site, expected in checks.items():
        if decoded.get(site) != expected:
            raise ValueError(f"native edge mismatch {site:x}")
    print(f"PASS {len(WINDOWS)} pinned windows, {len(WINDOWS)} mutation negatives, "
          f"{len(checks)} instructions. Cross-pool/reset exclusion NOT proven.")


if __name__ == "__main__":
    main()
