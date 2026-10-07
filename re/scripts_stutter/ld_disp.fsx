// ld_disp.fsx - sweep every .pdata function of re\Nioh3.dump.exe for memory operands
//   dotnet fsi ld_disp.fsx disp <hexDisp> [w]       instructions whose [reg+disp] (non-RIP) == disp; 'w' = writes only
//   dotnet fsi ld_disp.fsx glob <hexRva> [w]         instructions referencing a RIP-relative address in [rva, rva+8)
//   dotnet fsi ld_disp.fsx imm <hexImm>              instructions with an immediate == imm
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open Iced.Intel
open Wj_common
let args = fsi.CommandLineArgs |> Array.skip 1
let mode = args.[0]
let v = Convert.ToUInt64(args.[1].Replace("0x",""), 16)
let wOnly = args.Length > 2 && args.[2] = "w"
let isWrite (ins: Instruction) =
    // operand 0 is memory and the instruction writes it (mov/add/sub/inc/dec/or/and/xchg/cmpxchg/set*)
    ins.Op0Kind = OpKind.Memory &&
      (match ins.Mnemonic with
       | Mnemonic.Cmp | Mnemonic.Test | Mnemonic.Push | Mnemonic.Call | Mnemonic.Jmp | Mnemonic.Prefetcht0 | Mnemonic.Prefetchw -> false
       | _ -> true)
let mutable n = 0
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            let hit =
                match mode with
                | "disp" ->
                    let mutable h = false
                    for k in 0 .. ins.OpCount - 1 do
                        if ins.GetOpKind k = OpKind.Memory && ins.MemoryBase <> Register.RIP && ins.MemoryBase <> Register.None
                           && uint64 (uint32 ins.MemoryDisplacement64) = v then h <- true
                    h
                | "glob" ->
                    let mutable h = false
                    for k in 0 .. ins.OpCount - 1 do
                        if ins.GetOpKind k = OpKind.Memory && ins.IsIPRelativeMemoryOperand then
                            let t = ins.IPRelativeMemoryAddress
                            if t >= v && t < v + 8UL then h <- true
                    h
                | "imm" ->
                    let mutable h = false
                    for k in 0 .. ins.OpCount - 1 do
                        let ok = ins.GetOpKind k
                        if (ok = OpKind.Immediate32 || ok = OpKind.Immediate32to64 || ok = OpKind.Immediate8to32 || ok = OpKind.Immediate8to64 || ok = OpKind.Immediate8 || ok = OpKind.Immediate16 || ok = OpKind.Immediate64)
                           && ins.GetImmediate k = v then h <- true
                    h
                | _ -> false
            if hit && (not wOnly || isWrite ins) then
                let p = match primaryEntry (uint32 ins.IP) with Some x -> x | None -> s
                printfn "%8X  fn %8X  %s" ins.IP p (fmtIns ins)
                n <- n + 1
eprintfn "%d hits" n
