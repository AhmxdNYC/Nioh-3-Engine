// tiles_dis.fsx -- disassemble dump ranges: dotnet fsi tiles_dis.fsx <rva>[:<bytes>|f] ...
//   <rva>:f  disassembles the whole .pdata function containing rva. Reads re\Nioh3.dump.exe only.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let pB = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12))
let pE = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12 + 4))
let funcOf (rva: uint32) =
    // walk chained unwind: take the entry whose range contains rva, then the smallest begin with the same unwind chain is
    // not tracked; we just report the containing .pdata range.
    let mutable best = None
    for i in 0 .. pB.Length - 1 do
        if pB.[i] <= rva && rva < pE.[i] then best <- Some (pB.[i], pE.[i])
    best
let fmt = NasmFormatter()
fmt.Options.FirstOperandCharIndex <- 8
let so = StringOutput()
for a in fsi.CommandLineArgs |> Array.skip 1 do
    let parts = a.Split(':')
    let rva = Convert.ToUInt32(parts.[0], 16)
    let s, e =
        if parts.Length > 1 && parts.[1] = "f" then (match funcOf rva with Some r -> r | None -> rva, rva + 0x100u)
        else rva, rva + (if parts.Length > 1 then Convert.ToUInt32(parts.[1], 16) else 0x100u)
    printfn "; ---- %X .. %X" s e
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
    dec.IP <- uint64 s
    while dec.IP < uint64 e do
        let i = dec.Decode()
        fmt.Format(&i, so)
        let t = so.ToStringAndReset()
        let extra =
            if i.IsIPRelativeMemoryOperand then
                let t = int i.IPRelativeMemoryAddress
                if t > 0 && t < img.Length - 8 then
                    let mutable e2 = t
                    while e2 < img.Length && e2 - t < 60 && img.[e2] >= 32uy && img.[e2] < 127uy do e2 <- e2 + 1
                    if e2 - t >= 4 then sprintf "   ; \"%s\"" (Text.Encoding.ASCII.GetString(img, t, e2 - t)) else ""
                else ""
            else ""
        printfn "%8X  %s%s" i.IP t extra
