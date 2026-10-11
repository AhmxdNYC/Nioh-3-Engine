"""Pin reviewed retirement-ring rebuild windows; no runtime/GPU safety verdict."""
import hashlib
from pathlib import Path

WINDOWS = ((49214384, 144, '5660404c335cd88bf0fea22a1bffb4a8eb026c9cc0c8544950ef0121dd6eced2'), (49210128, 147, '4cc865425ad512267d9e4e836978d719d8ae2f349be622c3f1f46ea213912442'), (49218036, 418, 'c397f09627c48abef590bd4e2c47acf04b1bfb4ea235ac1fbb2a251026b0a9d0'), (49215824, 44, 'd0fe9f1829b45dedf954e073d4d255b3d29f6262734b2f4bb3296a12f8e5d4e7'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {rva:X} byte provenance changed')
            print(f'PASS: {rva:X} {size} bytes')
    print('4 static provenance checks passed; no runtime/GPU/lifetime verdict')


if __name__ == '__main__':
    main()
