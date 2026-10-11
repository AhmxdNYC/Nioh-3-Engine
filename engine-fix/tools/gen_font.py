# gen_font.py - rasterize ASCII 32..126 of a TrueType font into 8x16 cells with 4-bit coverage and write
# src/osd_font.h for the in-game overlay (osd.cpp). Run once on Windows (uses GDI through ctypes); the header is
# checked in, so build.bat does not need this script.
#
#   py -I tools\gen_font.py <font.ttf> "<face name>" [emPx=-14] [weight=700] [--preview]
#
# The checked-in header was made from Inconsolata (SIL Open Font License 1.1, (c) The Inconsolata Project Authors):
#   py -I tools\gen_font.py Inconsolata-Regular.ttf Inconsolata -15 400
import ctypes
import ctypes.wintypes as wt
import os
import sys

W, H = 8, 16
FIRST, LAST = 32, 126

gdi = ctypes.WinDLL("gdi32")
user = ctypes.WinDLL("user32")


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]


class TEXTMETRICW(ctypes.Structure):
    _fields_ = [("tmHeight", wt.LONG), ("tmAscent", wt.LONG), ("tmDescent", wt.LONG),
                ("tmInternalLeading", wt.LONG), ("tmExternalLeading", wt.LONG), ("tmAveCharWidth", wt.LONG),
                ("tmMaxCharWidth", wt.LONG), ("tmWeight", wt.LONG), ("tmOverhang", wt.LONG),
                ("tmDigitizedAspectX", wt.LONG), ("tmDigitizedAspectY", wt.LONG), ("tmFirstChar", wt.WCHAR),
                ("tmLastChar", wt.WCHAR), ("tmDefaultChar", wt.WCHAR), ("tmBreakChar", wt.WCHAR),
                ("tmItalic", ctypes.c_byte), ("tmUnderlined", ctypes.c_byte), ("tmStruckOut", ctypes.c_byte),
                ("tmPitchAndFamily", ctypes.c_byte), ("tmCharSet", ctypes.c_byte)]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    preview = "--preview" in sys.argv
    path, face = args[0], args[1]
    em = int(args[2]) if len(args) > 2 else -14
    weight = int(args[3]) if len(args) > 3 else 700
    gdi.AddFontResourceExW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.LPVOID]
    if gdi.AddFontResourceExW(os.path.abspath(path), 0x10, None) == 0:
        sys.exit("AddFontResourceExW failed")
    gdi.CreateFontW.restype = wt.HANDLE
    gdi.CreateFontW.argtypes = [ctypes.c_int] * 5 + [wt.DWORD] * 8 + [wt.LPCWSTR]
    font = gdi.CreateFontW(em, 0, 0, 0, weight, 0, 0, 0, 0, 7, 0, 4, 1 | 0x30, face)  # OUT_TT_ONLY, ANTIALIASED
    gdi.CreateCompatibleDC.restype = wt.HANDLE
    gdi.CreateCompatibleDC.argtypes = [wt.HANDLE]
    dc = gdi.CreateCompatibleDC(None)
    gdi.SelectObject.restype = wt.HANDLE
    gdi.SelectObject.argtypes = [wt.HANDLE, wt.HANDLE]
    BW, BH = 32, 32
    bi = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), BW, -BH, 1, 32, 0, 0, 0, 0, 0, 0)
    bits = ctypes.c_void_p()
    gdi.CreateDIBSection.restype = wt.HANDLE
    gdi.CreateDIBSection.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.UINT, ctypes.POINTER(ctypes.c_void_p), wt.HANDLE, wt.DWORD]
    bmp = gdi.CreateDIBSection(dc, ctypes.byref(bi), 0, ctypes.byref(bits), None, 0)
    gdi.SelectObject(dc, bmp)
    gdi.SelectObject(dc, font)
    tm = TEXTMETRICW()
    gdi.GetTextMetricsW.argtypes = [wt.HANDLE, ctypes.POINTER(TEXTMETRICW)]
    gdi.GetTextMetricsW(dc, ctypes.byref(tm))
    face_buf = ctypes.create_unicode_buffer(64)
    gdi.GetTextFaceW.argtypes = [wt.HANDLE, ctypes.c_int, wt.LPWSTR]
    gdi.GetTextFaceW(dc, 64, face_buf)
    print(f"face={face_buf.value} height={tm.tmHeight} ascent={tm.tmAscent} descent={tm.tmDescent} avgW={tm.tmAveCharWidth} maxW={tm.tmMaxCharWidth}")
    gdi.SetTextColor.argtypes = [wt.HANDLE, wt.DWORD]
    gdi.SetBkColor.argtypes = [wt.HANDLE, wt.DWORD]
    gdi.SetTextColor(dc, 0xFFFFFF)
    gdi.SetBkColor(dc, 0)
    gdi.TextOutW.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_int, wt.LPCWSTR, ctypes.c_int]
    gdi.PatBlt.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.DWORD]
    # vertical placement: baseline at row 12 of 16 (ascent above, descent below)
    base = 12
    oy = base - tm.tmAscent
    ox = (W - tm.tmAveCharWidth) // 2
    buf = (ctypes.c_uint32 * (BW * BH)).from_address(bits.value)
    glyphs = []
    for c in range(FIRST, LAST + 1):
        gdi.PatBlt(dc, 0, 0, BW, BH, 0x00000042)  # BLACKNESS
        gdi.TextOutW(dc, 8 + ox, 8 + oy, chr(c), 1)
        g = []
        for y in range(H):
            row = []
            for x in range(W):
                v = (buf[(8 + y) * BW + 8 + x] >> 8) & 0xFF
                row.append(min(15, (v * 15 + 127) // 255))
            g.append(row)
        # report clipped ink outside the cell
        clipped = 0
        for y in range(BH):
            for x in range(BW):
                if not (8 <= x < 8 + W and 8 <= y < 8 + H) and (buf[y * BW + x] >> 8) & 0xFF > 64:
                    clipped += 1
        if clipped:
            print(f"warning: '{chr(c)}' has {clipped} px outside the {W}x{H} cell")
        glyphs.append(g)
    if preview:
        ramp = " .:-=+*#%@@@@@@@"
        for i, g in enumerate(glyphs):
            print(f"--- '{chr(FIRST + i)}'")
            for row in g:
                print("".join(ramp[v] for v in row))
    # pack: per glyph 16 rows x 8 nibbles = one uint32 per row (pixel x at bits 4x..4x+3)
    words = []
    for g in glyphs:
        for row in g:
            w = 0
            for x, v in enumerate(row):
                w |= v << (4 * x)
            words.append(w)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "osd_font.h")
    with open(out, "w", newline="\n") as f:
        f.write("// Generated by tools/gen_font.py - do not edit.\n")
        f.write(f"// {face_buf.value} {em}px weight {weight}, ASCII {FIRST}..{LAST}, {W}x{H} cells, 4-bit coverage;\n")
        f.write("// one uint32 per glyph row, pixel x in bits 4x..4x+3.\n")
        f.write("// Glyph shapes derived from Inconsolata, (c) The Inconsolata Project Authors, SIL Open Font License 1.1\n")
        f.write("// (https://openfontlicense.org). The bitmap font itself is licensed under the OFL 1.1.\n")
        f.write("#pragma once\n#include <cstdint>\n")
        f.write(f"namespace osdfont {{\nconstexpr int kW = {W}, kH = {H}, kFirst = {FIRST}, kLast = {LAST};\n")
        f.write(f"constexpr int kWords = {len(words)};\nalignas(16) static const uint32_t kBits[kWords] = {{\n")
        for i in range(0, len(words), 8):
            f.write("    " + ", ".join(f"0x{w:08X}" for w in words[i:i + 8]) + ",\n")
        f.write("};\n}  // namespace osdfont\n")
    print("wrote", os.path.normpath(out), len(words), "words")


if __name__ == "__main__":
    main()
