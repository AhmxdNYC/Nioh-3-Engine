"""Identity checks for reviewed retirement callback windows, not safety proof."""
import hashlib
from pathlib import Path

WINDOWS = (
    (0x2EEF6D4, 288, '46ca46fe968bb72515acc387d3b356713cb49caf808be3112f332ee24000165f'),
    (0x2EEFEB0, 181, '12da7794a530dd571cf06d9cba533c4e9111471573c5c446b10538ac1dc1a2be'),
    (0x2EEF8F4, 30, '3277d42a7140a7c462656aeee75e4d061d0c23a41397e37f678bef9ef1f47bdf'),
    (0x1A757B8, 276, '4abef14c7eee68082f9fcc1b974496eadd8bc43358a1ad4486d845209bff9898'),
    (0x2EF08B8, 39, '8fc6fa644ea1038aeebdc0627327be8709ee7344fc287f1bd176a4b33a240065'),
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
    print('5 static provenance checks passed; no runtime/lifetime/safety verdict')


if __name__ == '__main__':
    main()
