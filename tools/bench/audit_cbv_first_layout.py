"""Bounded inert-image audit of fresh-layout dirty-slot admission.

This checks native provenance, not lifetime safety or runtime reachability.
No game access, build, or writes. Run from the private repository root.
"""
import argparse
import hashlib
from pathlib import Path


WINDOWS = (
    (0x114B00, 0x101, "f41e2bce3b20b7e01014965c43ff31900b63b7c298fedfb91dd91ef3e68169d7"),
    (0x115484, 299, "94575888b9329dc93e4c191a1055647b7a5223edb32ecb6a69d3779a78804fcf"),
    (0x1A49ADE, 0x42, "d91a76733ecc61391777291b332eeb872f75ddd35b44a7e4254afdd4f3a031aa"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=Path("re/Nioh3.dump.exe"))
    args = parser.parse_args()
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    native = {}
    with args.image.open("rb") as image:
        for rva, size, expected in WINDOWS:
            image.seek(rva)
            data = image.read(size)
            digest = hashlib.sha256(data).hexdigest()
            if len(data) != size or digest != expected:
                raise SystemExit(f"FAIL provenance {rva:X}/{size:X}")
            native.update({i.address: (i.mnemonic, i.op_str) for i in decoder.disasm(data, rva)})
            print(f"PASS native {rva:X}/{size:X} {digest}")
    checks = {
        0x114B2A: ("ja", "0x114bf2"),
        0x114BF2: ("bts", "dword ptr [rcx + r11*4 + 0xb70], 0x1f"),
        0x114B69: ("jbe", "0x114b98"),
        0x114B88: ("cmp", "dword ptr [r11 + rdx], eax"),
        0x114B8C: ("ja", "0x114ba3"),
        0x1154D0: ("ja", "0x115565"),
        0x115500: ("jbe", "0x115528"),
        0x11551D: ("ja", "0x115541"),
        0x115528: ("mov", "r8d, dword ptr [rbx + 0x64]"),
        0x11552F: ("jne", "0x115572"),
        0x1A49B15: ("call", "0x114b00"),
        0x1A49B1B: ("jmp", "0x97ca31"),
    }
    for rva, expected in checks.items():
        if native.get(rva) != expected:
            raise SystemExit(f"FAIL instruction {rva:X}")
    print(f"PASS {len(checks)} native instruction checks")
    print("BOUND: table growth alone sets sentinel; slot upload requires declared size > native size.")
    print("OPEN: producer invariant n>0 implies a positive declared slot; no native defect/gain verdict.")


if __name__ == "__main__":
    main()
