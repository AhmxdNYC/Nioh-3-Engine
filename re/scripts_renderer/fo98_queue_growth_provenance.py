"""Verify reviewed queue-growth windows; identity checks, not lifetime proof."""
import hashlib
from pathlib import Path

WINDOWS = (
    (0x2EEF3B0, 144, '5660404c335cd88bf0fea22a1bffb4a8eb026c9cc0c8544950ef0121dd6eced2'),
    (0x2EEE310, 147, '4cc865425ad512267d9e4e836978d719d8ae2f349be622c3f1f46ea213912442'),
    (0x2EF08B8, 140, 'c41177fa7c3ae27f24190c65ef53a77066d0b65c39642c283190b77150b1507e'),
    (0x2EF01F4, 422, '2060a08668679071d0420e50f4c4ba413534286062dae5db69b20827044d57fc'),
    (0x2EEF950, 44, 'd0fe9f1829b45dedf954e073d4d255b3d29f6262734b2f4bb3296a12f8e5d4e7'),
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
