"""Pin native VS registry protocol seams; no runtime/layout/safety verdict."""
import argparse
import hashlib
from pathlib import Path

WINDOWS = (
    (0x3032CFC, 171, "4af240efe56606b3626a626e6d021484cc1106b5acbe63f9d51721ae864a7009"),
    (0x3034528, 52, "94995eda8851fd30bf781189755e4c05814a9c898c8875a785cd780f31d354c0"),
    (0x3032FF4, 79, "8255725386baaa42da4731f3ce7961c937b6f49cd79552df8c28dbe5994838e5"),
)
CHECKS = {
    0x3032D14: ("mov", "ebx, dword ptr [r8 + 0xa8]"),
    0x3032D1B: ("mov", "rsi, r9"),
    0x3032D1E: ("mov", "r15, rdx"),
    0x3032D21: ("mov", "r14, rcx"),
    0x3032D24: ("test", "ebx, ebx"),
    0x3032D26: ("je", "0x3032d8e"),
    0x3032D28: ("mov", "rcx, r8"),
    0x3032D2B: ("call", "0x3034528"),
    0x3032D32: ("mov", "rbp, rax"),
    0x3032D39: ("mov", "r10, qword ptr [r14]"),
    0x3032D3C: ("lea", "rax, [rsp + 0x60]"),
    0x3032D41: ("mov", "r9d, dword ptr [rsp + 0x70]"),
    0x3032D46: ("mov", "r8, rbp"),
    0x3032D49: ("and", "qword ptr [rsp + 0x60], 0"),
    0x3032D4F: ("mov", "rdx, r15"),
    0x3032D52: ("mov", "qword ptr [rsp + 0x28], rax"),
    0x3032D57: ("mov", "rcx, r14"),
    0x3032D5A: ("and", "dword ptr [rsp + 0x20], 0"),
    0x3032D5F: ("call", "qword ptr [r10]"),
    0x3032D62: ("mov", "rcx, qword ptr [rsp + 0x60]"),
    0x3032D67: ("test", "rcx, rcx"),
    0x3032D6A: ("je", "0x3032d86"),
    0x3032D6C: ("test", "al, al"),
    0x3032D6E: ("je", "0x3032d86"),
    0x3032D70: ("mov", "eax, dword ptr [rsp + 0x70]"),
    0x3032D76: ("mov", "qword ptr [rsi], rcx"),
    0x3032D79: ("add", "rbp, rax"),
    0x3032D7C: ("add", "rsi, 8"),
    0x3032D80: ("cmp", "edi, ebx"),
    0x3032D82: ("jb", "0x3032d39"),
    0x3032D86: ("mov", "rax, qword ptr [rsp + 0x78]"),
    0x3032D8B: ("mov", "byte ptr [rax], 1"),
    0x3034528: ("add", "rcx, 0x40"),
    0x303452C: ("cmp", "dword ptr [rcx + 0xfc], 0"),
    0x3034533: ("je", "0x3034543"),
    0x3034535: ("mov", "eax, dword ptr [rcx + 0x100]"),
    0x303453B: ("test", "eax, eax"),
    0x303453D: ("jle", "0x3034543"),
    0x303453F: ("add", "rax, rcx"),
    0x3034543: ("xor", "eax, eax"),
    0x3033003: ("mov", "eax, dword ptr [rcx + 0xa8]"),
    0x3033009: ("mov", "rbx, rdx"),
    0x303300C: ("mov", "rsi, qword ptr [rip + 0x1b9da25]"),
    0x3033013: ("cmp", "eax, 1"),
    0x3033016: ("jb", "0x3033033"),
    0x303301A: ("mov", "rax, qword ptr [rsi]"),
    0x303301D: ("xor", "r8d, r8d"),
    0x3033020: ("mov", "rdx, rbx"),
    0x3033023: ("mov", "rcx, rsi"),
    0x3033026: ("call", "qword ptr [rax + 0x18]"),
    0x3033029: ("add", "rbx, 8"),
    0x303302D: ("sub", "rdi, 1"),
    0x3033031: ("jne", "0x303301a"),
}


def validate(chunks):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    if len(chunks) != len(WINDOWS):
        raise ValueError("window count")
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    instructions = {}
    for (rva, size, digest), data in zip(WINDOWS, chunks):
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"provenance {rva:X}")
        decoded = list(decoder.disasm(data, rva))
        # The accessor window includes padding and neighboring getters; all
        # protocol checks there are bounded to the first accessor body.
        instructions.update({i.address: (i.mnemonic, i.op_str) for i in decoded})
    for rva, expected in CHECKS.items():
        if instructions.get(rva) != expected:
            raise ValueError(f"instruction {rva:X}")
    return len(CHECKS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=Path("re/Nioh3.dump.exe"))
    args = parser.parse_args()
    chunks = []
    with args.image.open("rb") as image:
        for rva, size, _ in WINDOWS:
            image.seek(rva)
            chunks.append(image.read(size))
    print(f"PASS {len(WINDOWS)} pinned windows/{sum(w[1] for w in WINDOWS)} bytes; {validate(chunks)} instruction checks")
    for index, (rva, _, _) in enumerate(WINDOWS):
        for label, replacement in (
            ("truncated", chunks[index][:-1]),
            ("corrupted", bytes([chunks[index][0] ^ 1]) + chunks[index][1:]),
        ):
            altered = list(chunks)
            altered[index] = replacement
            try:
                validate(altered)
            except ValueError as error:
                if str(error) != f"provenance {rva:X}":
                    raise
            else:
                raise AssertionError(label)
        print(f"PASS {rva:X} truncated/corrupted memory-only rejection")
    print("BOUND: slot +0 admission/output and slot +18 handle-slot protocol; implementations unresolved.")
    print("OPEN: concrete vtable, bound-layout identity, positive-size/generation and retirement contracts.")


if __name__ == "__main__":
    main()
