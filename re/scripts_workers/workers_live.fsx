// workers_live.fsx - worker-thread CPU patches for Nioh 3 (Steam v2.02). See notes\worker-threads.md.
//
//   dotnet fsi workers_live.fsx offline              NO GAME ACCESS: verify every site against re\Nioh3.dump.exe,
//                                                    build the cave at a dummy address, prove the zero-source /
//                                                    dead-flag / dead-register conditions by static path walks,
//                                                    sweep .text for branch targets inside patched bytes, and run
//                                                    the M1 cave natively against the original bytes (own process).
//   dotnet fsi workers_live.fsx dry   [M1,M2,J1]     READ-ONLY on the live game: bytes + cave preview.
//   dotnet fsi workers_live.fsx apply [ids]          WRITES (needs FT_ALLOW_WRITE=1): one cave per game process.
//   dotnet fsi workers_live.fsx status               READ-ONLY: site states, M1 switch, M1 refreshes/s (10 x 100 ms).
//   dotnet fsi workers_live.fsx on|off [ids]         WRITES (FT_ALLOW_WRITE=1). M1: 4-byte cave switch, no code
//                                                    touched. M2/J1 are in-place byte rows: on = patched bytes,
//                                                    off = original bytes (same frozen safeWrite as apply/revert).
//   dotnet fsi workers_live.fsx revert [ids]         WRITES: original bytes back (the M1 cave stays allocated).
//
// Patches:
//   M1  TS_MESHLET_REQ job 48E90 (MeshletRequestLoad worker; 0.34-0.77 core across the 10 workers in r5/r6/r7/ingame2,
//       36-42 % of it at loop 49150..4915A). For EVERY sub-chunk it considers (resident or not) it re-sums the
//       streaming page-pool capacity by walking the pool's segment list(s) g+67A8 (+67B0/+67B8 when the per-type flag
//       g+67C0 is 0): sum([node+14h]) over node=[node+30h]. The lists only change when the pool is built/grown.
//       Site 49131 `mov rdx,[rsi+1C8h]` (7 B) -> `jmp cave; nop2`. Cave: same rdx; if (head0|1, rdtsc>>22) matches the
//       cached key -> ecx = cached sum, else walk exactly like the original, store the key + sum; jmp 491AC.
//       Staleness bound: one rdtsc>>22 window (~0.9 ms at 4.7 GHz TSC, ~1.7 ms at 2.5 GHz) AND an immediate refresh
//       when the head node of list 0 changes. Switch off -> `mov rdx,[rsi+1C8h]; jmp 49138` = the original code.
//   M2  48E90: 5 "atomic loads" `xor eax,eax; lock cmpxchg [m],r13d` (r13d == 0) -> `mov eax,[m]` + NOP. Hot one is
//       49261 (page-bitmap word, hot RIP 49266 = 10-15 % of 48E90): 10 workers take the bitmap lines exclusive just
//       to read them. Same idiom/argument as rows A1-A3 in notes\mainthread-drawprep.md.
//   J1  job system (every KIDSTS/KIDSTaskScheduler thread + main thread): the queue/priority-bitmap polls
//       411C0 (0.16-0.19 core self, hot RIP 4126B = 44-50 %), 40800 (local pop, hot RIP 4089F), 403A0 (no-sleep flag),
//       3E7CC (job-counter wait loop) use the same `lock cmpxchg [m],zero` load. -> `mov eax,[m]` + NOP, or
//       `mov eax,[m]; test eax,eax` where the following jcc consumes ZF (ZF identical; all other flags proven dead).
//
// Safety model = patcher\cpu_caves.fsx / re\scripts_meshlet\meshlet_caves.fsx: original-byte check, Mem.fsx safeWrite
// (process frozen, thread-RIP boundary check), revert leaves the cave allocated.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let ALL = "M1,M2,J1"
let selected = (if args.Length > 1 then args.[1] else ALL).ToUpperInvariant().Split(',') |> Set.ofArray
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "workers_live_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let img = File.ReadAllBytes DUMP

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- sites -------------------------------------------------------------------------------------------------
let M1_SITE, M1_ORIG = 0x49131, "48 8B 96 C8 01 00 00"      // mov rdx,[rsi+1C8h]
let M1_STOCK = 0x49138UL                                     // mov ecx,r13d  (rest of the original walk)
let M1_RESUME = 0x491ACUL                                    // imul ecx,[rsi+134h]

/// In-place rows: id, rva, original, new, function entry (for the zero proofs), zero-source register,
/// needsTest = the next flag consumer is a jcc on ZF (new = mov + test), else flags must be dead.
type Row = { Id: string; Rva: int; Orig: string; New: string; Fn: uint64; Zero: Register; NeedsTest: bool; Note: string }
let rows =
  [ // M2: 48E90 (r13d is the function's zero register: 48F98 / 4940A `xor r13d,r13d`)
    { Id = "M2"; Rva = 0x48FB9; Orig = "F0 44 0F B1 AD B8 00 00 00"; New = "8B 85 B8 00 00 00 0F 1F 00"; Fn = 0x48E90UL; Zero = Register.R13D; NeedsTest = false; Note = "per call: [entry obj+B8h]" }
    { Id = "M2"; Rva = 0x49046; Orig = "F0 46 0F B1 2C 21";          New = "42 8B 04 21 66 90";          Fn = 0x48E90UL; Zero = Register.R13D; NeedsTest = false; Note = "per page: entry flags" }
    { Id = "M2"; Rva = 0x49261; Orig = "F0 45 0F B1 29";             New = "41 8B 01 66 90";             Fn = 0x48E90UL; Zero = Register.R13D; NeedsTest = false; Note = "per 64K page: claim bitmap word (hot)" }
    { Id = "M2"; Rva = 0x4933D; Orig = "F0 46 0F B1 2C 21";          New = "42 8B 04 21 66 90";          Fn = 0x48E90UL; Zero = Register.R13D; NeedsTest = false; Note = "per page: entry flags" }
    { Id = "M2"; Rva = 0x493DC; Orig = "F0 45 0F B1 28";             New = "41 8B 00 66 90";             Fn = 0x48E90UL; Zero = Register.R13D; NeedsTest = false; Note = "per call: entry flags before bts 9 CAS" }
    // J1: job system
    { Id = "J1"; Rva = 0x4122D; Orig = "F0 42 0F B1 1C 19";          New = "42 8B 04 19 66 90";          Fn = 0x411C0UL; Zero = Register.EBX;  NeedsTest = false; Note = "411C0 first priority word" }
    { Id = "J1"; Rva = 0x41265; Orig = "F0 42 0F B1 1C 19";          New = "42 8B 04 19 85 C0";          Fn = 0x411C0UL; Zero = Register.EBX;  NeedsTest = true;  Note = "411C0 priority-word scan loop (hot RIP 4126B)" }
    { Id = "J1"; Rva = 0x41397; Orig = "F0 42 0F B1 1C 19";          New = "42 8B 04 19 66 90";          Fn = 0x411C0UL; Zero = Register.EBX;  NeedsTest = false; Note = "411C0 re-read after empty pop" }
    { Id = "J1"; Rva = 0x414B6; Orig = "F0 42 0F B1 1C 19";          New = "42 8B 04 19 85 C0";          Fn = 0x411C0UL; Zero = Register.EBX;  NeedsTest = true;  Note = "411C0 re-read after empty pop" }
    { Id = "J1"; Rva = 0x40899; Orig = "F0 42 0F B1 34 01";          New = "42 8B 04 01 85 C0";          Fn = 0x40800UL; Zero = Register.ESI;  NeedsTest = true;  Note = "40800 own-queue priority word (hot RIP 4089F)" }
    { Id = "J1"; Rva = 0x40481; Orig = "F0 45 0F B1 AF 54 04 00 00"; New = "41 8B 87 54 04 00 00 66 90"; Fn = 0x403A0UL; Zero = Register.R13D; NeedsTest = false; Note = "403A0 sched+454h no-sleep flag, once per empty poll" }
    { Id = "J1"; Rva = 0x3E7F5; Orig = "F0 44 0F B1 71 34";          New = "8B 41 34 0F 1F 00";          Fn = 0x3E7CCUL; Zero = Register.R14D; NeedsTest = false; Note = "3E7CC job counter, first read" }
    { Id = "J1"; Rva = 0x3E821; Orig = "F0 44 0F B1 73 34";          New = "8B 43 34 0F 1F 00";          Fn = 0x3E7CCUL; Zero = Register.R14D; NeedsTest = false; Note = "3E7CC job counter, wait loop" } ]
let rowsOf id = rows |> List.filter (fun r -> r.Id = id)
let ids () = [ "M1"; "M2"; "J1" ] |> List.filter selected.Contains

/// sites of the other patch sets (cpu_caves W3/R1/L1, fg_cave F1, ft_vrr, mainthread A/B/D, W1/W2, meshlet_caves,
/// pool_patch) - must not overlap
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4
      0x419A8; 0x4ABD2; 0x1E72C6; 0x1E71CD; 0x3777A40; 0x1FE30D5 ]

// ---- M1 cave ------------------------------------------------------------------------------------------------
type Cave = { Bytes: byte[]; M1: uint64; En: uint64; KHead: uint64; KVal: uint64; Cnt: uint64 }

/// Data: line 0 (read-mostly, shared by all workers): en dword @+0, kHead qword @+8, kVal qword @+10h.
/// line 1 (written on refresh only): refresh counter @+40h. Code from +80h.
/// `B` = module base the cave jumps back into (the native test passes a fake base).
let build (at: uint64) (B: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, lKHead, lKVal, lCnt = L(), L(), L(), L()
    let mutable cM1, stock, refresh, one, three, w1, w2, wDone = L(), L(), L(), L(), L(), L(), L(), L()
    a.Label(&lEn); a.dd(1u); a.dd(0u)
    a.Label(&lKHead); a.dq(0UL)
    a.Label(&lKVal); a.dq(0UL)
    for _ in 1 .. 10 do a.dd(0u)                      // pad line 0 to 40h
    a.Label(&lCnt); a.dd(0u)
    for _ in 1 .. 15 do a.dd(0u)                      // pad line 1 to 80h
    // ===== M1. Entered by jmp from 49131. Live: rsi = meshlet manager, r13d = 0. Out: ecx = segment-way sum.
    // Clobbers rax, rcx, rdx, flags - exactly what the original 49131..491AB clobbers.
    a.Label(&cM1)
    a.cmp(__dword_ptr.[lEn], 0); a.je(stock)
    a.rdtsc()
    a.shrd(eax, edx, 22uy)                            // eax = low32(tsc >> 22)
    a.mov(ecx, eax)                                   // ecx = time tag
    a.mov(rdx, __qword_ptr.[rsi + 0x1C8L])            // g (the original's first instruction)
    a.mov(rax, __qword_ptr.[rdx + 0x67A8L]); a.``or``(rax, 1)
    a.cmp(rax, __qword_ptr.[lKHead]); a.jne(refresh)  // pool list 0 head changed (or cache empty)
    a.mov(rax, __qword_ptr.[lKVal])                   // hi32 = tag, lo32 = sum
    a.shl(rcx, 32uy); a.xor(rcx, rax); a.shr(rcx, 32uy)
    a.jne(refresh)                                    // tag differs -> older than one rdtsc>>22 window
    a.mov(ecx, eax)                                   // cached sum
    a.jmp(B + M1_RESUME)
    // ---- refresh: the original walk (49138..491AA), then store key + sum
    a.Label(&refresh)
    a.mov(rdx, __qword_ptr.[rsi + 0x1C8L])
    a.mov(ecx, r13d)
    a.mov(rax, __qword_ptr.[rdx + 0x67A8L])
    a.cmp(__byte_ptr.[rdx + 0x67C0L], cl); a.je(three)
    a.Label(&one)                                     // per-type pools: list 0 only
    a.test(rax, rax); a.je(wDone)
    a.add(ecx, __dword_ptr.[rax + 0x14L]); a.mov(rax, __qword_ptr.[rax + 0x30L]); a.jmp(one)
    a.Label(&three)                                   // shared pool: lists 0, 1, 2
    a.test(rax, rax); a.je(w1)
    a.add(ecx, __dword_ptr.[rax + 0x14L]); a.mov(rax, __qword_ptr.[rax + 0x30L]); a.jmp(three)
    a.Label(&w1)
    a.mov(rax, __qword_ptr.[rdx + 0x67B0L])
    let mutable w1l = L()
    a.Label(&w1l)
    a.test(rax, rax); a.je(w2)
    a.add(ecx, __dword_ptr.[rax + 0x14L]); a.mov(rax, __qword_ptr.[rax + 0x30L]); a.jmp(w1l)
    a.Label(&w2)
    a.mov(rax, __qword_ptr.[rdx + 0x67B8L])
    let mutable w2l = L()
    a.Label(&w2l)
    a.test(rax, rax); a.je(wDone)
    a.add(ecx, __dword_ptr.[rax + 0x14L]); a.mov(rax, __qword_ptr.[rax + 0x30L]); a.jmp(w2l)
    a.Label(&wDone)                                   // ecx = sum (upper rcx zero: 32-bit ops only)
    a.mov(rax, __qword_ptr.[rdx + 0x67A8L]); a.``or``(rax, 1)
    a.mov(__qword_ptr.[lKHead], rax)                  // key first, then value (x64 TSO keeps the order)
    a.rdtsc()
    a.shrd(eax, edx, 22uy)
    a.shl(rax, 32uy); a.``or``(rax, rcx)
    a.mov(__qword_ptr.[lKVal], rax)
    a.inc(__dword_ptr.[lCnt])                         // refreshes (not atomic; statistics only)
    a.jmp(B + M1_RESUME)
    // ---- switch off: the original instructions, unchanged
    a.Label(&stock)
    a.mov(rdx, __qword_ptr.[rsi + 0x1C8L])
    a.jmp(B + M1_STOCK)
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); M1 = addr cM1; En = addr lEn; KHead = addr lKHead; KVal = addr lKVal; Cnt = addr lCnt }
    if c.KHead - at <> 8UL || c.KVal - at <> 0x10UL || c.Cnt - at <> 0x40UL || c.M1 - at <> 0x80UL then failwith "cave layout"
    c

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let m1New (B: uint64) (c: Cave) = Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 M1_SITE + 5UL) c.M1; hexb "66 90" ]

/// every (rva, new, orig) write for an id
let writesFor (B: uint64) (c: Cave) (id: string) =
    match id with
    | "M1" -> [ M1_SITE, m1New B c, hexb M1_ORIG ]
    | _ -> [ for r in rowsOf id -> r.Rva, hexb r.New, hexb r.Orig ]

// ---- disassembly helpers ------------------------------------------------------------------------------------
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

// ---- static proofs ------------------------------------------------------------------------------------------
let infoFactory = InstructionInfoFactory()
let volatileRegs = [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ]
let argRegs = [ Register.RCX; Register.RDX; Register.R8; Register.R9 ]
let allStatus = RflagsBits.OF ||| RflagsBits.SF ||| RflagsBits.ZF ||| RflagsBits.AF ||| RflagsBits.CF ||| RflagsBits.PF
let isShiftByCl (ins: Instruction) =
    ins.OpCount = 2 && ins.Op1Kind = OpKind.Register && ins.Op1Register = Register.CL &&
    (match ins.Mnemonic with
     | Mnemonic.Shl | Mnemonic.Shr | Mnemonic.Sar | Mnemonic.Sal | Mnemonic.Rol | Mnemonic.Ror | Mnemonic.Rcl | Mnemonic.Rcr -> true
     | _ -> false)
/// Walk every path from `starts` until each tracked register is fully overwritten (32/64-bit write, or clobbered by a
/// call) and, if `flags`, until all status flags are written (a shift/rotate by CL does NOT count: count 0 keeps the
/// flags). Fails on any read of a still-tracked register/flag, on indirect branches, and on indirect calls with a
/// tracked argument register. Direct calls with a tracked argument register are followed (depth <= 4).
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
/// Forward dataflow from the function entry: is `reg` provably zero every time execution reaches `site`?
/// Zeroed by `xor r,r` / `sub r,r` / `mov r,0` (32/64-bit); any other write kills; calls kill volatile registers only
/// (MS x64 ABI). Returns (visits of the site, failures).
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

/// Check one in-place row: decoded original = `lock cmpxchg [m], zeroReg` (32-bit), new = `mov eax,[same m]`
/// [+ `test eax,eax`] + NOPs, same length; eax and the source register provably zero at the site; flags: either dead
/// after the site, or (NeedsTest) the first flag reader is a ZF-only jcc and all flags are dead at both successors.
let checkRow (r: Row) =
    let o, n = hexb r.Orig, hexb r.New
    let errs = ResizeArray<string>()
    if img.[r.Rva .. r.Rva + o.Length - 1] <> o then errs.Add "original bytes differ from the dump"
    if o.Length <> n.Length then errs.Add "length differs"
    let oi = decodeAll o (uint64 r.Rva)
    let ni = decodeAll n (uint64 r.Rva)
    match oi with
    | [ c ] when c.Mnemonic = Mnemonic.Cmpxchg && c.HasLockPrefix && c.Op0Kind = OpKind.Memory && c.Op1Kind = OpKind.Register
                 && c.Op1Register = r.Zero && c.Op1Register.GetSize() = 4 ->
        match ni with
        | mv :: rest when mv.Mnemonic = Mnemonic.Mov && mv.Op0Register = Register.EAX && mv.Op1Kind = OpKind.Memory
                          && mv.MemoryBase = c.MemoryBase && mv.MemoryIndex = c.MemoryIndex && mv.MemoryIndexScale = c.MemoryIndexScale
                          && mv.MemoryDisplacement64 = c.MemoryDisplacement64 && mv.MemorySize = c.MemorySize && mv.SegmentPrefix = c.SegmentPrefix ->
            let rest2 =
                match rest with
                | t :: tl when r.NeedsTest ->
                    if not (t.Mnemonic = Mnemonic.Test && t.Op0Register = Register.EAX && t.Op1Register = Register.EAX) then errs.Add "expected test eax,eax"
                    tl
                | _ -> (if r.NeedsTest then errs.Add "missing test eax,eax"); rest
            for x in rest2 do if x.Mnemonic <> Mnemonic.Nop then errs.Add (sprintf "filler is not a NOP: %s" (fmtIns x))
        | _ -> errs.Add "new bytes are not `mov eax,[same memory operand]`"
    | _ -> errs.Add (sprintf "original is not `lock cmpxchg [m],%O`" r.Zero)
    // eax and the source register are zero at the site
    for reg in [ Register.EAX; r.Zero ] do
        let v, f = provenZeroAt r.Fn reg (uint64 r.Rva)
        if v = 0 then errs.Add (sprintf "site not reached from %X" r.Fn)
        for x in f do errs.Add x
    // flags
    let next = uint64 r.Rva + uint64 o.Length
    let rec firstFlagUser (ip: uint64) k =
        let ins = decodeAt ip
        if k > 40 then None
        elif ins.RflagsRead <> RflagsBits.None then Some (ins, true)
        elif not (isShiftByCl ins) && (ins.RflagsModified &&& allStatus) = allStatus then Some (ins, false)
        elif ins.FlowControl = FlowControl.UnconditionalBranch then firstFlagUser ins.NearBranchTarget (k + 1)
        elif ins.FlowControl <> FlowControl.Next then None
        else firstFlagUser ins.NextIP (k + 1)
    let flagNote =
        match firstFlagUser next 0 with
        | Some (ins, false) ->
            if r.NeedsTest then errs.Add "flags are dead - the test is unnecessary (row marked NeedsTest)"
            sprintf "flags dead: overwritten by %X %s" ins.IP (fmtIns ins)
        | Some (ins, true) when ins.FlowControl = FlowControl.ConditionalBranch && ins.RflagsRead = RflagsBits.ZF ->
            if not r.NeedsTest then errs.Add "ZF is consumed but the row has no test"
            let f = walk [ ins.NearBranchTarget; ins.NextIP ] [] true false 0
            for x in f do errs.Add ("after jcc: " + x)
            sprintf "ZF consumed by %X %s; all flags dead at %X and %X" ins.IP (fmtIns ins) ins.NearBranchTarget ins.NextIP
        | Some (ins, true) -> errs.Add (sprintf "flags read by %X %s" ins.IP (fmtIns ins)); ""
        | None -> errs.Add "could not prove the flag state (control flow before a flag writer)"; ""
    List.ofSeq errs, flagNote

/// direct branch/call targets anywhere in .text that land strictly inside a patched range
let scanTargets (ranges: (string * uint64 * uint64) list) =
    let pe = BitConverter.ToInt32(img, 0x3C)
    let nsec = int (BitConverter.ToUInt16(img, pe + 6))
    let optsz = int (BitConverter.ToUInt16(img, pe + 20))
    let hits = ResizeArray<string>()
    for i in 0 .. nsec - 1 do
        let sh = pe + 24 + optsz + i * 40
        let vs = BitConverter.ToInt32(img, sh + 8)
        let va = BitConverter.ToInt32(img, sh + 12)
        let ch = BitConverter.ToUInt32(img, sh + 36)
        if ch &&& 0x20000000u <> 0u then
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

// ---- native equivalence test: the original 49131..491AB bytes vs the M1 cave (this process only) -----------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type RegionFn = delegate of nativeint * nativeint -> int

let testM1 () =
    let REG_LEN = int (M1_RESUME - uint64 M1_SITE)          // 0x7B
    // the copied region must only branch inside itself or to its end (491AC)
    for ins in decodeAll img.[M1_SITE .. M1_SITE + REG_LEN - 1] (uint64 M1_SITE) do
        if ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch then
            let t = ins.NearBranchTarget
            if t < uint64 M1_SITE || t > M1_RESUME then failwithf "region branch %X -> %X leaves the region" ins.IP t
        elif ins.FlowControl <> FlowControl.Next then failwithf "unexpected flow at %X" ins.IP
    let exec = VirtualAlloc(0n, 0x3000un, 0x3000u, 0x40u)
    let X, T, C = exec, exec + 0x200n, exec + 0x1000n      // region copy, thunk, cave
    Marshal.Copy(img, M1_SITE, X, REG_LEN)
    Marshal.WriteByte(X + nativeint REG_LEN, 0xC3uy)          // 491AC -> ret
    // thunk(M, target): rsi = M, r13d = 0, call target, return ecx
    let th = assemble T (fun a ->
        a.push(rsi); a.push(r13); a.sub(rsp, 8)
        a.mov(rsi, rcx); a.xor(r13d, r13d)
        a.call(rdx)
        a.mov(eax, ecx)
        a.add(rsp, 8); a.pop(r13); a.pop(rsi); a.ret())
    Marshal.Copy(th, 0, T, th.Length)
    let fakeB = uint64 X - uint64 M1_SITE                     // B+49138 = copy's 49138, B+491AC = the ret
    let cave = build (uint64 C) fakeB
    Marshal.Copy(cave.Bytes, 0, C, cave.Bytes.Length)
    let f = Marshal.GetDelegateForFunctionPointer<RegionFn>(T)
    let orig m = f.Invoke(m, X)
    let viaCave m = f.Invoke(m, nativeint cave.M1)
    let setEn (on: bool) = Marshal.WriteInt32(nativeint cave.En, if on then 1 else 0)
    let invalidate () = Marshal.WriteInt64(nativeint cave.KHead, 0L)
    let rnd = Random(20261006)
    let keep = ResizeArray<nativeint>()                      // never freed during the test: no address reuse
    let alloc (n: int) = (let p = Marshal.AllocHGlobal(n) in Marshal.Copy(Array.zeroCreate<byte> n, 0, p, n); keep.Add p; p)
    let mutable cases, bad, staleSeen, hitsChecked, expiryChecked = 0, 0, 0, 0, 0
    let report (what: string) (a: int) (b: int) =
        bad <- bad + 1
        if bad <= 8 then printfn "  MISMATCH %s: original %d cave %d" what a b
    let refreshes () = Marshal.ReadInt32(nativeint cave.Cnt)
    for round in 1 .. 3000 do
        let g = alloc 0x7000
        let m = alloc 0x200
        Marshal.WriteIntPtr(m + 0x1C8n, g)
        let mkList (headOff: int) =
            let n = [| 0; 0; 1; 2; 3; 8; 20; 60 |].[rnd.Next 8]
            let mutable head = 0n
            for _ in 1 .. n do
                let node = alloc 0x40
                Marshal.WriteInt32(node + 0x14n, rnd.Next(0, 0x2000))
                Marshal.WriteIntPtr(node + 0x30n, head)
                head <- node
            Marshal.WriteIntPtr(g + nativeint headOff, head)
        mkList 0x67A8; mkList 0x67B0; mkList 0x67B8
        Marshal.WriteByte(g + 0x67C0n, (match rnd.Next 3 with 0 -> 0uy | 1 -> 1uy | _ -> 0x80uy))
        let en = rnd.Next 10 <> 0
        setEn en
        invalidate ()
        let r0 = orig m
        let c1 = viaCave m
        let before = refreshes ()
        let c2 = viaCave m
        if c1 <> r0 then report (sprintf "first call en=%b" en) r0 c1
        if c2 <> r0 then report (sprintf "second call en=%b" en) r0 c2
        if en && refreshes () = before then hitsChecked <- hitsChecked + 1
        cases <- cases + 1
        // (a) node content changes, list-0 head unchanged: cave may return the value from <= 1 tsc window ago
        let h0 = Marshal.ReadIntPtr(g + 0x67A8n)
        if h0 <> 0n then
            let old = orig m
            let mutable p = h0
            for _ in 1 .. rnd.Next 3 do (let nx = Marshal.ReadIntPtr(p + 0x30n) in if nx <> 0n then p <- nx)
            Marshal.WriteInt32(p + 0x14n, rnd.Next(0, 0x2000))
            let rA, cA = orig m, viaCave m
            if cA <> rA then
                if en && cA = old then staleSeen <- staleSeen + 1 else report "after content change" rA cA
            if rnd.Next 40 = 0 then                           // window expiry: must be fresh again
                Threading.Thread.Sleep 3
                let rE, cE = orig m, viaCave m
                expiryChecked <- expiryChecked + 1
                if cE <> rE then report "after tsc window expiry" rE cE
        // (b) list-0 head changes (segment prepended): detected immediately
        let node = alloc 0x40
        Marshal.WriteInt32(node + 0x14n, rnd.Next(1, 0x2000))
        Marshal.WriteIntPtr(node + 0x30n, Marshal.ReadIntPtr(g + 0x67A8n))
        Marshal.WriteIntPtr(g + 0x67A8n, node)
        let rB, cB = orig m, viaCave m
        if cB <> rB then report "after head change" rB cB
        cases <- cases + 1
    for p in keep do Marshal.FreeHGlobal p
    printfn "  M1 native test: %d cases (10%% with the switch off), %d mismatches; cache hits verified %d, stale-within-window %d, window expiries checked %d, cave refreshes %d"
        cases bad hitsChecked staleSeen expiryChecked (refreshes ())
    bad = 0

// ---- offline mode -------------------------------------------------------------------------------------------
let offline () =
    printfn "dump: %s (raw offset == RVA)" DUMP
    let mutable allOk = true
    let B = 0x7FF74F410000UL
    let at = B + 0x5E00000UL
    let c = build at B
    printfn "site bytes vs dump:"
    let allSites = (M1_SITE, hexb M1_ORIG, "M1") :: [ for r in rows -> r.Rva, hexb r.Orig, r.Id ]
    for (rva, o, id) in allSites do
        let cur = img.[rva .. rva + o.Length - 1]
        let ok = cur = o
        allOk <- allOk && ok
        printfn "  %-3s %8X  %-9s %s" id rva (if ok then "MATCH" else "DIFFERENT") (hexs cur)
        for s in OTHER_SITES do
            if s < rva + o.Length && rva < s + 10 then (printfn "  !! %s %X overlaps another patch site %X" id rva s; allOk <- false)
    printfn "M1 cave @dummy %X: %d bytes (data 80h, code %d)" at c.Bytes.Length (c.Bytes.Length - 0x80)
    printDis c.Bytes.[0x80 ..] c.M1 B
    printfn "M1 patched site:"
    let nb = m1New B c
    printfn "  49131  orig %s  ->  new %s" M1_ORIG (hexs nb)
    printDis nb (B + uint64 M1_SITE) B
    let both = Set.intersect (Set.ofSeq (boundaries nb (B + uint64 M1_SITE))) (Set.ofSeq (boundaries (hexb M1_ORIG) (B + uint64 M1_SITE)))
    printfn "  instruction starts valid in both encodings: %A" (both |> Set.map (fun x -> sprintf "%X" (x - B)) |> Set.toList)
    printfn "M1 dead-register proof (the cave clobbers rax/rdx/flags; rcx = result, as the original region):"
    let f1 = walk [ M1_RESUME ] [ Register.RAX; Register.RDX ] true false 0
    if f1.IsEmpty then printfn "  [ok] rax, rdx, flags dead at 491AC" else (allOk <- false; for x in f1 do printfn "  [FAIL] %s" x)
    printfn "in-place rows (M2, J1):"
    for r in rows do
        let errs, note = checkRow r
        printfn "  %-2s %6X  %-27s -> %-27s %s" r.Id r.Rva r.Orig r.New (if errs.IsEmpty then "[ok]" else "[FAIL]")
        printfn "             %s; zero src %O + eax proven; %s" r.Note r.Zero note
        for e in errs do printfn "      !! %s" e
        allOk <- allOk && errs.IsEmpty
    printfn "direct branch/call targets strictly inside the patched ranges (whole .text linear sweep):"
    let ranges = ("M1", uint64 M1_SITE, uint64 M1_SITE + 7UL) :: [ for r in rows -> r.Id, uint64 r.Rva, uint64 r.Rva + uint64 (hexb r.Orig).Length ]
    let hits = scanTargets ranges
    if hits.Count = 0 then printfn "  none" else (for h in hits do printfn "  !! %s" h); allOk <- allOk && hits.Count = 0
    printfn "native equivalence test (M1 cave vs the original 49131..491AB bytes copied from the dump):"
    allOk <- testM1 () && allOk
    printfn "%s" (if allOk then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live modes ---------------------------------------------------------------------------------------------
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readAt (rva: int) n = t.Read(nativeint (B + uint64 rva), n)
    let classifyRow (rva: int) (nw: byte[]) (o: byte[]) =
        let cur = readAt rva o.Length
        (if cur = o then "original" elif cur = nw then "patched" else "UNKNOWN"), cur
    let classify (c: Cave) id =
        let st = [ for (rva, nw, o) in writesFor B c id -> classifyRow rva nw o ]
        for ((rva, _, _), (s, cur)) in List.zip (writesFor B c id) st do printfn "  %-2s %8X  %-8s  %s" id rva s (hexs cur)
        let ss = st |> List.map fst |> List.distinct
        match ss with [ s ] -> s | _ -> if ss |> List.contains "UNKNOWN" then "UNKNOWN" else "MIXED"
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]   // cave pid
    let caveOrProbe () = match readState () with Some a -> build a B | None -> build (B + 0x5E00000UL) B
    let applyIds (c: Cave) (todo: string list) (toNew: bool) =
        let ws = [ for id in todo do for (rva, nw, o) in writesFor B c id -> nativeint (B + uint64 rva), (if toNew then nw else o), (if toNew then o else nw) ]
        if not ws.IsEmpty then safeWrite t ws else 0
    match mode with
    | "dry" ->
        let c = caveOrProbe ()
        printfn "base=%X selected=%A cave=%s" B (ids ()) (match readState () with Some a -> sprintf "%X" a | None -> "(none, preview)")
        let st = ids () |> List.map (classify c)
        printfn "states: %A" st
        if selected.Contains "M1" then
            printfn "M1 cave preview:"
            printDis c.Bytes.[0x80 .. 0xC0] c.M1 B
    | "apply" ->
        writeGuard ()
        let probe = caveOrProbe ()
        if ids () |> List.exists (fun id -> classify probe id <> "original") then failwith "sites not original - aborting (revert first)"
        let c =
            match readState () with
            | Some a -> build a B                                            // reuse this process's cave
            | None ->
                let caveMem = t.AllocNear(nativeint (B + uint64 M1_SITE), 0x1000)
                let c = build (uint64 caveMem) B
                t.Write(caveMem, c.Bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" (uint64 caveMem) t.Process.Id)
                c
        let n = applyIds c (ids ()) true
        printfn "applied %A on attempt %d; cave %X (M1 switch = %d)" (ids ()) n (c.M1 - 0x80UL) (BitConverter.ToInt32(t.Read(nativeint c.En, 4), 0))
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        let st = ids () |> List.map (fun id -> id, classify c id)
        if st |> List.exists (fun (_, s) -> s = "UNKNOWN") then failwith "unexpected bytes - aborting"
        let todo = st |> List.filter (fun (_, s) -> s <> "original") |> List.map fst
        applyIds c todo false |> ignore
        printfn "reverted %A (cave left allocated)" todo
    | "on" | "off" ->
        writeGuard ()
        let c = caveOrProbe ()
        for id in ids () do
            if id = "M1" then
                match readState () with
                | Some _ when classify c "M1" = "patched" ->
                    t.Write(nativeint c.En, BitConverter.GetBytes(if mode = "on" then 1u else 0u))
                    printfn "M1 switch %s" mode
                | _ -> printfn "M1 is not applied in this process - use apply"
            else
                let s = classify c id
                if s = "UNKNOWN" then failwithf "%s: unexpected bytes - aborting" id
                if (mode = "on" && s <> "patched") || (mode = "off" && s <> "original") then
                    applyIds c [ id ] (mode = "on") |> ignore
                printfn "%s %s" id mode
    | "status" ->
        let c = caveOrProbe ()
        for id in [ "M1"; "M2"; "J1" ] do printfn "%s: %s" id (classify c id)
        match readState () with
        | None -> printfn "no cave in this process"
        | Some _ ->
            let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
            printfn "M1 switch=%d  kHead=%X  kVal=%X" (rd c.En) (BitConverter.ToUInt64(t.Read(nativeint c.KHead, 8), 0)) (BitConverter.ToUInt64(t.Read(nativeint c.KVal, 8), 0))
            let mutable p = rd c.Cnt
            for _ in 1 .. 10 do
                Threading.Thread.Sleep 100
                let q = rd c.Cnt
                printfn "  M1 cache refreshes/s %6d   (each refresh = one list walk; stock = one walk per sub-chunk)" ((q - p) * 10u)
                p <- q
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
