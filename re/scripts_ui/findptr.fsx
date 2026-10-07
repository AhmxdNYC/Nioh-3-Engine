// findptr.fsx - find 8-byte absolute pointers (imagebase+rva) to given RVAs in the dump (vtable slots etc.)
//   dotnet fsi findptr.fsx <rvaHex>[,<rvaHex>...]
open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let ib = 0x7FF74F410000UL
let pe = BitConverter.ToInt32(img, 0x3C)
let ibh = BitConverter.ToUInt64(img, pe + 24 + 24)
let targets = fsi.CommandLineArgs.[1].Split(',') |> Array.map (fun s -> Convert.ToUInt64(s, 16))
for t in targets do
    for b in [ib; ibh] |> List.distinct do
        let v = b + t
        let mutable i = 0
        while i <= img.Length - 8 do
            if BitConverter.ToUInt64(img, i) = v then printfn "%X -> %X (base %X)" i t b
            i <- i + 8
