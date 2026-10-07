// ld_disprange.fsx <lo> <hi> - histogram of [reg+disp] (non-RIP) with lo<=disp<hi, with up to 6 example functions
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.Collections.Generic
open Iced.Intel
open Wj_common
let a = fsi.CommandLineArgs
let lo = Convert.ToUInt64(a.[1], 16)
let hi = Convert.ToUInt64(a.[2], 16)
let h = Dictionary<uint64, List<string>>()
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            for k in 0 .. ins.OpCount - 1 do
                if ins.GetOpKind k = OpKind.Memory && ins.MemoryBase <> Register.RIP && ins.MemoryBase <> Register.RSP && ins.MemoryBase <> Register.RBP && ins.MemoryBase <> Register.None then
                    let d = uint64 (uint32 ins.MemoryDisplacement64)
                    if d >= lo && d < hi then
                        if not (h.ContainsKey d) then h.[d] <- List<string>()
                        let p = match primaryEntry (uint32 ins.IP) with Some x -> x | None -> s
                        h.[d].Add(sprintf "%X(%X)" ins.IP p)
for kv in h |> Seq.sortBy (fun kv -> kv.Key) do
    printfn "%8X %4d  %s" kv.Key kv.Value.Count (String.Join(" ", kv.Value |> Seq.truncate 100000))
