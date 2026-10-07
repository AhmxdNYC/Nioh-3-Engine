// dis.fsx — disassemble a function from the decrypted dump by RVA, with import names resolved.
//   dotnet fsi dis.fsx <rvaHex> [maxBytes=0 (whole .pdata function)] [rvaHex ...]
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let imageBase = BitConverter.ToUInt64(img, opt + 24)
let dd = opt + 112
// imports: IAT slot rva -> "dll!name"
let iat = Dictionary<uint64, string>()
let cstr o = let e = Array.IndexOf(img, 0uy, o) in Text.Encoding.ASCII.GetString(img, o, e - o)
let mutable d = int (u32 (dd + 8))
while u32 (d + 12) <> 0u do
    let dll = cstr (int (u32 (d + 12)))
    let oft, ft = u32 d, u32 (d + 16)
    let th = if oft <> 0u then oft else ft
    let mutable i = 0
    while BitConverter.ToUInt64(img, int th + i * 8) <> 0UL do
        let ent = BitConverter.ToUInt64(img, int th + i * 8)
        let nm = if ent &&& (1UL <<< 63) = 0UL && ent < uint64 img.Length then cstr (int ent + 2) else sprintf "ord%d" (ent &&& 0xFFFFUL)
        iat.[uint64 ft + uint64 (i * 8)] <- sprintf "%s!%s" dll nm
        i <- i + 1
    d <- d + 20
// pdata
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let pBegin = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12))
let pEnd = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12 + 4))
let funcRange (rva: uint32) =
    let i = Array.BinarySearch(pBegin, rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && rva < pEnd.[i] then Some (pBegin.[i], pEnd.[i]) else None

let dis (rva: uint32) (maxBytes: int) =
    let s, e =
        match funcRange rva with
        | Some (s, e) when maxBytes = 0 -> s, e
        | _ -> rva, rva + uint32 (if maxBytes = 0 then 0x100 else maxBytes)
    printfn "; ---- Nioh3+%X .. +%X (pdata fn %A)" s e (funcRange rva)
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
    dec.IP <- uint64 s
    let fmt = NasmFormatter()
    fmt.Options.FirstOperandCharIndex <- 8
    let out = StringOutput()
    while dec.IP < uint64 e do
        let ins = dec.Decode()
        fmt.Format(&ins, out)
        let txt = out.ToStringAndReset()
        let note =
            if ins.IsIPRelativeMemoryOperand then
                let t = ins.IPRelativeMemoryAddress
                match iat.TryGetValue t with
                | true, n -> "  ; " + n
                | _ ->
                    // string literal hint
                    let o = int t
                    if o > 0 && o < img.Length - 4 && img.[o] >= 0x20uy && img.[o] < 0x7Fuy && img.[o+1] >= 0x20uy && img.[o+1] < 0x7Fuy && img.[o+2] >= 0x20uy then
                        let e2 = Array.IndexOf(img, 0uy, o)
                        if e2 - o < 120 then sprintf "  ; \"%s\"" (Text.Encoding.ASCII.GetString(img, o, e2 - o)) else ""
                    else ""
            else ""
        printfn "%8X  %-50s%s" ins.IP txt note

let args = fsi.CommandLineArgs |> Array.skip 1
// usage: dis.fsx <rva>[:lenHex] ...   (no len => whole .pdata fragment)
for a in args do
    let parts = a.Split(':')
    let v = Convert.ToUInt64(parts.[0].Replace("0x", ""), 16)
    let len = if parts.Length > 1 then Convert.ToInt32(parts.[1].Replace("0x", ""), 16) else 0
    dis (uint32 (if v >= imageBase then v - imageBase else v)) len
