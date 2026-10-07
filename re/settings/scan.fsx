// Static analysis helper over the memory dump (raw offset == RVA).
// usage: dotnet fsi ft_x.fsx xref RVA [RVA...]      -> instructions referencing (mem disp / branch target) each RVA
//        dotnet fsi ft_x.fsx dis RVA [count]         -> disassemble count instrs from RVA
//        dotnet fsi ft_x.fsx func RVA                -> disassemble whole function containing RVA (via .pdata)
//        dotnet fsi ft_x.fsx xrefrange LO HI         -> refs to any address in [LO,HI)
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let imageBase = BitConverter.ToUInt64(img, opt + 24)
let pdataRva = BitConverter.ToInt32(img, opt + 112 + 3*8)
let pdataSz = BitConverter.ToInt32(img, opt + 112 + 3*8 + 4)
let funcs =
    [| for i in 0 .. pdataSz / 12 - 1 do
         let o = pdataRva + i*12
         yield (BitConverter.ToInt32(img, o), BitConverter.ToInt32(img, o+4)) |]
let funcOf (rva:int) =
    // binary search
    let mutable lo, hi = 0, funcs.Length - 1
    let mutable res = None
    while lo <= hi do
        let m = (lo+hi)/2
        let (b,e) = funcs.[m]
        if rva < b then hi <- m-1 elif rva >= e then lo <- m+1 else (res <- Some funcs.[m]; lo <- hi+1)
    res
let fmt = NasmFormatter()
fmt.Options.RipRelativeAddresses <- false
let out = StringOutput()
let p (ins: Instruction) =
    fmt.Format(&ins, out)
    let s = out.ToStringAndReset()
    // express absolute addrs as RVAs where large
    let s = Text.RegularExpressions.Regex.Replace(s, "([0-9A-F]{12,16})h", fun m ->
                let v = Convert.ToUInt64(m.Groups.[1].Value, 16)
                if v >= imageBase && v < imageBase + uint64 img.Length then sprintf "R_%X" (v - imageBase) else m.Value)
    sprintf "%08X  %-24s %s" (ins.IP - imageBase) (BitConverter.ToString(img, int (ins.IP-imageBase), ins.Length).Replace("-","")) s
let decodeRange (b:int) (e:int) = seq {
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, b, e-b))
    dec.IP <- imageBase + uint64 b
    let endIp = imageBase + uint64 e
    while dec.IP < endIp do
        yield dec.Decode() }
let objAddr = imageBase + 0x4759910UL
let dispSet = set [0x270UL;0x274UL;0x278UL;0x27CUL;0x280UL;0x281UL;0x282UL;0x284UL]
for (b,e) in funcs do
    let ins = decodeRange b e |> Seq.toArray
    let usesObj = ins |> Array.exists (fun i -> i.IsIPRelativeMemoryOperand && i.IPRelativeMemoryAddress = objAddr)
    if usesObj then
        let hits = ins |> Array.filter (fun i -> (not i.IsIPRelativeMemoryOperand) && i.MemoryBase <> Register.None && i.MemoryBase <> Register.RSP && i.MemoryBase <> Register.RBP && dispSet.Contains i.MemoryDisplacement64)
        if hits.Length > 0 then
            printfn "== fn_%X" b
            for h in hits do printfn "%s" (p h)
