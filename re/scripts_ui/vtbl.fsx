// vtbl.fsx - dump consecutive qwords as RVAs: dotnet fsi vtbl.fsx <startRvaHex> <count>
open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let ib = 0x7FF74F410000UL
let s = Convert.ToInt32(fsi.CommandLineArgs.[1], 16)
let n = int fsi.CommandLineArgs.[2]
for i in 0 .. n - 1 do
    let v = BitConverter.ToUInt64(img, s + i * 8)
    let r = if v >= ib && v < ib + 0x8000000UL then sprintf "%X" (v - ib) else sprintf "(%X)" v
    printfn "%X [+%03X]  %s" (s + i * 8) (i * 8) r
