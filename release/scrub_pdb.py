"""Make a distinct PE copy with only RSDS PDB pathnames replaced by bare filenames.

No input/overwrite writes. Validate every record first, preserve length/GUID/age and
all executable bytes, then refuse output containing ASCII/UTF-16 home-path markers.
Non-CodeView debug payloads are opaque and remain unchanged; unsupported CodeView
formats, malformed bounds, overlapping records and paths in executable data fail.
"""
import argparse
import hashlib
import json
import os
import pathlib
import struct
import sys


class ScrubError(ValueError):
    pass


def _need(condition, message):
    if not condition:
        raise ScrubError(message)


def _span(data, offset, size, label):
    _need(offset >= 0 and size >= 0 and offset + size <= len(data), f"invalid {label} bounds")


def _overlap(a, b):
    return a[0] < b[1] and b[0] < a[1]


def parse_pe(data):
    _span(data, 0, 64, "DOS header")
    _need(data[:2] == b"MZ", "DOS signature missing")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    _span(data, pe, 24, "PE header")
    _need(pe >= 64 and data[pe:pe + 4] == b"PE\0\0", "PE signature missing")
    sections_count, optional_size = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
    _need(0 < sections_count <= 96, "unsupported section count")
    opt = pe + 24
    _span(data, opt, optional_size, "optional header")
    _need(optional_size >= 2, "optional header truncated")
    magic = struct.unpack_from("<H", data, opt)[0]
    _need(magic in (0x10B, 0x20B), "unsupported optional header format")
    directory_offset = 96 if magic == 0x10B else 112
    _need(optional_size >= directory_offset + 7 * 8, "debug directory entry missing")
    count = struct.unpack_from("<I", data, opt + directory_offset - 4)[0]
    _need(count >= 7 and directory_offset + count * 8 <= optional_size, "invalid data-directory count")
    headers_size = struct.unpack_from("<I", data, opt + 60)[0]
    table = opt + optional_size
    _span(data, table, sections_count * 40, "section table")
    _need(table + sections_count * 40 <= headers_size <= len(data), "invalid header size")
    sections, executable = [], []
    for i in range(sections_count):
        s = table + i * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, s + 8)
        characteristics = struct.unpack_from("<I", data, s + 36)[0]
        if raw_size:
            _span(data, raw_pointer, raw_size, "section data")
            _need(raw_pointer >= headers_size, "section overlaps headers")
            current = (raw_pointer, raw_pointer + raw_size)
            _need(all(not _overlap(current, (p, p + n)) for _, _, p, n in sections), "overlapping raw sections")
            if characteristics & (0x20000000 | 0x20):  # executable or declared code
                executable.append(current)
        sections.append((virtual_address, virtual_size, raw_pointer, raw_size))

    def mapped(rva, size):
        found = [p + rva - va for va, _, p, n in sections if va <= rva and rva + size <= va + n]
        _need(len(found) == 1, "unmapped or ambiguous debug RVA")
        _span(data, found[0], size, "mapped debug data")
        return found[0]

    debug_rva, debug_size = struct.unpack_from("<II", data, opt + directory_offset + 6 * 8)
    _need(debug_rva != 0 and debug_size > 0 and debug_size % 28 == 0, "missing or malformed debug directory")
    debug_offset = mapped(debug_rva, debug_size)
    debug_range = (debug_offset, debug_offset + debug_size)
    records, payloads = [], []
    for i in range(debug_size // 28):
        _, _, _, _, kind, size, rva, pointer = struct.unpack_from("<IIHHIIII", data, debug_offset + i * 28)
        _need(size > 0 and pointer > 0, "empty debug payload unsupported")
        _span(data, pointer, size, "debug payload")
        if rva:
            _need(mapped(rva, size) == pointer, "debug RVA/raw pointer mismatch")
        current = (pointer, pointer + size)
        _need(pointer >= headers_size and not _overlap(current, debug_range), "debug payload overlaps structural metadata")
        _need(all(not _overlap(current, old) for old in payloads), "overlapping debug payloads")
        payloads.append(current)
        if kind != 2:
            continue
        _need(size >= 25 and data[pointer:pointer + 4] == b"RSDS", "unsupported or truncated CodeView record")
        path_start, path_size = pointer + 24, size - 24
        path_data = data[path_start:path_start + path_size]
        terminator = path_data.find(b"\0")
        _need(terminator >= 0, "unterminated PDB pathname")
        _need(not any(path_data[terminator:]), "nonzero bytes after PDB pathname")
        path = path_data[:terminator]
        basename = path.replace(b"\\", b"/").rsplit(b"/", 1)[-1]
        _need(len(basename) > 4 and basename.lower().endswith(b".pdb") and b":" not in basename,
              "unsupported PDB filename")
        try:
            name = basename.decode("utf-8")
        except UnicodeDecodeError:
            raise ScrubError("unsupported PDB filename encoding") from None
        _need(all(ord(c) >= 32 and c not in '<>"|?*' for c in name), "invalid PDB filename")
        path_range = (path_start, path_start + path_size)
        _need(all(not _overlap(path_range, x) for x in executable), "PDB pathname overlaps executable bytes")
        records.append(dict(offset=path_start, size=path_size, filename=name, basename=basename,
                            guid=data[pointer + 4:pointer + 20].hex(), age=struct.unpack_from("<I", data, pointer + 20)[0]))
    _need(bool(records), "no supported RSDS PDB record")
    return records, executable


def home_path_markers(data):
    lowered = data.lower()
    markers = ("\\users\\", "/users/", "/home/", "\\documents and settings\\")
    return any(m.encode(encoding) in lowered for m in markers for encoding in ("ascii", "utf-16-le", "utf-16-be"))


def scrub_bytes(data):
    original = bytes(data)
    records, executable = parse_pe(original)
    output = bytearray(original)
    for record in records:
        start, size, name = record["offset"], record["size"], record["basename"]
        output[start:start + size] = name + b"\0" * (size - len(name))
    result = bytes(output)
    _need(len(result) == len(original), "file length changed")
    cursor, changed = 0, 0
    for record in sorted(records, key=lambda r: r["offset"]):
        start, size = record["offset"], record["size"]
        _need(original[cursor:start] == result[cursor:start], "non-path metadata changed")
        changed += sum(a != b for a, b in zip(original[start:start + size], result[start:start + size]))
        cursor = start + size
    _need(original[cursor:] == result[cursor:], "non-path bytes changed")
    _need(all(original[a:b] == result[a:b] for a, b in executable), "executable bytes changed")
    parsed_after, _ = parse_pe(result)
    _need([(r["guid"], r["age"], r["filename"]) for r in records] ==
          [(r["guid"], r["age"], r["filename"]) for r in parsed_after], "CodeView identity changed")
    _need(not home_path_markers(result), "home-path marker remains outside scrubbed metadata")
    proof = dict(input_sha256=hashlib.sha256(original).hexdigest(), output_sha256=hashlib.sha256(result).hexdigest(),
                 bytes=len(result), changed_bytes=changed, only_pdb_path_bytes_changed=True,
                 executable_bytes_identical=True, executable_bytes=sum(b - a for a, b in executable),
                 home_path_markers_remaining=False,
                 records=[{k: v for k, v in r.items() if k != "basename"} for r in records])
    return result, proof


def scrub_file(source, destination):
    source, destination = pathlib.Path(source), pathlib.Path(destination)
    try:
        source_resolved, destination_resolved = source.resolve(), destination.resolve()
    except (OSError, RuntimeError):
        raise ScrubError("cannot resolve input/output paths") from None
    _need(os.path.normcase(str(source_resolved)) != os.path.normcase(str(destination_resolved)), "distinct output required")
    _need(not destination.exists() and not destination.is_symlink(), "output already exists; overwrite refused")
    original = source.read_bytes()
    result, proof = scrub_bytes(original)
    created = False
    try:
        with destination.open("xb") as f:
            created = True
            _need(f.write(result) == len(result), "incomplete output write")
        _need(destination.read_bytes() == result, "output readback mismatch")
    except BaseException:
        if created:
            destination.unlink()
        raise
    return proof


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path, help="distinct new output file; parent directory must exist")
    args = parser.parse_args(argv)
    try:
        proof = scrub_file(args.input, args.output)
    except ScrubError as exc:
        print("ERROR:", exc, file=sys.stderr)
        return 1
    except OSError as exc:
        print("ERROR: filesystem operation failed (" + type(exc).__name__ + ")", file=sys.stderr)
        return 1
    print(json.dumps(proof, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
