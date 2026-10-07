// tiles_tokemit.fsx -- find ECL-stream token emitters: `mov dword [r],imm` followed within 4 insns by `add qword [r],4`
//   dotnet fsi tiles_tokemit.fsx   (reads re\Nioh3.dump.exe only)
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let funcs = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12), u32 (excRva + i * 12 + 4)) |> Array.filter (fun (b, e) -> e > b && e - b < 0x200000u) |> Array.distinct
for (s, e) in funcs do
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
    dec.IP <- uint64 s
    let ins = Collections.Generic.List<Instruction>()
    while dec.IP < uint64 e do ins.Add(dec.Decode())
    for k in 0 .. ins.Count - 5 do
        let i = ins.[k]
        if i.Mnemonic = Mnemonic.Mov && i.Op0Kind = OpKind.Memory && i.MemorySize = MemorySize.UInt32 && i.Op1Kind = OpKind.Immediate32 && i.MemoryDisplacement64 = 0UL && i.MemoryIndex = Register.None then
            let imm = i.Immediate32
            if imm <= 12u then
                let mutable hit = false
                for j in k + 1 .. k + 4 do
                    let p = ins.[j]
                    if p.Mnemonic = Mnemonic.Add && p.Op0Kind = OpKind.Memory && p.MemorySize = MemorySize.UInt64 && p.Op1Kind <> OpKind.Register && p.Immediate64 = 4UL then hit <- true
                if hit then printfn "tok%d\t%X\t%X" imm s (uint32 i.IP)
