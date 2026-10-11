"""Pin reviewed native FO98 end-frame drain/reset boundary; provenance only."""
import hashlib
from pathlib import Path

WINDOWS = (('final publication and reset', 4295792, 229, '49904ab34d8724ade5ef486e67d6d1ed17740ec4254f5796c88995caebad2990'), ('dispatcher drain loop', 4296512, 73, '27f11fe508e2b64e3d0bde6e60e76f9ac52c9b16e4a31e0215bbdcaae1145023'), ('dispatcher busy predicate', 4296592, 20, 'd320d5923eba0de51b65d9fdccf55bac68d3065f5798a5060d66b864a17dc29f'), ('stream pending predicate', 4296744, 17, '9418eb5a7cc11f934737358c177e4ff6d97290928a5570eada26e24d109ffef2'), ('stream pending cold tail', 26896164, 13, '816ddf7f42126a1e94d0409adfaa4310a2a0ff60089740fa2d15f3587465be9b'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {name} {rva:X} byte provenance changed')
            print(f'PASS: {name} {rva:X} {size} bytes')
    print('5 static provenance checks passed; no producer/GPU/safety verdict')


if __name__ == '__main__':
    main()
