// fieldrefs.fsx - every instruction whose memory operand has displacement <disp> (any base register), grouped by
// function. Reads re\Nioh3.dump.exe only.   dotnet fsi fieldrefs.fsx <dispHex> [w]   (w = writes only)
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open Iced.Intel
open Wj_common
let args = fsi.CommandLineArgs |> Array.skip 1
let disp = Convert.ToInt64(args.[0], 16)
let writesOnly = args.Length > 1 && args.[1] = "w"
let fac = InstructionInfoFactory()
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.Op0Kind = OpKind.Memory || ins.Op1Kind = OpKind.Memory then
                if int64 ins.MemoryDisplacement64 = disp && ins.MemoryBase <> Register.RIP && ins.MemoryBase <> Register.RSP then
                    let w = ins.Op0Kind = OpKind.Memory && ins.Mnemonic <> Mnemonic.Cmp && ins.Mnemonic <> Mnemonic.Test
                    if (not writesOnly) || w then
                        let p = match primaryEntry (uint32 ins.IP) with Some x -> x | None -> s
                        printfn "%8X fn %8X %s %s" ins.IP p (if w then "W" else "R") (fmtIns ins)
