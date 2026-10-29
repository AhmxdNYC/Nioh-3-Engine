"""Focused PE privacy/malformed/nonoverwrite proof; standard library only."""
import pathlib
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import scrub_pdb


def fixture(pe32=False, path=b"C:\\Users\\Test Person\\build\\EngineFix.pdb"):
    b = bytearray(0x800)
    b[:2] = b"MZ"; struct.pack_into("<I", b, 0x3C, 0x80)
    b[0x80:0x84] = b"PE\0\0"
    osize = 0xE0 if pe32 else 0xF0
    struct.pack_into("<H", b, 0x84, 0x14C if pe32 else 0x8664)
    struct.pack_into("<H", b, 0x86, 2); struct.pack_into("<H", b, 0x94, osize)
    opt, directory = 0x98, 96 if pe32 else 112
    struct.pack_into("<H", b, opt, 0x10B if pe32 else 0x20B)
    struct.pack_into("<I", b, opt + 60, 0x200)
    struct.pack_into("<I", b, opt + directory - 4, 16)
    struct.pack_into("<II", b, opt + directory + 48, 0x2000, 28)
    for i, (name, va, rp, size, flags) in enumerate(((b".text", 0x1000, 0x200, 0x200, 0x60000020),
                                                   (b".rdata", 0x2000, 0x400, 0x400, 0x40000040))):
        s = opt + osize + i * 40; b[s:s + 8] = name.ljust(8, b"\0")
        struct.pack_into("<IIII", b, s + 8, size, va, size, rp)
        struct.pack_into("<I", b, s + 36, flags)
    b[0x200:0x400] = bytes(range(256)) * 2
    payload = b"RSDS" + bytes(range(16)) + struct.pack("<I", 7) + path + b"\0"
    b[0x440:0x440 + len(payload)] = payload
    struct.pack_into("<IIHHIIII", b, 0x400, 0, 123, 0, 0, 2, len(payload), 0x2040, 0x440)
    return b


class ScrubTests(unittest.TestCase):
    def test_metadata_only_pe32_and_pe64(self):
        for pe32 in (False, True):
            with self.subTest(pe32=pe32):
                raw = bytes(fixture(pe32)); out, proof = scrub_pdb.scrub_bytes(raw)
                record = proof["records"][0]; start, size = record["offset"], record["size"]
                self.assertEqual(len(raw), len(out))
                self.assertEqual(raw[:start], out[:start]); self.assertEqual(raw[start + size:], out[start + size:])
                self.assertEqual(raw[0x200:0x400], out[0x200:0x400])
                self.assertEqual(out[start:start + size], b"EngineFix.pdb" + b"\0" * (size - 13))
                self.assertEqual(record["guid"], bytes(range(16)).hex()); self.assertEqual(record["age"], 7)
                self.assertTrue(proof["only_pdb_path_bytes_changed"]); self.assertTrue(proof["executable_bytes_identical"])
                self.assertFalse(scrub_pdb.home_path_markers(out))

    def test_bare_name_idempotent_and_utf8_filename(self):
        for path in (b"EngineFix.pdb", "D:/build/engine-é.pdb".encode()):
            first, _ = scrub_pdb.scrub_bytes(fixture(path=path)); second, proof = scrub_pdb.scrub_bytes(first)
            self.assertEqual(first, second); self.assertEqual(proof["changed_bytes"], 0)

    def test_multiple_records_and_opaque_payload_preserved(self):
        b = fixture(); struct.pack_into("<II", b, 0x98 + 112 + 48, 0x2000, 84)
        # Relocate first payload because the expanded directory would overlap its old location.
        size = struct.unpack_from("<I", b, 0x410)[0]; old = bytes(b[0x440:0x440 + size])
        b[0x440:0x440 + size] = b"\0" * size; b[0x480:0x480 + size] = old
        struct.pack_into("<II", b, 0x414, 0x2080, 0x480)
        second = b"RSDS" + b"Q" * 16 + struct.pack("<I", 9) + b"D:/build/Other.pdb\0"
        b[0x540:0x540 + len(second)] = second
        struct.pack_into("<IIHHIIII", b, 0x41C, 0, 0, 0, 0, 2, len(second), 0x2140, 0x540)
        b[0x600:0x610] = b"opaque-metadata!"
        struct.pack_into("<IIHHIIII", b, 0x438, 0, 0, 0, 0, 12, 16, 0x2200, 0x600)
        out, proof = scrub_pdb.scrub_bytes(b)
        self.assertEqual(len(proof["records"]), 2); self.assertEqual(out[0x600:0x610], bytes(b[0x600:0x610]))

    def test_malformed_records_never_mutate_input(self):
        cases = []
        def case(name, change):
            b = fixture(); change(b); cases.append((name, b))
        case("DOS", lambda b: b.__setitem__(slice(0, 2), b"NO"))
        case("PE", lambda b: b.__setitem__(slice(0x80, 0x84), b"NOPE"))
        case("magic", lambda b: struct.pack_into("<H", b, 0x98, 0))
        case("directories", lambda b: struct.pack_into("<I", b, 0x98 + 108, 0xFFFF))
        case("directory-size", lambda b: struct.pack_into("<I", b, 0x98 + 112 + 52, 27))
        case("unmapped-directory", lambda b: struct.pack_into("<I", b, 0x98 + 112 + 48, 0x9000))
        case("payload-bounds", lambda b: struct.pack_into("<I", b, 0x418, 0x7F0))
        case("RVA-mismatch", lambda b: struct.pack_into("<I", b, 0x414, 0x2050))
        case("CV-NB10", lambda b: b.__setitem__(slice(0x440, 0x444), b"NB10"))
        case("short-RSDS", lambda b: struct.pack_into("<I", b, 0x410, 24))
        case("no-terminator", lambda b: b.__setitem__(0x440 + struct.unpack_from("<I", b, 0x410)[0] - 1, 65))
        case("no-CodeView", lambda b: struct.pack_into("<I", b, 0x40C, 12))
        for path in (b"C:/build/", b"C:engine.pdb", b"C:/bad/name.exe", b"C:/bad/name\x01.pdb", b"C:/bad/\xff.pdb", b"C:/build/A.pdb\0hidden"):
            cases.append(("invalid-path", fixture(path=path)))
        cases.append(("truncated", fixture()[:0x450]))
        for name, b in cases:
            with self.subTest(name=name):
                before = bytes(b)
                with self.assertRaises(scrub_pdb.ScrubError): scrub_pdb.scrub_bytes(b)
                self.assertEqual(bytes(b), before)

    def test_overlapping_payloads_and_executable_path_refused(self):
        b = fixture(); struct.pack_into("<I", b, 0x98 + 112 + 52, 56)
        b[0x41C:0x438] = b[0x400:0x41C]
        with self.assertRaises(scrub_pdb.ScrubError): scrub_pdb.scrub_bytes(b)
        b = fixture(); size = struct.unpack_from("<I", b, 0x410)[0]
        b[0x240:0x240 + size] = b[0x440:0x440 + size]
        struct.pack_into("<II", b, 0x414, 0x1040, 0x240)
        with self.assertRaisesRegex(scrub_pdb.ScrubError, "executable"): scrub_pdb.scrub_bytes(b)
        struct.pack_into("<I", b, 0x98 + 0xF0 + 36, 0x40000020)  # code without executable flag remains protected
        with self.assertRaisesRegex(scrub_pdb.ScrubError, "executable"): scrub_pdb.scrub_bytes(b)

    def test_home_path_outside_metadata_refused_ascii_and_utf16(self):
        for text, encoding in (("C:\\Users\\Hidden\\secret", "ascii"), ("/home/hidden/secret", "utf-16-le"),
                               ("/Users/hidden/secret", "utf-16-be")):
            b = fixture(); secret = text.encode(encoding); b[0x700:0x700 + len(secret)] = secret
            with self.assertRaisesRegex(scrub_pdb.ScrubError, "home-path"): scrub_pdb.scrub_bytes(b)

    def test_files_nonoverwrite_failure_and_valid_output(self):
        with tempfile.TemporaryDirectory() as temp:
            source, output = pathlib.Path(temp) / "raw.asi", pathlib.Path(temp) / "copy.asi"
            raw = bytes(fixture()); source.write_bytes(raw)
            with self.assertRaisesRegex(scrub_pdb.ScrubError, "distinct"): scrub_pdb.scrub_file(source, source)
            output.write_bytes(b"keep")
            with self.assertRaisesRegex(scrub_pdb.ScrubError, "overwrite"): scrub_pdb.scrub_file(source, output)
            self.assertEqual(output.read_bytes(), b"keep"); self.assertEqual(source.read_bytes(), raw)
            output.unlink(); source.write_bytes(b"malformed")
            with self.assertRaises(scrub_pdb.ScrubError): scrub_pdb.scrub_file(source, output)
            self.assertFalse(output.exists()); self.assertEqual(source.read_bytes(), b"malformed")
            source.write_bytes(raw); proof = scrub_pdb.scrub_file(source, output)
            self.assertEqual(source.read_bytes(), raw); self.assertEqual(output.stat().st_size, source.stat().st_size)
            self.assertFalse(scrub_pdb.home_path_markers(output.read_bytes())); self.assertTrue(proof["executable_bytes_identical"])

    def test_resolution_failure_omits_private_error_path(self):
        with mock.patch.object(pathlib.Path, "resolve", side_effect=RuntimeError("private home pathname")):
            with self.assertRaisesRegex(scrub_pdb.ScrubError, "cannot resolve") as caught:
                scrub_pdb.scrub_file("raw.asi", "copy.asi")
            self.assertNotIn("private home pathname", str(caught.exception))


if __name__ == "__main__":
    unittest.main(verbosity=2)
