// ws_sites.fsx - every direct call to the given targets (default JobWakeOneInPool 48B9C, JobWakeOne 48C84,
// JobPushToPool 31A514), with the primary function and the N instructions before each call. Dump only.
//   dotnet fsi ws_sites.fsx [targetsHexCsv] [nBefore]
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.Collections.Generic
open Iced.Intel
open Wj_common

let args = fsi.CommandLineArgs |> Array.skip 1
let targets = (if args.Length > 0 then args.[0] else "48B9C,48C84,31A514").Split(',') |> Array.map (fun s -> Convert.ToUInt64(s, 16)) |> Set.ofArray
let nb = if args.Length > 1 then int args.[1] else 10
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        let win = Queue<Instruction>()
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.Mnemonic = Mnemonic.Call && ins.Op0Kind = OpKind.NearBranch64 && targets.Contains ins.NearBranchTarget then
                let p = match primaryEntry (uint32 ins.IP) with Some x -> x | None -> s
                printfn "== call %X at %X  fn %X (frag %X)" ins.NearBranchTarget ins.IP p s
                for w in win do printfn "     %8X  %s" w.IP (fmtIns w)
                printfn "  >> %8X  %s" ins.IP (fmtIns ins)
            win.Enqueue ins
            if win.Count > nb then win.Dequeue() |> ignore
