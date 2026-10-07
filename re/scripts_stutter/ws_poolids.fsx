// ws_poolids.fsx - find stores to the g_App (4759910) pool-id fields +690h..+6D0h: every `mov [reg+disp],r32/imm32`
// in a function that also loads [rip->4759910] or has >=3 such stores. Dump only.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.Collections.Generic
open Iced.Intel
open Wj_common
let lo, hi = 0x690UL, 0x6D4UL
let byFn = Dictionary<uint32, List<Instruction>>()
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.Mnemonic = Mnemonic.Mov && ins.Op0Kind = OpKind.Memory && ins.MemoryBase <> Register.RIP && ins.MemoryIndex = Register.None
               && ins.MemoryDisplacement64 >= lo && ins.MemoryDisplacement64 < hi && ins.MemorySize = MemorySize.UInt32 || 
               (ins.Mnemonic = Mnemonic.Mov && ins.Op0Kind = OpKind.Memory && ins.MemoryBase <> Register.RIP && ins.MemoryIndex = Register.None
               && ins.MemoryDisplacement64 >= lo && ins.MemoryDisplacement64 < hi && ins.MemorySize = MemorySize.Int32) then
                let p = match primaryEntry (uint32 ins.IP) with Some x -> x | None -> s
                if not (byFn.ContainsKey p) then byFn.[p] <- List()
                byFn.[p].Add ins
for kv in byFn |> Seq.sortByDescending (fun kv -> kv.Value.Count) |> Seq.truncate 25 do
    printfn "fn %X : %d stores" kv.Key kv.Value.Count
    for ins in kv.Value |> Seq.truncate 30 do printfn "    %8X  %s" ins.IP (fmtIns ins)
