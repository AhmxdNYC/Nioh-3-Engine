"""Minimal reader for AMD RDF containers (".rgp" files written by the 2024+ Radeon
Developer Panel / driver are RDF: magic "AMD_RDF ", chunk index at the end).

Layout (verified against RDTS 2026-05-28 sample_profile.rgp):
  header : char[8] "AMD_RDF " | u32 version(3) | u32 reserved | i64 indexOffset | i64 indexSize
  index  : N x 64-byte entries
           char[16] id | u8 compression(0=none,1=zstd) | u8[3] pad | u32 chunkVersion |
           i64 hdrOffset | i64 hdrSize | i64 dataOffset | i64 dataSize | i64 uncompressedSize
Stdlib only (zstd via Python 3.14 `compression.zstd` if a chunk is compressed).
"""
from __future__ import annotations
import struct
from dataclasses import dataclass


@dataclass
class Chunk:
    id: str
    index: int          # occurrence index among chunks with the same id
    version: int
    compression: int
    hdr_off: int
    hdr_size: int
    data_off: int
    data_size: int
    uncomp_size: int


class RDF:
    def __init__(self, path: str):
        import mmap
        self._f = open(path, "rb")
        self.buf = mmap.mmap(self._f.fileno(), 0, access=mmap.ACCESS_READ)  # no full copy in RAM
        b = self.buf
        if b[:8] != b"AMD_RDF ":
            raise ValueError(f"{path}: not an RDF file (magic={b[:8]!r}); legacy SQTT-file .rgp not supported")
        self.version, _, io, isz = struct.unpack_from("<IIqq", b, 8)
        self.chunks: list[Chunk] = []
        counts: dict[str, int] = {}
        for i in range(isz // 64):
            cid, comp, ver, ho, hs, do, ds, us = struct.unpack_from("<16sB3xIqqqqq", b, io + i * 64)
            name = cid.rstrip(b"\0").decode("ascii", "replace")
            n = counts.get(name, 0)
            counts[name] = n + 1
            self.chunks.append(Chunk(name, n, ver, comp, ho, hs, do, ds, us))

    def find(self, name: str) -> list[Chunk]:
        return [c for c in self.chunks if c.id == name]

    def header(self, c: Chunk) -> bytes:
        return self.buf[c.hdr_off:c.hdr_off + c.hdr_size]

    def data(self, c: Chunk) -> bytes:
        raw = self.buf[c.data_off:c.data_off + c.data_size]
        if c.compression == 0:
            return raw
        if c.compression == 1:
            from compression import zstd  # Python 3.14+
            return zstd.decompress(raw)
        raise ValueError(f"unknown compression {c.compression} in chunk {c.id}")

    def summary(self) -> str:
        agg: dict[str, list[int]] = {}
        for c in self.chunks:
            a = agg.setdefault(c.id, [0, 0, c.version])
            a[0] += 1
            a[1] += c.data_size
        return "\n".join(f"{k:20s} x{v[0]:<5d} {v[1]:>12d} B  v{v[2]}" for k, v in agg.items())


if __name__ == "__main__":
    import sys
    print(RDF(sys.argv[1]).summary())
