"""Bound registry discovery in an inert image; no runtime or layout verdict."""
import argparse
import hashlib
import re
import struct
from pathlib import Path

TEXT_RVA = 0x1000
TEXT_SIZE = 0x38E028B
TEXT_HASH = "3d9b968371bdb5e0866695a3d9b8b3bb935c2f18d813a34aa1d5c3b68d9df19d"
TARGET = 0x4BD0A38
# Overlapping candidates allowed. Only single-REX MOV/LEA RIP-relative forms.
PATTERN = re.compile(rb"(?=([\x40-\x4f][\x89\x8b\x8d][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]....))", re.S)
WINDOWS = (
    (0x61D2F1, 16, "7bb2f76440b5eb70a4c6fb0dc0d2c1b0a12032e6f24925d709c124a2b68d307d"),
    (0xA08D6C, 22, "e336cc988f49b5d54d9f627bb361e728b7a0361a3235a8fe9f655e261ad9c9c6"),
    (0x19E9876, 140, "681ea075816cba573693304d407d1ffa8d1194113eb172e6707079841af6f95b"),
)


def references(data, start, target):
    return [(start + m.start(), m.group(1)[1]) for m in PATTERN.finditer(data)
            if start + m.start() + 7 + struct.unpack_from("<i", m.group(1), 3)[0] == target]


def validate(chunks):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    native = {}
    if len(chunks) != len(WINDOWS):
        raise ValueError("window count")
    for (rva, size, digest), data in zip(WINDOWS, chunks):
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"provenance {rva:X}")
        native.update({i.address: (i.mnemonic, i.op_str) for i in decoder.disasm(data, rva)})
    checks = {
        0x61D2F1: ("mov", "r15, qword ptr [rip + 0x45b3740]"),
        0x61D2F8: ("test", "r15, r15"),
        0x61D2FB: ("jne", "0x19e97e2"),
        0xA08D6C: ("mov", "r12, qword ptr [rip + 0x41c7cc5]"),
        0xA08D79: ("test", "r12, r12"),
        0xA08D7C: ("jne", "0x1a5d117"),
        0x19E9876: ("lea", "r9, [rax + 0x40]"),
        0x19E987A: ("mov", "qword ptr [rax], r9"),
        0x19E988C: ("mov", "qword ptr [rax + 8], rdx"),
        0x19E989A: ("mov", "qword ptr [rax + 0x10], r8"),
        0x19E98AB: ("mov", "qword ptr [rax + 0x18], rdx"),
        0x19E98B9: ("mov", "qword ptr [rbx + 0x20], rcx"),
        0x19E98C7: ("mov", "qword ptr [rbx + 0x28], rdx"),
        0x19E98D8: ("mov", "qword ptr [rbx + 0x30], rcx"),
        0x19E98EB: ("mov", "qword ptr [rbx + 0x38], rcx"),
        0x19E98F4: ("mov", "rcx, r15"),
        0x19E98FB: ("call", "0x3032cfc"),
    }
    for rva, expected in checks.items():
        if native.get(rva) != expected:
            raise ValueError(f"instruction {rva:X}")
    return len(checks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=Path("re/Nioh3.dump.exe"))
    args = parser.parse_args()
    with args.image.open("rb") as image:
        image.seek(TEXT_RVA)
        text = image.read(TEXT_SIZE)
        if len(text) != TEXT_SIZE or hashlib.sha256(text).hexdigest() != TEXT_HASH:
            raise ValueError("text provenance")
        chunks = []
        for rva, size, _ in WINDOWS:
            image.seek(rva)
            chunks.append(image.read(size))
    hits = references(text, TEXT_RVA, TARGET)
    writes = [rva for rva, opcode in hits if opcode == 0x89]
    if writes != [0x30345E3]:
        raise ValueError("supported-form writer census")
    if references(text, TEXT_RVA, 0x30345DC):
        raise ValueError("unexpected supported-form setter address reference")
    print(f"PASS pinned .text supported-form census: {len(hits)} registry candidates; one writer 30345E3")
    print(f"PASS {len(WINDOWS)} native windows; {validate(chunks)} instruction checks")
    for label, data in (("truncated", chunks[0][:-1]),
                        ("corrupted", bytes([chunks[0][0] ^ 1]) + chunks[0][1:])):
        try:
            validate([data, *chunks[1:]])
        except ValueError as error:
            if str(error) != "provenance 61D2F1":
                raise
        else:
            raise AssertionError(label)
        print(f"PASS {label} memory-only rejection")
    # Negative displacement, adjacent instructions, unrelated target and truncation.
    fixture = b"\x48\x8b\x05" + struct.pack("<i", -23)
    fixture += b"\x48\x89\x0d" + struct.pack("<i", -30)
    assert references(fixture, 0x100, 0xF0) == [(0x100, 0x8B), (0x107, 0x89)]
    assert not references(fixture, 0x100, 0xF1)
    assert references(fixture[:-1], 0x100, 0xF0) == [(0x100, 0x8B)]
    print("PASS scanner signed-displacement/adjacency/unrelated/truncated controls")
    print("BOUND: byte candidates are not exhaustive decoded xrefs; other encodings/indirection excluded.")
    print("OPEN: initialization, concrete registry vtable, returned-object/bound-layout identity and positive-size contract.")


if __name__ == "__main__":
    main()
