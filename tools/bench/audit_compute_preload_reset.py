"""Read-only pinned preload joins and native scheduler-pause evidence."""
import argparse
import mmap
from pathlib import Path
from audit_compute_cache_lifetime import verify
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

WINDOWS = [
    (0x11022fc, 0x2b8, "9c1bfc353e57cef86471d488d7f1facc8576b7edbc5fc31ede11f7f15cf467c1"),
    (0x2948b48, 0x8c, "908a173c33be17371589dfbad46fde520b1d08672247e81497f0ce3edc6f7ba9"),
    (0x2947ed8, 0x79, "cc7cb9ca8861ad8676e3cd2156c767c08724578051c49f5357183145dd63bf3a"),
    (0x2948b2c, 0x1a, "0310765cbe86cf27976d85d66b4b60212956ea2d1a2efd7fd9eb47a7449c00f0"),
    (0x2002a0, 0x4, "85c65a0003b0225546ba37cd5c58b2ba1bcfad2a259eb9019dc1caa892b64fc7"),
    (0x3aad6c8, 0x68, "a517aa1fcd21801546fd3df3206250cb3ae60d7b55259d3bd8f6ee45f5f706c5"),
    (0x11030a0, 0x3, "e18aacce29affcdddaa5f641ac1ec12552e0fe9d066e6408941b1792818b8619"),
    (0x1ff92e, 0x209, "92e302de5d8edd4130e5de1cdd577baec29083bf73b73e055ce0e41947ccfb0f"),
    (0x1343cb0, 0x50, "1e1aa6878a20c88ef1b2ffaf877616594ab3ebe1fdbc9dc820c832619662a97b"),
    (0x3b84658, 0x10, "24d7e948b7098cd5e94e7e39a6dd8de5d6f8a606795a0c003ddfb4c1bf0b4d42"),
    (0x2ee0d90, 0x37, "3dc675cc0ce4b4f57f4862bef32eda195643cfb73a0a5e81c2d1d11391d9368e"),
    (0x2ee09fc, 0x22, "26f0f99440c5a8b06bbc3e82047204666e1049c3fe7d7f6a07653c2af6501246"),
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
        0x1102466: ("cmp", "byte ptr [r14 + 0x24e], r12b"),
        0x110248e: ("call", "0xb614a8"),
        0x1102506: ("inc", "qword ptr [r14 + 0x1e0]"),
        0x110252e: ("call", "0xb61524"),
        0x2948b55: ("mov", "byte ptr [rcx + 0x24e], 0"),
        0x2948b5c: ("call", "0x2947ed8"),
        0x2947f05: ("mov", "byte ptr [rcx + 0x56], 1"),
        0x2948b81: ("mov", "byte ptr [rax + 0x56], 1"),
        0x2002a0: ("movzx", "eax, byte ptr [rcx + 0x56]"),
        0x11030a0: ("mov", "al, 1"),
        0x1ff942: ("mov", "eax, dword ptr [rax + 0x6b4]"),
        0x1ffaba: ("mov", "qword ptr [rcx], rbx"),
        0x1ffad0: ("mov", "qword ptr [rbx + 0x48], rcx"),
        0x1ffaeb: ("mov", "r9, qword ptr [rax + 0xe8]"),
        0x1ffaf4: ("mov", "rax, qword ptr [rax + 0x3a0]"),
        0x1ffb0f: ("cmp", "edx, dword ptr [rcx + 0x34]"),
        0x1ffb2b: ("call", "0x31a63c"),
        0x1343cd9: ("mov", "rdx, qword ptr [rbx + 0x48]"),
        0x1343ce0: ("call", "qword ptr [rax + 8]"),
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
