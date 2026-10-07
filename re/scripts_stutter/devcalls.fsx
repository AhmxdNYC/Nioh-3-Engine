// devcalls.fsx - find ID3D12Device creation calls (PSO / resource / heap / descriptor heap / pipeline library) in the
// dump. Reads re\Nioh3.dump.exe only.
//   dotnet fsi devcalls.fsx hist <disp>          histogram of the `this` source of every call [reg+disp]
//   dotnet fsi devcalls.fsx sites <srcRegex>     every create-method call whose `this` load matches srcRegex
// For each `call qword ptr [rV+disp]` it walks back <= 16 instructions in the same function for the load of rV
// (`mov rV,[rT]`) and of rcx (`mov rcx,...`), and prints both, so a device pointer (e.g. [x+4268h]) is recognisable.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.Collections.Generic
open System.Text.RegularExpressions
open Iced.Intel
open Wj_common

let NAMES = dict [ 0x40, "CreateCommandQueue"; 0x48, "CreateCommandAllocator"; 0x50, "CreateGraphicsPipelineState"
                   0x58, "CreateComputePipelineState"; 0x60, "CreateCommandList"; 0x70, "CreateDescriptorHeap"
                   0x80, "CreateRootSignature"; 0xC8, "GetResourceAllocationInfo"; 0xD8, "CreateCommittedResource"
                   0xE0, "CreateHeap"; 0xE8, "CreatePlacedResource"; 0xF0, "CreateReservedResource"; 0x110, "MakeResident"
                   0x118, "Evict"; 0x120, "CreateFence"; 0x138, "CreateQueryHeap"; 0x148, "CreateCommandSignature"
                   0x160, "CreatePipelineLibrary"; 0x178, "CreatePipelineState"; 0x190, "EnqueueMakeResident"
                   0x1A8, "CreateCommittedResource1"; 0x1B0, "CreateHeap1"; 0x1F0, "CreateStateObject"
                   0x228, "CreateCommittedResource2"; 0x230, "CreatePlacedResource1"; 0x260, "CreateCommittedResource3"
                   0x268, "CreatePlacedResource2" ]

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "hist"

/// all instructions of all .pdata functions (primary + chained fragments decoded as they come)
let calls = List<uint32 * Instruction * string * string>()   // fn, call, vtable-load text, rcx-load text
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        let win = Queue<Instruction>()
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.Mnemonic = Mnemonic.Call && ins.Op0Kind = OpKind.Memory && ins.MemoryBase <> Register.RIP
               && ins.MemoryBase <> Register.None && ins.MemoryIndex = Register.None then
                let disp = int ins.MemoryDisplacement64
                if NAMES.ContainsKey disp then
                    let vreg = ins.MemoryBase
                    let arr = win.ToArray() |> Array.rev
                    let vload = arr |> Array.tryFind (fun x -> x.Op0Kind = OpKind.Register && x.Op0Register = vreg && x.Mnemonic = Mnemonic.Mov)
                    let rload = arr |> Array.tryFind (fun x -> x.Op0Kind = OpKind.Register && x.Op0Register = Register.RCX && (x.Mnemonic = Mnemonic.Mov || x.Mnemonic = Mnemonic.Lea))
                    // second hop: if rcx came from another register, find that register's load
                    let rtxt =
                        match rload with
                        | Some r when r.Op1Kind = OpKind.Register ->
                            let src = r.Op1Register
                            let r2 = arr |> Array.tryFind (fun x -> x.IP < r.IP && x.Op0Kind = OpKind.Register && x.Op0Register = src && x.Mnemonic = Mnemonic.Mov)
                            fmtIns r + (match r2 with Some y -> " <= " + fmtIns y | None -> "")
                        | Some r -> fmtIns r
                        | None -> "?"
                    calls.Add((s, ins, (match vload with Some v -> fmtIns v | None -> "?"), rtxt))
            win.Enqueue ins
            if win.Count > 16 then win.Dequeue() |> ignore

match mode with
| "hist" ->
    let d = if args.Length > 1 then Convert.ToInt32(args.[1], 16) else 0xD8
    let h = Dictionary<string, int>()
    for (_, c, v, r) in calls do
        if int c.MemoryDisplacement64 = d then
            let k = Regex.Replace(r, "^mov rcx,", "")
            h.[k] <- (match h.TryGetValue k with | true, n -> n + 1 | _ -> 1)
    for kv in h |> Seq.sortByDescending (fun kv -> kv.Value) |> Seq.truncate 60 do printfn "%5d  %s" kv.Value kv.Key
| "sites" ->
    let rx = Regex(if args.Length > 1 then args.[1] else "4268h")
    let only = if args.Length > 2 then args.[2].Split(',') |> Array.map (fun s -> Convert.ToInt32(s, 16)) |> Set.ofArray else Set.empty
    for (fn, c, v, r) in calls do
        let d = int c.MemoryDisplacement64
        if rx.IsMatch r && (only.IsEmpty || only.Contains d) then
            let p = match primaryEntry (uint32 c.IP) with Some x -> x | None -> fn
            printfn "%8X  fn %8X  %-28s this: %-50s vt: %s" c.IP p NAMES.[d] r v
| m -> printfn "unknown mode %s" m
