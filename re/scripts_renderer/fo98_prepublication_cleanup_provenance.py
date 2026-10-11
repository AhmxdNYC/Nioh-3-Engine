"""Pin reviewed FO98 pre-publication cleanup bytes; no behavioral safety verdict."""
import hashlib
from pathlib import Path

WINDOWS = (('resource set insert', 1210732, 155, '2f0a79e5fbe74fb27ae09c81cc5a2419f790542c8d9811f5a3d086e8d9860bcd'), ('cache invalidation', 1211612, 225, '7cfaa85bfa864d6786fbf191e08010db8483ecb6afe03e9bf09bed00cfa80307'), ('typed record cleanup', 1211212, 397, '2a574899dee0d3f224908bcf98fa026baf8ffbcdb89d36cfc909aad910d10d15'), ('three list traversal', 1211008, 107, '50b670975d9ce0b275b622ba440272ba180b64a5a5390c2ecb9df89235e0df75'), ('empty node cleanup', 1211116, 55, '0fcbc34e9a3ffccbb6f18f99369621fdf02c60cb2dbddfb030a74d53c8e8a940'), ('resource membership', 1212088, 85, '018426edf33ad5b6b06428ebbfbbbe9ac9298662e03da157a1c6118a4f26bcbb'), ('node record scan', 1211840, 171, '8e4bb7c36e4da25017d3d3003d4731dd1bc5f7766ff3a3c96c36a8dcde18ccc3'), ('cache object virtual calls', 4794884, 39, 'ffc8d39420b6f586114ac733b8b515fa9174b6515dd947f1d8799525e278d7b8'))


def main():
    dump = Path(__file__).resolve().parents[1] / 'Nioh3.dump.exe'
    with dump.open('rb') as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f'FAIL: {name} {rva:X} byte provenance changed')
            print(f'PASS: {name} {rva:X} {size} bytes')
    print('8 static provenance checks passed; no runtime/GPU/lifetime verdict')


if __name__ == '__main__':
    main()
