"""Pin reviewed live-map ownership windows; not runtime/GPU safety proof."""
import hashlib
from pathlib import Path

WINDOWS = ((2430560, 176, '26e03c2287fbf86eab5340f135ff68a31c64807df8201db0598ccd6f1dba3f3d'), (2427496, 79, '9b3f998bf51693f1df2fe9834aa44e95b1529341c672f5f48612a826f013f435'), (49299120, 57, '8cbb1b166c4b227cc1b213ccda229b332b8fd0016d6aacd1c1f50d90493094ad'))

def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {rva:X} byte provenance changed')
            print(f'PASS: {rva:X} {size} bytes')
    print('3 static provenance checks passed; no runtime/GPU/lifetime verdict')

if __name__ == '__main__':
    main()
