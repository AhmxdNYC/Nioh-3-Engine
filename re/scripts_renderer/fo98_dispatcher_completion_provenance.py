"""Pin reviewed dispatcher completion ordering; static provenance only."""
import hashlib
from pathlib import Path

WINDOWS = (('dispatcher consume/replay/finish', 27771872, 232, 'e47f60e9609e48bac726ad7c95cfb56ca9a60c5330b4c5983f395ae6413b7907'), ('close wrapper', 4808824, 38, '0255236bfb8756779f71062180bb530d9446508d5ae78d854ebd7100585f4c66'), ('submit through target', 4796616, 224, 'daf3085cc615e4933c16bf76f53b4873dfbdb33459a1ec5680c0c875982e14a4'), ('ECL enqueue wrapper', 8845152, 111, '1cef2d3327d4637a502bcf094e39df3cb2958582d95ce2bd1e7c74c7161f2060'), ('ECL enqueue hot body', 8845352, 140, '51ebb7aef5f67d3e3fdce7008322c6c93586ccd31c596d1dd297d32104288010'), ('ECL capacity wait cold tail', 26352176, 41, '5f4aac5771dfd14b6bd4003bd4a2ebe7ef3d4d70f04b8f128078a195efbc32a2'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {name} {rva:X} byte provenance changed')
            print(f'PASS: {name} {rva:X} {size} bytes')
    print('6 static provenance checks passed; no producer/GPU/safety verdict')


if __name__ == '__main__':
    main()
