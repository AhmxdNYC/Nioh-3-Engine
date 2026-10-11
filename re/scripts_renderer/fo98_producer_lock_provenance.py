"""Pin reviewed FO98 mode-selected resource helper edges; provenance only, not safety."""
import hashlib
from pathlib import Path

WINDOWS = (('mode selected resource acquire', 1224912, 43, '71d32cf9155ca1525e758eb105d207c11d6c0be473e3bc6644b8af84f6d08e5d'), ('mode selected resource release', 1224512, 43, 'cd5b8d45b2a8ff70ba2bba5b0ac94857e39bb1ead54a03488e922029f6cba4f9'), ('resource bitmap update', 1387848, 432, '61c929404eff276b66856526ab91b67f20c2957428cea1e0b6c886ca8c5192b9'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {name} {rva:X} byte provenance changed')
            print(f'PASS: {name} {rva:X} {size} bytes')
    print('3 static provenance checks passed; no runtime/GPU/lifetime verdict')


if __name__ == '__main__':
    main()
