// stageprefetch_live.fsx - SPF: software prefetch for the StageUpdate placeable loop (notes\worker-jobs-opt.md).
//
//   dotnet fsi stageprefetch_live.fsx offline         NO GAME ACCESS: site bytes vs dump, branch-target sweep, dead-register
//                                                     proof at the site, cave discipline (no stores, only rax/rsi/flags
//                                                     written) and a native transparency test of the cave (the dump image
//                                                     is mapped into this process; 30,000 random entries/placeables/
//                                                     registers: every other GPR identical, rax = [g_CollisionWorld] as
//                                                     the original instruction, no memory changed), 
//   dotnet fsi stageprefetch_live.fsx dry|status      READ-ONLY on the game.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi stageprefetch_live.fsx apply|on|off|revert
//
// What it does. KIDSTS[s2:StageUpdate] runs PlaceableVisibilityUpdate -> DA830, whose second loop (DB8B6..DB9BC) walks
// the stage's flat {key, placeable*} map [r15+180h] (count [r15+188h], 16-byte entries, rbx = current, r14 = end) and
// for EVERY placeable reads +10h, +70h..+98h (DBA70 distance LOD: two sqrt), +B0h, +B8h (DA6E0 flags), +C8h,
// +109h..+10Dh. The first touch of each placeable is a cache miss: hot RIP DB928 = 88-95 % of DA830 self, DBA89/DBABD
// in DBA70, DA71F in DA6E0 (PC r6/r7), and on the Ally DA879 / DBA70 / DBBE1 are the largest StageUpdate items.
// Patch: DB8BF `mov rax,[rel g_CollisionWorld]` (7 B, RIP-relative) -> `jmp cave; nop2`. The cave prefetches the
// placeable D=4 entries ahead (+10h, +70h, +98h, +C8h, +109h) and, D/2=2 entries ahead (whose lines are already on
// the way), the object behind its +B0h pointer (read by DBB90); then executes the original load of g_CollisionWorld
// (re-encoded RIP-relative from the cave) and jumps to DB8C6.
// Equivalence: the cave writes no memory and only rax, rsi and the flags. rsi is written at DB8C6 and the flags at DB8CA
// before any read on every path (walker proof), and rax gets exactly the original value. Loads: map entries strictly
// below r14 (the loop's own end) and fields inside placeables the loop visits later; a prefetch never faults.
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.IO
open System.Diagnostics
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel
open type Iced.Intel.AssemblerRegisters
open Mem
open Wj_common

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "stageprefetch_state.txt")

let SITE, SITE_ORIG = 0xDB8BF, "48 8B 05 92 E5 4D 04"       // mov rax,[rel 45B9E58h]
let G_WORLD = 0x45B9E58UL
let RESUME = 0xDB8C6UL                                      // mov rsi,[rbx+8]
let D = 4                                                   // entries ahead (placeable lines)
let D2 = 2                                                  // entries ahead (+B0h target)

type Cave = { Bytes: byte[]; Code: uint64; En: uint64 }
let build (at: uint64) (B: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, code, stock, s1 = L(), L(), L(), L()
    a.Label(&lEn); a.dd(1u)
    for _ in 1 .. 15 do a.dd(0u)
    a.Label(&code)
    a.cmp(__dword_ptr.[lEn], 0); a.je(stock)
    a.lea(rsi, __[rbx + int64 (D * 16)]); a.cmp(rsi, r14); a.jae(s1)
    a.mov(rsi, __qword_ptr.[rsi + 8L]); a.test(rsi, rsi); a.je(s1)
    for o in [ 0x10L; 0x70L; 0x98L; 0xC8L; 0x109L ] do a.prefetcht0(__byte_ptr.[rsi + o])
    a.Label(&s1)
    a.lea(rsi, __[rbx + int64 (D2 * 16)]); a.cmp(rsi, r14); a.jae(stock)
    a.mov(rsi, __qword_ptr.[rsi + 8L]); a.test(rsi, rsi); a.je(stock)
    a.mov(rsi, __qword_ptr.[rsi + 0xB0L]); a.prefetcht0(__byte_ptr.[rsi])
    a.Label(&stock)
    a.mov(rax, __qword_ptr.[B + G_WORLD])                 // the original instruction (RIP-relative from here)
    a.jmp(B + RESUME)
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); Code = addr code; En = addr lEn }
    if c.Code - at <> 0x40UL then failwith "cave layout"
    c
let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let siteNew (B: uint64) (c: Cave) = Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 SITE + 5UL) c.Code; hexb "66 90" ]

[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<DllImport("kernel32.dll")>] extern bool VirtualFree(nativeint addr, unativeint size, uint32 typ)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type ThunkFn = delegate of nativeint * nativeint -> unit
let allocNear (near: nativeint) (size: int) =
    let mutable r = 0n
    let mutable probe = (int64 near &&& ~~~0xFFFFL) - 0x10000000L
    while r = 0n && probe > int64 near - 0x7FF00000L && probe > 0x10000L do
        r <- VirtualAlloc(nativeint probe, unativeint size, 0x3000u, 0x40u)
        probe <- probe - 0x10000L
    probe <- (int64 near &&& ~~~0xFFFFL) + 0x10000000L                 // then above
    while r = 0n && probe < int64 near + 0x7FF00000L do
        r <- VirtualAlloc(nativeint probe, unativeint size, 0x3000u, 0x40u)
        probe <- probe + 0x10000L
    if r = 0n then failwith "allocNear failed"
    r

let staticChecks () =
    let mutable ok = true
    let o = hexb SITE_ORIG
    let cur = img.[SITE .. SITE + o.Length - 1]
    printfn "site %X: %s  %s" SITE (hexs cur) (if cur = o then "MATCH" else "DIFFERENT")
    ok <- ok && cur = o
    let i0 = decodeAt (uint64 SITE)
    printfn "  = %s (len %d, RIP-relative target %X); next %X = %s" (fmtIns i0) i0.Length i0.IPRelativeMemoryAddress RESUME (fmtIns (decodeAt RESUME))
    if i0.Length <> 7 || i0.IPRelativeMemoryAddress <> G_WORLD then (ok <- false; printfn "  !! unexpected instruction")
    for s in OTHER_SITES do
        if s < SITE + 7 && SITE < s + 10 then (printfn "  !! overlaps patch site %X" s; ok <- false)
    printfn "dead-register proof at %X: rsi and the flags (the cave's scratch) are written before any read; rax is the original's result:" SITE
    let f = walk [ RESUME ] [ Register.RSI ] true false 0
    if f.IsEmpty then printfn "  [ok] rsi + all status flags dead at %X (rsi written at %X, flags at DB8CA)" RESUME RESUME
    else (ok <- false; for x in f do printfn "  [FAIL] %s" x)
    let hits = scanTargets [ "SPF", uint64 SITE, uint64 SITE + 7UL ]
    printfn "direct branch/call targets strictly inside %X..%X: %s" SITE (SITE + 7) (if hits.Count = 0 then "none" else String.Join("; ", hits))
    ok <- ok && hits.Count = 0
    let B = DUMP_BASE
    let c = build (B + 0x5E20000UL) B
    printfn "cave @dummy %X: %d bytes" (B + 0x5E20000UL) c.Bytes.Length
    printDis c.Bytes.[0x40 ..] c.Code B
    let nb = siteNew B c
    printfn "patched site: %s -> %s" SITE_ORIG (hexs nb)
    let ins = decodeAll c.Bytes.[0x40 ..] c.Code
    let mutable stores = 0
    let written = HashSet<Register>()
    for x in ins do
        let info = infoFactory.GetInfo(&x)
        for m in info.GetUsedMemory() do
            if m.Access = OpAccess.Write || m.Access = OpAccess.ReadWrite then stores <- stores + 1
        for u in info.GetUsedRegisters() do
            if u.Access = OpAccess.Write || u.Access = OpAccess.ReadWrite then written.Add(u.Register.GetFullRegister()) |> ignore
    printfn "cave discipline: memory writes = %d; registers written = %A" stores (written |> Seq.map string |> List.ofSeq)
    if stores <> 0 || written |> Seq.exists (fun r -> r <> Register.RAX && r <> Register.RSI) then (ok <- false; printfn "  !! unexpected writes")
    ok

/// Native transparency test: map the dump image (so B + 45B9E58 and B + DB8C6 exist), put an exit stub at the copy's
/// DB8C6, run the cave with random registers over random placeable maps; compare.
let transparency () =
    let R = VirtualAlloc(0n, unativeint img.Length, 0x3000u, 0x40u)
    Marshal.Copy(img, 0, R, img.Length)
    let B = uint64 R
    let caveMem = allocNear (R + nativeint SITE) 0x1000
    let c = build (uint64 caveMem) B
    Marshal.Copy(c.Bytes, 0, caveMem, c.Bytes.Length)
    let regs = [| Register.RBX; Register.RBP; Register.RSI; Register.RDI; Register.R8; Register.R9; Register.R10; Register.R11; Register.R12; Register.R13; Register.R14; Register.R15; Register.RDX; Register.RCX |]
    let T = allocNear (R + nativeint SITE) 0x1000
    let entry = assemble T (fun a ->
        a.push(rbx); a.push(rbp); a.push(rsi); a.push(rdi); a.push(r12); a.push(r13); a.push(r14); a.push(r15)
        a.sub(rsp, 0x28)
        a.mov(__qword_ptr.[rsp + 0x20L], rdx)
        a.mov(rax, rcx)
        for i in 0 .. regs.Length - 1 do a.mov(AssemblerRegister64(regs.[i]), __qword_ptr.[rax + int64 (i * 8)])
        a.jmp(c.Code))
    Marshal.Copy(entry, 0, T, entry.Length)
    // exit stub written over the copy's DB8C6 (the cave's jump target)
    let exitc = assemble (R + nativeint RESUME) (fun a ->
        a.push(rax)
        a.mov(rax, __qword_ptr.[rsp + 0x28L])
        for i in 0 .. regs.Length - 1 do a.mov(__qword_ptr.[rax + int64 (i * 8)], AssemblerRegister64(regs.[i]))
        a.pop(rcx)
        a.mov(__qword_ptr.[rax + int64 (regs.Length * 8)], rcx)
        a.add(rsp, 0x28)
        a.pop(r15); a.pop(r14); a.pop(r13); a.pop(r12); a.pop(rdi); a.pop(rsi); a.pop(rbp); a.pop(rbx)
        a.ret())
    Marshal.Copy(exitc, 0, R + nativeint RESUME, exitc.Length)
    let f = Marshal.GetDelegateForFunctionPointer<ThunkFn>(T)
    let rnd = Random(77)
    let inp, outp = Marshal.AllocHGlobal 0x100, Marshal.AllocHGlobal 0x100
    let arena = VirtualAlloc(0n, 0x400000un, 0x3000u, 0x04u)
    let mutable bad, calls = 0, 0
    for world in 1 .. 300 do
        // random map: n entries {key, placeable*}, placeables in the arena, +B0h pointers random/valid/null
        let n = rnd.Next(0, 40)
        let ent = arena
        let objs = arena + 0x10000n
        for i in 0 .. n - 1 do
            Marshal.WriteInt64(ent + nativeint (i * 16), rnd.NextInt64())
            let p = if rnd.Next 15 = 0 then 0n else objs + nativeint (rnd.Next(0, 4000) * 0x100)
            Marshal.WriteInt64(ent + nativeint (i * 16 + 8), int64 p)
            if p <> 0n then Marshal.WriteInt64(p + 0xB0n, (if rnd.Next 3 = 0 then 0L else rnd.NextInt64()))
        let gval = rnd.NextInt64()
        Marshal.WriteInt64(R + nativeint G_WORLD, gval)
        let snap = Array.zeroCreate<byte> 0x400000
        Marshal.Copy(arena, snap, 0, snap.Length)
        for j in 1 .. 100 do
            let vals = regs |> Array.map (fun _ -> rnd.NextInt64())
            let i = if n = 0 then 0 else rnd.Next(0, n)
            vals.[0] <- int64 (ent + nativeint (i * 16))                    // rbx = current entry
            vals.[10] <- int64 (ent + nativeint (n * 16))                   // r14 = end
            vals |> Array.iteri (fun k v -> Marshal.WriteInt64(inp + nativeint (k * 8), v))
            Marshal.WriteInt32(nativeint c.En, if j % 10 = 0 then 0 else 1)
            f.Invoke(inp, outp)
            calls <- calls + 1
            for k in 0 .. regs.Length - 1 do
                if regs.[k] <> Register.RSI && Marshal.ReadInt64(outp + nativeint (k * 8)) <> vals.[k] then (bad <- bad + 1; if bad < 5 then printfn "  !! %O changed" regs.[k])
            if Marshal.ReadInt64(outp + nativeint (regs.Length * 8)) <> gval then (bad <- bad + 1; if bad < 5 then printfn "  !! rax != [g_CollisionWorld]")
        let now = Array.zeroCreate<byte> 0x400000
        Marshal.Copy(arena, now, 0, now.Length)
        if not (Linq.Enumerable.SequenceEqual(now, snap)) then (bad <- bad + 1; printfn "  !! memory changed")
    printfn "  transparency: %d cave executions over 300 random maps (0..39 entries, null placeables/pointers, 10 %% switch off): %d differences (13 GPRs + rax value + 4 MB of memory)" calls bad
    VirtualFree(R, 0un, 0x8000u) |> ignore
    bad = 0

let offline () =
    let mutable ok = staticChecks ()
    ok <- transparency () && ok
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let caveOrProbe () = match readState () with Some a -> build a B | None -> build (B + 0x5E20000UL) B
    let classify (c: Cave) =
        let cur = t.Read(nativeint (B + uint64 SITE), 7)
        (if cur = hexb SITE_ORIG then "original" elif cur = siteNew B c then "patched" else "UNKNOWN"), cur
    match mode with
    | "dry" | "status" ->
        let c = caveOrProbe ()
        let s, cur = classify c
        printfn "base=%X site DB8BF: %s (%s)" B s (hexs cur)
        match readState () with
        | Some a -> printfn "cave %X switch=%d" a (BitConverter.ToInt32(t.Read(nativeint c.En, 4), 0))
        | None -> printfn "no cave in this process"
    | "apply" ->
        writeGuard ()
        let probe = caveOrProbe ()
        if fst (classify probe) <> "original" then failwith "site not original - revert first"
        let c =
            match readState () with
            | Some a -> build a B
            | None ->
                let mem = t.AllocNear(nativeint (B + uint64 SITE), 0x1000)
                let c = build (uint64 mem) B
                t.Write(mem, c.Bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" (uint64 mem) t.Process.Id)
                c
        let n = safeWrite t [ nativeint (B + uint64 SITE), siteNew B c, hexb SITE_ORIG ]
        printfn "SPF applied (attempt %d); cave %X" n c.En
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some a -> (let c = build a B in t.Write(nativeint c.En, BitConverter.GetBytes(if mode = "on" then 1u else 0u)); printfn "SPF switch %s" mode)
        | None -> printfn "SPF is not applied in this process - use apply"
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        match classify c with
        | "patched", _ -> (safeWrite t [ nativeint (B + uint64 SITE), hexb SITE_ORIG, siteNew B c ] |> ignore; printfn "SPF reverted (cave left allocated)")
        | "original", _ -> printfn "already original"
        | _ -> failwith "unexpected bytes at the site - aborting"
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
