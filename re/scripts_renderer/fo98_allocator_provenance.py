"""Pin reviewed allocator submit/reuse provenance; no FO safety verdict."""
import hashlib
from pathlib import Path

WINDOWS = (('context allocator construction', 49260608, 89, '60f8dc2052159957f372d7a5488a3b4d645feed58ef4f1e515339ddc52ada226'), ('allocator field initialization', 49280807, 35, '29c9772ae5ea77cca7bf62b26ec6ca7d4d8230e8f70b4a1df85d9feb2caf159b'), ('allocator vtable', 67741216, 56, 'c712eea974baef6954a13d91b95b875511e686ea02037850a86535180a4acb05'), ('reuse check and cold branch', 4792952, 96, '3b2670d2f1f8130353c63179469ac5857cd6ad7835104ea6fe06e3e252f07ecf'), ('reuse wait cold tail', 26937746, 21, 'dd00f414b229a68159bfab3a6d7d0e309a5b244a1d2032e5bcd2a0f7301aa607'), ('command list reset', 4792864, 88, '8f3749270dad5d7c1bb5225655a83cf99fdc5b7166425a477c2d5b7720167cda'), ('list submit and context fence signal', 5214428, 91, 'fdfa4a63435addfa9244bf525095f8db0a11463d3fc08ad38386031d05b1bc0b'), ('next recording sequence', 4787792, 15, '115260c06b8595eaf37c8b142de5507b43c4c945de46febd60686891f64cbb02'), ('context completion cache update', 10264304, 59, '2b3bef37d041069bfe68350dfe162c10e9cb5b187d640879fc75b777b6c204c5'), ('graphics fence wait', 8232476, 113, '6b940e8a3d903d0fb739dde6895e745e5db6442d50bc427536f5fca3b8ce8042'))

def main():
    dump = Path(__file__).resolve().parents[1] / "Nioh3.dump.exe"
    with dump.open("rb") as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f"FAIL: {name} {rva:X} byte provenance changed")
            print(f"PASS: {name} {rva:X} {size} bytes")
    print("10 static provenance checks passed; no retirement/producer/safety verdict")

if __name__ == "__main__":
    main()
