"""Bounded, read-only native cache lifetime evidence; not a serving-cache proof."""
import argparse
import hashlib
import mmap
from pathlib import Path

WINDOWS = [
    (0x2efa840, 0x640, "f771332962cb1ca114be0214ea90de98d96ada05aad208feeb61b8780d3bf821"),
    (0x58ba50, 0x3a, "d2d83352db64aaace664b9073368bda8bafca685e87716606b4715e936515757"),
    (0x58bad7, 0x33, "722625c528151e1ff3ec0de3a5103a794c074b3f66dbe53fb4ea9ebdb8086f51"),
    (0x19d9806, 0x9, "aa1dca57c7bd42b35619eb9054273fddf92986aa96235ac9e1ed02fc97484759"),
    (0x58bc52, 0x20, "a08983df493bbb2ea9284c305e9a91990144eabf44f84c2a563343b59678d900"),
    (0x2eff3c2, 0x55, "4f9dcf1b25b5035055b0134b4e6585265a50e0587d1f18a20a71de6c067249d3"),
    (0x7507c8, 0x5d, "08bf7fce23936f8d59b23d0e26932e37e436327938ed9075fbd58a2a2cdf832c"),
    (0x2eeddec, 0x3d, "7d13bdd2fbd3d366138d295125da7a84c5b25cf809f993fb9cae9c1828cd297b"),
    (0x2f00a74, 0x21, "02332335632360e2a0d006b73b0c53554df41c03bc6306afb6acff3d980801d8"),
    (0x2f01108, 0x45, "5a301d7cee475f82eb7378ae287e0da5f2e6fd58eff7ed2f1cbe958466bc3b7f"),
    (0x2f14328, 0x3c, "19adf529482897ebf7b4958b1c025833a8dddda5098c2a7a57508bd497961068"),
    (0x2f00ac8, 0x52, "74cd9c943228100f857f50c034bf7d9d3f5f599465c557a118eea21899a25f4a"),
    (0x1899bd4, 0x5e, "3c0366776e588ebd1ec329886e9b52f25c6cedf07645e5fe4cb85648d9bc48ef"),
]


def verify(data, expected):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("native evidence window changed")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("dump", type=Path)
    a = p.parse_args()
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    with a.dump.open("rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as b:
        decoded = {}
        for rva, size, sha in WINDOWS:
            body = b[rva:rva + size]
            verify(body, sha)
            mutant = bytearray(body)
            mutant[-1] ^= 1
            try:
                verify(mutant, sha)
            except ValueError:
                pass
            else:
                raise AssertionError("binding admitted changed bytes")
            decoded.update({i.address: (i.mnemonic, i.op_str) for i in md.disasm(body, rva)})
        checks = {
            0x19d9806: ("mov", "qword ptr [r14 + 0x18], rdi"),
            0x19d980a: ("jmp", "0x58bc52"),
            0x2eff3d3: ("je", "0x2eff3fb"),
            0x2eff3e3: ("call", "0x1899bd4"),
            0x2eff402: ("call", "0x7507c8"),
            0x2eff407: ("mov", "qword ptr [r15 + 0x818], rax"),
            0x2efa8ed: ("mov", "qword ptr [rdi + 0x7d8], r14"),
            0x2efabac: ("mov", "qword ptr [rdi + 0x818], r14"),
            0x2f00aec: ("call", "qword ptr [rax + 0x58]"),
            0x2f00b09: ("and", "qword ptr [rdi], rbx"),
            0x2eede0d: ("call", "0x418f90"),
            0x2eede24: ("jmp", "0x2f00a74"),
            0x2f00a7c: ("jne", "0x2f01108"),
            0x2f00a90: ("jmp", "0x2f14328"),
        }
        for site, expected in checks.items():
            if decoded.get(site) != expected:
                raise ValueError(f"instruction mismatch at {site:x}")
    print(f"PASS: {len(WINDOWS)} pinned windows; {len(WINDOWS)} byte mutants rejected; "
          f"{len(checks)} native instruction assertions. Serving cache remains NO-GO.")


if __name__ == "__main__":
    main()
