// tiles_scan.fsx -- census of D3D12 tiled-resource / queue-sync call sites in the v2.02 dump.
//   dotnet fsi tiles_scan.fsx [refs hex,...]
// Reads re\Nioh3.dump.exe only (image layout: file offset == RVA).
// 1. every `call/jmp qword [reg+disp]` whose disp is a tiled-resource or queue slot, with the highest
//    stack-argument slot written since the previous call (arity filter):
//      +40 queue UpdateTileMappings (10 args -> [rsp+48h])   +48 queue CopyTileMappings (7 args -> [rsp+30h])
//      +F0 device CreateReservedResource (6 args -> [rsp+28h]) +150 device GetResourceTiling (8 args -> [rsp+30h])
//      +1B8 Device4 CreateReservedResource1 (7 args)          +E0 CreateHeap (4 args) / +118 MakeResident / +120 Evict (by context)
//    plus the instruction that set rcx (this) and the vtable load.
// 2. rip-relative references to the RVAs given on the command line (strings, globals).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let dd = opt + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let funcs =
    Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12), u32 (excRva + i * 12 + 4))
    |> Array.filter (fun (b, e) -> e > b && int e <= img.Length && e - b < 0x200000u)
    |> Array.distinct
let fmt = NasmFormatter()
let so = StringOutput()
let txt (i: Instruction) = fmt.Format(&i, so); so.ToStringAndReset()

let refs = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1].Split(',') |> Array.map (fun s -> Convert.ToUInt64(s, 16)) |> HashSet else HashSet()
let interesting = dict [ 0x40,("UpdateTileMappings?",0x48); 0x48,("CopyTileMappings?",0x30); 0xF0,("CreateReservedResource?",0x28); 0x50,("IQ.UpdateTileMappings?",0x48); 0x58,("IQ.CopyTileMappings?",0x28)
                         0x150,("GetResourceTiling?",0x30); 0x1B8,("CreateReservedResource1?",0x30) ]
let out = List<string>()
let refOut = List<string>()
for (s, e) in funcs do
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
    dec.IP <- uint64 s
    let ins = List<Instruction>()
    while dec.IP < uint64 e do ins.Add(dec.Decode())
    let mutable maxArg = -1
    for k in 0 .. ins.Count - 1 do
        let i = ins.[k]
        // stack-arg stores
        if i.Op0Kind = OpKind.Memory && (i.MemoryBase = Register.RSP) && i.MemoryIndex = Register.None then
            let d = int i.MemoryDisplacement64
            if d >= 0x20 && d <= 0x60 && (i.Mnemonic = Mnemonic.Mov || i.Mnemonic = Mnemonic.Movss || i.Mnemonic = Mnemonic.Movsd || i.Mnemonic = Mnemonic.Movq || i.Mnemonic = Mnemonic.Movd || i.Mnemonic = Mnemonic.And) then
                maxArg <- max maxArg d
        // rip refs
        if refs.Count > 0 then
            for op in 0 .. i.OpCount - 1 do
                if i.GetOpKind(op) = OpKind.Memory && i.IsIPRelativeMemoryOperand && refs.Contains(i.IPRelativeMemoryAddress) then
                    refOut.Add(sprintf "ref\t%X\t%X\t%X\t%s" i.IPRelativeMemoryAddress s (uint32 i.IP) (txt i))
        if i.Mnemonic = Mnemonic.Call || i.Mnemonic = Mnemonic.Jmp then
            if i.Op0Kind = OpKind.Memory && i.MemoryBase <> Register.RIP && i.MemoryBase <> Register.None && i.MemoryIndex = Register.None then
                let disp = int i.MemoryDisplacement64
                match interesting.TryGetValue disp with
                | true, (nm, need) when maxArg >= need || (i.Mnemonic = Mnemonic.Jmp && disp = 0x48) ->
                    let mutable thisSrc = "?"
                    let mutable vt = "?"
                    let mutable j = k - 1
                    while j >= 0 && j >= k - 30 && thisSrc = "?" do
                        let p = ins.[j]
                        if (p.Mnemonic = Mnemonic.Mov || p.Mnemonic = Mnemonic.Lea) && p.Op0Kind = OpKind.Register && p.Op0Register = Register.RCX then thisSrc <- txt p
                        if vt = "?" && p.Mnemonic = Mnemonic.Mov && p.Op0Kind = OpKind.Register && p.Op0Register = i.MemoryBase then vt <- txt p
                        j <- j - 1
                    out.Add(sprintf "%s\t%X\t%X\t+%X\tmaxarg=%X\tthis: %s\tvt: %s" nm s (uint32 i.IP) disp maxArg thisSrc vt)
                | _ -> ()
            maxArg <- -1
for l in out do printfn "%s" l
for l in refOut do printfn "%s" l
