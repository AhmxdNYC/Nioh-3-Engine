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
let refTarget (ins: Instruction) =
    let r = ResizeArray<uint64>()
    for k in 0 .. ins.OpCount-1 do
        match ins.GetOpKind k with
        | OpKind.Memory when ins.IsIPRelativeMemoryOperand -> r.Add(ins.IPRelativeMemoryAddress)
        | OpKind.NearBranch64 -> r.Add(ins.NearBranch64)
        | OpKind.Immediate32to64 | OpKind.Immediate64 -> r.Add(ins.GetImmediate k)
        | _ -> ()
    r
let args = fsi.CommandLineArgs |> Array.skip 1
let hx (s:string) = Convert.ToInt32(s.Replace("0x",""), 16)
let allIns () = seq { for (b,e) in funcs do yield! decodeRange b e }
match args.[0] with
| "dis" ->
    let a = hx args.[1]
    let n = if args.Length > 2 then int args.[2] else 40
    decodeRange a (min img.Length (a + n*15)) |> Seq.truncate n |> Seq.iter (p >> printfn "%s")
| "func" ->
    for a in args.[1..] do
      match funcOf (hx a) with
      | Some (b,e) -> printfn "== func %X-%X" b e; decodeRange b e |> Seq.iter (p >> printfn "%s")
      | None -> printfn "no pdata for %s" a
| "xref" | "xrefrange" ->
    let targets =
        if args.[0] = "xref" then args.[1..] |> Array.map (fun s -> imageBase + uint64 (hx s)) |> Set.ofArray
        else Set.empty
    let lo, hi = if args.[0] = "xrefrange" then imageBase + uint64 (hx args.[1]), imageBase + uint64 (hx args.[2]) else 0UL,0UL
    for ins in allIns () do
        for t in refTarget ins do
            if (args.[0] = "xref" && targets.Contains t) || (args.[0] = "xrefrange" && t >= lo && t < hi) then
                let f = match funcOf (int (ins.IP - imageBase)) with Some (b,_) -> sprintf "fn_%X" b | None -> "?"
                printfn "%s   [%s -> %X]" (p ins) f (t - imageBase)
| _ -> printfn "unknown cmd"
// dispscan D1 D2 ... : functions containing writes to [reg+D] for >=2 of the given displacements (non-RIP)
if args.[0] = "dispscan" then
    let ds = args.[1..] |> Array.map (fun s -> uint64 (hx s)) |> Set.ofArray
    for (b,e) in funcs do
        let hits = ResizeArray<Instruction>()
        for ins in decodeRange b e do
            if ins.OpCount > 0 && ins.Op0Kind = OpKind.Memory && not ins.IsIPRelativeMemoryOperand
               && ins.MemoryBase <> Register.RSP && ins.MemoryBase <> Register.RBP && ds.Contains ins.MemoryDisplacement64 then
                hits.Add ins
        let distinct = hits |> Seq.map (fun i -> i.MemoryDisplacement64) |> Seq.distinct |> Seq.length
        if distinct >= 2 then
            printfn "== fn_%X  distinct=%d" b distinct
            for i in hits do printfn "   %s" (p i)
// gfield RVA_GLOBAL D1 D2..: small functions (<0x80 bytes) that load [RVA_GLOBAL] then access [reg+D]
if args.[0] = "gfield" then
    let g = imageBase + uint64 (hx args.[1])
    let ds = args.[2..] |> Array.map (fun s -> uint64 (hx s)) |> Set.ofArray
    for (b,e) in funcs do
        if e - b < (if args.Length > 0 && Environment.GetEnvironmentVariable("FT_BIG") = "1" then 0x100000 else 0x200) then
            let ins = decodeRange b e |> Seq.toArray
            if ins |> Array.exists (fun i -> i.IsIPRelativeMemoryOperand && i.IPRelativeMemoryAddress = g) then
                let hits = ins |> Array.filter (fun i -> i.MemoryBase <> Register.None && not i.IsIPRelativeMemoryOperand && ds.Contains i.MemoryDisplacement64 && i.MemoryBase <> Register.RSP)
                if hits.Length > 0 then
                    printfn "== fn_%X (%X bytes)" b (e-b)
                    for i in hits do printfn "   %s" (p i)
// ctx FILE N : for each RVA in FILE, print N instructions after it
if args.[0] = "ctx" then
    let n = int args.[2]
    for line in IO.File.ReadAllLines args.[1] do
        let a = hx (line.Trim())
        printfn "-- %X" a
        decodeRange a (a + n*15) |> Seq.truncate n |> Seq.iter (p >> printfn "  %s")
// imm V1 V2..: instructions with immediate operand equal to any value (hex)
if args.[0] = "imm" then
    let vs = args.[1..] |> Array.map (fun s -> Convert.ToUInt64(s.Replace("0x",""),16)) |> Set.ofArray
    for (b,e) in funcs do
        for ins in decodeRange b e do
            for k in 0 .. ins.OpCount-1 do
                match ins.GetOpKind k with
                | OpKind.Immediate32 | OpKind.Immediate32to64 | OpKind.Immediate64 | OpKind.Immediate8to32 ->
                    if vs.Contains (ins.GetImmediate k &&& 0xFFFFFFFFUL) then printfn "%s   [fn_%X]" (p ins) b
                | _ -> ()
// disp8 D : instructions with a BYTE-sized memory operand at [reg+D] (non-stack)
if args.[0] = "disp8" then
    let d = uint64 (hx args.[1])
    for (b,e) in funcs do
        for ins in decodeRange b e do
            if ins.MemoryDisplacement64 = d && ins.MemoryBase <> Register.None && not ins.IsIPRelativeMemoryOperand
               && ins.MemoryBase <> Register.RSP && ins.MemoryBase <> Register.RBP && ins.MemorySize = MemorySize.UInt8 then
                printfn "%s   [fn_%X]" (p ins) b
// cmpimm V1 V2 ... : functions that compare against ALL given immediates (cmp reg/mem, imm)
if args.[0] = "cmpimm" then
    let vs = args.[1..] |> Array.map (fun s -> Convert.ToUInt64(s.Replace("0x",""),16))
    for (b,e) in funcs do
        let found = Collections.Generic.HashSet<uint64>()
        let hits = ResizeArray<Instruction>()
        for ins in decodeRange b e do
            if ins.Mnemonic = Mnemonic.Cmp && ins.OpCount = 2 then
                match ins.GetOpKind 1 with
                | OpKind.Immediate8to32 | OpKind.Immediate32 | OpKind.Immediate8 | OpKind.Immediate8to64 | OpKind.Immediate32to64 ->
                    let v = ins.GetImmediate 1 &&& 0xFFFFFFFFUL
                    if Array.contains v vs then (found.Add v |> ignore; hits.Add ins)
                | _ -> ()
        if found.Count = vs.Length then
            printfn "== fn_%X" b
            for i in hits do printfn "   %s" (p i)
// dispread D : instructions READING [reg+D] as a 4-byte float/int (non-stack), any function
if args.[0] = "dispread" then
    let d = uint64 (hx args.[1])
    for (b,e) in funcs do
        for ins in decodeRange b e do
            if ins.OpCount = 2 && ins.Op1Kind = OpKind.Memory && ins.MemoryDisplacement64 = d && ins.MemoryBase <> Register.None
               && not ins.IsIPRelativeMemoryOperand && ins.MemoryBase <> Register.RSP && ins.MemoryBase <> Register.RBP
               && (ins.MemorySize = MemorySize.Float32 || ins.MemorySize = MemorySize.UInt32 || ins.MemorySize = MemorySize.Int32) then
                printfn "%s   [fn_%X]" (p ins) b
