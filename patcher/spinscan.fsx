// spinscan.fsx — static census of spin/yield/sleep primitives in the Nioh3 dump.
//   dotnet fsi spinscan.fsx [out.txt]
// Lists every call (direct import call or via 1-instruction thunk) to Sleep/SleepEx/SwitchToThread/
// NtYieldExecution/QueryPerformanceCounter/WaitForSingleObject(Ex)/WaitOnAddress/SleepConditionVariable*/
// PeekMessage, with the constant first argument when it can be seen (xor ecx,ecx / mov ecx,imm),
// and every function containing PAUSE (with loop sizes).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let outPath = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else @"C:\Users\Ahmad Hamza\nioh3-engine\re\out\spinscan.txt"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let cstr o = let e = Array.IndexOf(img, 0uy, o) in Text.Encoding.ASCII.GetString(img, o, e - o)
let iat = Dictionary<uint64, string>()
let mutable d = int (u32 (dd + 8))
while u32 (d + 12) <> 0u do
    let oft, ft = u32 d, u32 (d + 16)
    let th = if oft <> 0u then oft else ft
    let mutable i = 0
    while BitConverter.ToUInt64(img, int th + i * 8) <> 0UL do
        let ent = BitConverter.ToUInt64(img, int th + i * 8)
        iat.[uint64 ft + uint64 (i * 8)] <- (if ent &&& (1UL <<< 63) = 0UL then cstr (int ent + 2) else sprintf "ord%d" (ent &&& 0xFFFFUL))
        i <- i + 1
    d <- d + 20
let interesting (n: string) =
    [ "Sleep"; "SwitchToThread"; "Yield"; "QueryPerformanceCounter"; "WaitForSingleObject"; "WaitForMultipleObjects"; "WaitOnAddress"; "WakeByAddress"
      "SleepConditionVariable"; "PeekMessage"; "MsgWaitFor"; "GetMessage"; "timeBeginPeriod"; "SetWaitableTimer"; "CreateWaitableTimer" ]
    |> List.exists n.Contains
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let n = excSz / 12
let begins = Array.init n (fun i -> u32 (excRva + i * 12))
let ends = Array.init n (fun i -> u32 (excRva + i * 12 + 4))
// pass 1: thunks  (jmp [iat])  and tiny wrappers  (xor ecx,ecx ; jmp [Sleep])
let thunks = Dictionary<uint64, string>()
let decodeRange (s: uint32) (e: uint32) (f: Instruction -> unit) =
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
    dec.IP <- uint64 s
    while dec.IP < uint64 e do
        let ins = dec.Decode()
        f ins
// scan whole .text for "jmp [rip+iat]" thunks (they usually have no pdata)
let sections =
    let nsec = int (BitConverter.ToUInt16(img, pe + 6))
    let optSize = int (BitConverter.ToUInt16(img, pe + 20))
    [ for i in 0 .. nsec - 1 do
        let sh = pe + 24 + optSize + i * 40
        let name = Text.Encoding.ASCII.GetString(img, sh, 8).TrimEnd('\000')
        let vs, va, ch = u32 (sh + 8), u32 (sh + 12), u32 (sh + 36)
        if ch &&& 0x20000000u <> 0u then yield name, va, vs ]
for (_, va, vs) in sections do
    for o in int va .. int (va + vs) - 6 do
        if img.[o] = 0xFFuy && img.[o + 1] = 0x25uy then
            let t = uint64 (int64 o + 6L + int64 (BitConverter.ToInt32(img, o + 2)))
            match iat.TryGetValue t with
            | true, nm when interesting nm ->
                let pre = if o >= 2 && img.[o - 2] = 0x33uy && img.[o - 1] = 0xC9uy then Some (o - 2) else None
                thunks.[uint64 o] <- nm
                match pre with Some p -> thunks.[uint64 p] <- nm + "(0)" | None -> ()
            | _ -> ()
let calls = ResizeArray<string * uint32 * uint64 * string>()   // name, fnStart, site, arg
let pauseFns = ResizeArray<uint32 * int * string>()
for i in 0 .. n - 1 do
    let s, e = begins.[i], ends.[i]
    let mutable lastEcx = ""
    let mutable pauses = 0
    let loops = ResizeArray<string>()
    decodeRange s e (fun ins ->
        if ins.Mnemonic = Mnemonic.Pause then
            pauses <- pauses + 1
        if ins.Mnemonic = Mnemonic.Xor && ins.Op0Register = Register.ECX && ins.Op1Register = Register.ECX then lastEcx <- "0"
        elif ins.Mnemonic = Mnemonic.Mov && ins.Op0Register = Register.ECX && ins.Op1Kind = OpKind.Immediate32 then lastEcx <- string ins.Immediate32
        elif ins.Op0Register = Register.ECX || ins.Op0Register = Register.RCX || ins.Op0Register = Register.CL then lastEcx <- "?"
        if ins.FlowControl = FlowControl.Call || ins.FlowControl = FlowControl.IndirectCall then
            let name =
                if ins.IsIPRelativeMemoryOperand then (match iat.TryGetValue ins.IPRelativeMemoryAddress with | true, nm -> nm | _ -> "")
                elif ins.FlowControl = FlowControl.Call then (match thunks.TryGetValue ins.NearBranchTarget with | true, nm -> nm | _ -> "")
                else ""
            if name <> "" && interesting name then calls.Add((name, s, ins.IP, (if name.EndsWith "(0)" then "0" else lastEcx)))
            lastEcx <- "?")
    if pauses > 0 then
        // loops containing pause: backward branches
        decodeRange s e (fun ins ->
            if (ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch) && ins.NearBranchTarget < ins.IP && ins.NearBranchTarget >= uint64 s then
                let t = ins.NearBranchTarget
                let mutable hasPause = false
                decodeRange (uint32 t) (uint32 ins.IP) (fun j -> if j.Mnemonic = Mnemonic.Pause then hasPause <- true)
                if hasPause then loops.Add(sprintf "%X..%X" t ins.IP))
        pauseFns.Add((s, pauses, String.Join(" ", loops)))

let sb = Text.StringBuilder()
let pr (s: string) = sb.AppendLine s |> ignore
pr (sprintf "# spinscan  thunks=%d  calls=%d  pause-fragments=%d" thunks.Count calls.Count pauseFns.Count)
pr "## thunks"
for kv in thunks |> Seq.sortBy (fun kv -> kv.Key) do pr (sprintf "  %X  %s" kv.Key kv.Value)
pr "## call counts by import (arg)"
for (k, g) in calls |> Seq.groupBy (fun (nm, _, _, a) -> nm.Replace("(0)", "") + "(" + a + ")") |> Seq.sortByDescending (fun (_, g) -> Seq.length g) do
    pr (sprintf "  %5d  %s" (Seq.length g) k)
pr "## all call sites: import(arg) fragmentStart site"
for (nm, s, ip, a) in calls |> Seq.sortBy (fun (nm, _, ip, _) -> nm, ip) do
    pr (sprintf "  %-36s fn %8X  site %8X" (nm.Replace("(0)", "") + "(" + a + ")") s ip)
pr "## fragments containing PAUSE: start count loops"
for (s, c, l) in pauseFns do pr (sprintf "  %8X  %3d  %s" s c l)
Directory.CreateDirectory(Path.GetDirectoryName outPath) |> ignore
File.WriteAllText(outPath, sb.ToString())
printfn "%s" (sb.ToString().Split('\n') |> Seq.takeWhile (fun l -> not (l.StartsWith "## all")) |> String.concat "\n")
printfn "wrote %s" outPath
