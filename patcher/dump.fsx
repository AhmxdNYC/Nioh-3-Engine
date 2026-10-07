// Snapshot the running (SteamStub-decrypted) Nioh3 image for static analysis ONLY.
// Output is a memory-layout PE: raw offsets == RVAs, ImageBase == runtime base, so
// Ghidra addresses equal live addresses. It is not runnable and is never redistributed.
#load "Mem.fsx"
open System
open System.IO
open Mem

let outPath = fsi.CommandLineArgs.[1]
let t = attach "Nioh3"
let size = t.ModuleSize
printfn "base=%X size=%X" t.Base size
let img = Array.zeroCreate<byte> size
let page = 0x1000
let mutable bad = 0
for off in 0 .. page .. size - 1 do
    try Array.blit (t.Read(t.Base + nativeint off, page)) 0 img off page
    with _ -> bad <- bad + 1
printfn "unreadable pages: %d" bad

// Rewrite headers: ImageBase := runtime base; each section raw == virtual.
let pe = BitConverter.ToInt32(img, 0x3C)
let nsec = int (BitConverter.ToUInt16(img, pe + 6))
let optSize = int (BitConverter.ToUInt16(img, pe + 20))
let opt = pe + 24
Array.blit (BitConverter.GetBytes(int64 t.Base)) 0 img (opt + 24) 8
for i in 0 .. nsec - 1 do
    let sh = opt + optSize + i * 40
    let vsize = BitConverter.ToInt32(img, sh + 8)
    let va = BitConverter.ToInt32(img, sh + 12)
    Array.blit (BitConverter.GetBytes(vsize)) 0 img (sh + 16) 4   // SizeOfRawData
    Array.blit (BitConverter.GetBytes(va)) 0 img (sh + 20) 4      // PointerToRawData
    let name = Text.Encoding.ASCII.GetString(img, sh, 8).TrimEnd('\000')
    printfn "  %-8s va=%08X vsize=%08X" name va vsize
File.WriteAllBytes(outPath, img)
printfn "wrote %s (%d MB)" outPath (img.Length / 1048576)
