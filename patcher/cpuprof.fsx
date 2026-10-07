// cpuprof.fsx — external sampling CPU profiler for Nioh 3 (no admin, nothing injected, nothing written).
//
//   dotnet fsi cpuprof.fsx <seconds> <out.txt> [intervalUs=1000] [minCpuPct=2] [maxThreads=24] [stackBytes=32768]
//
//   * Per-thread CPU: GetThreadTimes (whole run) + QueryThreadCycleTime (per sample interval).
//   * Each tick (high-res waitable timer, jittered 70-130 %) every busy thread is sampled ONE AT A TIME:
//       SuspendThread -> GetThreadContext -> ReadProcessMemory(stack) -> ResumeThread  (try/finally;
//     the handle in flight is also resumed from ProcessExit / Ctrl+C handlers). Hold times are reported.
//   * Call stacks: real x64 unwinding with each module's .pdata/UNWIND_INFO (+ epilogue emulation),
//     falling back to a "looks like a return address" stack scan only if unwinding fails.
//   * "busy" = cycles the thread burned in the interval before the sample / (interval * clock).
//     A wait/sleep/yield function with high busy% == spin-waiting (Sleep(0), SwitchToThread, Peek loops).
//   * Hot Nioh3 functions are scanned statically for PAUSE / lock cmpxchg / Sleep / SwitchToThread / QPC calls,
//     and the hottest exact RIPs are listed (tight-loop detection).
//   * All aggregation is online (memory stays small). Names from re\out\functions.tsv (Ghidra) if present.
// Writes <out.txt> (report) and <out.txt>.tsv.
#r "nuget: Iced, 1.21.0"

open System
open System.IO
open System.Diagnostics
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel

module N =
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenProcess(uint32 access, bool inh, int pid)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenThread(uint32 access, bool inh, int tid)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern uint32 SuspendThread(nativeint h)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern uint32 ResumeThread(nativeint h)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool GetThreadContext(nativeint h, nativeint ctx)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool GetThreadTimes(nativeint h, int64& c, int64& e, int64& k, int64& u)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool QueryThreadCycleTime(nativeint h, uint64& cycles)
    [<DllImport("kernel32.dll")>]
    extern int GetThreadDescription(nativeint h, nativeint& s)
    [<DllImport("kernel32.dll")>]
    extern nativeint LocalFree(nativeint p)
    [<DllImport("ntdll.dll")>]
    extern int NtQueryInformationThread(nativeint h, int cls, int64& buf, int len, int& ret)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool ReadProcessMemory(nativeint h, nativeint addr, byte[] buf, nativeint size, nativeint& read)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool CloseHandle(nativeint h)
    [<DllImport("kernel32.dll", SetLastError = true, EntryPoint = "ReadProcessMemory")>]
    extern bool RPMPtr(nativeint h, nativeint addr, nativeint buf, nativeint size, nativeint& read)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint CreateWaitableTimerExW(nativeint attr, nativeint name, uint32 flags, uint32 access)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool SetWaitableTimer(nativeint h, int64& due, int period, nativeint f, nativeint a, bool resume)
    [<DllImport("kernel32.dll")>]
    extern uint32 WaitForSingleObject(nativeint h, uint32 ms)

// NtQueryInformationThread with a byte[] buffer (EntryPoint alias)
module N2 =
    [<DllImport("ntdll.dll", EntryPoint = "NtQueryInformationThread")>]
    extern int NtQIT(nativeint h, int cls, byte[] buf, int len, int& ret)

// ---------------------------------------------------------------- args
let argv = fsi.CommandLineArgs |> Array.skip 1
let arg i (d: string) = if argv.Length > i then argv.[i] else d
let durationS = float (arg 0 "20")
let outPath = Path.GetFullPath(arg 1 "cpuprof.txt")
let intervalUs = int (arg 2 "1000")
let minCpuPct = float (arg 3 "2")
let maxThreads = int (arg 4 "24")
let stackBytes = int (arg 5 "32768")
let heldBytes = 4096
let maxFrames = 64
let ROOT = @"C:\Users\Ahmad Hamza\nioh3-engine"
let DUMP = Path.Combine(ROOT, @"re\Nioh3.dump.exe")
let GHIDRA_TSV = Path.Combine(ROOT, @"re\out\functions.tsv")

let procName = match Environment.GetEnvironmentVariable "CPUPROF_PROC" with null | "" -> "Nioh3" | s -> s   // test on other processes
let proc =
    (match Environment.GetEnvironmentVariable "CPUPROF_PID" with
     | null | "" -> Process.GetProcessesByName(procName) |> Array.tryHead
     | s -> Some (Process.GetProcessById(int s))) |> Option.defaultWith (fun () -> failwithf "%s not running" procName)
let pid = proc.Id
let hp = N.OpenProcess(0x0410u (* QUERY_INFORMATION | VM_READ *), false, pid)
if hp = 0n then failwithf "OpenProcess failed %d" (Marshal.GetLastWin32Error())
let mainBase = uint64 (proc.MainModule.BaseAddress.ToInt64())

let rpm (addr: uint64) (n: int) =
    let buf = Array.zeroCreate<byte> n
    let mutable r = 0n
    if N.ReadProcessMemory(hp, nativeint addr, buf, nativeint n, &r) then Some buf else None

// ---------------------------------------------------------------- safety: never leave a thread suspended
let mutable inFlight = 0n
let resumeInFlight () =
    let h = Threading.Interlocked.Exchange(&inFlight, 0n)
    if h <> 0n then N.ResumeThread h |> ignore
AppDomain.CurrentDomain.ProcessExit.Add(fun _ -> resumeInFlight ())
AppDomain.CurrentDomain.UnhandledException.Add(fun _ -> resumeInFlight ())
Console.CancelKeyPress.Add(fun _ -> resumeInFlight ())

// ---------------------------------------------------------------- modules
let dumpImg = File.ReadAllBytes DUMP
let u32 (b: byte[]) o = BitConverter.ToUInt32(b, o)

type Mod =
    { Name: string; Base: uint64; Size: uint64
      Begins: uint32[]; Ends: uint32[]; Unwinds: uint32[]
      ExpRva: uint32[]; ExpName: string[]
      Read: uint32 -> int -> byte[] option }

let loadMod (name: string) (b: uint64) (size: uint64) : Mod =
    let isMain = b = mainBase && procName = "Nioh3"
    let cache = Dictionary<uint64, byte[] option>()
    let read : uint32 -> int -> byte[] option =
        if isMain then
            fun rva n -> if int64 rva + int64 n <= int64 dumpImg.Length then Some (Array.sub dumpImg (int rva) n) else None
        else
            fun rva n ->
                let k = (uint64 rva <<< 16) ||| uint64 n
                match cache.TryGetValue k with
                | true, v -> v
                | _ -> let v = rpm (b + uint64 rva) n in (if n <= 64 then cache.[k] <- v); v
    let empty = { Name = name; Base = b; Size = size; Begins = [||]; Ends = [||]; Unwinds = [||]; ExpRva = [||]; ExpName = [||]; Read = read }
    try
        let hdr = (read 0u 0x1000).Value
        let pe = BitConverter.ToInt32(hdr, 0x3C)
        let dd = pe + 24 + 112
        let expRva, expSz = u32 hdr dd, u32 hdr (dd + 4)
        let excRva, excSz = u32 hdr (dd + 24), u32 hdr (dd + 28)
        let begins, ends, unw =
            if excSz = 0u then [||], [||], [||] else
            match read excRva (int excSz) with
            | None -> [||], [||], [||]
            | Some p ->
                let n = int excSz / 12
                Array.init n (fun i -> u32 p (i * 12)), Array.init n (fun i -> u32 p (i * 12 + 4)), Array.init n (fun i -> u32 p (i * 12 + 8))
        let er, en =
            if expSz = 0u then [||], [||] else
            match read expRva (int expSz) with
            | None -> [||], [||]
            | Some e ->
                let nNames = int (u32 e 0x18)
                let aFun, aNames, aOrd = u32 e 0x1C, u32 e 0x20, u32 e 0x24
                let inDir (r: uint32) = r >= expRva && r < expRva + expSz
                let res = ResizeArray<uint32 * string>()
                for i in 0 .. nNames - 1 do
                    let nr = u32 e (int (aNames - expRva) + i * 4)
                    let ord = int (BitConverter.ToUInt16(e, int (aOrd - expRva) + i * 2))
                    let fr = u32 e (int (aFun - expRva) + ord * 4)
                    if inDir nr && not (inDir fr) then
                        let o = int (nr - expRva)
                        let mutable j = o
                        while j < e.Length && e.[j] <> 0uy do j <- j + 1
                        res.Add(fr, Text.Encoding.ASCII.GetString(e, o, j - o))
                let s = res |> Seq.sortBy fst |> Seq.toArray
                Array.map fst s, Array.map snd s
        { empty with Begins = begins; Ends = ends; Unwinds = unw; ExpRva = er; ExpName = en }
    with _ -> empty

let mods =
    proc.Modules |> Seq.cast<ProcessModule>
    |> Seq.map (fun m -> m.ModuleName, uint64 (m.BaseAddress.ToInt64()), uint64 m.ModuleMemorySize)
    |> Seq.sortBy (fun (_, b, _) -> b) |> Seq.toArray
let modBases = mods |> Array.map (fun (_, b, _) -> b)
let modCache = Dictionary<uint64, Mod>()
let findModIdx (a: uint64) =
    let i = Array.BinarySearch(modBases, a)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 then (let (_, b, s) = mods.[i] in if a >= b + 0x1000UL && a < b + s then i else -1) else -1
let getMod i =
    let (n, b, s) = mods.[i]
    match modCache.TryGetValue b with
    | true, m -> m
    | _ -> let m = loadMod n b s in modCache.[b] <- m; m
let modOf (a: uint64) = let i = findModIdx a in if i < 0 then None else Some (getMod i)

// Nioh3 imports: IAT slot rva -> name
let iatNames =
    let d = Dictionary<uint64, string>()
    let pe = BitConverter.ToInt32(dumpImg, 0x3C)
    let dd = pe + 24 + 112
    let cstr o = let e = Array.IndexOf(dumpImg, 0uy, o) in Text.Encoding.ASCII.GetString(dumpImg, o, e - o)
    let mutable p = int (u32 dumpImg (dd + 8))
    while u32 dumpImg (p + 12) <> 0u do
        let oft, ft = u32 dumpImg p, u32 dumpImg (p + 16)
        let th = if oft <> 0u then oft else ft
        let mutable i = 0
        while BitConverter.ToUInt64(dumpImg, int th + i * 8) <> 0UL do
            let ent = BitConverter.ToUInt64(dumpImg, int th + i * 8)
            let nm = if ent &&& (1UL <<< 63) = 0UL && ent < uint64 dumpImg.Length then cstr (int ent + 2) else sprintf "ord%d" (ent &&& 0xFFFFUL)
            d.[uint64 ft + uint64 (i * 8)] <- nm
            i <- i + 1
        p <- p + 20
    d

// Ghidra names (optional): re\outunctions.tsv with header "rva<TAB>size<TAB>name<TAB>callers"; FUN_* skipped
let ghidraNames =
    let d = Dictionary<uint32, string>()
    if File.Exists GHIDRA_TSV && procName = "Nioh3" then
        let lines = File.ReadAllLines GHIDRA_TSV
        let hdr = lines.[0].Split('\t') |> Array.map (fun h -> h.Trim().ToLowerInvariant())
        let ci n = Array.IndexOf(hdr, n)
        let ri, ni = ci "rva", ci "name"
        if ri >= 0 && ni >= 0 then
            for line in Seq.skip 1 lines do
                let f = line.Split('\t')
                if f.Length > max ri ni && not (f.[ni].StartsWith "FUN_") then
                    try d.[Convert.ToUInt32(f.[ri].Trim().Replace("0x", ""), 16)] <- f.[ni].Trim() with _ -> ()
    d

// ---------------------------------------------------------------- pdata lookup
let entryIdx (m: Mod) (rva: uint32) =
    if m.Begins.Length = 0 then -1 else
    let i = Array.BinarySearch(m.Begins, rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && rva < m.Ends.[i] then i else -1

let chainCache = Dictionary<struct (uint64 * int), uint32>()
let primaryStart (m: Mod) (i: int) =
    let key = struct (m.Base, i)
    match chainCache.TryGetValue key with
    | true, v -> v
    | _ ->
        let mutable start = m.Begins.[i]
        let mutable uw = m.Unwinds.[i]
        let mutable guard = 0
        let mutable go = true
        while go && guard < 16 do
            guard <- guard + 1
            match m.Read (uw &&& ~~~1u) 4 with
            | Some h when (h.[0] >>> 3) &&& 4uy <> 0uy ->
                let off = (uw &&& ~~~1u) + 4u + uint32 (2 * ((int h.[2] + 1) &&& ~~~1))
                match m.Read off 12 with
                | Some rf -> start <- u32 rf 0; uw <- u32 rf 8
                | None -> go <- false
            | _ -> go <- false
        chainCache.[key] <- start
        start

let funcStart (m: Mod) (rva: uint32) : uint32 * bool =
    let i = entryIdx m rva
    if i >= 0 then primaryStart m i, true
    elif m.Begins.Length = 0 then (rva &&& ~~~0xFFFu), false
    else
        let j = Array.BinarySearch(m.Begins, rva)
        let j = if j >= 0 then j else ~~~j - 1
        if j >= 0 then ((m.Ends.[j] + 15u) &&& ~~~15u), false else (rva &&& ~~~0xFFFu), false

let label (m: Mod) (start: uint32) (hasPdata: bool) =
    if m.Base = mainBase then
        let g = match ghidraNames.TryGetValue start with | true, n -> " " + n | _ -> ""
        sprintf "%s+%X%s%s" (if procName = "Nioh3" then "Nioh3" else m.Name) start (if hasPdata then "" else "(leaf?)") g
    else
        let i = Array.BinarySearch(m.ExpRva, start)
        let i = if i >= 0 then i else ~~~i - 1
        if i >= 0 && start = m.ExpRva.[i] then sprintf "%s!%s" m.Name m.ExpName.[i]
        elif i >= 0 && start - m.ExpRva.[i] < 0x4000u then sprintf "%s+%X (%s+%X)" m.Name start m.ExpName.[i] (start - m.ExpRva.[i])
        else sprintf "%s+%X%s" m.Name start (if hasPdata then "" else "(leaf?)")

[<CustomEquality; NoComparison>]
type Sym =
    { Key: uint64; Label: string; IsMain: bool; Rva: uint32 }   // Key = absolute function start
    override x.Equals o = match o with :? Sym as s -> s.Key = x.Key | _ -> false
    override x.GetHashCode() = x.Key.GetHashCode()
let symCache = Dictionary<uint64, Sym>()
let symbolize (a: uint64) =
    match symCache.TryGetValue a with
    | true, s -> s
    | _ ->
        let s =
            match modOf a with
            | None -> let k = a &&& ~~~0xFFFFUL in { Key = k; Label = sprintf "<anon %X>" k; IsMain = false; Rva = 0u }
            | Some m ->
                let st, hp = funcStart m (uint32 (a - m.Base))
                { Key = m.Base + uint64 st; Label = label m st hp; IsMain = (m.Base = mainBase); Rva = st }
        if symCache.Count < 2_000_000 then symCache.[a] <- s
        s

// ---------------------------------------------------------------- unwinder
let regIdx (r: Register) =
    match r with
    | Register.RAX -> 0 | Register.RCX -> 1 | Register.RDX -> 2 | Register.RBX -> 3
    | Register.RSP -> 4 | Register.RBP -> 5 | Register.RSI -> 6 | Register.RDI -> 7
    | Register.R8 -> 8 | Register.R9 -> 9 | Register.R10 -> 10 | Register.R11 -> 11
    | Register.R12 -> 12 | Register.R13 -> 13 | Register.R14 -> 14 | Register.R15 -> 15
    | _ -> -1

/// one frame of RtlVirtualUnwind; regs.[4] = rsp. Returns new rip or 0 on failure.
let rec unwindOne (rip: uint64) (regs: uint64[]) (stk: byte[]) (stkBase: uint64) (stkLen: int) : uint64 =
    let rd (a: uint64) =
        if a >= stkBase && a + 8UL <= stkBase + uint64 stkLen then Some (BitConverter.ToUInt64(stk, int (a - stkBase))) else None
    let popRet () =
        match rd regs.[4] with
        | Some r -> regs.[4] <- regs.[4] + 8UL; r
        | None -> 0UL
    match modOf rip with
    | None -> popRet ()
    | Some m ->
        let rva = uint32 (rip - m.Base)
        let i = entryIdx m rva
        if i < 0 then popRet () else   // leaf
        let epi =
            match m.Read rva 32 with
            | None -> None
            | Some code ->
                let dec = Decoder.Create(64, ByteArrayCodeReader(code))
                dec.IP <- rip
                let r2 = Array.copy regs
                let mutable result = None
                let mutable go = true
                let mutable n = 0
                let mutable sawAdj = false
                while go && n < 12 do
                    n <- n + 1
                    let ins = dec.Decode()
                    if ins.IsInvalid then go <- false
                    elif ins.Mnemonic = Mnemonic.Add && ins.Op0Kind = OpKind.Register && ins.Op0Register = Register.RSP && ins.Op1Kind <> OpKind.Register then
                        r2.[4] <- r2.[4] + uint64 (int64 ins.Immediate32to64); sawAdj <- true
                    elif ins.Mnemonic = Mnemonic.Lea && ins.Op0Register = Register.RSP && ins.Op1Kind = OpKind.Memory && regIdx ins.MemoryBase >= 0 && ins.MemoryIndex = Register.None then
                        r2.[4] <- r2.[regIdx ins.MemoryBase] + uint64 (int64 ins.MemoryDisplacement64); sawAdj <- true
                    elif ins.Mnemonic = Mnemonic.Pop && ins.Op0Kind = OpKind.Register && regIdx ins.Op0Register >= 0 then
                        match rd r2.[4] with
                        | Some v -> r2.[regIdx ins.Op0Register] <- v; r2.[4] <- r2.[4] + 8UL; sawAdj <- true
                        | None -> go <- false
                    elif ins.Mnemonic = Mnemonic.Ret then
                        match rd r2.[4] with
                        | Some v -> r2.[4] <- r2.[4] + 8UL; result <- Some (v, r2); go <- false
                        | None -> go <- false
                    elif ins.Mnemonic = Mnemonic.Jmp && sawAdj && ins.FlowControl = FlowControl.UnconditionalBranch then
                        // tail jump out of the function
                        let t = ins.NearBranchTarget
                        let e = entryIdx m (uint32 (t - m.Base))
                        if e <> i then (match rd r2.[4] with Some v -> r2.[4] <- r2.[4] + 8UL; result <- Some (v, r2) | None -> ())
                        go <- false
                    else go <- false
                result
        match epi with
        | Some (v, r2) -> Array.blit r2 0 regs 0 16; v
        | None -> unwindCodes m i rva regs rd popRet

and unwindCodes (m: Mod) (i: int) (rva: uint32) (regs: uint64[]) (rd: uint64 -> uint64 option) (popRet: unit -> uint64) : uint64 =
        let mutable uw = m.Unwinds.[i]
        let mutable begin_ = m.Begins.[i]
        let mutable first = true
        let mutable ok = true
        let mutable machFrame = 0UL
        let mutable chain = true
        let mutable guard = 0
        while chain && ok && guard < 8 do
            guard <- guard + 1
            chain <- false
            match m.Read (uw &&& ~~~1u) 4 with
            | None -> ok <- false
            | Some h ->
                let flags = int (h.[0] >>> 3)
                let cnt = int h.[2]
                let frameReg = int (h.[3] &&& 0xFuy)
                let frameOff = int (h.[3] >>> 4)
                let offInFn = if first then int (rva - begin_) else Int32.MaxValue
                match m.Read ((uw &&& ~~~1u) + 4u) (2 * cnt + 12) with
                | None -> ok <- false
                | Some codes ->
                    let slot k = int (BitConverter.ToUInt16(codes, 2 * k))
                    // walk codes
                    let mutable k = 0
                    let mutable frameBase = regs.[4]
                    // determine if SET_FPREG already executed: find it while walking first
                    let mutable kk = 0
                    while kk < cnt do
                        let c = slot kk
                        let off = c &&& 0xFF
                        let op = (c >>> 8) &&& 0xF
                        let info = (c >>> 12) &&& 0xF
                        if op = 3 && off <= offInFn then frameBase <- regs.[frameReg] - uint64 (frameOff * 16)
                        kk <- kk + (match op with 1 -> (if info = 0 then 2 else 3) | 4 -> 2 | 5 -> 3 | 6 -> (if int (h.[0] &&& 7uy) >= 2 then 1 else 2) | 7 -> 3 | 8 -> 2 | 9 -> 3 | _ -> 1)
                    while ok && k < cnt do
                        let c = slot k
                        let off = c &&& 0xFF
                        let op = (c >>> 8) &&& 0xF
                        let info = (c >>> 12) &&& 0xF
                        let exec = off <= offInFn
                        let size = match op with 1 -> (if info = 0 then 2 else 3) | 4 -> 2 | 5 -> 3 | 6 -> (if int (h.[0] &&& 7uy) >= 2 then 1 else 2) | 7 -> 3 | 8 -> 2 | 9 -> 3 | _ -> 1
                        if exec then
                            match op with
                            | 0 -> (match rd regs.[4] with Some v -> regs.[info] <- v; regs.[4] <- regs.[4] + 8UL | None -> ok <- false)
                            | 1 ->
                                let sz = if info = 0 then uint64 (slot (k + 1)) * 8UL else uint64 (slot (k + 1)) ||| (uint64 (slot (k + 2)) <<< 16)
                                regs.[4] <- regs.[4] + sz
                            | 2 -> regs.[4] <- regs.[4] + uint64 (info * 8 + 8)
                            | 3 -> regs.[4] <- regs.[frameReg] - uint64 (frameOff * 16)
                            | 4 -> (match rd (frameBase + uint64 (slot (k + 1)) * 8UL) with Some v -> regs.[info] <- v | None -> ())
                            | 5 -> (match rd (frameBase + (uint64 (slot (k + 1)) ||| (uint64 (slot (k + 2)) <<< 16))) with Some v -> regs.[info] <- v | None -> ())
                            | 10 ->
                                let b = regs.[4] + (if info = 1 then 8UL else 0UL)
                                match rd b, rd (b + 24UL) with
                                | Some r, Some s -> machFrame <- r; regs.[4] <- s
                                | _ -> ok <- false
                            | _ -> ()
                        k <- k + size
                    if ok && flags &&& 4 <> 0 then
                        let o = 2 * ((cnt + 1) &&& ~~~1)
                        uw <- u32 codes (o + 8)
                        begin_ <- u32 codes o
                        first <- false
                        chain <- true
        if not ok then 0UL
        elif machFrame <> 0UL then machFrame
        else popRet ()

// return-address check (fallback scan only)
let retCache = Dictionary<uint64, bool>()
let isRetAddr (a: uint64) =
    match retCache.TryGetValue a with
    | true, v -> v
    | _ ->
        let v =
            match modOf a with
            | None -> false
            | Some m ->
                let rva = uint32 (a - m.Base)
                rva >= 0x1007u &&
                (match m.Read (rva - 7u) 7 with
                 | None -> false
                 | Some b ->
                    [ 5; 6; 2; 3; 7; 4 ] |> List.exists (fun k ->
                        let dec = Decoder.Create(64, ByteArrayCodeReader(b, 7 - k, k))
                        dec.IP <- a - uint64 k
                        let ins = dec.Decode()
                        not ins.IsInvalid && ins.Length = k && (ins.FlowControl = FlowControl.Call || ins.FlowControl = FlowControl.IndirectCall)))
        if retCache.Count < 1_000_000 then retCache.[a] <- v
        v

// ---------------------------------------------------------------- threads
type TRec =
    { Tid: int; H: nativeint; Desc: string; Start: uint64; StackBase: uint64
      mutable C0: int64; mutable CL: int64; mutable K0: int64; mutable KL: int64; mutable Win: float
      mutable Cyc0: uint64; mutable CycLast: uint64; mutable TsLast: int64; mutable Samples: int }

let threads = Dictionary<int, TRec>()
let times (h: nativeint) =
    let mutable c, e, k, u = 0L, 0L, 0L, 0L
    if N.GetThreadTimes(h, &c, &e, &k, &u) then Some (k, u) else None
let cycles (h: nativeint) = let mutable c = 0UL in (if N.QueryThreadCycleTime(h, &c) then c else 0UL)

let openThread tid =
    let h = N.OpenThread(0x084Au, false, tid)   // SUSPEND_RESUME|GET_CONTEXT|QUERY_INFORMATION|QUERY_LIMITED
    if h = 0n then None else
    let mutable p = 0n
    let desc =
        if N.GetThreadDescription(h, &p) >= 0 && p <> 0n then
            let s = Marshal.PtrToStringUni p in N.LocalFree p |> ignore; s
        else ""
    let mutable sa, rl = 0L, 0
    N.NtQueryInformationThread(h, 9, &sa, 8, &rl) |> ignore
    // THREAD_BASIC_INFORMATION: ExitStatus(4)+pad, TebBaseAddress @8
    let tbi = Array.zeroCreate<byte> 48
    let stackBase =
        if N2.NtQIT(h, 0, tbi, 48, &rl) >= 0 then
            let teb = BitConverter.ToUInt64(tbi, 8)
            match rpm (teb + 8UL) 8 with Some b -> BitConverter.ToUInt64(b, 0) | None -> 0UL
        else 0UL
    match times h with
    | Some (k, u) ->
        let cy = cycles h
        Some { Tid = tid; H = h; Desc = desc; Start = uint64 sa; StackBase = stackBase
               C0 = k + u; CL = k + u; K0 = k; KL = k; Win = 0.0; Cyc0 = cy; CycLast = cy; TsLast = Stopwatch.GetTimestamp(); Samples = 0 }
    | None -> N.CloseHandle h |> ignore; None

let mutable lastRefresh = Stopwatch.GetTimestamp()
let refresh () =
    let now = Stopwatch.GetTimestamp()
    let dtSec = float (now - lastRefresh) / float Stopwatch.Frequency
    lastRefresh <- now
    let live = try (Process.GetProcessById pid).Threads |> Seq.cast<ProcessThread> |> Seq.map (fun t -> t.Id) |> Seq.toArray with _ -> [||]
    for tid in live do
        if not (threads.ContainsKey tid) then
            match openThread tid with Some r -> threads.[tid] <- r | None -> ()
    for r in threads.Values do
        match times r.H with
        | Some (k, u) ->
            let c = k + u
            r.Win <- if dtSec > 0.0 then float (c - r.CL) / 1e7 / dtSec * 100.0 else 0.0
            r.CL <- c; r.KL <- k
        | None -> r.Win <- 0.0

let pickSet () =
    threads.Values |> Seq.filter (fun r -> r.Win >= minCpuPct) |> Seq.sortByDescending (fun r -> r.Win)
    |> Seq.truncate maxThreads |> Seq.toArray

// ---------------------------------------------------------------- online aggregation
type FnAgg = { mutable N: int; mutable Busy: float; mutable BusyN: int; Callers: Dictionary<uint64, int>; Rips: Dictionary<uint64, int> }
type ThAgg =
    { Self: Dictionary<uint64, FnAgg>; Incl: Dictionary<uint64, int>; Attr: Dictionary<uint64, int>
      Mods: Dictionary<string, int>; mutable N: int; mutable BusySum: float; mutable BusyN: int; mutable Unwound: int; mutable Depth: int64 }
let aggs = Dictionary<int, ThAgg>()
let labels = Dictionary<uint64, Sym>()
let bump (d: Dictionary<'k, int>) k = d.[k] <- (match d.TryGetValue k with | true, v -> v + 1 | _ -> 1)

let ctxMem = Marshal.AllocHGlobal(0x4D0 + 64)
let ctx = nativeint ((int64 ctxMem + 15L) &&& ~~~15L)
let stackBuf = Array.zeroCreate<byte> stackBytes
let regs = Array.zeroCreate<uint64> 16
let holdHist = Array.zeroCreate<int> 64     // hold time histogram, 5 us buckets (last = overflow)
let deliverHist = Array.zeroCreate<int> 64
let mutable holdMax, deliverMax, sampleCount, sampleFails = 0.0, 0.0, 0, 0
let tickUs (t: int64) = float t * 1e6 / float Stopwatch.Frequency
let histAdd (h: int[]) (us: float) = h.[min 63 (int (us / 5.0))] <- h.[min 63 (int (us / 5.0))] + 1
let mutable cyclesPerSec = 4.7e9   // re-estimated after calibration

let processSample (r: TRec) (rip: uint64) (rsp: uint64) (nread: int) (busy: float) =
    let a = match aggs.TryGetValue r.Tid with
            | true, a -> a
            | _ -> let a = { Self = Dictionary(); Incl = Dictionary(); Attr = Dictionary(); Mods = Dictionary(); N = 0; BusySum = 0.0; BusyN = 0; Unwound = 0; Depth = 0L } in aggs.[r.Tid] <- a; a
    a.N <- a.N + 1
    let hasBusy = not (Double.IsNaN busy)
    if hasBusy then a.BusySum <- a.BusySum + busy; a.BusyN <- a.BusyN + 1
    // unwind
    let frames = ResizeArray<uint64>(16)
    let mutable pc = rip
    let mutable n = 0
    let mutable good = true
    while good && n < maxFrames do
        let next = try unwindOne pc regs stackBuf rsp nread with _ -> 0UL
        if next = 0UL || modOf next |> Option.isNone then good <- false
        else frames.Add next; pc <- next; n <- n + 1
    let unwoundOk = frames.Count >= 2 || (frames.Count >= 1 && regs.[4] >= rsp + uint64 nread - 64UL)
    if frames.Count >= 2 then a.Unwound <- a.Unwound + 1
    if not unwoundOk then
        // fallback: scan
        frames.Clear()
        let mutable i = 0
        while i + 8 <= nread && frames.Count < 24 do
            let v = BitConverter.ToUInt64(stackBuf, i)
            if v > 0x10000UL && findModIdx v >= 0 && isRetAddr v then frames.Add v
            i <- i + 8
    a.Depth <- a.Depth + int64 frames.Count
    let sy = symbolize rip
    labels.[sy.Key] <- sy
    let f = match a.Self.TryGetValue sy.Key with
            | true, f -> f
            | _ -> let f = { N = 0; Busy = 0.0; BusyN = 0; Callers = Dictionary(); Rips = Dictionary() } in a.Self.[sy.Key] <- f; f
    f.N <- f.N + 1
    if hasBusy then f.Busy <- f.Busy + busy; f.BusyN <- f.BusyN + 1
    if f.Rips.Count < 4096 then bump f.Rips rip
    if frames.Count > 0 then (if f.Callers.Count < 512 then bump f.Callers frames.[0])
    bump a.Mods (match modOf rip with Some m -> m.Name | None -> "<anon>")
    let seen = HashSet<uint64>()
    seen.Add sy.Key |> ignore
    let mutable attr = if sy.IsMain then sy.Key else 0UL
    for ra in frames do
        let c = symbolize ra
        labels.[c.Key] <- c
        seen.Add c.Key |> ignore
        if attr = 0UL && c.IsMain then attr <- c.Key
    for k in seen do bump a.Incl k
    bump a.Attr attr

let sampleOne (r: TRec) =
    // cycles burned since the previous sample of this thread -> busy fraction
    let cyNow = cycles r.H
    let tsNow = Stopwatch.GetTimestamp()
    let dtS = float (tsNow - r.TsLast) / float Stopwatch.Frequency
    let busy = if r.Samples > 0 && dtS > 0.0 then min 1.5 (float (cyNow - r.CycLast) / (dtS * cyclesPerSec)) else nan
    r.CycLast <- cyNow; r.TsLast <- tsNow
    let mutable rip, rsp, nread, got = 0UL, 0UL, 0, false
    let mutable restLen = 0
    let t0 = Stopwatch.GetTimestamp()
    let mutable t1 = t0
    inFlight <- r.H
    let prev = N.SuspendThread r.H
    if prev = 0xFFFFFFFFu then
        inFlight <- 0n
        sampleFails <- sampleFails + 1
    else
        try
            Marshal.WriteInt32(ctx, 0x30, 0x100003)   // CONTEXT_AMD64 | CONTROL | INTEGER
            if N.GetThreadContext(r.H, ctx) then
                t1 <- Stopwatch.GetTimestamp()
                rsp <- uint64 (Marshal.ReadInt64(ctx, 0x98))
                let lim = if r.StackBase > rsp then int (min (uint64 stackBytes) (r.StackBase - rsp)) else stackBytes
                let first = min lim heldBytes   // only the innermost frames are read while the thread is held
                let mutable rd = 0n
                if not (N.ReadProcessMemory(hp, nativeint rsp, stackBuf, nativeint first, &rd)) then
                    let toEnd = 0x1000 - int (rsp &&& 0xFFFUL)
                    if not (N.ReadProcessMemory(hp, nativeint rsp, stackBuf, nativeint (min toEnd first), &rd)) then rd <- 0n
                nread <- int rd
                if nread = first && lim > first then restLen <- lim - first
                got <- true
        finally
            inFlight <- 0n
            N.ResumeThread r.H |> ignore
        let t2 = Stopwatch.GetTimestamp()
        let dUs, hUs = tickUs (t1 - t0), tickUs (t2 - t1)
        histAdd deliverHist dUs; histAdd holdHist hUs
        deliverMax <- max deliverMax dUs; holdMax <- max holdMax hUs
        if got && restLen > 0 then
            // outer frames: read after resume (they do not change on a microsecond scale)
            let mutable rd2 = 0n
            let pin = GCHandle.Alloc(stackBuf, GCHandleType.Pinned)
            try
                if N.RPMPtr(hp, nativeint (rsp + uint64 nread), pin.AddrOfPinnedObject() + nativeint nread, nativeint restLen, &rd2) then nread <- nread + int rd2
            finally pin.Free()
        if got then
            rip <- uint64 (Marshal.ReadInt64(ctx, 0xF8))
            for i in 0 .. 15 do regs.[i] <- uint64 (Marshal.ReadInt64(ctx, 0x78 + 8 * i))
            r.Samples <- r.Samples + 1
            sampleCount <- sampleCount + 1
            if prev = 0u then processSample r rip rsp nread busy
        else sampleFails <- sampleFails + 1

// ---------------------------------------------------------------- run
let startWall = DateTime.Now
printfn "cpuprof: pid=%d base=%X modules=%d duration=%.0fs interval=%dus stack=%dB -> %s" pid mainBase mods.Length durationS intervalUs stackBytes outPath
Threading.Thread.CurrentThread.Priority <- Threading.ThreadPriority.Highest
try Process.GetCurrentProcess().PriorityClass <- ProcessPriorityClass.High with _ -> ()
refresh ()
Threading.Thread.Sleep 1000
refresh ()
// estimate clock: cycles per CPU-second over the calibration second
let calCyc, calCpu =
    threads.Values |> Seq.fold (fun (cy, cpu) r -> let c = cycles r.H in (cy + float (c - r.Cyc0), cpu + float (r.CL - r.C0) / 1e7)) (0.0, 0.0)
if calCpu > 0.5 then cyclesPerSec <- calCyc / calCpu
for r in threads.Values do r.C0 <- r.CL; r.K0 <- r.KL; r.Cyc0 <- cycles r.H; r.CycLast <- r.Cyc0; r.TsLast <- Stopwatch.GetTimestamp()
let mutable set = pickSet ()
printfn "clock est %.2f GHz; busy threads: %s" (cyclesPerSec / 1e9) (set |> Array.map (fun r -> sprintf "%d(%.0f%%)" r.Tid r.Win) |> String.concat " ")

// warm-up: JIT + marshalling stubs for everything used inside the hold window (first run had a 32 ms hold)
do
    let r0 = threads.Values |> Seq.head
    let mutable rd = 0n
    Marshal.WriteInt32(ctx, 0x30, 0x100003)
    N.GetThreadContext(r0.H, ctx) |> ignore   // no suspend: just exercises the stub
    N.ReadProcessMemory(hp, nativeint mainBase, stackBuf, 64n, &rd) |> ignore
    let pin = GCHandle.Alloc(stackBuf, GCHandleType.Pinned)
    N.RPMPtr(hp, nativeint mainBase, pin.AddrOfPinnedObject(), 64n, &rd) |> ignore
    pin.Free()
    Stopwatch.GetTimestamp() |> ignore
    histAdd holdHist 0.0; holdHist.[0] <- 0
let timer = N.CreateWaitableTimerExW(0n, 0n, 2u (* HIGH_RESOLUTION *), 0x1F0003u)
let rnd = Random(1)
let swAll = Stopwatch.StartNew()
let mutable ticks = 0
try
    while swAll.Elapsed.TotalSeconds < durationS do
        let jitter = intervalUs * (70 + rnd.Next(61)) / 100
        let mutable due = -(int64 jitter * 10L)
        if timer <> 0n && N.SetWaitableTimer(timer, &due, 0, 0n, 0n, false) then N.WaitForSingleObject(timer, 100u) |> ignore
        else Threading.Thread.Sleep 1
        for r in set do sampleOne r
        ticks <- ticks + 1
        if float (Stopwatch.GetTimestamp() - lastRefresh) / float Stopwatch.Frequency >= 1.0 then
            refresh ()
            set <- pickSet ()
finally
    resumeInFlight ()
let elapsed = swAll.Elapsed.TotalSeconds
refresh ()
try Process.GetCurrentProcess().PriorityClass <- ProcessPriorityClass.Normal with _ -> ()
printfn "sampling done: %d samples, %d ticks, %.1fs" sampleCount ticks elapsed

// ---------------------------------------------------------------- static spin analysis of a Nioh3 function
let fragments =
    lazy (
        let m = getMod (findModIdx mainBase |> fun i -> if i >= 0 then i else (mods |> Array.findIndex (fun (_, b, _) -> b = mainBase)))
        let d = Dictionary<uint32, ResizeArray<uint32 * uint32>>()
        for i in 0 .. m.Begins.Length - 1 do
            let p = primaryStart m i
            let l = match d.TryGetValue p with | true, l -> l | _ -> (let l = ResizeArray() in d.[p] <- l; l)
            l.Add((m.Begins.[i], m.Ends.[i]))
        d)
let spinFlags (startRva: uint32) =
    let flags = HashSet<string>()
    let mutable size = 0
    match fragments.Value.TryGetValue startRva with
    | true, frs ->
        for (b, e) in frs do
            size <- size + int (e - b)
            let dec = Decoder.Create(64, ByteArrayCodeReader(dumpImg, int b, int (e - b)))
            dec.IP <- uint64 b
            while dec.IP < uint64 e do
                let ins = dec.Decode()
                if ins.Mnemonic = Mnemonic.Pause then flags.Add "PAUSE" |> ignore
                if ins.HasLockPrefix then flags.Add "lock" |> ignore
                if ins.Mnemonic = Mnemonic.Xchg && ins.Op0Kind = OpKind.Memory then flags.Add "xchg" |> ignore
                if ins.FlowControl = FlowControl.IndirectCall || ins.FlowControl = FlowControl.IndirectBranch then
                    if ins.IsIPRelativeMemoryOperand then
                        match iatNames.TryGetValue ins.IPRelativeMemoryAddress with
                        | true, n when n.Contains "Sleep" || n.Contains "SwitchToThread" || n.Contains "Yield" || n.Contains "QueryPerformance"
                                       || n.Contains "WaitFor" || n.Contains "WaitOnAddress" || n.Contains "SleepConditionVariable" || n.Contains "PeekMessage" || n.Contains "timeGetTime" ->
                            flags.Add n |> ignore
                        | _ -> ()
    | _ -> ()
    size, (flags |> Seq.sort |> String.concat ",")

// tight loop check around a hot RIP: find a backward branch within 64 bytes after rip that jumps to <= rip
let loopAt (rip: uint64) =
    match modOf rip with
    | Some m when m.Base = mainBase && procName = "Nioh3" && int (rip - m.Base) + 96 <= dumpImg.Length ->
        let rva = int (rip - m.Base)
        let dec = Decoder.Create(64, ByteArrayCodeReader(dumpImg, rva, 96))
        dec.IP <- uint64 rva
        let mutable res = ""
        let mutable n = 0
        while res = "" && n < 24 do
            n <- n + 1
            let ins = dec.Decode()
            if ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch then
                let t = ins.NearBranchTarget
                if t <= uint64 rva && uint64 rva - t < 128UL then res <- sprintf "loop %X..%X (%d B)" t ins.IP (int (ins.IP - t) + ins.Length)
            if ins.FlowControl = FlowControl.Return then n <- 99
        res
    | _ -> ""

// ---------------------------------------------------------------- report
let sb = Text.StringBuilder()
let pr fmt = Printf.kprintf (fun s -> sb.AppendLine s |> ignore) fmt
let histPct (h: int[]) p =
    let total = Array.sum h
    let mutable acc, i = 0, 0
    while i < 63 && float acc < float total * p do acc <- acc + h.[i]; i <- i + 1
    float i * 5.0
let over50 (h: int[]) = h.[10..] |> Array.sum

pr "# cpuprof — %s pid" procName; pr "pid %d, base %X" pid mainBase
pr "start %s, wall %.1f s, interval %d us (jittered), stack read %d B, clock est %.2f GHz" (startWall.ToString "yyyy-MM-dd HH:mm:ss") elapsed intervalUs stackBytes (cyclesPerSec / 1e9)
pr "samples %d (%.0f Hz aggregate), failures %d, ghidra names %d" sampleCount (float sampleCount / elapsed) sampleFails ghidraNames.Count
pr "suspend->context (delivery, target not yet stopped) us: p50<=%.0f p99<=%.0f max %.0f" (histPct deliverHist 0.5) (histPct deliverHist 0.99) deliverMax
pr "context->resume (target HELD stopped) us: p50<=%.0f p99<=%.0f max %.0f; holds >50us: %d" (histPct holdHist 0.5) (histPct holdHist 0.99) holdMax (over50 holdHist)
pr ""
let totalCpu = threads.Values |> Seq.sumBy (fun r -> float (r.CL - r.C0) / 1e7 / elapsed * 100.0)
pr "## Threads (CPU%% = %% of one core; process total %.0f%% = %.2f cores; busy = mean cycle-busy fraction between samples)" totalCpu (totalCpu / 100.0)
pr "%-7s %6s %6s %6s %7s  %-34s %s" "TID" "CPU%" "kern%" "busy" "samples" "start function" "description"
let threadRows =
    threads.Values |> Seq.map (fun r -> r, float (r.CL - r.C0) / 1e7 / elapsed * 100.0, float (r.KL - r.K0) / 1e7 / elapsed * 100.0)
    |> Seq.sortByDescending (fun (_, c, _) -> c) |> Seq.toArray
for (r, c, k) in threadRows do
    if c >= 0.5 || r.Samples > 0 then
        let sl = if r.Start <> 0UL then (symbolize r.Start).Label else "?"
        let b = match aggs.TryGetValue r.Tid with | true, a when a.BusyN > 0 -> sprintf "%5.0f%%" (a.BusySum / float a.BusyN * 100.0) | _ -> "     -"
        pr "%-7d %6.1f %6.1f %6s %7d  %-34s %s" r.Tid c k b r.Samples sl r.Desc
pr "(+%d threads under 0.5%% CPU not listed)" (threadRows |> Array.filter (fun (r, c, _) -> c < 0.5 && r.Samples = 0) |> Array.length)
// group totals by thread-name family
pr ""
pr "### CPU by thread family"
let family (d: string) =
    let d = if String.IsNullOrEmpty d then "(unnamed)" else d
    let d = Text.RegularExpressions.Regex.Replace(d, @"\d+$", "")
    d
for (fam, rows) in threadRows |> Array.groupBy (fun (r, _, _) -> family r.Desc) |> Array.sortByDescending (fun (_, rs) -> rs |> Array.sumBy (fun (_, c, _) -> c)) |> Array.truncate 25 do
    pr "  %6.1f%%  %3d threads  %s" (rows |> Array.sumBy (fun (_, c, _) -> c)) rows.Length fam
pr ""

let tsv = Text.StringBuilder()
tsv.AppendLine "tid\tthread\tview\tcount\tpct\tbusy\tfunction" |> ignore
let lbl k = match labels.TryGetValue k with | true, s -> s.Label | _ -> sprintf "%X" k
let busyFns = ResizeArray<string>()
for (r, c, _) in threadRows do
    match aggs.TryGetValue r.Tid with
    | true, a when a.N >= 50 ->
        let n = float a.N
        pr "## Thread %d  %s  CPU %.1f%%  busy %.0f%%  samples %d  unwound %.0f%%  avg depth %.1f  start=%s" r.Tid r.Desc c (a.BusySum / float (max 1 a.BusyN) * 100.0) a.N (float a.Unwound / n * 100.0) (float a.Depth / n) (if r.Start <> 0UL then (symbolize r.Start).Label else "?")
        pr "  modules: %s" (a.Mods |> Seq.sortByDescending (fun kv -> kv.Value) |> Seq.truncate 8 |> Seq.map (fun kv -> sprintf "%s %.1f%%" kv.Key (float kv.Value / n * 100.0)) |> String.concat ", ")
        pr "  -- self top 30:  samples%%  busy%%  function  <- callers (return sites)  [static flags]  hottest RIPs --"
        for kv in a.Self |> Seq.sortByDescending (fun kv -> kv.Value.N) |> Seq.truncate 30 do
            let f = kv.Value
            let sy = labels.[kv.Key]
            let cl =
                f.Callers |> Seq.sortByDescending (fun x -> x.Value) |> Seq.truncate 3
                |> Seq.map (fun x -> sprintf "%s @%s(%d%%)" (symbolize x.Key).Label (match modOf x.Key with Some m -> sprintf "%X" (x.Key - m.Base) | None -> "?") (x.Value * 100 / f.N)) |> String.concat "; "
            let fl = if sy.IsMain && procName = "Nioh3" then (let sz, fl = spinFlags sy.Rva in sprintf " [%dB %s]" sz fl) else ""
            let rips =
                f.Rips |> Seq.sortByDescending (fun x -> x.Value) |> Seq.truncate 3
                |> Seq.map (fun x -> sprintf "%X:%d%%%s" (match modOf x.Key with Some m -> x.Key - m.Base | None -> x.Key) (x.Value * 100 / f.N) (let l = loopAt x.Key in if l <> "" then " " + l else ""))
                |> String.concat " "
            let bz = if f.BusyN > 0 then f.Busy / float f.BusyN * 100.0 else nan
            pr "  %5.1f%% %4.0f%%  %s%s  <- %s  | %s" (float f.N / n * 100.0) bz sy.Label fl cl rips
            tsv.AppendLine(sprintf "%d\t%s\tself\t%d\t%.3f\t%.1f\t%s" r.Tid r.Desc f.N (float f.N / n * 100.0) bz sy.Label) |> ignore
            if float f.N / n > 0.03 && bz > 50.0 then
                let l = sy.Label
                if l.Contains "Delay" || l.Contains "Yield" || l.Contains "Sleep" || l.Contains "SwitchToThread" || l.Contains "Wait" || l.Contains "PeekMessage" || l.Contains "QueryPerformance" || fl.Contains "PAUSE" then
                    busyFns.Add(sprintf "tid %d %s: %.1f%% of samples in %s with %.0f%% busy  <- %s" r.Tid r.Desc (float f.N / n * 100.0) l bz cl)
        pr "  -- attributed to first Nioh3 frame top 20 --"
        for kv in a.Attr |> Seq.sortByDescending (fun kv -> kv.Value) |> Seq.truncate 20 do
            pr "  %5.1f%%  %s" (float kv.Value / n * 100.0) (if kv.Key = 0UL then "<no Nioh3 frame>" else lbl kv.Key)
            tsv.AppendLine(sprintf "%d\t%s\tattr\t%d\t%.3f\t\t%s" r.Tid r.Desc kv.Value (float kv.Value / n * 100.0) (if kv.Key = 0UL then "-" else lbl kv.Key)) |> ignore
        pr "  -- inclusive top 30 --"
        for kv in a.Incl |> Seq.sortByDescending (fun kv -> kv.Value) |> Seq.truncate 30 do
            pr "  %5.1f%%  %s" (float kv.Value / n * 100.0) (lbl kv.Key)
            tsv.AppendLine(sprintf "%d\t%s\tincl\t%d\t%.3f\t\t%s" r.Tid r.Desc kv.Value (float kv.Value / n * 100.0) (lbl kv.Key)) |> ignore
        pr ""
    | _ -> ()

let head = Text.StringBuilder()
head.AppendLine("## SPIN-WAIT SUSPECTS (wait/sleep/yield/QPC/PAUSE functions with >3% of a thread's samples and >50% busy)") |> ignore
for s in busyFns do head.AppendLine("  " + s) |> ignore
if busyFns.Count = 0 then head.AppendLine "  (none)" |> ignore
head.AppendLine() |> ignore
let text = sb.ToString()
let split = text.IndexOf("## Threads")
let final = text.Substring(0, split) + head.ToString() + text.Substring(split)
Directory.CreateDirectory(Path.GetDirectoryName outPath) |> ignore
File.WriteAllText(outPath, final)
File.WriteAllText(outPath + ".tsv", tsv.ToString())
for r in threads.Values do N.CloseHandle r.H |> ignore
printfn "wrote %s (+ .tsv)" outPath
