"""Identity checks for reviewed pool-return windows; not lifecycle proof."""
import hashlib
from pathlib import Path

WINDOWS = ((2431836, 41, '8c8faac65d8db60f87e4b2ed0e4bef833fad4ec9f8909163197c9a03142bc940'), (2432180, 87, '9f8169b7043735ecffffeba5b4d2e5e870b0a61105442f35b268e5d268d23120'), (2431880, 300, '3d501ed2c5ce12bb517301200f858a71973f53ff8e2c2e9065ce033f45e382a7'), (26659390, 23, '4c2838f62c7e01523b2fd06f8538fb6602fc6518dbf549583b72369041bf2912'), (2432852, 52, '0caa998ee78dd31d2bc4ce374b10de779bcd450db0f5cab9ef62827e03290f62'), (2432904, 68, 'e0541f4cb8e3355ca9947934e59c7603191f29cacb76a6c879d7e62ac9acccf2'), (2432740, 112, '291ad165d851d8449ff3beb573ed8e54899bdb9dfa1d88cb592046201c01b674'), (5450852, 35, 'af539a7fae8af3fbd532ea8ea22995b6d3fe7d2ab827bbe0b43f79f715024d79'), (2427140, 88, 'b71982cb6ef4d3853a0595bd0ef4eec227917ec6448710b62c3ccc73f5f8896f'))

def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {rva:X} byte provenance changed')
            print(f'PASS: {rva:X} {size} bytes')
    print('9 static provenance checks passed; no runtime/lifetime/safety verdict')

if __name__ == '__main__':
    main()
