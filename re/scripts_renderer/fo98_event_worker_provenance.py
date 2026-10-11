"""Verify reviewed event-worker windows; identity checks, not lifetime proof."""
import hashlib
from pathlib import Path

WINDOWS = (
    (0x1266FC, 41, 'aaae9da4c186fad37d737ae38ec1679e6da28f90862e7c16b5a519da538f8913'),
    (0x126A14, 44, '16cfd2bcdf68e73d9d1a7ebd2a2fa26c7a896261a198a302177f7ac84c4a9403'),
    (0x126984, 49, 'ff2c612f0ef51e6d20e29cd0cd3fffa5a1c394914a30e6a8811055fbc3983bcd'),
    (0x1269D4, 46, '511a58bb118e8069e62a6e84f7dc338aeb4a78611681de0ecbae79e7717a5f9b'),
    (0x2EEE73C, 77, 'af93dbd37334ef8a2a4c803ef1af8436d2dddb8ccd3c2a96350818fc16c115ca'),
    (0x2EF00F8, 139, 'd9e7e3c9233230c323d8651e9fc8abd406dd594d1376a0611e122e2c5850d46f'),
    (0x2EEE880, 133, '34970ed7f6cbc4d8f256c18d73b05b53ffaa7e1fc69b89b6f3dd789210dece8f'),
    (0x1A71FDC, 98, '5f7d05dded8ec609610bf1d30e62563a946cab5ac2977404e54f93e8fcf3cd89'),
)


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for rva, size, expected in WINDOWS:
            stream.seek(rva)
            actual = hashlib.sha256(stream.read(size)).hexdigest()
            if actual != expected:
                raise SystemExit(f'FAIL: {rva:X} byte provenance changed')
            print(f'PASS: {rva:X} {size} bytes SHA256 {actual}')
    print('8 static provenance checks passed; no runtime/lifetime/safety verdict')


if __name__ == '__main__':
    main()
