"""Verify reviewed native removal windows; static identity, not lifetime proof."""
import hashlib
from pathlib import Path

WINDOWS = (
    (0x252104, 172, '4524d9c95f658399a9dd821aa33df549ba783f289b3eb6e8ae8c047dbb4b7663'),
    (0x196CA82, 66, '145eff54d734b932b3eb83474d6035a40c6f9e81af124d7cb0e4cf2a5df78a7f'),
    (0x250ADC, 282, '1334c565cfa43000162a51a61838264c4860706aebfb3d776d9aefc5822becd2'),
    (0x25208C, 120, 'eb7cce87b087c5492ddf58cf8c55a006745ad63587f46a1889731b6192253c44'),
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
    print('4 static provenance checks passed; no runtime/lifetime/safety verdict')


if __name__ == '__main__':
    main()
