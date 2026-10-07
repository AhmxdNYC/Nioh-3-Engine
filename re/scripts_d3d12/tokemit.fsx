// tokemit.fsx -- find code that writes a KTGL token id into the command stream.
//   dotnet fsi tokemit.fsx <tokHex>[,<tokHex>...]
// Heuristic: `mov dword|word [reg(+disp)], imm` with imm == token id (low 16 bits; the high 16 bits of the
// dword are the token's argument and must be 0 for a match unless the write is a word), within a .pdata
// function, followed within 6 instructions by an `add [..],imm` / `lea` that advances a pointer.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let toks = fsi.CommandLineArgs.[1].Split(',') |> Array.map (fun s -> Convert.ToUInt32(s, 16)) |> Set.ofArray
let fmt = NasmFormatter()
let so = StringOutput()
for i in 0 .. excSz / 12 - 1 do
    let s, e = u32 (excRva + i * 12), u32 (excRva + i * 12 + 4)
    if e > s && e - s < 0x100000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        let ins = Collections.Generic.List<Instruction>()
        while dec.IP < uint64 e do ins.Add(dec.Decode())
        for k in 0 .. ins.Count - 1 do
            let x = ins.[k]
            if x.Mnemonic = Mnemonic.Mov && x.Op0Kind = OpKind.Memory && (x.Op1Kind = OpKind.Immediate32 || x.Op1Kind = OpKind.Immediate16) then
                let v = x.Immediate32
                let tok = v &&& 0xFFFFu
                if toks.Contains tok && (x.MemorySize = MemorySize.UInt16 || x.MemorySize = MemorySize.UInt32 || x.MemorySize = MemorySize.Int32) then
                    let mutable adv = false
                    for j in k + 1 .. min (ins.Count - 1) (k + 6) do
                        let y = ins.[j]
                        if (y.Mnemonic = Mnemonic.Add && y.Op0Kind = OpKind.Memory) || (y.Mnemonic = Mnemonic.Add && y.Op0Kind = OpKind.Register) || y.Mnemonic = Mnemonic.Lea then adv <- true
                    fmt.Format(&x, so)
                    printfn "%X\tfn %X\ttok %02X\targ %X\t%s%s" x.IP s tok (v >>> 16) (so.ToStringAndReset()) (if adv then "" else "\t(no advance seen)")
