"""Pin the native stage-record lookup seam; no layout/lifetime verdict.

Reads small inert-image windows only. Does not launch, build or modify the game.
"""
import argparse
import hashlib
from pathlib import Path


WINDOWS = (
    (0x3033044, 0xFA, "f996e589fcecaab6873bfd6e6f0f9f8667354b125b279a09aeef8f93c155749b"),
    (0x3034588, 0x64, "4af83e8ba2822d55e299f5540160d5689ae4469f3a2e96b110b42a2487d3d3b6"),
    (0x303465C, 0x20, "84880237e1166d22b59580035fdf45fa4be92b8d8a9bf3bfcc90c5a6ff07c5f6"),
)


def validate(chunks):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    native = {}
    for (rva, size, digest), data in zip(WINDOWS, chunks):
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"provenance {rva:X}")
        native.update({i.address: (i.mnemonic, i.op_str)
                       for i in decoder.disasm(data, rva)})
    checks = {}
    for base, kind in zip(range(0x3033044, 0x3033124, 0x20), (7, 5, 3, 4, 2, 6, 1)):
        checks.update({
            base: ("mov", f"rcx, qword ptr [rip + {hex(0x4BD0A38 - base - 7)}]"),
            base + 7: ("mov", "r9, r8"),
            base + 10: ("mov", "edx, edx"),
            base + 12: ("mov", f"r8d, {kind}"),
            base + 18: ("mov", "rax, qword ptr [rcx]"),
            base + 21: ("mov", "rdx, qword ptr [r9 + rdx*8]"),
            base + 25: ("jmp", "qword ptr [rax + 0x10]"),
        })
    checks.update({
        0x3033124: ("mov", "rcx, qword ptr [rip + 0x1b9d90d]"),
        0x303312B: ("mov", "r9, r8"),
        0x303312E: ("mov", "edx, edx"),
        0x3033130: ("xor", "r8d, r8d"),
        0x3033133: ("mov", "rax, qword ptr [rcx]"),
        0x3033136: ("mov", "rdx, qword ptr [r9 + rdx*8]"),
        0x303313A: ("jmp", "qword ptr [rax + 0x10]"),
        0x30345DC: ("mov", "rax, qword ptr [rip + 0x1b9c455]"),
        0x30345E3: ("mov", "qword ptr [rip + 0x1b9c44e], rcx"),
        0x30345EA: ("ret", ""),
        0x303465C: ("mov", "r8, qword ptr [rcx + 0x30]"),
        0x3034660: ("mov", "r8, qword ptr [r8]"),
        0x3034663: ("jmp", "0x3033124"),
    })
    for rva, expected in checks.items():
        if native.get(rva) != expected:
            raise ValueError(f"instruction {rva:X}")
    return len(checks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=Path("re/Nioh3.dump.exe"))
    args = parser.parse_args()
    chunks = []
    with args.image.open("rb") as image:
        for rva, size, _ in WINDOWS:
            image.seek(rva)
            chunks.append(image.read(size))
    count = validate(chunks)
    print(f"PASS {len(WINDOWS)} native windows; {count} instruction checks")
    for label, replacement in (("truncated", chunks[0][:-1]),
                               ("corrupted", bytes([chunks[0][0] ^ 1]) + chunks[0][1:])):
        try:
            validate([replacement, *chunks[1:]])
        except ValueError as error:
            if str(error) != "provenance 3033044":
                raise
        else:
            raise SystemExit(f"FAIL {label} negative control")
        print(f"PASS {label} memory-only rejection")
    print("BOUND: record handles dispatch through registry 4BD0A38 vtable+10 with stage kind.")
    print("OPEN: concrete registry implementation and layout+38/+40/+44 producer contract.")


if __name__ == "__main__":
    main()
