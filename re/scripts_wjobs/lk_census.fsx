// lk_census.fsx - census of `lock cmpxchg [m], zero` used as an atomic LOAD in the hot worker functions.
//   dotnet fsi lk_census.fsx [hot_workers.tsv]
// For every hot worker function (PC profiles r5/r6/r7/r9/ingame2, >= 0.002 core self, see hotlist.py), decode the
// primary .pdata function (follows chained unwind info) and report each `lock cmpxchg [m], r32/r64` whose source
// register AND eax/rax are provably zero at the site (forward dataflow from the function entry), plus the flag state
// after it. Such an instruction never changes memory value-wise (it writes back the 0 it found, or fails) - it is a
// load that takes the line exclusive.
#load "wj_common.fsx"
open System
open System.IO
open Iced.Intel
open Wj_common

let listFile = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else Path.Combine(__SOURCE_DIRECTORY__, "hot_workers.tsv")
let hot = [ for l in File.ReadAllLines listFile do
              let p = l.Split('\t')
              yield Convert.ToUInt32(p.[0], 16), float p.[1] ]
let seenFn = Collections.Generic.HashSet<uint32>()
for (rva, core) in hot do
    match primaryEntry rva with
    | None -> ()
    | Some entry when seenFn.Add entry ->
        // collect all .pdata ranges whose primary is `entry` (function + chained parts)
        let ranges = [ for i in 0 .. pBegin.Length - 1 do
                         if primaryEntry pBegin.[i] = Some entry then yield pBegin.[i], pEnd.[i] ]
        for (b, e) in ranges do
            for ins in decodeAll img.[int b .. int e - 1] (uint64 b) do
                if ins.Mnemonic = Mnemonic.Cmpxchg && ins.HasLockPrefix && ins.Op0Kind = OpKind.Memory && ins.Op1Kind = OpKind.Register then
                    let src = ins.Op1Register
                    let acc = if src.GetSize() = 8 then Register.RAX else Register.EAX
                    let vS, fS = provenZeroAt (uint64 entry) src ins.IP
                    let vA, fA = provenZeroAt (uint64 entry) acc ins.IP
                    let zero = vS > 0 && fS.IsEmpty && vA > 0 && fA.IsEmpty
                    let flag =
                        match firstFlagUser ins.NextIP 0 with
                        | Some (f, false) -> sprintf "flags dead (%X %s)" f.IP (fmtIns f)
                        | Some (f, true) -> sprintf "FLAGS READ by %X %s" f.IP (fmtIns f)
                        | None -> "flags: unknown (control flow)"
                    printfn "%-8X fn %-8X (%.3f core)  %-36s len %d  %s  %s" ins.IP entry core (fmtIns ins) ins.Length
                        (if zero then "ZERO-LOAD" else sprintf "not-proven(src %d/%d acc %d/%d)" vS fS.Length vA fA.Length) flag
    | _ -> ()
