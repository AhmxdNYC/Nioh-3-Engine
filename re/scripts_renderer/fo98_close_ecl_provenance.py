"""Pin reviewed close/recycle and ECL CPU completion evidence; no safety verdict."""
import hashlib
from pathlib import Path

WINDOWS = (('context close thunk', 27772156, 19, '693c864e34ed74a65a2bd4b3b9d48a4afeff974b706679bad92d04638c5ff0ba'), ('close and recycle wrapper', 4788068, 62, 'ab99d884a19099630b1a6fb9727bc9749e2e1cc0116082b4f9d20346c698a51f'), ('command list close', 4787548, 230, '59037a22a315112ffcae76cdb2628e8361e98f763d649af419ab4c9197ad574f'), ('close token writer', 4787868, 102, '77834713580343d535b9e432445a4465f11f3eb306502604ba3f30d2c4e3d6a6'), ('flush and recycle', 4792544, 317, '218f86fa3d2bc91ee386884948007e2702048d1601649c35d5f1491dced36010'), ('ECL consumer', 27744624, 89, '14b8a879eb5e942230bf7f336555d92cfb08447c9abbf87c02e707a65113deee'), ('ECL replay', 5214292, 73, 'b11d2051539e6257f18794f5780492634035b99d680974a4215978b7eb9f9bff'), ('ECL item done', 2434788, 41, '883047b4f3c5dc377a14bfcf68143367b2df6051805c713c0b01e7ea14cf687c'), ('ECL token table', 62737904, 72, '92a8fa58a2d28f2d284a213082e608c70ef0f7bf757997f0e0cf27ba82e2bf33'), ('ECL token zero', 5214368, 59, '63acf5ae572c2fe29a1d6a0fe329007198cc446518e6fac3259764350a3dbc42'), ('ECL execute token', 5214740, 80, 'b00fd1a02e1663af81aea110a7a2ee0023265eec33a7df490db334bbc2437a9f'), ('ECL signal token', 5214656, 83, '1aa5d7c24a053f6420d08347723df3fed0a355efe6a76f8c31e2a510ca64c423'))


def main():
    dump = Path(__file__).resolve().parents[1] / "Nioh3.dump.exe"
    with dump.open("rb") as stream:
        for name, rva, size, expected in WINDOWS:
            stream.seek(rva)
            if hashlib.sha256(stream.read(size)).hexdigest() != expected:
                raise SystemExit(f"FAIL: {name} {rva:X} byte provenance changed")
            print(f"PASS: {name} {rva:X} {size} bytes")
    print("12 static provenance checks passed; no producer/GPU/safety verdict")


if __name__ == "__main__":
    main()
