// lockload_live.fsx - LK: "atomic loads" written as `lock cmpxchg [m],zero` in the hot worker/job functions become
// plain loads (notes\worker-jobs-opt.md). Extends J1/M2 (re\scripts_workers\workers_live.fsx) to the object-update,
// meshlet-job and stage functions that the census (re\scripts_wjobs\lk_census.fsx) found.
//
//   dotnet fsi lockload_live.fsx offline            NO GAME ACCESS: per row - bytes vs dump, decoded original is
//                                                   `lock cmpxchg [m],r` with r AND eax/rax provably zero at the site
//                                                   (forward dataflow from the function entry), new bytes are
//                                                   `mov eax/rax,[same m]` (+ `test` when the next flag reader is a ZF
//                                                   jcc, all other flags proven dead at both successors) + NOPs, same
//                                                   length; plain-store-before-load (StoreLoad / Dekker) screen; branch
//                                                   targets inside the rows (whole .text sweep); overlap with all other
//                                                   patch sets; and a native test of every row (orig vs new bytes run
//                                                   on random memory: eax/rax + memory + live flags identical).
//   dotnet fsi lockload_live.fsx dry [groups]       READ-ONLY on the game: per-row state.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi lockload_live.fsx apply|revert [groups]   frozen safeWrite of the rows.
//   dotnet fsi lockload_live.fsx status             same as dry.
// Groups: LKO (object update 1A7BF0 / 1A188C / 121FC0 / 425B0), LKM (meshlet job 4A2D0, stage 130014),
//         LKJ (job system 40800 / 411C0 rows not covered by J1). Default: all.
//
// Why it is the same result: with eax == 0 and src == 0, `lock cmpxchg [m],src` either finds [m] == 0 (stores the 0
// back, ZF=1, eax stays 0 == [m]) or finds [m] != 0 (no store, ZF=0, eax = [m]). In both cases eax = [m] and the
// value in memory is unchanged; only the flags and the locked (exclusive, fenced) access differ. `mov eax,[m]` gives
// the same eax; the flags are proven dead, or the only reader is a ZF jcc and `test eax,eax` gives the same ZF.
// Memory ordering: x86 plain loads are acquire (not reordered with older loads); the only ordering lost is
// StoreLoad, which matters only if a plain store earlier in the thread must be visible before this load (Dekker).
// The offline screen reports every row whose function performs a plain non-stack store on some path since entry /
// the last locked instruction / call before the site; such rows are excluded from the default groups.
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.IO
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel
open type Iced.Intel.AssemblerRegisters
open Mem
open Wj_common

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"

/// (group, site rva, function entry for the dataflow, note)
let SITES =
  [ "LKO", 0x1A18C9, 0x1A188C, "1A188C object-handle resolve: state word [obj] (hot RIP 1A18CD = 46-73 % of 1A188C)"
    "LKO", 0x1A192A, 0x1A188C, "1A188C: bitmap word +4"
    "LKO", 0x1A1935, 0x1A188C, "1A188C: bitmap word"
    "LKO", 0x1A1992, 0x1A188C, "1A188C: [obj+4] flag"
    "LKO", 0x1A7DCD, 0x1A7BF0, "1A7BF0 object update"
    "LKO", 0x1A83D3, 0x1A7BF0, "1A7BF0 object update (hot RIP 1A83D8 up to 10 % of 1A7BF0)"
    "LKO", 0x1A88CE, 0x1A7BF0, "1A7BF0 object update"
    "LKO", 0x1A8EE9, 0x1A7BF0, "1A7BF0 object update"
    "LKO", 0x1AB0B4, 0x1A7BF0, "1A7BF0: load before CAS loop"
    "LKO", 0x1AED09, 0x1A7BF0, "1A7BF0: load before CAS loop"
    "LKO", 0x122BF1, 0x121FC0, "121FC0: load before CAS add loop"
    "LKO", 0x122E1C, 0x121FC0, "121FC0: load before CAS add loop"
    "LKO", 0x123108, 0x121FC0, "121FC0: load before CAS add loop"
    "LKO", 0x123195, 0x121FC0, "121FC0: load before CAS loop"
    "LKO", 0x12333A, 0x121FC0, "121FC0: flag word, ZF consumer"
    "LKO", 0x123429, 0x121FC0, "121FC0: [obj+4034h]"
    "LKO", 0x42877,  0x425B0,  "425B0: load before CAS loop"
    "LKM", 0x4A539,  0x4A2D0,  "4A2D0 TS_MESHLET_REQ job: entry flags"
    "LKM", 0x4A545,  0x4A2D0,  "4A2D0: entry flags"
    "LKM", 0x4A55A,  0x4A2D0,  "4A2D0: [obj+B4h] (hot RIP 4A563)"
    "LKM", 0x4A6F8,  0x4A2D0,  "4A2D0: entry flags"
    "LKM", 0x4A708,  0x4A2D0,  "4A2D0: entry flags"
    "LKM", 0x4A719,  0x4A2D0,  "4A2D0: [obj+B4h]"
    "LKM", 0x4A74A,  0x4A2D0,  "4A2D0: load before CAS loop"
    "LKM", 0x130096, 0x130014, "130014 (stage/stream job): bitmap word, ZF consumer (hot RIP 13009E)"
    "LKM", 0x1301B4, 0x130014, "130014"
    "LKM", 0x1301C5, 0x130014, "130014: ZF consumer"
    "LKM", 0x1301E4, 0x130014, "130014"
    "LKM", 0x130225, 0x130014, "130014"
    "LKJ", 0x40921,  0x40800,  "40800 JobWorkerPoll: stack head (64-bit) before pop CAS"
    "LKJ", 0x40A14,  0x40800,  "40800: pool idle count"
    "LKJ", 0x40A55,  0x40800,  "40800: pool worker count"
    "LKJ", 0x40C4D,  0x40800,  "40800"
    "LKJ", 0x40C7E,  0x40800,  "40800"
    "LKJ", 0x40CE4,  0x40800,  "40800: thread flags before CAS"
    "LKJ", 0x40D75,  0x40800,  "40800: 64-bit load before CAS"
    "LKJ", 0x41325,  0x411C0,  "411C0 steal: stack head (64-bit) before pop CAS"
    "LKJ", 0x41447,  0x411C0,  "411C0 steal: stack head (64-bit)"
    "LKJ", 0x41380,  0x411C0,  "411C0: 64-bit, ZF consumer"
    "LKJ", 0x4149F,  0x411C0,  "411C0: 64-bit, ZF consumer" ]

type Row = { Group: string; Rva: int; Fn: int; Note: string; Orig: byte[]; New: byte[]; Errs: string list; Info: string; StoreLoad: string }

let nopFill (n: int) =
    match n with
    | 0 -> [||] | 1 -> [| 0x90uy |] | 2 -> [| 0x66uy; 0x90uy |] | 3 -> [| 0x0Fuy; 0x1Fuy; 0x00uy |]
    | 4 -> [| 0x0Fuy; 0x1Fuy; 0x40uy; 0x00uy |] | 5 -> [| 0x0Fuy; 0x1Fuy; 0x44uy; 0x00uy; 0x00uy |]
    | _ -> failwith "nop size"

/// encode one instruction at ip
let enc (ins: Instruction) (ip: uint64) =
    use ms = new MemoryStream()
    let e = Encoder.Create(64, StreamCodeWriter(ms))
    let mutable i = ins
    e.Encode(&i, ip) |> ignore
    ms.ToArray()

/// does the function use rbp as a frame pointer (lea rbp,[rsp+x] / mov rbp,rsp in the first instructions)?
let rbpFrame (entry: uint64) =
    let mutable ip, k, found = entry, 0, false
    while k < 40 && not found do
        let i = decodeAt ip
        if (i.Mnemonic = Mnemonic.Lea && i.Op0Register = Register.RBP && i.MemoryBase = Register.RSP) ||
           (i.Mnemonic = Mnemonic.Mov && i.Op0Register = Register.RBP && i.Op1Kind = OpKind.Register && i.Op1Register = Register.RSP) then found <- true
        ip <- i.NextIP; k <- k + 1
    found

/// StoreLoad screen: forward dataflow from the entry; state = "a plain store to non-stack memory happened since
/// entry / the last locked instruction / xchg-with-memory / call". Returns the store sites that reach `site`.
let storeLoadScreen (entry: uint64) (site: uint64) =
    let fp = rbpFrame entry
    let seen = HashSet<struct (uint64 * uint64)>()
    let stack = Stack<uint64 * uint64>()             // ip, last plain store ip (0 = clean)
    stack.Push((entry, 0UL))
    let found = HashSet<uint64>()
    let mutable steps = 0
    while stack.Count > 0 && steps < 3000000 do
        steps <- steps + 1
        let ip, st = stack.Pop()
        if seen.Add(struct (ip, st)) then
            let ins = decodeAt ip
            if ip = site then (if st <> 0UL then found.Add st |> ignore)
            else
                let info = infoFactory.GetInfo(&ins)
                let fence = ins.HasLockPrefix || (ins.Mnemonic = Mnemonic.Xchg && ins.Op0Kind = OpKind.Memory) || ins.Mnemonic = Mnemonic.Mfence
                let plainStore =
                    info.GetUsedMemory() |> Seq.exists (fun m ->
                        (m.Access = OpAccess.Write || m.Access = OpAccess.ReadWrite || m.Access = OpAccess.CondWrite) &&
                        m.Base <> Register.RSP && not (fp && m.Base = Register.RBP))    // stack locals
                let st' = if fence then 0UL elif ins.FlowControl = FlowControl.Call || ins.FlowControl = FlowControl.IndirectCall then 0x1UL elif plainStore && not ins.HasLockPrefix then ip else st
                match ins.FlowControl with
                | FlowControl.Return | FlowControl.IndirectBranch | FlowControl.Interrupt -> ()
                | FlowControl.UnconditionalBranch -> stack.Push((ins.NearBranchTarget, st'))
                | FlowControl.ConditionalBranch -> stack.Push((ins.NearBranchTarget, st')); stack.Push((ins.NextIP, st'))
                | _ -> stack.Push((ins.NextIP, st'))
    List.ofSeq found

let mkRow (g: string, rva: int, fn: int, note: string) =
    let errs = ResizeArray<string>()
    let c = decodeAt (uint64 rva)
    let orig = img.[rva .. rva + c.Length - 1]
    let mutable nw = [||]
    let mutable info = ""
    if not (c.Mnemonic = Mnemonic.Cmpxchg && c.HasLockPrefix && c.Op0Kind = OpKind.Memory && c.Op1Kind = OpKind.Register) then
        errs.Add "not lock cmpxchg [m],reg"
    else
        let src = c.Op1Register
        let wide = src.GetSize() = 8
        let acc = if wide then Register.RAX else Register.EAX
        for reg in [ acc; src ] do
            let v, f = provenZeroAt (uint64 fn) reg (uint64 rva)
            if v = 0 then errs.Add (sprintf "site not reached from %X" fn)
            for x in f do errs.Add x
        // flags after the site
        let next = uint64 rva + uint64 c.Length
        let needTest, note2 =
            match firstFlagUser next 0 with
            | Some (f, false) -> false, sprintf "flags dead: overwritten by %X %s" f.IP (fmtIns f)
            | Some (f, true) when f.FlowControl = FlowControl.ConditionalBranch && f.RflagsRead = RflagsBits.ZF ->
                let w = walk [ f.NearBranchTarget; f.NextIP ] [] true false 0
                for x in w do errs.Add ("after jcc: " + x)
                true, sprintf "ZF consumed by %X %s; all flags dead at both successors" f.IP (fmtIns f)
            | Some (f, true) -> errs.Add (sprintf "flags read by %X %s" f.IP (fmtIns f)); false, ""
            | None -> errs.Add "flag state unknown (control flow before a flag writer)"; false, ""
        info <- note2
        // new bytes: mov acc,[same m] (+ test acc,acc) + NOP fill
        let mutable mo = MemoryOperand(c.MemoryBase, c.MemoryIndex, c.MemoryIndexScale, int64 c.MemoryDisplacement64, c.MemoryDisplSize, false, Register.None)
        let mv = if wide then Instruction.Create(Code.Mov_r64_rm64, Register.RAX, &mo) else Instruction.Create(Code.Mov_r32_rm32, Register.EAX, &mo)
        let mb = enc mv (uint64 rva)
        let tb = if needTest then (if wide then [| 0x48uy; 0x85uy; 0xC0uy |] else [| 0x85uy; 0xC0uy |]) else [||]
        let body = Array.append mb tb
        if body.Length > orig.Length then errs.Add (sprintf "new code %d bytes > original %d" body.Length orig.Length)
        else
            nw <- Array.append body (nopFill (orig.Length - body.Length))
            // re-decode and compare operands
            match decodeAll nw (uint64 rva) with
            | m :: rest when m.Mnemonic = Mnemonic.Mov && m.Op0Register = acc && m.MemoryBase = c.MemoryBase && m.MemoryIndex = c.MemoryIndex
                             && m.MemoryIndexScale = c.MemoryIndexScale && m.MemoryDisplacement64 = c.MemoryDisplacement64 && m.MemorySize = c.MemorySize ->
                let rest2 = if needTest then (match rest with t :: tl when t.Mnemonic = Mnemonic.Test -> tl | _ -> (errs.Add "missing test"; rest)) else rest
                for x in rest2 do if x.Mnemonic <> Mnemonic.Nop then errs.Add (sprintf "filler is not a NOP: %s" (fmtIns x))
            | _ -> errs.Add "new bytes are not mov acc,[same memory operand]"
    for s in OTHER_SITES do
        if s < rva + orig.Length && rva < s + 10 && s <> rva then errs.Add (sprintf "overlaps patch site %X" s)
    if List.contains rva OTHER_SITES then errs.Add "already patched by another set"
    let sl = storeLoadScreen (uint64 fn) (uint64 rva)
    let slNote =
        match sl with
        | [] -> "no plain non-stack store reaches the site within the function"
        | xs -> sprintf "REVIEW: plain store(s) reach the site: %s" (xs |> List.map (fun x -> if x = 1UL then "call" else sprintf "%X" x) |> String.concat ", ")
    { Group = g; Rva = rva; Fn = fn; Note = note; Orig = orig; New = nw; Errs = List.ofSeq errs; Info = info; StoreLoad = slNote }

let rowsAll = SITES |> List.map mkRow
/// default selection: rows that pass and have a clean StoreLoad screen
let isClean (r: Row) = r.Errs.IsEmpty && r.StoreLoad.StartsWith "no plain"

// ---- native row test: execute orig vs new on random memory --------------------------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type RowFn = delegate of nativeint * nativeint -> unit

/// For a row: a thunk loads every GPR used by the memory operand so the address lands in a 4 KB buffer, sets the
/// accumulator and the source register to 0, runs the instruction bytes, then stores rax and RFLAGS. Compares rax,
/// the memory word, and (if the row consumes ZF) ZF, between the original and the new bytes.
let nativeTest (r: Row) =
    let c = decodeAt (uint64 r.Rva)
    let mem = VirtualAlloc(0n, 0x10000un, 0x3000u, 0x40u)
    let buf = mem + 0x8000n
    let code = mem
    let regsUsed = [ c.MemoryBase; c.MemoryIndex ] |> List.filter (fun x -> x <> Register.None) |> List.map (fun x -> x.GetFullRegister())
    let rnd = Random(r.Rva)
    let run (bytes: byte[]) (target: nativeint) (baseVal: int64) (idxVal: int64) =
        let t = assemble code (fun a ->
            a.push(rbx); a.push(rbp); a.push(rsi); a.push(rdi); a.push(r12); a.push(r13); a.push(r14); a.push(r15)
            a.push(rdx)                                   // out*
            for reg in [ Register.RBX; Register.RBP; Register.RSI; Register.RDI; Register.R8; Register.R9; Register.R10; Register.R11; Register.R12; Register.R13; Register.R14; Register.R15; Register.RCX ] do
                a.mov(AssemblerRegister64(reg), 0x5A5A5A5AL)
            match regsUsed with
            | [ b ] -> a.mov(AssemblerRegister64(b), baseVal)
            | [ b; i ] when b = i -> a.mov(AssemblerRegister64(b), baseVal)
            | [ b; i ] -> a.mov(AssemblerRegister64(b), baseVal); a.mov(AssemblerRegister64(i), idxVal)
            | _ -> ()
            let src = c.Op1Register.GetFullRegister()
            if not (List.contains src regsUsed) then a.xor(AssemblerRegister64(src), AssemblerRegister64(src))
            a.xor(eax, eax)
            a.db(bytes)
            a.pushfq()
            a.pop(rcx)
            a.pop(rdx)
            a.mov(__qword_ptr.[rdx], rax); a.mov(__qword_ptr.[rdx + 8L], rcx)
            a.pop(r15); a.pop(r14); a.pop(r13); a.pop(r12); a.pop(rdi); a.pop(rsi); a.pop(rbp); a.pop(rbx)
            a.ret())
        Marshal.Copy(t, 0, code, t.Length)
        let out = Marshal.AllocHGlobal 16
        let f = Marshal.GetDelegateForFunctionPointer<RowFn>(code)
        f.Invoke(0n, out)
        let rax, fl = Marshal.ReadInt64 out, Marshal.ReadInt64(out + 8n)
        Marshal.FreeHGlobal out
        rax, fl
    let mutable bad, n = 0, 0
    let srcInMem = regsUsed |> List.contains (c.Op1Register.GetFullRegister())
    for k in 1 .. (if srcInMem then 0 else 400) do
        // choose base / index so the effective address is buf + 0x100 (aligned to the access size)
        let scale = int64 (max 1 c.MemoryIndexScale)
        let disp = int64 c.MemoryDisplacement64
        let target = buf + 0x100n
        let idxVal = if c.MemoryIndex <> Register.None && regsUsed.Length = 2 && regsUsed.[0] <> regsUsed.[1] then int64 (rnd.Next(0, 64)) else 0L
        let baseVal =
            if c.MemoryBase = Register.None then 0L
            elif regsUsed.Length = 2 && regsUsed.[0] = regsUsed.[1] then (int64 target - disp) / (1L + scale)
            else int64 target - disp - idxVal * scale
        let ea = if c.MemoryBase = Register.None then int64 target else (if regsUsed.Length = 2 && regsUsed.[0] = regsUsed.[1] then baseVal * (1L + scale) + disp else baseVal + idxVal * scale + disp)
        let v = match rnd.Next 4 with 0 -> 0L | 1 -> 1L | 2 -> int64 (rnd.Next()) | _ -> rnd.NextInt64()
        Marshal.WriteInt64(nativeint ea, v)
        let r1, f1 = run r.Orig code baseVal idxVal
        let m1 = Marshal.ReadInt64(nativeint ea)
        Marshal.WriteInt64(nativeint ea, v)
        let r2, f2 = run r.New code baseVal idxVal
        let m2 = Marshal.ReadInt64(nativeint ea)
        let zfMask = 0x40L
        let flagsOk = if r.Info.StartsWith "ZF" then (f1 &&& zfMask) = (f2 &&& zfMask) else true
        if r1 <> r2 || m1 <> m2 || not flagsOk then (bad <- bad + 1; if bad < 4 then printfn "    !! %X: v=%X rax %X/%X mem %X/%X fl %X/%X" r.Rva v r1 r2 m1 m2 f1 f2)
        n <- n + 1
    (n, bad)

let offline () =
    let mutable ok = true
    for r in rowsAll do
        printfn "%-3s %8X %-30s -> %-30s %s" r.Group r.Rva (hexs r.Orig) (hexs r.New) (if r.Errs.IsEmpty then "[ok]" else "[FAIL]")
        printfn "      %s | %s" r.Note (fmtIns (decodeAt (uint64 r.Rva)))
        printfn "      %s; %s" r.Info r.StoreLoad
        for e in r.Errs do printfn "      !! %s" e
        if r.Errs.IsEmpty then
            let n, bad = nativeTest r
            printfn "      native: %d random memory values, %d differences (rax, memory word%s)" n bad (if r.Info.StartsWith "ZF" then ", ZF" else "")
            if bad <> 0 then ok <- false
    let clean = rowsAll |> List.filter isClean
    let failed = rowsAll |> List.filter (fun r -> not r.Errs.IsEmpty)
    let review = rowsAll |> List.filter (fun r -> r.Errs.IsEmpty && not (isClean r))
    printfn "branch targets strictly inside any row (whole executable sweep):"
    let hits = scanTargets [ for r in rowsAll -> r.Group, uint64 r.Rva, uint64 r.Rva + uint64 r.Orig.Length ]
    if hits.Count = 0 then printfn "  none" else (for h in hits do printfn "  !! %s" h)
    printfn "summary: %d rows; %d clean (default set), %d need StoreLoad review (excluded), %d failed proofs (excluded)" rowsAll.Length clean.Length review.Length failed.Length
    for g in [ "LKO"; "LKM"; "LKJ" ] do
        printfn "  %s default rows: %s" g (clean |> List.filter (fun r -> r.Group = g) |> List.map (fun r -> sprintf "%X" r.Rva) |> String.concat " ")
    // ASI-ready rows
    printfn "ASI rows (patches.json style, kind=bytes):"
    for r in clean do
        printfn "  {\"id\": \"%s_%X\", \"group\": \"%s\", \"rva\": \"0x%X\", \"len\": %d, \"orig\": \"%s\", \"kind\": \"bytes\", \"new\": \"%s\"}" r.Group r.Rva r.Group r.Rva r.Orig.Length (hexs r.Orig) (hexs r.New)
    ok <- ok && hits.Count = 0
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED (default set = clean rows)" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live -----------------------------------------------------------------------------------------------------
let selected = (if args.Length > 1 then args.[1] else "LKO,LKM,LKJ").ToUpperInvariant().Split(',') |> Set.ofArray
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let rows = rowsAll |> List.filter (fun r -> isClean r && selected.Contains r.Group)
    let state (r: Row) =
        let cur = t.Read(nativeint (B + uint64 r.Rva), r.Orig.Length)
        if cur = r.Orig then "original" elif cur = r.New then "patched" else "UNKNOWN"
    match mode with
    | "dry" | "status" ->
        for r in rows do printfn "  %s %8X %s" r.Group r.Rva (state r)
    | "apply" | "revert" ->
        if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
        let toNew = mode = "apply"
        if rows |> List.exists (fun r -> state r = "UNKNOWN") then failwith "unexpected bytes - aborting"
        let ws = [ for r in rows do
                     let s = state r
                     if (toNew && s = "original") || ((not toNew) && s = "patched") then
                         yield nativeint (B + uint64 r.Rva), (if toNew then r.New else r.Orig), (if toNew then r.Orig else r.New) ]
        let n = if ws.IsEmpty then 0 else safeWrite t ws
        printfn "%s %d rows (attempt %d)" (if toNew then "applied" else "reverted") ws.Length n
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
