"""Pin reviewed retirement work-publication windows; no behavioral safety verdict."""
import hashlib
from pathlib import Path

WINDOWS = ((49219896, 474, '23de208d6abb185dd610296db0b1bd9d06cd968c50b07be8abdaa3371b6f6fca'), (2426448, 446, 'e31d98f2ee51bd3c2a2ad60cddf88020b23c3eb1dd73722cd6569a3b2bcc8c70'), (2433164, 120, 'eb7cce87b087c5492ddf58cf8c55a006745ad63587f46a1889731b6192253c44'), (1206184, 130, '4e75df56e5a55b585a8650ef679147cdcc8e0427328a3d4226cbcb06dace0d1a'), (1210584, 146, 'd30ced8e5b3f081dd83cb7f89c34cd1f598b8db094c919667db86f5d9ba9645d'), (27730048, 29, 'bfc88419d009a6a2407c0423cc8c375666e6e48b21d886abc91f76e59b743916'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {rva:X} byte provenance changed')
            print(f'PASS: {rva:X} {size} bytes')
    print('6 static provenance checks passed; no runtime/GPU/lifetime verdict')


if __name__ == '__main__':
    main()
