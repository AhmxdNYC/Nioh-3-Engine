// peek.fsx - print qwords at RVAs of the dump as RVAs (pointers rebased by the dump image base).
//   dotnet fsi peek.fsx <rvaHex>[:countDec] ...
open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let pe = BitConverter.ToInt32(img, 0x3C)
let ib = BitConverter.ToUInt64(img, pe + 24 + 24)
for a in fsi.CommandLineArgs |> Array.skip 1 do
    let p = a.Split(':')
    let r = Convert.ToInt32(p.[0].Replace("0x",""), 16)
    let n = if p.Length > 1 then int p.[1] else 8
    for i in 0 .. n - 1 do
        let v = BitConverter.ToUInt64(img, r + i * 8)
        let s = if v >= ib && v < ib + uint64 img.Length then sprintf "-> rva %X" (v - ib) else ""
        printfn "%X: %016X %s" (r + i * 8) v s
