// jobs_live.fsx - KIDS job-system patches for Nioh 3 (Steam v2.02). See notes\job-system.md.
//
//   dotnet fsi jobs_live.fsx offline                 NO GAME ACCESS. Verifies every site against re\Nioh3.dump.exe, builds
//                                                    the WK cave at a dummy address, proves the register/flag conditions,
//                                                    sweeps .text for branch targets inside patched bytes, checks every
//                                                    ASI pattern is unique, and runs two native harnesses in THIS process:
//                                                    WK cave vs synthetic pools, WC = the real SizeWorkerPools code copied
//                                                    from the dump (original vs patched vs an independent model).
//   dotnet fsi jobs_live.fsx dry                     READ-ONLY on the live game: site states + live pool table.
//   dotnet fsi jobs_live.fsx apply [WK]              WRITES (FT_ALLOW_WRITE=1): WK cave + site (one cave per process).
//   dotnet fsi jobs_live.fsx on|off [WK]             WRITES: WK cave switch only (no code bytes touched).
//   dotnet fsi jobs_live.fsx revert [WK]             WRITES: original bytes back (only while no thread is inside the cave).
//   dotnet fsi jobs_live.fsx status                  READ-ONLY: states, WK wakes kept/skipped per second, live pool counts.
//   dotnet fsi jobs_live.fsx boot <cap> [p12] [sec]  WRITES at startup (FT_ALLOW_WRITE=1): start this FIRST, then launch the
//                                                    game. Waits for SteamStub to decrypt .text, then writes WorkerCap (and
//                                                    P12 if asked) before SizeWorkerPools runs. Same mechanism as
//                                                    patcher\pool_patch.fsx. The exe on disk is never touched.
//   dotnet fsi jobs_live.fsx counters [sec] [proc]   READ-ONLY, no handle to the game process: NtQuerySystemInformation
//                                                    per-thread kernel/user CPU and context switches/s, process page
//                                                    faults/s (thread names via OpenThread QUERY_LIMITED + GetThreadDescription).
//
// Patches:
//   WK  wake gate. 40800 (JobWorkerPoll) wakes ONE sleeping worker on the first job it pops per wait session (flag *param_3)
//       and on every 30th pop while all other workers sleep (40A01..40B10, SetEvent at 40B0A). It never checks whether any
//       work is left, so a single-job group (push 1, pop it yourself) always wakes a worker that finds nothing and goes back
//       to sleep: SetEvent + 2 context switches + a full steal scan. The gate runs before that wake: if no queue of the pool
//       has any priority bit set, skip the wake (and clear the flag, exactly like the stock "no sleeper found" path).
//       Site 40A4C `mov rcx,[r15+440h]` (7 B) -> `call cave; nop2`. Keep path: `mov rcx,[r15+440h]; ret` (= original).
//       Skip path: return address += 0xBF (40A51 -> 40B10, the stock end of the wake block), r8 := 0. Every other register
//       and the flags are preserved, so the state at 40B1B equals the stock flag==0 path (je 40B1B at 40A46) except rax
//       (= &flag, as on the stock wake path) and the flags - both proven dead at 40B1B in offline.
//   WC  WorkerCap (boot-time). SizeWorkerPools 1FE2FD4 clamps the logical CPU count to 6..32 and sizes every weighted pool
//       as count = clamp(trunc(f(base)/13 * weight), 1, 32). Lowering the upper clamp shrinks all weighted pools at once:
//       1FE300B `41 83 FB 20` (cmp r11d,32) -> imm = cap; 1FE3011 `B0 20` (mov al,32) -> imm = cap. r11d (raw count) still
//       drives the <=6 / <=8 / <=12 special cases, so on a 16-thread APU no penalty kicks in. cap must be 6..32.
//
// Safety model = re\scripts_workers\workers_live.fsx: original-byte checks, Mem.fsx safeWrite (process frozen, thread-RIP
// boundary check). WK revert additionally refuses while any thread RIP is inside the cave (a thread in the cave has the
// return address 40A51, which is not an instruction boundary of the original bytes).
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open System.Diagnostics
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "jobs_live_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let img = File.ReadAllBytes DUMP
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- sites -------------------------------------------------------------------------------------------------------
let WK_SITE, WK_ORIG = 0x40A4C, "49 8B 8F 40 04 00 00"     // mov rcx,[r15+440h]  (first instruction of the wake block)
let WK_RET = 0x40A51UL                                     // return address of the 5-byte call
let WK_CONT = 0x40A53UL                                    // xor eax,eax (rest of the stock wake block)
let WK_SKIP = 0x40B10UL                                    // mov rax,[rsp+0C0h]; mov [rax],sil  (stock end of the wake block)
let WK_DELTA = int (WK_SKIP - WK_RET)                      // 0xBF
let WC_A, WC_A_ORIG = 0x1FE300B, "41 83 FB 20"             // cmp r11d,20h
let WC_B, WC_B_ORIG = 0x1FE3011, "B0 20"                   // mov al,20h
let P12_SITE, P12_ORIG, P12_NEW = 0x1FE30D5, "41 83 FB 0C", "41 83 FB 00"
let wcA (cap: int) = [| 0x41uy; 0x83uy; 0xFBuy; byte cap |]
let wcB (cap: int) = [| 0xB0uy; byte cap |]
let POOL_TABLE = 0x4B722D0                                 // g_WorkerPoolTable: 74 records x 18h (+0 name*, +8 prio, +0Ch count u8, +10h weight i32)
let POOL_NAMES =
    [| "[App]Game"; "MOTOR_RUN_SYNERGISTIC"; "MOTOR_RUN_COMMAND_LIST_SUBMIT"; "KIDSTaskScheduler"; "KIDS_RESOURCE"; "ResourceLoad"
       "StageUpdate"; "CharacterUpdate"; "RequestLoad"; "TrySync"; "CollisionUpdate"; "StgGdtUpd"; "PhysUpdate"; "UpdStgEdt"
       "UpdSclEdt"; "BackupThread"; "MiscAsync" |]
let poolName i = if i < POOL_NAMES.Length then POOL_NAMES.[i] elif i = 48 then "[App]ProceduralUpdateThread" elif i = 55 then "[App]UpdateStageEffectThread"
                 elif i = 56 then "[App]EffectUpdateDispThread" elif i = 71 then "Dummy:CharacterObjectUpdate" elif i = 72 then "Dummy:StageGadgetUpdate"
                 elif i = 73 then "Dummy:StageObjectUpdate" else sprintf "pool%d" i

/// sites of the other patch sets (cpu_caves, fg_cave, ft_vrr, mainthread, W1/W2, meshlet_caves, pool_patch, workers_live)
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4
      0x419A8; 0x4ABD2; 0x1E72C6; 0x1E71CD; 0x3777A40
      // workers_live.fsx M1/M2/J1
      0x49131; 0x48FB9; 0x49046; 0x49261; 0x4933D; 0x493DC; 0x4122D; 0x41265; 0x41397; 0x414B6; 0x40899; 0x40481; 0x3E7F5; 0x3E821 ]

// ---- WK cave -----------------------------------------------------------------------------------------------------
type Cave = { Bytes: byte[]; Entry: uint64; En: uint64; Keep: uint64; Skip: uint64 }

/// Data: +0 en (1 = gate on), +40h wakes kept (statistics, plain inc), +80h wakes skipped. Code from +C0h.
/// Entered by `call` from 40A4C (return address 40A51). Live: r15 = worker (0x458-byte record), [r15+440h] = pool,
/// pool+10h = worker array, pool+28h = worker count, worker+448h -> 4 priority-bitmap dwords (128 priorities).
let build (at: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, lKeepN, lSkipN = L(), L(), L()
    let mutable entry, scan, next, keepCount, keep = L(), L(), L(), L(), L()
    a.Label(&lEn); a.dd(1u); for _ in 1 .. 15 do a.dd(0u)
    a.Label(&lKeepN); a.dd(0u); for _ in 1 .. 15 do a.dd(0u)
    a.Label(&lSkipN); a.dd(0u); for _ in 1 .. 15 do a.dd(0u)
    a.Label(&entry)
    a.pushfq()
    a.push(rax); a.push(rdx); a.push(r8); a.push(r9)                 // return address now at [rsp+28h]
    a.cmp(__dword_ptr.[lEn], 0); a.je(keep)
    a.mov(rax, __qword_ptr.[r15 + 0x440L])                           // pool
    a.mov(r8d, __dword_ptr.[rax + 0x28L])                            // worker count
    a.mov(rax, __qword_ptr.[rax + 0x10L])                            // worker array
    a.test(r8d, r8d); a.jle(keepCount)                               // no count: stock
    a.Label(&scan)
    a.mov(rdx, __qword_ptr.[rax + 0x448L])                           // this worker's priority bitmap
    a.test(rdx, rdx); a.je(next)
    a.mov(r9d, __dword_ptr.[rdx]); a.``or``(r9d, __dword_ptr.[rdx + 4L]); a.``or``(r9d, __dword_ptr.[rdx + 8L]); a.``or``(r9d, __dword_ptr.[rdx + 0xCL])
    a.jne(keepCount)                                                 // some queue has work -> stock wake
    a.Label(&next)
    a.add(rax, 0x458); a.dec(r8d); a.jne(scan)
    // nothing queued anywhere in this pool: skip the wake, continue at 40B10 (clears the flag like the stock no-sleeper path)
    a.inc(__dword_ptr.[lSkipN])
    a.add(__qword_ptr.[rsp + 0x28L], WK_DELTA)
    a.mov(__qword_ptr.[rsp + 8L], 0)                                 // saved r8 := 0, the value of the stock flag==0 path
    a.pop(r9); a.pop(r8); a.pop(rdx); a.pop(rax); a.popfq()
    a.ret()
    a.Label(&keepCount)
    a.inc(__dword_ptr.[lKeepN])
    a.Label(&keep)
    a.pop(r9); a.pop(r8); a.pop(rdx); a.pop(rax); a.popfq()
    a.mov(rcx, __qword_ptr.[r15 + 0x440L])                           // the original instruction
    a.ret()
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); Entry = addr entry; En = addr lEn; Keep = addr lKeepN; Skip = addr lSkipN }
    if c.Keep - at <> 0x40UL || c.Skip - at <> 0x80UL || c.Entry - at <> 0xC0UL then failwith "cave layout"
    c

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let wkNew (B: uint64) (c: Cave) = Array.concat [ [| 0xE8uy |]; rel32 (B + uint64 WK_SITE + 5UL) c.Entry; hexb "66 90" ]

// ---- disassembly + static proofs (same walker as workers_live.fsx) ----------------------------------------------------
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
let printDis (bytes: byte[]) (at: uint64) =
    for ins in decodeAll bytes at do printfn "  %016X  %s" ins.IP (fmtIns ins)

let infoFactory = InstructionInfoFactory()
let volatileRegs = [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ]
let argRegs = [ Register.RCX; Register.RDX; Register.R8; Register.R9 ]
let allStatus = RflagsBits.OF ||| RflagsBits.SF ||| RflagsBits.ZF ||| RflagsBits.AF ||| RflagsBits.CF ||| RflagsBits.PF
let isShiftByCl (ins: Instruction) =
    ins.OpCount = 2 && ins.Op1Kind = OpKind.Register && ins.Op1Register = Register.CL &&
    (match ins.Mnemonic with
     | Mnemonic.Shl | Mnemonic.Shr | Mnemonic.Sar | Mnemonic.Sal | Mnemonic.Rol | Mnemonic.Ror | Mnemonic.Rcl | Mnemonic.Rcr -> true
     | _ -> false)
/// Every path from `starts`: fails on a read of a tracked register / flags before it is fully overwritten.
let rec walk (starts: uint64 list) (regs: Register list) (flags: bool) (depth: int) : string list =
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
                    else for f in walk [ ins.NearBranchTarget ] live false (depth + 1) do fail (sprintf "callee %X: %s" ins.NearBranchTarget f)
            | FlowControl.IndirectCall ->
                let live = argRegs |> List.filter (tracked m)
                if not live.IsEmpty then fail (sprintf "virtual call with %A tracked" live)
            | FlowControl.Return -> if tracked m Register.RAX then fail "ret with rax tracked"
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

let execSections () =
    let pe = BitConverter.ToInt32(img, 0x3C)
    let nsec = int (BitConverter.ToUInt16(img, pe + 6))
    let optsz = int (BitConverter.ToUInt16(img, pe + 20))
    [ for i in 0 .. nsec - 1 do
        let sh = pe + 24 + optsz + i * 40
        let vs = BitConverter.ToInt32(img, sh + 8)
        let va = BitConverter.ToInt32(img, sh + 12)
        let ch = BitConverter.ToUInt32(img, sh + 36)
        if ch &&& 0x20000000u <> 0u then yield va, min vs (img.Length - va) ]
/// direct branch/call targets anywhere in executable sections that land strictly inside a patched range
let scanTargets (ranges: (string * uint64 * uint64) list) =
    let hits = ResizeArray<string>()
    for (va, vs) in execSections () do
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
/// IDA-style pattern matches in executable sections
let patternHits (pattern: string) =
    let pat = pattern.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun s -> if s.StartsWith "?" then -1 else Convert.ToInt32(s, 16))
    [ for (va, vs) in execSections () do
        for i in va .. va + vs - pat.Length do
            let mutable j = 0
            while j < pat.Length && (pat.[j] = -1 || int img.[i + j] = pat.[j]) do j <- j + 1
            if j = pat.Length then yield i ]

// ---- ASI site entries (patches.json style) ------------------------------------------------------------------------
type AsiSite = { Id: string; Group: string; Rva: int; Len: int; Orig: string; Kind: string; New: string; Target: string; Pattern: string; Offset: int }
let asiSites =
  [ { Id = "WK"; Group = "WK"; Rva = WK_SITE; Len = 7; Orig = WK_ORIG; Kind = "call"; New = ""; Target = "caveWakeGate"
      Pattern = "45 84 C0 0F 84 ?? ?? ?? ?? 49 8B 8F 40 04 00 00 33 C0 F0 0F B1 71 2C"; Offset = 9 }
    { Id = "WCa"; Group = "WC"; Rva = WC_A; Len = 4; Orig = WC_A_ORIG; Kind = "bytes"; New = "41 83 FB 08"; Target = ""
      Pattern = "41 83 FB 20 7E 04 B0 20 EB 0A 44 3B DD 41 0F B6 C3 0F 4C C5 88 87 40 07 00 00"; Offset = 0 }
    { Id = "WCb"; Group = "WC"; Rva = WC_B; Len = 2; Orig = WC_B_ORIG; Kind = "bytes"; New = "B0 08"; Target = ""
      Pattern = "41 83 FB 20 7E 04 B0 20 EB 0A 44 3B DD 41 0F B6 C3 0F 4C C5 88 87 40 07 00 00"; Offset = 6 } ]
let asiJson (s: AsiSite) =
    if s.Kind = "call" then
        sprintf """{"id": "%s", "group": "%s", "rva": "0x%X", "len": %d, "orig": "%s", "kind": "call", "target": "%s", "call_at": 0, "pattern": "%s", "offset": %d}""" s.Id s.Group s.Rva s.Len s.Orig s.Target s.Pattern s.Offset
    else
        sprintf """{"id": "%s", "group": "%s", "rva": "0x%X", "len": %d, "orig": "%s", "kind": "bytes", "new": "%s", "pattern": "%s", "offset": %d}""" s.Id s.Group s.Rva s.Len s.Orig s.New s.Pattern s.Offset

// ---- native harnesses (this process only) ---------------------------------------------------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type ThunkFn = delegate of nativeint * nativeint * nativeint -> int
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type SizeFn = delegate of nativeint * nativeint -> unit

let asmAt (at: uint64) (f: Assembler -> unit) =
    let a = Assembler(64)
    f a
    use ms = new MemoryStream()
    a.Assemble(StreamCodeWriter(ms), at) |> ignore
    ms.ToArray()

/// WK: run the cave against synthetic pools. The thunk calls the cave the way 40A4C does; the code after the call has
/// "keep" at +0 and "skip" at +0xBF, like 40A51 / 40B10.
let testWK () =
    let exec = VirtualAlloc(0n, 0x4000un, 0x3000u, 0x40u)
    let C = exec + 0x2000n
    let cave = build (uint64 C)
    Marshal.Copy(cave.Bytes, 0, C, cave.Bytes.Length)
    let T = exec
    let SENT_RCX, SENT_R9 = 0x1111222233334444L, 0x5555666677778888L
    // prologue: rcx = worker, rdx = cave entry, r8 = out[4]
    let pro = asmAt (uint64 T) (fun a ->
        a.push(rbx); a.push(r15); a.push(r12)
        a.mov(r15, rcx); a.mov(r12, r8); a.mov(r8, 0x0BADF00D)
        a.mov(rcx, SENT_RCX); a.mov(r9, SENT_R9)
        a.mov(eax, 0x7777); a.stc()
        a.call(rdx))
    Marshal.Copy(pro, 0, T, pro.Length)
    let retKeep = T + nativeint pro.Length
    let E = retKeep + 0x100n
    // keep stub: setc bl; mov eax,1; jmp E   |  skip stub at +0xBF: setc bl; mov eax,2; jmp E
    let keepStub = asmAt (uint64 retKeep) (fun a -> a.setb(bl); a.mov(eax, 1); a.jmp(uint64 E))
    Marshal.Copy(Array.create 0x100 0xCCuy, 0, retKeep, 0x100)
    Marshal.Copy(keepStub, 0, retKeep, keepStub.Length)
    let retSkip = retKeep + nativeint WK_DELTA
    let skipStub = asmAt (uint64 retSkip) (fun a -> a.setb(bl); a.mov(eax, 2); a.jmp(uint64 E))
    if keepStub.Length > WK_DELTA then failwith "keep stub too long"
    Marshal.Copy(skipStub, 0, retSkip, skipStub.Length)
    let epi = asmAt (uint64 E) (fun a ->
        a.mov(__qword_ptr.[r12], rcx); a.mov(__qword_ptr.[r12 + 8L], rdx); a.mov(__qword_ptr.[r12 + 16L], r9)
        a.movzx(ebx, bl); a.mov(__qword_ptr.[r12 + 24L], rbx); a.mov(__qword_ptr.[r12 + 32L], r8)
        a.pop(r12); a.pop(r15); a.pop(rbx); a.ret())
    Marshal.Copy(epi, 0, E, epi.Length)
    let f = Marshal.GetDelegateForFunctionPointer<ThunkFn>(T)
    let out = Marshal.AllocHGlobal 40
    let rnd = Random(20261007)
    let keep = ResizeArray<nativeint>()
    let alloc n = (let p = Marshal.AllocHGlobal(n: int) in Marshal.Copy(Array.zeroCreate<byte> n, 0, p, n); keep.Add p; p)
    let rd (a: uint64) = Marshal.ReadInt32(nativeint a)
    let mutable cases, bad, kept, skipped = 0, 0, 0, 0
    for _ in 1 .. 20000 do
        let n = rnd.Next(1, 33)
        let pool = alloc 0x60
        let arr = alloc (n * 0x458)
        Marshal.WriteIntPtr(pool + 0x10n, arr)
        Marshal.WriteInt32(pool + 0x28n, n)
        Marshal.WriteInt32(pool + 0x2Cn, n)
        let density = [| 0; 0; 1; 5; 30 |].[rnd.Next 5]
        let mutable any = false
        for i in 0 .. n - 1 do
            let w = arr + nativeint (i * 0x458)
            Marshal.WriteIntPtr(w + 0x440n, pool)
            if rnd.Next 10 <> 0 then
                let bm = alloc 16
                Marshal.WriteIntPtr(w + 0x448n, bm)
                for k in 0 .. 3 do
                    if density > 0 && rnd.Next 100 < density then
                        Marshal.WriteInt32(bm + nativeint (k * 4), 1 <<< rnd.Next 32)
                        any <- true
            // a non-zero word behind a null bitmap pointer must not count (it is never read)
        let me = arr + nativeint (rnd.Next n * 0x458)
        let en = rnd.Next 8 <> 0
        Marshal.WriteInt32(nativeint cave.En, if en then 1 else 0)
        let k0, s0 = rd cave.Keep, rd cave.Skip
        let r = f.Invoke(me, nativeint cave.Entry, out)
        let rcx, rdx, r9, cf = Marshal.ReadInt64 out, Marshal.ReadInt64(out + 8n), Marshal.ReadInt64(out + 16n), Marshal.ReadInt64(out + 24n)
        let r8 = Marshal.ReadInt64(out + 32n)
        let expectKeep = (not en) || any
        let ok =
            (if expectKeep then r = 1 && rcx = int64 pool && r8 = 0x0BADF00DL else r = 2 && rcx = SENT_RCX && r8 = 0L)
            && rdx = int64 cave.Entry && r9 = SENT_R9 && cf = 1L
            && (if not en then rd cave.Keep = k0 && rd cave.Skip = s0
                elif any then rd cave.Keep = k0 + 1 && rd cave.Skip = s0
                else rd cave.Keep = k0 && rd cave.Skip = s0 + 1)
        if expectKeep then kept <- kept + 1 else skipped <- skipped + 1
        cases <- cases + 1
        if not ok then
            bad <- bad + 1
            if bad <= 8 then printfn "  MISMATCH n=%d en=%b any=%b -> path %d rcx=%X rdx ok=%b r9 ok=%b CF=%d" n en any r rcx (rdx = int64 cave.Entry) (r9 = SENT_R9) cf
    for p in keep do Marshal.FreeHGlobal p
    Marshal.FreeHGlobal out
    printfn "  WK native test: %d cases (%d keep, %d skip, 1/8 with the switch off), %d mismatches; keep: rcx=pool, r8/rdx/r9/CF preserved; skip: rcx/rdx/r9/CF preserved, r8=0; counters exact" cases kept skipped bad
    bad = 0

/// WC: the real SizeWorkerPools (1FE2FD4..1FE3139) + its helper 1FDFEB4 copied from the dump; the mutex calls go to `ret`.
let FN, FN_END = 0x1FE2FD4, 0x1FE3139
let HELP, HELP_END = 0x1FDFEB4, 0x1FDFF26
type PoolRun = { Base: int; Counts: int[] }
let wcRunner () =
    let exec = VirtualAlloc(0n, 0x2000un, 0x3000u, 0x40u)
    let stub = exec
    Marshal.WriteByte(stub, 0xC3uy)
    let helper = exec + 0x100n
    Marshal.Copy(img, HELP, helper, HELP_END - HELP)
    for ins in decodeAll img.[HELP .. HELP_END - 1] (uint64 HELP) do
        if ins.FlowControl = FlowControl.Call || ins.FlowControl = FlowControl.IndirectBranch || ins.FlowControl = FlowControl.IndirectCall then failwith "helper is not a leaf"
        if (ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch)
           && (ins.NearBranchTarget < uint64 HELP || ins.NearBranchTarget >= uint64 HELP_END) then failwithf "helper branch %X leaves" ins.IP
    let mk (patch: (int * byte[]) list) (at: nativeint) =
        let code = Array.copy img.[FN .. FN_END - 1]
        for (rva, b) in patch do Array.blit b 0 code (rva - FN) b.Length
        for ins in decodeAll code (uint64 FN) do
            match ins.FlowControl with
            | FlowControl.Call | FlowControl.UnconditionalBranch | FlowControl.ConditionalBranch ->
                let t = ins.NearBranchTarget
                if t < uint64 FN || t >= uint64 FN_END then
                    let dst =
                        match t with
                        | 0x48B44UL | 0xB61524UL -> stub                        // _Mtx_lock / _Mtx_unlock wrappers
                        | 0x1FDFEB4UL -> helper
                        | _ -> failwithf "unexpected external target %X at %X" t ins.IP
                    if ins.Length <> 5 then failwithf "external branch at %X is not rel32" ins.IP
                    let src = at + nativeint (int ins.IP - FN) + 5n
                    Array.blit (BitConverter.GetBytes(int (int64 dst - int64 src))) 0 code (int ins.IP - FN + 1) 4
            | FlowControl.IndirectBranch | FlowControl.IndirectCall -> failwithf "indirect flow at %X" ins.IP
            | _ -> ()
        Marshal.Copy(code, 0, at, code.Length)
        Marshal.GetDelegateForFunctionPointer<SizeFn>(at)
    let fresh () = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    let fOrig = mk [] (exec + 0x400n)
    let table = Marshal.AllocHGlobal 0x800
    let info = Marshal.AllocHGlobal 0x40
    let run (f: SizeFn) (logical: int) (zen1: bool) =
        Marshal.Copy(img, POOL_TABLE, table, 0x748)
        Marshal.WriteInt16(table + 0x740n, 0x0D01s)                      // static init: base 1, divisor 13 (mov word [4B72A10h],0D01h)
        Marshal.Copy(Array.zeroCreate<byte> 0x40, 0, info, 0x40)
        Marshal.WriteInt32(info + 4n, logical)                           // DetectCpuTopology +274h: logical processors
        Marshal.WriteByte(info + 0x12n, if zen1 then 1uy else 0uy)       // +282h: Zen1/Zen+ Ryzen -> divisor 16
        f.Invoke(table, info)
        { Base = int (Marshal.ReadByte(table + 0x740n)); Counts = Array.init 74 (fun i -> int (Marshal.ReadByte(table + nativeint (0xC + i * 0x18)))) }
    let patched (cap: int) (p12: bool) =
        let ps = [ WC_A, wcA cap; WC_B, wcB cap ] @ (if p12 then [ P12_SITE, hexb P12_NEW ] else [])
        mk ps (fresh ())
    fOrig, patched, run

/// Independent model of SizeWorkerPools (from the decompile), cap = upper clamp of the base, p12 = penalty disabled.
let model (logical: int) (zen1: bool) (cap: int) (p12: bool) =
    let baseN = if logical > cap then cap elif logical < 6 then 6 else logical
    let div = if zen1 then 16 else 13
    let counts = Array.init 74 (fun i -> int img.[POOL_TABLE + 0xC + i * 0x18])
    let weight i = BitConverter.ToInt32(img, POOL_TABLE + 0x10 + i * 0x18)
    let f (b: int) (i: int) =
        if b <= 12 then b
        else
            match i with
            | 3 | 6 | 7 | 10 | 11 | 13 | 14 | 48 | 55 | 56 -> min b 16
            | 5 | 8 | 9 | 16 -> 12
            | _ -> b
    let penaltyLimit = if p12 then 0 else 12
    for i in 0 .. 73 do
        let special =
            if logical <= 6 && i = 6 then Some 4 elif logical <= 6 && i = 11 then Some 2
            elif logical > 6 && logical <= 8 && i = 6 then Some 6 elif logical > 6 && logical <= 8 && i = 11 then Some 3
            else None
        match special with
        | Some v -> counts.[i] <- v
        | None ->
            if weight i >= 0 then
                let v = int (float32 (f baseN i) / float32 div * float32 (weight i))
                counts.[i] <- max 1 (min 32 v)
            if logical <= penaltyLimit && (i = 3 || i = 5) then
                let v = counts.[i] - 3
                counts.[i] <- if v > 32 then 32 elif v < 3 then 3 else v
    baseN, counts

let threadPools = [ 3; 5; 6; 7; 8; 9; 10; 11; 13; 14; 16; 48; 55; 56 ]   // weighted pools that create threads (71..73 are Dummy)
let testWC () =
    let fOrig, patched, run = wcRunner ()
    let mutable bad, cases = 0, 0
    let check what (r: PoolRun) (b, c: int[]) =
        cases <- cases + 1
        if r.Base <> b || r.Counts <> c then
            bad <- bad + 1
            if bad <= 6 then
                let diffs = [ for i in 0 .. 73 do if r.Counts.[i] <> c.[i] then yield sprintf "%d:%d/%d" i r.Counts.[i] c.[i] ]
                printfn "  MISMATCH %s base native %d model %d; pools native/model %s" what r.Base b (String.Join(" ", diffs))
    // the dump itself was sized on the 7600X (12 logical): original code must reproduce the dump's counts
    let r12 = run fOrig 12 false
    let dumpCounts = Array.init 74 (fun i -> int img.[POOL_TABLE + 0xC + i * 0x18])
    let dumpOk = r12.Counts = dumpCounts && r12.Base = int img.[POOL_TABLE + 0x740]
    printfn "  original code at 12 logical reproduces the dump's live pool table: %b" dumpOk
    if not dumpOk then bad <- bad + 1
    let caps = [ 6; 8; 10; 12; 16; 24; 32 ]
    let pfs = [ for c in caps do for p in [ false; true ] -> (c, p), patched c p ]
    for logical in 1 .. 64 do
        for zen1 in [ false; true ] do
            check (sprintf "orig L=%d zen1=%b" logical zen1) (run fOrig logical zen1) (model logical zen1 32 false)
            for ((c, p), f) in pfs do
                check (sprintf "cap=%d p12=%b L=%d zen1=%b" c p logical zen1) (run f logical zen1) (model logical zen1 c p)
            // cap 32 must be byte-identical behaviour to the original
            let a, b = run fOrig logical zen1, run (snd (pfs |> List.find (fun ((c, p), _) -> c = 32 && not p))) logical zen1
            if a <> b then (bad <- bad + 1; printfn "  cap=32 differs from original at L=%d" logical)
    printfn "  WC native test: %d (config x CPU) runs of the real SizeWorkerPools vs the model, %d mismatches" cases bad
    // tables for the write-up
    let show (title: string) (rs: (string * PoolRun) list) =
        printfn "  %s" title
        printfn "    %-30s %s" "pool" (String.Join("", rs |> List.map (fun (h, _) -> sprintf "%10s" h)))
        for i in threadPools do
            printfn "    %2d %-27s %s" i (poolName i) (String.Join("", rs |> List.map (fun (_, r) -> sprintf "%10d" r.Counts.[i])))
        printfn "    %-30s %s" "SUM weighted thread pools" (String.Join("", rs |> List.map (fun (_, r) -> sprintf "%10d" (threadPools |> List.sumBy (fun i -> r.Counts.[i])))))
        printfn "    %-30s %s" "base (+740h)" (String.Join("", rs |> List.map (fun (_, r) -> sprintf "%10d" r.Base)))
    let pf c p = pfs |> List.find (fun ((c2, p2), _) -> c2 = c && p2 = p) |> snd
    show "ROG Ally / Z1 Extreme / 8C16T (16 logical):"
        [ "stock", run fOrig 16 false; "cap12", run (pf 12 false) 16 false; "cap10", run (pf 10 false) 16 false; "cap8", run (pf 8 false) 16 false; "cap6", run (pf 6 false) 16 false ]
    show "Steam Deck (8 logical) and 7600X (12 logical):"
        [ "Deck", run fOrig 8 false; "7600X", run fOrig 12 false; "7600X+P12", run (pf 32 true) 12 false; "7600Xcap8", run (pf 8 false) 12 false; "cap8+P12", run (pf 8 true) 12 false ]
    bad = 0

// ---- offline ---------------------------------------------------------------------------------------------------------
let offline () =
    printfn "dump: %s (raw offset == RVA)" DUMP
    let mutable allOk = true
    let B = 0x7FF74F410000UL
    let at = B + 0x5E00000UL
    let c = build at
    printfn "site bytes vs dump:"
    for (id, rva, o) in [ "WK", WK_SITE, WK_ORIG; "WCa", WC_A, WC_A_ORIG; "WCb", WC_B, WC_B_ORIG; "P12", P12_SITE, P12_ORIG ] do
        let ob = hexb o
        let cur = img.[rva .. rva + ob.Length - 1]
        let ok = cur = ob
        allOk <- allOk && ok
        printfn "  %-4s %8X  %-9s %s" id rva (if ok then "MATCH" else "DIFFERENT") (hexs cur)
        if id <> "P12" then
            for s in OTHER_SITES do
                if s < rva + ob.Length && rva < s + 10 then (printfn "  !! %s %X overlaps another patch site %X" id rva s; allOk <- false)
    // WK site decode
    let i0 = decodeAt (uint64 WK_SITE)
    let okDec = fmtIns i0 = "mov rcx,[r15+440h]" && i0.NextIP = WK_CONT
    printfn "WK site decodes as `%s`, next %X: %s" (fmtIns i0) i0.NextIP (if okDec then "[ok]" else "[FAIL]")
    allOk <- allOk && okDec
    let i1 = decodeAt WK_SKIP
    let i2 = decodeAt i1.NextIP
    let okSkip = fmtIns i1 = "mov rax,[rsp+0C0h]" && fmtIns i2 = "mov [rax],sil" && i2.NextIP = 0x40B1BUL
    printfn "WK skip target %X: `%s; %s` -> %X %s" WK_SKIP (fmtIns i1) (fmtIns i2) i2.NextIP (if okSkip then "[ok]" else "[FAIL]")
    allOk <- allOk && okSkip
    // esi == 0 at the site (40A12 xor esi,esi, no write until 40A4C) so the skip path stores 0 into the flag like stock
    let esiOk =
        let acc = ResizeArray<Instruction>()
        let mutable ip = 0x40A12UL
        while ip < uint64 WK_SITE do (let x = decodeAt ip in acc.Add x; ip <- x.NextIP)
        let insns = List.ofSeq acc
        (fmtIns insns.Head = "xor esi,esi") &&
        insns.Tail |> List.forall (fun x ->
            let mutable y = x
            let info = infoFactory.GetInfo(&y)
            info.GetUsedRegisters() |> Seq.forall (fun u -> u.Register.GetFullRegister() <> Register.RSI || u.Access = OpAccess.Read))
    // the only ways into 40A4C are fall-through from 40A46 and the jmp at 40A3D (no other branch targets it)
    printfn "WK: esi is zeroed at 40A12 and not written before 40A4C (skip path stores sil=0 into *param_3): %s" (if esiOk then "[ok]" else "[FAIL]")
    allOk <- allOk && esiOk
    printfn "WK cave @dummy %X: %d bytes (data C0h, code %d)" at c.Bytes.Length (c.Bytes.Length - 0xC0)
    printDis c.Bytes.[0xC0 ..] c.Entry
    let nb = wkNew B c
    printfn "WK patched site: 40A4C  %s  ->  %s" WK_ORIG (hexs nb)
    printDis nb (B + uint64 WK_SITE)
    let both = Set.intersect (Set.ofSeq (boundaries nb (B + uint64 WK_SITE))) (Set.ofSeq (boundaries (hexb WK_ORIG) (B + uint64 WK_SITE)))
    printfn "  instruction starts valid in both encodings: %A (40A51 = the call's return address is NOT one; revert checks the cave)" (both |> Set.map (fun x -> sprintf "%X" (x - B)) |> Set.toList)
    printfn "WK skip path state proof: the cave preserves every register and the flags; vs the stock flag==0 path into 40B1B"
    printfn "  (r8 is set to 0 = its value on that path); only rax (written at 40B10) and the flags can differ -> must be dead at 40B1B:"
    let f1 = walk [ 0x40B1BUL ] [ Register.RAX ] true 0
    if f1.IsEmpty then printfn "  [ok] rax, flags dead at 40B1B" else (allOk <- false; for x in f1 do printfn "  [FAIL] %s" x)
    printfn "  (stock reaches 40B10 the same way when no sleeper is found: 40A9C / 40AFE -> 40B10)"
    let f2 = walk [ WK_CONT ] [] true 0
    printfn "  keep path: cave restores all registers + flags and executes the original `mov rcx,[r15+440h]`: identical state at 40A53"
    ignore f2
    printfn "WC sites (SizeWorkerPools 1FE2FD4):"
    for cap in [ 8 ] do
        printDis (wcA cap) (uint64 WC_A); printDis (wcB cap) (uint64 WC_B)
    printfn "direct branch/call targets strictly inside the patched ranges (executable sections):"
    let ranges = [ "WK", uint64 WK_SITE, uint64 WK_SITE + 7UL; "WCa", uint64 WC_A, uint64 WC_A + 4UL; "WCb", uint64 WC_B, uint64 WC_B + 2UL ]
    let hits = scanTargets ranges
    if hits.Count = 0 then printfn "  none" else (for h in hits do printfn "  !! %s" h); allOk <- allOk && hits.Count = 0
    printfn "ASI patterns (must be unique and point at the site):"
    for s in asiSites do
        let h = patternHits s.Pattern
        let ok = (h = [ s.Rva - s.Offset ])
        allOk <- allOk && ok
        printfn "  %-4s %d match(es) %s %s" s.Id h.Length (String.Join(",", h |> List.map (sprintf "%X"))) (if ok then "[ok]" else "[FAIL]")
    printfn "ASI site entries (patches.json; WC 'new' shown for cap 8 - the loader substitutes imm8 = WorkerCap):"
    for s in asiSites do printfn "  %s" (asiJson s)
    printfn "native test 1 (WK cave vs synthetic pools, own process):"
    allOk <- testWK () && allOk
    printfn "native test 2 (WC: real SizeWorkerPools bytes, original vs patched vs model, own process):"
    allOk <- testWC () && allOk
    printfn "%s" (if allOk then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- counters: NtQuerySystemInformation, no handle to the game process --------------------------------------------------
module Nt =
    [<DllImport("ntdll.dll")>] extern int NtQuerySystemInformation(int cls, nativeint buf, int len, int& retLen)
    [<DllImport("kernel32.dll", SetLastError = true)>] extern nativeint OpenThread(uint32 access, bool inh, int tid)
    [<DllImport("kernel32.dll")>] extern int GetThreadDescription(nativeint h, nativeint& desc)
    [<DllImport("kernel32.dll")>] extern nativeint LocalFree(nativeint p)
    [<DllImport("kernel32.dll")>] extern bool CloseHandle(nativeint h)
type ThreadSnap = { Tid: int; Kern: int64; User: int64; Cs: uint32 }
type ProcSnap = { Pid: int; Faults: uint32; HardFaults: uint32; Threads: ThreadSnap list; T: int64 }
let snapshot (name: string) =
    let mutable size = 0x400000
    let mutable buf = Marshal.AllocHGlobal size
    let mutable ret = 0
    let mutable st = Nt.NtQuerySystemInformation(5, buf, size, &ret)
    while st = 0xC0000004 do
        Marshal.FreeHGlobal buf
        size <- max (size * 2) (ret + 0x10000)
        buf <- Marshal.AllocHGlobal size
        st <- Nt.NtQuerySystemInformation(5, buf, size, &ret)
    if st <> 0 then failwithf "NtQuerySystemInformation 0x%X" st
    let t = Stopwatch.GetTimestamp()
    let mutable off = 0
    let mutable found = None
    let mutable fin = false
    while not fin do
        let p = buf + nativeint off
        let next = Marshal.ReadInt32 p
        let nameLen = int (Marshal.ReadInt16(p + 0x38n))
        let namePtr = Marshal.ReadIntPtr(p + 0x40n)
        let nm = if namePtr = 0n then "" else Marshal.PtrToStringUni(namePtr, nameLen / 2)
        if found.IsNone && String.Equals(nm, name + ".exe", StringComparison.OrdinalIgnoreCase) then
            let n = Marshal.ReadInt32(p + 4n)
            let ths =
                [ for i in 0 .. n - 1 do
                    let th = p + 0x100n + nativeint (i * 0x50)
                    yield { Tid = int (Marshal.ReadIntPtr(th + 0x30n)); Kern = Marshal.ReadInt64 th; User = Marshal.ReadInt64(th + 8n); Cs = uint32 (Marshal.ReadInt32(th + 0x40n)) } ]
            found <- Some { Pid = int (Marshal.ReadIntPtr(p + 0x50n)); Faults = uint32 (Marshal.ReadInt32(p + 0x80n)); HardFaults = uint32 (Marshal.ReadInt32(p + 0x10n)); Threads = ths; T = t }
        if next = 0 then fin <- true else off <- off + next
    Marshal.FreeHGlobal buf
    found
let threadName (tid: int) =
    let h = Nt.OpenThread(0x0800u, false, tid)                            // THREAD_QUERY_LIMITED_INFORMATION
    if h = 0n then ""
    else
        let mutable d = 0n
        let s = if Nt.GetThreadDescription(h, &d) >= 0 && d <> 0n then (let x = Marshal.PtrToStringUni d in Nt.LocalFree d |> ignore; x) else ""
        Nt.CloseHandle h |> ignore
        s
let counters (sec: float) (name: string) =
    match snapshot name with
    | None -> printfn "process %s not running" name
    | Some a ->
        Threading.Thread.Sleep(int (sec * 1000.0))
        match snapshot name with
        | None -> printfn "process exited"
        | Some b ->
            let dt = float (b.T - a.T) / float Stopwatch.Frequency
            printfn "%s pid %d, %.2f s: page faults %.0f/s (hard %.0f/s), %d threads" name b.Pid dt (float (b.Faults - a.Faults) / dt) (float (b.HardFaults - a.HardFaults) / dt) b.Threads.Length
            let before = a.Threads |> List.map (fun x -> x.Tid, x) |> dict
            let rows =
                [ for x in b.Threads do
                    match before.TryGetValue x.Tid with
                    | true, y ->
                        let k, u = float (x.Kern - y.Kern) / 1e7 / dt * 100.0, float (x.User - y.User) / 1e7 / dt * 100.0
                        yield x.Tid, k, u, float (x.Cs - y.Cs) / dt
                    | _ -> () ]
            let names = Dictionary<int, string>()
            let nameOf tid = (match names.TryGetValue tid with | true, s -> s | _ -> (let s = threadName tid in names.[tid] <- s; s))
            printfn "  %-7s %7s %7s %7s %9s  %s" "tid" "CPU%" "kern%" "user%" "cs/s" "name"
            for (tid, k, u, cs) in rows |> List.sortByDescending (fun (_, k, u, _) -> k + u) |> List.truncate 40 do
                printfn "  %-7d %7.1f %7.1f %7.1f %9.0f  %s" tid (k + u) k u cs (nameOf tid)
            let fam (pred: string -> bool) = rows |> List.filter (fun (tid, _, _, _) -> pred (nameOf tid))
            let sumF (rs: (int * float * float * float) list) = (rs |> List.sumBy (fun (_, k, u, _) -> k + u)), (rs |> List.sumBy (fun (_, k, _, _) -> k)), (rs |> List.sumBy (fun (_, _, _, c) -> c))
            for (label, pred) in [ "KIDSTaskScheduler*", (fun (s: string) -> s.StartsWith "KIDSTaskScheduler"); "KIDSTS[*", (fun s -> s.StartsWith "KIDSTS[")
                                   "[App]EffectUpdateThread", (fun s -> s = "[App]EffectUpdateThread"); "all threads", (fun _ -> true) ] do
                let rs = fam pred
                let c, k, cs = sumF rs
                printfn "  family %-24s n=%3d CPU %6.1f%% kern %6.1f%% cs/s %8.0f" label rs.Length c k cs

// ---- live modes ------------------------------------------------------------------------------------------------------
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let printPools (read: int -> int -> byte[]) =
    let tb = read POOL_TABLE 0x748
    printfn "live pool table @%X: base(+740h)=%d divisor(+741h)=%d" POOL_TABLE tb.[0x740] tb.[0x741]
    let line = threadPools |> List.map (fun i -> sprintf "%s=%d" (poolName i) tb.[0xC + i * 0x18])
    printfn "  %s" (String.Join("  ", line))
    printfn "  weighted thread pools total: %d" (threadPools |> List.sumBy (fun i -> int tb.[0xC + i * 0x18]))

let boot () =
    writeGuard ()
    let cap = if args.Length > 1 then int args.[1] else 8
    if cap < 6 || cap > 32 then failwith "cap must be 6..32"
    let p12 = args |> Array.exists (fun s -> s.ToLowerInvariant() = "p12")
    let timeout = args |> Array.skip 1 |> Array.tryPick (fun s -> match Double.TryParse s with | true, v when v > 32.0 -> Some v | _ -> None) |> Option.defaultValue 180.0
    let sw = Stopwatch.StartNew()
    let fail msg = printfn "WC FAILED: %s (%.2f s)" msg sw.Elapsed.TotalSeconds; exit 1
    printfn "WC waiting for Nioh3.exe (cap %d%s) ..." cap (if p12 then " + P12" else "")
    let mutable proc : Process = null
    while isNull proc do
        if sw.Elapsed.TotalSeconds > timeout then fail "game did not start"
        proc <- Process.GetProcessesByName "Nioh3" |> Array.tryHead |> Option.toObj
        if isNull proc then Threading.Thread.Sleep 2
    let tStart = sw.Elapsed.TotalSeconds
    let mutable t : Target option = None
    while t.IsNone do
        if sw.Elapsed.TotalSeconds - tStart > 30.0 then fail "could not open the game process"
        try (let x = new Target(proc) in x.Base |> ignore; t <- Some x) with _ -> Threading.Thread.Sleep 1
    let tg = t.Value
    let at (rva: int) = tg.Base + nativeint rva
    let rd rva n = try tg.Read(at rva, n) with _ -> [||]
    let origA, origB, origP = hexb WC_A_ORIG, hexb WC_B_ORIG, hexb P12_ORIG
    let mutable polls = 0
    while not (rd WC_A 4 = origA && rd WC_B 2 = origB && (not p12 || rd P12_SITE 4 = origP)) do
        if sw.Elapsed.TotalSeconds - tStart > 60.0 then fail (sprintf "original bytes never appeared (WCa %s WCb %s)" (hexs (rd WC_A 4)) (hexs (rd WC_B 2)))
        if proc.HasExited then fail "game exited"
        if rd WC_A 4 = wcA cap then fail "already patched in this process"
        polls <- polls + 1
    // order: mov al,cap first, then the compare. In-between state = stock for logical <= 32 (no torn behaviour).
    tg.Write(at WC_B, wcB cap)
    tg.Write(at WC_A, wcA cap)
    if p12 then tg.Write(at P12_SITE, hexb P12_NEW)
    printfn "WC written %.0f ms after the process appeared (%d polls): WCa %s  WCb %s%s" ((sw.Elapsed.TotalSeconds - tStart) * 1000.0) polls
        (hexs (rd WC_A 4)) (hexs (rd WC_B 2)) (if p12 then sprintf "  P12 %s" (hexs (rd P12_SITE 4)) else "")
    printfn "Check later with: dotnet fsi jobs_live.fsx status   (base(+740h) must be %d on a CPU with more than %d logical processors)" cap cap

let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readAt (rva: int) n = t.Read(nativeint (B + uint64 rva), n)
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let caveOrProbe () = match readState () with Some a -> build a | None -> build (B + 0x5E00000UL)
    let wkState (c: Cave) =
        let cur = readAt WK_SITE 7
        if cur = hexb WK_ORIG then "original" elif cur = wkNew B c then "patched" else sprintf "UNKNOWN (%s)" (hexs cur)
    let wcState () =
        let a, b = readAt WC_A 4, readAt WC_B 2
        if a = hexb WC_A_ORIG && b = hexb WC_B_ORIG then "original (no cap)"
        elif a.[0..2] = [| 0x41uy; 0x83uy; 0xFBuy |] && b.[0] = 0xB0uy && a.[3] = b.[1] then sprintf "cap %d" a.[3]
        else sprintf "UNKNOWN (%s / %s)" (hexs a) (hexs b)
    match mode with
    | "dry" ->
        let c = caveOrProbe ()
        printfn "base=%X  WK: %s  WC: %s  P12: %s" B (wkState c) (wcState ()) (hexs (readAt P12_SITE 4))
        printPools readAt
        printfn "WK cave preview:"
        printDis c.Bytes.[0xC0 ..] c.Entry
    | "apply" ->
        writeGuard ()
        let probe = caveOrProbe ()
        if wkState probe <> "original" then failwith "WK site not original - aborting (revert first)"
        let c =
            match readState () with
            | Some a -> build a
            | None ->
                let caveMem = t.AllocNear(nativeint (B + uint64 WK_SITE), 0x1000)
                let c = build (uint64 caveMem)
                t.Write(caveMem, c.Bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" (uint64 caveMem) t.Process.Id)
                c
        let n = safeWrite t [ nativeint (B + uint64 WK_SITE), wkNew B c, hexb WK_ORIG ]
        printfn "WK applied on attempt %d; cave %X (switch = %d)" n (c.Entry - 0xC0UL) (BitConverter.ToInt32(t.Read(nativeint c.En, 4), 0))
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some a ->
            let c = build a
            t.Write(nativeint c.En, BitConverter.GetBytes(if mode = "on" then 1u else 0u))
            printfn "WK switch %s" mode
        | None -> printfn "WK is not applied in this process - use apply"
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        match wkState c with
        | "original" -> printfn "WK already original"
        | "patched" ->
            let lo, hi = c.Entry, c.Entry + uint64 (c.Bytes.Length - 0xC0)
            let mutable fin, attempt = false, 0
            while not fin do
                attempt <- attempt + 1
                if attempt > 200 then failwith "a thread kept sitting inside the WK cave/site - nothing written"
                fin <- t.Frozen(fun () ->
                    let rips = threadRips t
                    let site = B + uint64 WK_SITE
                    if rips |> List.exists (fun (_, r) -> (r >= lo && r < hi) || (r > site && r < site + 7UL)) then false
                    else (t.Write(nativeint site, hexb WK_ORIG); true))
                if not fin then Threading.Thread.Sleep 2
            printfn "WK reverted on attempt %d (cave left allocated)" attempt
        | s -> failwithf "WK: %s - aborting" s
    | "status" ->
        let c = caveOrProbe ()
        printfn "WK: %s  WC: %s  P12: %s" (wkState c) (wcState ()) (hexs (readAt P12_SITE 4))
        printPools readAt
        match readState () with
        | None -> printfn "no WK cave in this process"
        | Some _ ->
            let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
            printfn "WK switch=%d" (rd c.En)
            let mutable k, s = rd c.Keep, rd c.Skip
            for _ in 1 .. 10 do
                Threading.Thread.Sleep 100
                let k2, s2 = rd c.Keep, rd c.Skip
                printfn "  wake attempts/s %7d   kept (work queued) %7d   skipped (pool empty) %7d" ((k2 - k + s2 - s) * 10u) ((k2 - k) * 10u) ((s2 - s) * 10u)
                k <- k2; s <- s2
    | m -> printfn "unknown mode %s" m

match mode with
| "offline" -> offline ()
| "boot" -> boot ()
| "counters" ->
    let sec = if args.Length > 1 then float args.[1] else 5.0
    let name = if args.Length > 2 then args.[2] else "Nioh3"
    counters sec name
| _ -> live ()
