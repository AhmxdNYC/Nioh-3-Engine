// wj_common.fsx - shared static-analysis helpers for the worker-job optimisation scripts (notes\worker-jobs-opt.md).
// Dump = re\Nioh3.dump.exe (raw offset == RVA). Helpers copied from re\scripts_workers\workers_live.fsx (walker,
// zero-dataflow, branch-target sweep) so this folder is self-contained; nothing here touches the game.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let img = File.ReadAllBytes DUMP
let DUMP_BASE = 0x7FF74F410000UL

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- PE: sections, .pdata -----------------------------------------------------------------------------------
let u32 (o: int) = BitConverter.ToUInt32(img, o)
let peOff = BitConverter.ToInt32(img, 0x3C)
let optOff = peOff + 24
let sizeOfImage = int (u32 (optOff + 56))
let dataDir = optOff + 112
let pdataRva, pdataSize = int (u32 (dataDir + 24)), int (u32 (dataDir + 28))
let pBegin = Array.init (pdataSize / 12) (fun i -> u32 (pdataRva + i * 12))
let pEnd = Array.init (pdataSize / 12) (fun i -> u32 (pdataRva + i * 12 + 4))
let pUnwind = Array.init (pdataSize / 12) (fun i -> u32 (pdataRva + i * 12 + 8))
/// .pdata entry containing rva: (begin, end, unwindRva)
let pdataOf (rva: uint32) =
    let i = Array.BinarySearch(pBegin, rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && rva < pEnd.[i] then Some (pBegin.[i], pEnd.[i], pUnwind.[i]) else None
/// follow UNW_FLAG_CHAININFO to the primary function entry
let rec primaryEntry (rva: uint32) =
    match pdataOf rva with
    | None -> None
    | Some (b, _, unw) ->
        let flags = int img.[int unw] >>> 3
        if flags &&& 4 <> 0 then
            let cnt = int img.[int unw + 2]
            let chainOff = int unw + 4 + ((cnt + 1) &&& ~~~1) * 2
            primaryEntry (u32 chainOff)
        else Some b
let execSections =
    let nsec = int (BitConverter.ToUInt16(img, peOff + 6))
    let optsz = int (BitConverter.ToUInt16(img, peOff + 20))
    [ for i in 0 .. nsec - 1 do
        let sh = peOff + 24 + optsz + i * 40
        let vs = BitConverter.ToInt32(img, sh + 8)
        let va = BitConverter.ToInt32(img, sh + 12)
        let ch = BitConverter.ToUInt32(img, sh + 36)
        if ch &&& 0x20000000u <> 0u then yield va, vs ]

// ---- decoding -----------------------------------------------------------------------------------------------
let decodeAt (rva: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(img, int rva, 16))
    d.IP <- rva
    d.Decode()
let decodeAll (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    [ while d.IP < ip + uint64 bytes.Length do yield d.Decode() ]
let fmtIns =
    let fmt = NasmFormatter()
    fmt.Options.RipRelativeAddresses <- true
    let so = StringOutput()
    fun (ins: Instruction) -> (let mutable i = ins in fmt.Format(&i, so); so.ToStringAndReset())
let printDis (bytes: byte[]) (at: uint64) (B: uint64) =
    for ins in decodeAll bytes at do
        let note =
            if ins.IsJmpNear || ins.IsJccNear || ins.IsJccShort || ins.IsJmpShort then
                let t = ins.NearBranchTarget
                if t >= B && t < B + 0x6000000UL && (t < at || t >= at + uint64 bytes.Length) then sprintf "   ; -> Nioh3+%X" (t - B) else ""
            else ""
        printfn "  %016X  %-46s%s" ins.IP (fmtIns ins) note
let boundariesOf (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    let s = HashSet<uint64>()
    while d.IP < ip + uint64 bytes.Length do s.Add d.IP |> ignore; d.Decode() |> ignore
    s

// ---- static proofs (from workers_live.fsx) ------------------------------------------------------------------
let infoFactory = InstructionInfoFactory()
let volatileRegs = [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ]
let argRegs = [ Register.RCX; Register.RDX; Register.R8; Register.R9 ]
let allStatus = RflagsBits.OF ||| RflagsBits.SF ||| RflagsBits.ZF ||| RflagsBits.AF ||| RflagsBits.CF ||| RflagsBits.PF
let isShiftByCl (ins: Instruction) =
    ins.OpCount = 2 && ins.Op1Kind = OpKind.Register && ins.Op1Register = Register.CL &&
    (match ins.Mnemonic with
     | Mnemonic.Shl | Mnemonic.Shr | Mnemonic.Sar | Mnemonic.Sal | Mnemonic.Rol | Mnemonic.Ror | Mnemonic.Rcl | Mnemonic.Rcr -> true
     | _ -> false)
/// Walk every path from `starts` until each tracked register is fully overwritten and (if `flags`) all status flags
/// are written. Fails on any read of a still-tracked register/flag and on indirect branches.
let rec walk (starts: uint64 list) (regs: Register list) (flags: bool) (retReadsRax: bool) (depth: int) : string list =
    let regIdx = regs |> List.mapi (fun i r -> r, i) |> dict
    let flagBit = 1 <<< regs.Length
    let full0 = (1 <<< regs.Length) - 1 ||| (if flags then flagBit else 0)
    let seen = HashSet<struct (uint64 * int)>()
    let fails = ResizeArray<string>()
    let stack = Stack<uint64 * int>()
    for s in starts do stack.Push((s, full0))
    while stack.Count > 0 && fails.Count < 8 do
        let ip, m0 = stack.Pop()
        if m0 <> 0 && seen.Add(struct (ip, m0)) then
            let ins = decodeAt ip
            let mutable m = m0
            let fail why = fails.Add(sprintf "%s%X  %s  : %s" (String(' ', 2 * depth)) ip (fmtIns ins) why)
            let tracked (m: int) r = match regIdx.TryGetValue r with | true, i -> (m &&& (1 <<< i)) <> 0 | _ -> false
            let isZeroIdiom =
                (ins.Mnemonic = Mnemonic.Xor || ins.Mnemonic = Mnemonic.Sub) && ins.OpCount = 2 &&
                ins.Op0Kind = OpKind.Register && ins.Op1Kind = OpKind.Register && ins.Op0Register = ins.Op1Register
            let info = infoFactory.GetInfo(&ins)
            if flags && (m &&& flagBit) <> 0 && ins.RflagsRead <> RflagsBits.None then fail "reads flags"
            if not isZeroIdiom then
                for u in info.GetUsedRegisters() do
                    if tracked m (u.Register.GetFullRegister()) then
                        match u.Access with
                        | OpAccess.Read | OpAccess.CondRead | OpAccess.ReadWrite | OpAccess.ReadCondWrite -> fail (sprintf "reads %O" u.Register)
                        | _ -> ()
            match ins.FlowControl with
            | FlowControl.Call ->
                let live = argRegs |> List.filter (tracked m)
                if not live.IsEmpty then
                    if depth >= 4 then fail (sprintf "call depth limit with %A tracked" live)
                    else for f in walk [ ins.NearBranchTarget ] live false false (depth + 1) do fail (sprintf "callee %X: %s" ins.NearBranchTarget f)
            | FlowControl.IndirectCall ->
                let live = argRegs |> List.filter (tracked m)
                if not live.IsEmpty then fail (sprintf "virtual call with %A tracked" live)
            | FlowControl.Return -> if retReadsRax && tracked m Register.RAX then fail "ret with rax tracked"
            | FlowControl.IndirectBranch -> fail "indirect branch (cannot follow)"
            | _ -> ()
            for u in info.GetUsedRegisters() do
                if u.Access = OpAccess.Write && u.Register.GetSize() >= 4 then
                    match regIdx.TryGetValue (u.Register.GetFullRegister()) with
                    | true, i -> m <- m &&& ~~~(1 <<< i)
                    | _ -> ()
            if isZeroIdiom && ins.Op0Register.GetSize() >= 4 then
                match regIdx.TryGetValue (ins.Op0Register.GetFullRegister()) with
                | true, i -> m <- m &&& ~~~(1 <<< i)
                | _ -> ()
            if flags && not (isShiftByCl ins) && (ins.RflagsModified &&& allStatus) = allStatus then m <- m &&& ~~~flagBit
            match ins.FlowControl with
            | FlowControl.Call | FlowControl.IndirectCall ->
                for r in volatileRegs do
                    match regIdx.TryGetValue r with
                    | true, i -> m <- m &&& ~~~(1 <<< i)
                    | _ -> ()
                m <- m &&& ~~~flagBit
                stack.Push((ins.NextIP, m))
            | FlowControl.Next | FlowControl.Exception -> stack.Push((ins.NextIP, m))
            | FlowControl.UnconditionalBranch -> stack.Push((ins.NearBranchTarget, m))
            | FlowControl.ConditionalBranch -> stack.Push((ins.NearBranchTarget, m)); stack.Push((ins.NextIP, m))
            | _ -> ()
    List.ofSeq fails

let nonvolatile = set [ Register.RBX; Register.RBP; Register.RSI; Register.RDI; Register.R12; Register.R13; Register.R14; Register.R15 ]
/// Forward dataflow from `entry`: is `reg` provably zero every time execution reaches `site`? (visits, failures)
let provenZeroAt (entry: uint64) (reg: Register) (site: uint64) =
    let full = reg.GetFullRegister()
    let seen = HashSet<struct (uint64 * bool)>()
    let stack = Stack<uint64 * bool>()
    stack.Push((entry, false))
    let fails = ResizeArray<string>()
    let mutable visits = 0
    let mutable steps = 0
    while stack.Count > 0 && fails.Count < 4 && steps < 2000000 do
        steps <- steps + 1
        let ip, z0 = stack.Pop()
        if seen.Add(struct (ip, z0)) then
            let ins = decodeAt ip
            if ip = site then
                visits <- visits + 1
                if not z0 then fails.Add(sprintf "reached %X with %O not provably zero" site reg)
            let info = infoFactory.GetInfo(&ins)
            let writes = info.GetUsedRegisters() |> Seq.exists (fun u -> u.Register.GetFullRegister() = full && (u.Access = OpAccess.Write || u.Access = OpAccess.ReadWrite || u.Access = OpAccess.CondWrite || u.Access = OpAccess.ReadCondWrite))
            let zeroes =
                ins.OpCount = 2 && ins.Op0Kind = OpKind.Register && ins.Op0Register.GetFullRegister() = full && ins.Op0Register.GetSize() >= 4 &&
                (((ins.Mnemonic = Mnemonic.Xor || ins.Mnemonic = Mnemonic.Sub) && ins.Op1Kind = OpKind.Register && ins.Op1Register = ins.Op0Register) ||
                 (ins.Mnemonic = Mnemonic.Mov && (ins.Op1Kind = OpKind.Immediate32 || ins.Op1Kind = OpKind.Immediate32to64 || ins.Op1Kind = OpKind.Immediate64) && ins.GetImmediate 1 = 0UL))
            let mutable z = if zeroes then true elif writes then false else z0
            match ins.FlowControl with
            | FlowControl.Call | FlowControl.IndirectCall ->
                if not (nonvolatile.Contains full) then z <- false
                stack.Push((ins.NextIP, z))
            | FlowControl.Next | FlowControl.Exception -> stack.Push((ins.NextIP, z))
            | FlowControl.UnconditionalBranch -> stack.Push((ins.NearBranchTarget, z))
            | FlowControl.ConditionalBranch -> stack.Push((ins.NearBranchTarget, z)); stack.Push((ins.NextIP, z))
            | FlowControl.IndirectBranch -> fails.Add(sprintf "indirect branch at %X (cannot follow)" ip)
            | _ -> ()
    if steps >= 2000000 then fails.Add "step limit"
    visits, List.ofSeq fails

/// first flag reader/writer after `ip` on the straight-line path: Some(ins, isReader)
let rec firstFlagUser (ip: uint64) k =
    let ins = decodeAt ip
    if k > 40 then None
    elif ins.RflagsRead <> RflagsBits.None then Some (ins, true)
    elif not (isShiftByCl ins) && (ins.RflagsModified &&& allStatus) = allStatus then Some (ins, false)
    elif ins.FlowControl = FlowControl.UnconditionalBranch then firstFlagUser ins.NearBranchTarget (k + 1)
    elif ins.FlowControl <> FlowControl.Next then None
    else firstFlagUser ins.NextIP (k + 1)

/// direct branch/call targets anywhere in executable sections that land strictly inside a patched range
let scanTargets (ranges: (string * uint64 * uint64) list) =
    let hits = ResizeArray<string>()
    for (va, vs) in execSections do
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, va, vs))
        dec.IP <- uint64 va
        let fin = uint64 (va + vs)
        while dec.IP < fin do
            let ins = dec.Decode()
            match ins.FlowControl with
            | FlowControl.UnconditionalBranch | FlowControl.ConditionalBranch | FlowControl.Call ->
                let t = ins.NearBranchTarget
                for (id, lo, hi) in ranges do
                    if t > lo && t < hi then hits.Add(sprintf "%X -> %X (inside %s @%X)" ins.IP t id lo)
            | _ -> ()
    hits

/// sites of all other known patch sets (ASI patches.json + live scripts) that must not overlap ours
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x3BC0C; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4
      0x419A8; 0x4ABD2; 0x1E72C6; 0x1E71CD; 0x3777A40; 0x1FE30D5; 0x1FE300B; 0x1FE3011; 0x1975BAA; 0x20E77B; 0x5AF526
      0x5AF682; 0x5AF69F; 0x5AD89B; 0x40A4C; 0x7D9EA2; 0x1A822D9; 0x5AB6CF; 0x5ACAAA; 0x5AD2BB; 0x4A52D4; 0x4A535E; 0x3D6552
      0x40B0A; 0x40D2F; 0x48C6B; 0x48D06; 0x48DEC; 0x48E3C; 0x1A6BC35; 0x3D6F46; 0x3D6F8B; 0x7D9E66; 0x117D9F
      0x49131; 0x48FB9; 0x49046; 0x49261; 0x4933D; 0x493DC; 0x4122D; 0x41265; 0x41397; 0x414B6; 0x40899; 0x40481; 0x3E7F5; 0x3E821
      0x6EA50; 0x435BB7; 0x160B83; 0x7E82B4; 0x418F68; 0x252677; 0x43312B; 0x38FB9D8 ]
