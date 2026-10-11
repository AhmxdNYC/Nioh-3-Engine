"""Read only named FO98 consumer windows; verify static provenance, not safety."""
import hashlib
from pathlib import Path
import struct

WINDOWS = (
    (0x891F24, 180, 'af4fdcf103dac07442fca0eb06204b3b5118f5ffb0ec5b370ca6b83832de88c0'),
    (0x891F80, 160, '9172ae6e5078d9f1faf31446835fabba4a47ad494f31db046aae71f4701daa71'),
    (0x7F6964, 160, '1db4a23aedc217053b8f6cefa8a6759c41e385388c402b659499c1d0894fd3fc'),
    (0x8F1764, 140, 'cab8753c5994d07c9b278f5f3243b9a789ed988dd0f1d07e3596adcbb098d85b'),
)

def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        stream.seek(0x3C02220 + 0x98 * 8)
        pointer = struct.unpack('<Q', stream.read(8))[0]
        if pointer != 0x7FF74F410000 + 0x891F24:
            raise SystemExit('FAIL: native token98 table pointer changed')
        print('PASS: native token98 table -> RVA 891F24')
        for rva, size, expected in WINDOWS:
            stream.seek(rva)
            actual = hashlib.sha256(stream.read(size)).hexdigest()
            if actual != expected:
                raise SystemExit(f'FAIL: {rva:X} byte provenance changed')
            print(f'PASS: {rva:X} {size} bytes SHA256 {actual}')
    print('5 static provenance checks passed; no runtime/lifetime/safety verdict')

if __name__ == '__main__':
    main()
