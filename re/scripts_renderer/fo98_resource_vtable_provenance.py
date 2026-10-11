"""Pin reviewed concrete FO98 resource edges; provenance only, not safety."""
import hashlib
from pathlib import Path

WINDOWS = (('resource constructor vtable assignment', 2444211, 13, '74bb939e537c537d14d7f8b52081caa51dba5ae24590dc59560984249c6406c9'), ('allocation record sixth argument A', 2423749, 139, 'b013a2945b7d4b59b81b1a666b0398204363a1c0fd0c3ccf27a31bbd41d03204'), ('allocation record sixth argument B', 6470372, 142, '802ac12fbd70538bf766a94e453ad27e8b1efb4c8c7fbadfbe2e18cd92e58f28'), ('resource vtable B0 B8 slots', 59684112, 16, '96629d2b4a26ff41eda9319bca67bd3e799b790cc33cefd277634382c7b99d98'), ('resource accounting callback', 1210952, 56, 'a92e50d79c5282fdf58e37a047239409078f985d09df435d6e3bd671b12f93d0'), ('resource pointer predicate', 1214520, 12, 'e26bc870350acf60ba36ef87d2b1ec81a99d3ba1c32e9701f7d2a9b26d6dd80e'), ('resource vtable 28 slot', 59683976, 8, 'fbd6e8d6df367c36d886ae8df875280987c2569b6ee68ed9b43cdc142990c00a'), ('resource 98 emitter', 49310388, 167, '31b68063354eb3c623c94fdbf1a56d0d9bf06c0d2f97883e614fae7cf8f29687'), ('bitmap record removal', 4787012, 59, '3a27531ec0e28eb80d5e52379d19524a9bcd613c7201bbfe1397a4e0523c2b1f'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {name} {rva:X} byte provenance changed')
            print(f'PASS: {name} {rva:X} {size} bytes')
    print('9 static provenance checks passed; no runtime/GPU/lifetime verdict')


if __name__ == '__main__':
    main()
