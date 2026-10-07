// rtti.fsx - class name (MSVC RTTI) for vtable RVAs: dotnet fsi rtti.fsx <vtblRva>[,...]
open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let ib = 0x7FF74F410000UL
let cstr o = let e = Array.IndexOf(img, 0uy, o) in Text.Encoding.ASCII.GetString(img, o, e - o)
for a in fsi.CommandLineArgs.[1].Split(',') do
    let v = Convert.ToInt32(a, 16)
    try
        let col = int (BitConverter.ToUInt64(img, v - 8) - ib)
        let td = BitConverter.ToInt32(img, col + 0x0C)
        printfn "%X  %s" v (cstr (td + 0x10))
    with e -> printfn "%X  ?? %s" v e.Message
