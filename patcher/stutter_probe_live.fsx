// stutter_probe_live.fsx - SPR: measurement-only probe for the hitch classes in notes\stutter.md. Wraps the engine's
// RUNTIME pipeline-state creations (PSO cache misses on the dispatcher thread), the PSO preload creations, the upload-
// heap overflow fallback (dedicated committed resource) and the per-upload CreatePlacedResource with a timing cave:
// count, total and max time, a 5-bucket duration histogram and a ring of the slow events (TSC stamps). Nothing about
// what the game does changes: each cave calls the original target with the original arguments and returns its result.
//
//   dotnet fsi stutter_probe_live.fsx offline          NO GAME ACCESS: site bytes vs the dump, decoded originals, branch
//                                                      targets inside the sites (whole .text sweep), overlap with every
//                                                      other patch set (wj_common OTHER_SITES + mod patches.json), and a
//                                                      native transparency test of every cave kind (random registers and
//                                                      stack arguments: the callee sees identical rcx/rdx/r8/r9 and stack
//                                                      args, the caller gets the callee's rax back, rbx/rbp/rsi/rdi/r12-r15
//                                                      and rsp are preserved, the counters move; switch-off path too).
//   dotnet fsi stutter_probe_live.fsx dry|status       READ-ONLY on the game: per-site state.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi stutter_probe_live.fsx apply [ids]   install (default: all sites)
//   $env:FT_ALLOW_WRITE=1; dotnet fsi stutter_probe_live.fsx on|off        global switch (off = caves tail-jump, no counting)
//   dotnet fsi stutter_probe_live.fsx stats            READ-ONLY: counters since apply
//   dotnet fsi stutter_probe_live.fsx watch [sec=1]    READ-ONLY: per-interval deltas + every slow event (>= 2 ms) with
//                                                      its wall-clock time, to line up with PresentMon / OSD frame spikes
//   $env:FT_ALLOW_WRITE=1; dotnet fsi stutter_probe_live.fsx revert [ids]  restore the original bytes (cave stays allocated)
//
// Sites (all v2.02 RVAs, verified against re\Nioh3.dump.exe by `offline`):
//   G1 11970A  call 9290D8 CreateGfxPso        in PsoGetOrCreateGfx 119670 (dispatcher, per PSO-dirty draw; the miss
//                                              path creates the PSO synchronously while holding the PSO-cache CS)
//   C1 119AE1  call 58C2B0 CreateComputePso    in PsoGetOrCreateCompute 119A48 (per-context compute cache miss)
//   C2 119D86  call 58C2B0                     in SynFlushComputeStateEx 119C08 (dispatcher compute state flush miss)
//   C3 58BF92  call 58C2B0                     in SynFlushComputeState 58BE90 (dispatcher)
//   C4 58C07A  call 58C2B0                     in PsoGetOrCreateComputeShared 58C000 (shared compute cache miss)
//   PG 2EFE47C call 9290D8                     in PsoPreloadGfxCreate 2EFE440 (PreloadPSO list, boot / stage load)
//   PC 2EFE1F2 call 58C2B0                     in PsoPreloadComputeCreate 2EFE1CC (PreloadPSO list)
//   UF 62D7E7  call 9C2EE0 CreateDedicatedUpload  in UploadHeapSuballoc 62D688 when the upload/readback heap ring is
//                                              full or the request is larger than one heap (CreateCommittedResource)
//   UP 62D784  call [r11+0E8h] CreatePlacedResource in UploadHeapSuballoc (one placed resource per upload; 8 args)
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.IO
open System.Diagnostics
open System.Collections.Generic
open System.Runtime.InteropServices
open System.Text.RegularExpressions
open Iced.Intel
open type Iced.Intel.AssemblerRegisters
open Mem
open Wj_common

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "stutter_probe_state.txt")

type Kind = Rel32 of target: int | VCallE8   // VCallE8 = `call qword [r11+0E8h]` with 4 stack arguments
type Site = { Id: string; Rva: int; Orig: string; Kind: Kind; Note: string }
let SITES =
  [ { Id = "G1"; Rva = 0x11970A;  Orig = "E8 C9 F9 80 00"; Kind = Rel32 0x9290D8; Note = "runtime gfx PSO create (dispatcher, PSO-cache CS held)" }
    { Id = "C1"; Rva = 0x119AE1;  Orig = "E8 CA 27 47 00"; Kind = Rel32 0x58C2B0; Note = "runtime compute PSO create (per-context cache)" }
    { Id = "C2"; Rva = 0x119D86;  Orig = "E8 25 25 47 00"; Kind = Rel32 0x58C2B0; Note = "runtime compute PSO create (compute state flush)" }
    { Id = "C3"; Rva = 0x58BF92;  Orig = "E8 19 03 00 00"; Kind = Rel32 0x58C2B0; Note = "runtime compute PSO create (SynFlushComputeState)" }
    { Id = "C4"; Rva = 0x58C07A;  Orig = "E8 31 02 00 00"; Kind = Rel32 0x58C2B0; Note = "runtime compute PSO create (shared cache)" }
    { Id = "PG"; Rva = 0x2EFE47C; Orig = "E8 57 AC A2 FD"; Kind = Rel32 0x9290D8; Note = "preload gfx PSO create (PreloadPSO list)" }
    { Id = "PC"; Rva = 0x2EFE1F2; Orig = "E8 B9 E0 68 FD"; Kind = Rel32 0x58C2B0; Note = "preload compute PSO create (PreloadPSO list)" }
    { Id = "UF"; Rva = 0x62D7E7;  Orig = "E8 F4 56 39 00"; Kind = Rel32 0x9C2EE0; Note = "upload heap full -> dedicated committed resource" }
    { Id = "UP"; Rva = 0x62D784;  Orig = "41 FF 93 E8 00 00 00"; Kind = VCallE8; Note = "CreatePlacedResource per upload suballocation" } ]
let siteLen (s: Site) = (hexb s.Orig).Length

// ---- data layout of the probe block (one 0x3000-byte RWX allocation near the module) ---------------------------
// +0000 dword enable | +0008 qword ringMinTicks | +0010 4 x qword histogram thresholds (ticks) | +0030 qword ringPos
// +0040 + 0x80*i  per-site block: +00 count, +08 sumTicks, +10 maxTicks, +18..+40 hist[5]
// +1000 ring: 256 x {qword t0, qword dt | siteIndex<<56}
// +2000 code: one cave per site, 0x140 bytes apart
let D_EN, D_RINGMIN, D_TH, D_RINGPOS, D_SITES, D_RING, D_CODE = 0, 8, 0x10, 0x30, 0x40, 0x1000, 0x2000
let SITE_BLK, RING_N, CAVE_STRIDE, BLOCK = 0x80, 256, 0x140, 0x3000

let build (blk: uint64) (B: uint64) (idx: int) (s: Site) =
    let a = Assembler(64)
    let sb = int64 blk + int64 D_SITES + int64 (idx * SITE_BLK)
    let mutable lOff, lNoRing, lH1, lH2, lH3, lH4, lHd, lNoMax = a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel()
    let stackArgs = match s.Kind with VCallE8 -> true | _ -> false
    let frame = if stackArgs then 0x40 else 0x20
    // switch: off -> behave exactly as the original instruction
    a.mov(r10, int64 blk)
    a.cmp(__dword_ptr.[r10 + int64 D_EN], 0)
    a.je(lOff)
    a.push(rbx); a.push(rsi); a.push(rdi)
    a.sub(rsp, frame)
    if stackArgs then
        // caller's stack args 5..8 were at [rsp+28h..40h] on entry; entry rsp = now + frame + 18h
        for k in 0 .. 3 do
            a.mov(rax, __qword_ptr.[rsp + int64 (frame + 0x18 + 0x28 + k * 8)])
            a.mov(__qword_ptr.[rsp + int64 (0x20 + k * 8)], rax)
    a.mov(rsi, rdx)
    a.rdtsc(); a.shl(rdx, 32uy); a.``or``(rax, rdx); a.mov(rbx, rax)
    a.mov(rdx, rsi)
    match s.Kind with
    | Rel32 t -> a.call(B + uint64 t)
    | VCallE8 -> a.call(__qword_ptr.[r11 + 0xE8L])
    a.mov(rsi, rax)                                        // callee result
    a.rdtsc(); a.shl(rdx, 32uy); a.``or``(rax, rdx); a.sub(rax, rbx)   // rax = dt ticks, rbx = t0
    a.mov(r10, sb)
    a.``lock``.inc(__qword_ptr.[r10])
    a.``lock``.add(__qword_ptr.[r10 + 8L], rax)
    a.cmp(rax, __qword_ptr.[r10 + 0x10L]); a.jbe(lNoMax); a.mov(__qword_ptr.[r10 + 0x10L], rax)   // racy max: stats only
    a.Label(&lNoMax)
    a.mov(r11, int64 blk)
    a.cmp(rax, __qword_ptr.[r11 + int64 D_TH]); a.jb(lH1)
    a.cmp(rax, __qword_ptr.[r11 + int64 (D_TH + 8)]); a.jb(lH2)
    a.cmp(rax, __qword_ptr.[r11 + int64 (D_TH + 16)]); a.jb(lH3)
    a.cmp(rax, __qword_ptr.[r11 + int64 (D_TH + 24)]); a.jb(lH4)
    a.``lock``.inc(__qword_ptr.[r10 + 0x38L]); a.jmp(lHd)
    a.Label(&lH4); a.``lock``.inc(__qword_ptr.[r10 + 0x30L]); a.jmp(lHd)
    a.Label(&lH3); a.``lock``.inc(__qword_ptr.[r10 + 0x28L]); a.jmp(lHd)
    a.Label(&lH2); a.``lock``.inc(__qword_ptr.[r10 + 0x20L]); a.jmp(lHd)
    a.Label(&lH1); a.``lock``.inc(__qword_ptr.[r10 + 0x18L])
    a.Label(&lHd)
    a.cmp(rax, __qword_ptr.[r11 + int64 D_RINGMIN]); a.jb(lNoRing)
    a.mov(edi, 1); a.``lock``.xadd(__qword_ptr.[r11 + int64 D_RINGPOS], rdi)
    a.``and``(edi, RING_N - 1); a.shl(rdi, 4uy); a.add(rdi, r11); a.add(rdi, D_RING)
    a.mov(__qword_ptr.[rdi], rbx)
    a.mov(rdx, int64 idx); a.shl(rdx, 56uy); a.``or``(rax, rdx); a.mov(__qword_ptr.[rdi + 8L], rax)
    a.Label(&lNoRing)
    a.mov(rax, rsi)
    a.add(rsp, frame); a.pop(rdi); a.pop(rsi); a.pop(rbx)
    a.ret()
    a.Label(&lOff)
    match s.Kind with
    | Rel32 t -> a.jmp(B + uint64 t)
    | VCallE8 -> a.jmp(__qword_ptr.[r11 + 0xE8L])
    use ms = new MemoryStream()
    let at = blk + uint64 D_CODE + uint64 (idx * CAVE_STRIDE)
    a.Assemble(StreamCodeWriter(ms), at) |> ignore
    let b = ms.ToArray()
    if b.Length > CAVE_STRIDE then failwithf "cave %s too long (%d)" s.Id b.Length
    at, b

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let siteNew (B: uint64) (s: Site) (cave: uint64) =
    let call = Array.append [| 0xE8uy |] (rel32 (B + uint64 s.Rva + 5UL) cave)
    match siteLen s with
    | 5 -> call
    | 7 -> Array.append call (hexb "66 90")
    | n -> failwithf "site length %d" n

// ---- offline ----------------------------------------------------------------------------------------------------
let patchesJsonRvas () =
    let p = @"C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix\patches.json"
    if File.Exists p then
        [ for m in Regex.Matches(File.ReadAllText p, "\"rva\"\\s*:\\s*\"0x([0-9A-Fa-f]+)\"") -> Convert.ToInt32(m.Groups.[1].Value, 16) ]
    else []

let staticChecks () =
    let mutable ok = true
    let others = (OTHER_SITES @ patchesJsonRvas ()) |> List.distinct
    for s in SITES do
        let o = hexb s.Orig
        let cur = img.[s.Rva .. s.Rva + o.Length - 1]
        let ins = decodeAt (uint64 s.Rva)
        let kindOk =
            match s.Kind with
            | Rel32 t -> ins.Mnemonic = Mnemonic.Call && ins.Op0Kind = OpKind.NearBranch64 && ins.NearBranchTarget = uint64 t && ins.Length = 5
            | VCallE8 -> ins.Mnemonic = Mnemonic.Call && ins.Op0Kind = OpKind.Memory && ins.MemoryBase = Register.R11 && ins.MemoryDisplacement64 = 0xE8UL && ins.Length = 7
        let ov = others |> List.filter (fun r -> r < s.Rva + o.Length && s.Rva < r + 10)
        let fnOk = (match primaryEntry (uint32 s.Rva) with Some f -> sprintf "fn %X" f | None -> "NO FUNCTION")
        printfn "%-2s %8X %-22s %-34s %s %s%s" s.Id s.Rva (hexs cur) (fmtIns ins) fnOk
            (if cur = o && kindOk then "[ok]" else "[FAIL]") (if ov.IsEmpty then "" else sprintf "  !! overlaps %A" ov)
        printfn "      %s" s.Note
        ok <- ok && cur = o && kindOk && ov.IsEmpty
    let hits = scanTargets [ for s in SITES -> s.Id, uint64 s.Rva, uint64 s.Rva + uint64 (siteLen s) ]
    printfn "direct branch/call targets strictly inside the sites (whole executable sweep): %s" (if hits.Count = 0 then "none" else String.Join("; ", hits))
    ok && hits.Count = 0

[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<DllImport("kernel32.dll")>] extern bool VirtualFree(nativeint addr, unativeint size, uint32 typ)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type HarnessFn = delegate of nativeint -> unit

/// Native transparency test. Harness H: loads random values into all GPRs from io[], pushes 4 random stack args,
/// calls the cave (as the patched site would), then stores all GPRs + rsp delta into io[]. Fake callee F: records
/// rcx/rdx/r8/r9 and its 4 stack args, clobbers every volatile register, returns a marker in rax.
let transparency () =
    let mem = VirtualAlloc(0n, unativeint (BLOCK + 0x2000), 0x3000u, 0x40u)
    let blk = uint64 mem
    let fAddr = blk + uint64 BLOCK
    let hAddr = fAddr + 0x800UL
    let io = Marshal.AllocHGlobal 0x400
    let rec_ = Marshal.AllocHGlobal 0x100
    // fake callee
    let fcode = assemble (nativeint fAddr) (fun a ->
        a.mov(rax, int64 rec_)
        a.mov(__qword_ptr.[rax], rcx); a.mov(__qword_ptr.[rax + 8L], rdx); a.mov(__qword_ptr.[rax + 16L], r8); a.mov(__qword_ptr.[rax + 24L], r9)
        for k in 0 .. 3 do
            a.mov(r10, __qword_ptr.[rsp + int64 (0x28 + k * 8)]); a.mov(__qword_ptr.[rax + int64 (32 + k * 8)], r10)
        a.mov(r10, rsp); a.mov(__qword_ptr.[rax + 64L], r10)
        a.mov(rcx, 0x1111L); a.mov(rdx, 0x2222L); a.mov(r8, 0x3333L); a.mov(r9, 0x4444L); a.mov(r10, 0x5555L); a.mov(r11, 0x6666L)
        a.mov(rax, 0x0BADC0DE12345678L)
        a.ret())
    Marshal.Copy(fcode, 0, nativeint fAddr, fcode.Length)
    // fake vtable for VCallE8: [vt + E8h] = F
    let vt = Marshal.AllocHGlobal 0x200
    Marshal.WriteInt64(vt + 0xE8n, int64 fAddr)
    let gprs = [| Register.RBX; Register.RBP; Register.RSI; Register.RDI; Register.R12; Register.R13; Register.R14; Register.R15
                  Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R11 |]
    let mutable bad, runs = 0, 0
    let rnd = Random(4242)
    // thresholds: 0, so every call lands in hist[4] and the ring
    let initBlock en =
        Marshal.Copy(Array.zeroCreate<byte> 0x2000, 0, mem, 0x2000)
        Marshal.WriteInt32(mem, en)
        Marshal.WriteInt64(mem + nativeint D_RINGMIN, 0L)
        for k in 0 .. 3 do Marshal.WriteInt64(mem + nativeint (D_TH + 8 * k), 0L)
    for (idx, kind) in [ 0, Rel32 0; 1, VCallE8 ] do
        for en in [ 1; 0 ] do
            initBlock en
            // a cave whose Rel32 target is F: express the target as an RVA from a fake base so B + t = F
            let s = { Id = "T"; Rva = 0; Orig = ""; Kind = (match kind with Rel32 _ -> Rel32 0x100 | k -> k); Note = "" }
            let fakeB = fAddr - 0x100UL
            let caveAt, cb = build blk fakeB idx s
            Marshal.Copy(cb, 0, nativeint caveAt, cb.Length)
            let hcode = assemble (nativeint hAddr) (fun a ->
                a.push(rbx); a.push(rbp); a.push(rsi); a.push(rdi); a.push(r12); a.push(r13); a.push(r14); a.push(r15)
                a.push(rcx)                                    // io; entry rsp = 8 mod 16, 9 pushes -> 0 mod 16
                a.mov(rax, rcx)
                // stack args: reserve 0x50 (shadow 0x20 + 4 args + pad): rsp = 0 mod 16 at the call
                a.sub(rsp, 0x50)
                for k in 0 .. 3 do
                    a.mov(r10, __qword_ptr.[rax + int64 (0x100 + k * 8)]); a.mov(__qword_ptr.[rsp + int64 (0x20 + k * 8)], r10)
                a.mov(r10, rsp); a.mov(__qword_ptr.[rax + 0x180L], r10)
                for i in 0 .. gprs.Length - 1 do
                    if gprs.[i] <> Register.R11 then a.mov(AssemblerRegister64(gprs.[i]), __qword_ptr.[rax + int64 (i * 8)])
                a.mov(r11, int64 vt)
                a.call(caveAt)
                a.mov(r10, __qword_ptr.[rsp + 0x50L])           // io
                a.mov(__qword_ptr.[r10 + 0x200L], rax)
                for i in 0 .. 7 do a.mov(__qword_ptr.[r10 + int64 (0x208 + i * 8)], AssemblerRegister64(gprs.[i]))
                a.mov(r11, rsp); a.mov(__qword_ptr.[r10 + 0x250L], r11)
                for k in 0 .. 3 do
                    a.mov(r11, __qword_ptr.[rsp + int64 (0x20 + k * 8)]); a.mov(__qword_ptr.[r10 + int64 (0x258 + k * 8)], r11)
                a.add(rsp, 0x50)
                a.pop(rcx)
                a.pop(r15); a.pop(r14); a.pop(r13); a.pop(r12); a.pop(rdi); a.pop(rsi); a.pop(rbp); a.pop(rbx)
                a.ret())
            Marshal.Copy(hcode, 0, nativeint hAddr, hcode.Length)
            let h = Marshal.GetDelegateForFunctionPointer<HarnessFn>(nativeint hAddr)
            let N = 2000
            for _ in 1 .. N do
                let vals = Array.init gprs.Length (fun _ -> rnd.NextInt64())
                let sargs = Array.init 4 (fun _ -> rnd.NextInt64())
                vals |> Array.iteri (fun i v -> Marshal.WriteInt64(io + nativeint (i * 8), v))
                sargs |> Array.iteri (fun k v -> Marshal.WriteInt64(io + nativeint (0x100 + k * 8), v))
                h.Invoke(io)
                runs <- runs + 1
                let rd (o: int) = Marshal.ReadInt64(io + nativeint o)
                let rr (o: int) = Marshal.ReadInt64(rec_ + nativeint o)
                let fail (m: string) = (bad <- bad + 1; if bad < 6 then printfn "  !! %s (kind %A en %d)" m kind en)
                // callee saw the caller's register args
                if rr 0 <> vals.[8] || rr 8 <> vals.[9] || rr 16 <> vals.[10] || rr 24 <> vals.[11] then fail "register args differ at the callee"
                // stack args (only meaningful for VCallE8; the Rel32 targets take 2-4 register args)
                (match kind with
                 | VCallE8 -> for k in 0 .. 3 do if rr (32 + k * 8) <> sargs.[k] then fail (sprintf "stack arg %d differs at the callee" (k + 5))
                 | _ -> ())
                // callee entered with rsp = 8 mod 16
                if (rr 64 &&& 0xFL) <> 8L then fail "callee entered with a misaligned stack"
                if rd 0x200 <> 0x0BADC0DE12345678L then fail "return value lost"
                for i in 0 .. 7 do if rd (0x208 + i * 8) <> vals.[i] then fail (sprintf "%O not preserved" gprs.[i])
                if rd 0x250 <> rd 0x180 then fail "rsp changed across the call"
                for k in 0 .. 3 do if rd (0x258 + k * 8) <> sargs.[k] then fail "caller's stack args were modified"
            let sb = mem + nativeint (D_SITES + idx * SITE_BLK)
            let cnt = Marshal.ReadInt64 sb
            let h4 = Marshal.ReadInt64(sb + 0x38n)
            let rp = Marshal.ReadInt64(mem + nativeint D_RINGPOS)
            let expect = if en = 1 then int64 N else 0L
            if cnt <> expect || h4 <> expect || rp <> expect then (bad <- bad + 1; printfn "  !! counters: count %d hist4 %d ring %d, expected %d" cnt h4 rp expect)
            printfn "  cave kind %-9s switch %d: %d calls, count %d, ring %d" (match kind with Rel32 _ -> "rel32" | _ -> "vcall+E8") en N cnt rp
    Marshal.FreeHGlobal io; Marshal.FreeHGlobal rec_; Marshal.FreeHGlobal vt
    VirtualFree(mem, 0un, 0x8000u) |> ignore
    printfn "  transparency: %d harness runs, %d differences" runs bad
    bad = 0

let offline () =
    let mutable ok = staticChecks ()
    let B = DUMP_BASE
    let blk = B + 0x5E30000UL
    for i, s in List.indexed SITES do
        let at, b = build blk B i s
        if i = 0 || i = SITES.Length - 1 then
            printfn "cave %s @dummy %X (%d bytes):" s.Id at b.Length
            printDis b at B
        printfn "  %s site %X: %s -> %s" s.Id s.Rva s.Orig (hexs (siteNew B s at))
    ok <- transparency () && ok
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live -------------------------------------------------------------------------------------------------------
type Tsc() =
    static member val PerMs = 0.0 with get, set
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type RdtscFn = delegate of unit -> int64
let rdtscFn =
    lazy (
        let m = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
        let c = assemble m (fun a -> a.rdtsc(); a.shl(rdx, 32uy); a.``or``(rax, rdx); a.ret())
        Marshal.Copy(c, 0, m, c.Length)
        Marshal.GetDelegateForFunctionPointer<RdtscFn>(m))
let calibrate () =
    let f = rdtscFn.Force()
    let sw = Stopwatch.StartNew()
    let t0 = f.Invoke()
    Threading.Thread.Sleep 250
    let t1 = f.Invoke()
    Tsc.PerMs <- float (t1 - t0) / sw.Elapsed.TotalMilliseconds
    Tsc.PerMs

let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let selected = if args.Length > 1 && mode <> "watch" then args.[1].ToUpperInvariant().Split(',') |> Set.ofArray else SITES |> List.map (fun s -> s.Id) |> Set.ofList

let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let classify (blk: uint64 option) (i: int) (s: Site) =
        let cur = t.Read(nativeint (B + uint64 s.Rva), siteLen s)
        if cur = hexb s.Orig then "original"
        else
            match blk with
            | Some b when cur = siteNew B s (fst (build b B i s)) -> "patched"
            | _ -> "UNKNOWN"
    let stats (b: uint64) =
        let raw = t.Read(nativeint b, 0x1000)
        let q (o: int) = BitConverter.ToInt64(raw, o)
        [ for i, s in List.indexed SITES ->
            let o = D_SITES + i * SITE_BLK
            s, q o, q (o + 8), q (o + 0x10), [ for k in 0 .. 4 -> q (o + 0x18 + k * 8) ] ]
    let printStats (b: uint64) (prev: Map<string, int64 * int64> option) =
        let perMs = if Tsc.PerMs > 0.0 then Tsc.PerMs else calibrate ()
        printfn "%-2s %-52s %9s %10s %9s  hist <0.5/<2/<8/<32/>=32 ms" "id" "site" "count" "total ms" "max ms"
        for (s, c, sum, mx, h) in stats b do
            let dc, ds = match prev with Some m when m.ContainsKey s.Id -> (let pc, ps = m.[s.Id] in c - pc, sum - ps) | _ -> c, sum
            if prev.IsNone || dc > 0L then
                printfn "%-2s %-52s %9d %10.1f %9.2f  %s" s.Id s.Note dc (float ds / perMs) (float mx / perMs) (h |> List.map string |> String.concat "/")
        stats b |> List.map (fun (s, c, sum, _, _) -> s.Id, (c, sum)) |> Map.ofList
    match mode with
    | "dry" | "status" ->
        let blk = readState ()
        for i, s in List.indexed SITES do printfn "  %-2s %8X %s" s.Id s.Rva (classify blk i s)
        match blk with
        | Some b -> printfn "probe block %X switch=%d" b (BitConverter.ToInt32(t.Read(nativeint b, 4), 0))
        | None -> printfn "no probe block in this process"
    | "apply" ->
        writeGuard ()
        let b =
            match readState () with
            | Some b -> b
            | None ->
                let mem = t.AllocNear(nativeint (B + 0x62D784UL), BLOCK)
                let b = uint64 mem
                let perMs = calibrate ()
                let hdr = Array.zeroCreate<byte> 0x40
                BitConverter.GetBytes(1).CopyTo(hdr, D_EN)
                BitConverter.GetBytes(int64 (2.0 * perMs)).CopyTo(hdr, D_RINGMIN)
                [ 0.5; 2.0; 8.0; 32.0 ] |> List.iteri (fun k ms -> BitConverter.GetBytes(int64 (ms * perMs)).CopyTo(hdr, D_TH + 8 * k))
                t.Write(mem, hdr)
                for i, s in List.indexed SITES do
                    let at, code = build b B i s
                    t.Write(nativeint at, code)
                File.WriteAllText(stateFile, sprintf "%X %X" b t.Process.Id)
                printfn "probe block %X (TSC %.0f ticks/ms)" b perMs
                b
        let ws =
            [ for i, s in List.indexed SITES do
                if selected.Contains s.Id then
                    match classify (Some b) i s with
                    | "original" -> yield nativeint (B + uint64 s.Rva), siteNew B s (fst (build b B i s)), hexb s.Orig
                    | "patched" -> ()
                    | _ -> failwithf "%s: unexpected bytes - aborting" s.Id ]
        let n = if ws.IsEmpty then 0 else safeWrite t ws
        printfn "SPR applied %d site(s) (attempt %d)" ws.Length n
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some b -> (t.Write(nativeint b, BitConverter.GetBytes(if mode = "on" then 1 else 0)); printfn "SPR switch %s" mode)
        | None -> printfn "SPR is not applied in this process"
    | "stats" ->
        match readState () with
        | Some b -> printStats b None |> ignore
        | None -> printfn "SPR is not applied in this process"
    | "watch" ->
        match readState () with
        | None -> printfn "SPR is not applied in this process"
        | Some b ->
            let sec = if args.Length > 1 then float args.[1] else 1.0
            let perMs = calibrate ()
            let f = rdtscFn.Force()
            let mutable prev = printStats b None
            let mutable ringSeen = BitConverter.ToInt64(t.Read(nativeint (b + uint64 D_RINGPOS), 8), 0)
            while true do
                Threading.Thread.Sleep(int (sec * 1000.0))
                let nowTsc, nowWall = f.Invoke(), DateTime.Now
                printfn "---- %s" (nowWall.ToString "HH:mm:ss.fff")
                prev <- printStats b (Some prev)
                let rp = BitConverter.ToInt64(t.Read(nativeint (b + uint64 D_RINGPOS), 8), 0)
                let ring = t.Read(nativeint (b + uint64 D_RING), RING_N * 16)
                for k in max ringSeen (rp - int64 RING_N) .. rp - 1L do
                    let o = int (k % int64 RING_N) * 16
                    let t0 = BitConverter.ToInt64(ring, o)
                    let w = BitConverter.ToInt64(ring, o + 8)
                    let si = int (w >>> 56)
                    let dt = w &&& 0x00FFFFFFFFFFFFFFL
                    let at = nowWall.AddMilliseconds(-(float (nowTsc - t0) / perMs))
                    if si < SITES.Length then printfn "   slow %s %7.2f ms at %s" SITES.[si].Id (float dt / perMs) (at.ToString "HH:mm:ss.fff")
                ringSeen <- rp
    | "revert" ->
        writeGuard ()
        match readState () with
        | None -> printfn "no probe block in this process (nothing to revert)"
        | Some b ->
            let ws =
                [ for i, s in List.indexed SITES do
                    if selected.Contains s.Id then
                        match classify (Some b) i s with
                        | "patched" -> yield nativeint (B + uint64 s.Rva), hexb s.Orig, siteNew B s (fst (build b B i s))
                        | "original" -> ()
                        | _ -> failwithf "%s: unexpected bytes - aborting" s.Id ]
            let n = if ws.IsEmpty then 0 else safeWrite t ws
            printfn "SPR reverted %d site(s) (attempt %d); probe block left allocated" ws.Length n
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
