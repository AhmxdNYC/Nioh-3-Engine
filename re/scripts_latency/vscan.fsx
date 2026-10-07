// vscan.fsx - find virtual calls [reg+disp] whose object came from [reg+field] within N instructions (static, dump only).
//   dotnet fsi vscan.fsx <fieldHex> <dispHex>[,<dispHex>...] [window=12]
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let dd = opt + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let args = fsi.CommandLineArgs |> Array.skip 1
let field = Convert.ToUInt64(args.[0].Replace("0x",""), 16)
let disps = args.[1].Split(',') |> Array.map (fun s -> Convert.ToUInt64(s.Replace("0x",""), 16)) |> Set.ofArray
let win = if args.Length > 2 then int args.[2] else 12
let fmt = NasmFormatter()
let out = StringOutput()
let seen = Collections.Generic.HashSet<uint32>()
for i in 0 .. excSz / 12 - 1 do
    let b, e = u32 (excRva + i * 12), u32 (excRva + i * 12 + 4)
    if e > b && e - b < 0x100000u && seen.Add b then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int b, int (e - b)))
        dec.IP <- uint64 b
        let hist = Collections.Generic.Queue<Instruction>()
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.Mnemonic = Mnemonic.Call && ins.OpCount = 1 && ins.Op0Kind = OpKind.Memory
               && ins.MemoryBase <> Register.RIP && disps.Contains ins.MemoryDisplacement64 then
                let hit = hist |> Seq.exists (fun h -> h.OpCount = 2 && h.Op1Kind = OpKind.Memory && h.MemoryDisplacement64 = field)
                if hit then
                    fmt.Format(&ins, out)
                    printfn "fn %X  site %X  %s" b ins.IP (out.ToStringAndReset())
            hist.Enqueue ins
            if hist.Count > win then hist.Dequeue() |> ignore
