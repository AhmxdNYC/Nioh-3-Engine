// collbook_live.fsx - CB: two same-result bookkeeping fixes in the collision worker loop CollisionObjectUpdate D80B0
// (notes\collision-opt.md). Default OFF: `apply` installs the caves with both switches off.
//
//   dotnet fsi --optimize- collbook_live.fsx offline [quick]   NO GAME ACCESS. Site bytes, branch-target sweep, dead-register/flag
//                                                  proofs, D8740 purity, cave discipline, then the REAL-CODE harness
//                                                  of collprefetch_live.fsx (dump image mapped, D80B0 + real callees on
//                                                  synthetic worlds): stock vs CB on / CB switched off / CB + CP, single
//                                                  thread and 5 concurrent jobs; every world byte + every record equal.
//   dotnet fsi --optimize- collbook_live.fsx dry|status        READ-ONLY on the game.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi --optimize- collbook_live.fsx apply          caves + 3 site writes (safeWrite), switches OFF
//   $env:FT_ALLOW_WRITE=1; dotnet fsi --optimize- collbook_live.fsx on|off [dl|cs] switches (default: both)
//   $env:FT_ALLOW_WRITE=1; dotnet fsi --optimize- collbook_live.fsx revert         original bytes back (cave stays allocated)
//
// DL (dead lookup). Per part, D80B0 calls CollisionPartMapFind D8740 on the manager's part map (D83F4). When the collider
// takes the global-owner path (sil != 0 and r11 = global owner != 0) the code at D840F..D8427 either calls D8740 again
// on the owner's map and overwrites the result (rdi, [rbp-51h]) or skips the part (r13 = [r11+60h] == 0 -> D8662,
// where rdi/[rbp-51h]/r8/r9/r10 are dead). So the first call is dead on that path. D8740 is pure (no stores; reads rcx,
// rdx and memory; writes rax, rcx, r8, r9, r10, flags). Site D83F4 `call D8740` -> `call caveDL` (same length, one
// instruction): if (en && sil && r11) { pop the return address; jmp D8400 } else jmp D8740 (tail call, returns to D83F9).
// D8400..D840D is the stock code (rax = 0, r13 = 0, re-tests sil/r11 -> D840F), so from D840F the state equals stock
// except the dead registers. PC profile: the second call (@D8424) is 17 % of D8740 time.
//
// CS (conditional stores). At the end of each collider D80B0 stores owner[+110h] = owner[+10Fh] (D867B) and
// mapobj[+126h] = 0 (D86A4) for every collider of that owner, from all 5 concurrent jobs. In steady state the value is
// already there; the unconditional store dirties a line that other jobs read (+10Fh, +106h are in the same line), so
// it ping-pongs between cores. CS: `cmp [x],v; je skip; mov [x],v`. Any execution of CS is an execution stock allows
// (the skipped store would have rewritten the value memory held at that moment). Flags are dead at D8682 / D86AB.
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\collprefetch_live.fsx"
open System
open System.IO
open System.Threading
open System.Diagnostics
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel
open type Iced.Intel.AssemblerRegisters
open Mem
open Wj_common
open Collprefetch_live

let cbState = Path.Combine(__SOURCE_DIRECTORY__, "collbook_state.txt")
// (rva, original bytes, length, resume)
let S_DL, O_DL = 0xD83F4, "E8 47 03 00 00"                 // call D8740 (returns to D83F9)
let S_C1, O_C1 = 0xD867B, "41 88 80 10 01 00 00"           // mov [r8+110h],al
let S_C2, O_C2 = 0xD86A4, "45 88 B9 26 01 00 00"           // mov [r9+126h],r15b

type CbCave = { Bytes: byte[]; DL: uint64; C1: uint64; C2: uint64; EnDL: uint64; EnCS: uint64 }
/// data: enDL dword @+0, enCS dword @+4; code @+40h
let buildCB (at: uint64) (B: uint64) (enDL: uint32) (enCS: uint32) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lDL, lCS, eDL, eC1, eC2, toFn, st1, sk1, st2, sk2 = L(), L(), L(), L(), L(), L(), L(), L(), L(), L()
    a.Label(&lDL); a.dd(enDL)
    a.Label(&lCS); a.dd(enCS)
    for _ in 1 .. 14 do a.dd(0u)
    // DL: entered by `call` from D83F4 (rcx = mgr part map, rdx = part, [rsp] = D83F9)
    a.Label(&eDL)
    a.cmp(__dword_ptr.[lDL], 0); a.je(toFn)
    a.test(sil, sil); a.je(toFn)
    a.test(r11, r11); a.je(toFn)
    a.lea(rsp, __[rsp + 8L])                                // drop the return address
    a.jmp(B + 0xD8400UL)                                    // stock: rax = 0, r13 = 0, sil/r11 tests -> D840F
    a.Label(&toFn)
    a.jmp(B + 0xD8740UL)                                    // the original call (tail jump; returns to D83F9)
    // CS1: from D867B
    a.Label(&eC1)
    a.cmp(__dword_ptr.[lCS], 0); a.je(st1)
    a.cmp(__byte_ptr.[r8 + 0x110L], al); a.je(sk1)
    a.Label(&st1)
    a.mov(__byte_ptr.[r8 + 0x110L], al)
    a.Label(&sk1)
    a.jmp(B + 0xD8682UL)
    // CS2: from D86A4
    a.Label(&eC2)
    a.cmp(__dword_ptr.[lCS], 0); a.je(st2)
    a.cmp(__byte_ptr.[r9 + 0x126L], r15b); a.je(sk2)
    a.Label(&st2)
    a.mov(__byte_ptr.[r9 + 0x126L], r15b)
    a.Label(&sk2)
    a.jmp(B + 0xD86ABUL)
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); DL = addr eDL; C1 = addr eC1; C2 = addr eC2; EnDL = addr lDL; EnCS = addr lCS }
    if c.DL - at <> 0x40UL || c.EnDL <> at || c.EnCS <> at + 4UL then failwith "cave layout"
    c

let cbSites (B: uint64) (c: CbCave) =
    [ S_DL, hexb O_DL, Array.concat [ [| 0xE8uy |]; rel32 (B + uint64 S_DL + 5UL) c.DL ]
      S_C1, hexb O_C1, Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 S_C1 + 5UL) c.C1; hexb "66 90" ]
      S_C2, hexb O_C2, Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 S_C2 + 5UL) c.C2; hexb "66 90" ] ]

// ---- offline: static checks -------------------------------------------------------------------------------------
let cbStatic () =
    let mutable ok = true
    let B = DUMP_BASE
    let c = buildCB (B + 0x5E20000UL) B 1u 1u
    for (s, o, nb) in cbSites B c do
        let cur = img.[s .. s + o.Length - 1]
        let i0 = decodeAt (uint64 s)
        printfn "site %X: %s = %s (len %d) %s -> %s" s (hexs cur) (fmtIns i0) i0.Length (if cur = o then "MATCH" else "DIFFERENT") (hexs nb)
        if cur <> o || i0.Length <> o.Length then ok <- false
        for x in 0xD8180 :: OTHER_SITES do
            if x < s + o.Length && s < x + 10 then (printfn "  !! overlaps another patch site %X" x; ok <- false)
    let hits = scanTargets [ for (s, o, _) in cbSites B c -> sprintf "%X" s, uint64 s, uint64 (s + o.Length) ]
    printfn "direct branch/call targets strictly inside the 3 sites (whole executable sweep): %s" (if hits.Count = 0 then "none" else String.Join("; ", hits))
    if hits.Count <> 0 then ok <- false
    // D8740 purity
    let fn = decodeAll img.[0xD8740 .. 0xD87A2] 0xD8740UL
    let mutable stores = 0
    let written = HashSet<Register>()
    for x in fn do
        let info = infoFactory.GetInfo(&x)
        for m in info.GetUsedMemory() do
            if m.Access = OpAccess.Write || m.Access = OpAccess.ReadWrite || m.Access = OpAccess.CondWrite then stores <- stores + 1
        for u in info.GetUsedRegisters() do
            if u.Access <> OpAccess.Read && u.Access <> OpAccess.CondRead then written.Add(u.Register.GetFullRegister()) |> ignore
    let wl = written |> Seq.map string |> Seq.sort |> List.ofSeq
    printfn "CollisionPartMapFind D8740 (%d instructions): memory writes %d; registers written %A" fn.Length stores wl
    let extra = written |> Seq.filter (fun r -> not (Set.contains r (set [ Register.RAX; Register.RCX; Register.R8; Register.R9; Register.R10; Register.RSP ]))) |> List.ofSeq
    if stores <> 0 || not extra.IsEmpty then (ok <- false; printfn "  !! not pure")
    let f0 = walk [ 0xD8740UL ] [ Register.R8; Register.R9; Register.R10 ] false false 0
    printfn "  D8740 does not read incoming r8/r9/r10: %s" (if f0.IsEmpty then "[ok]" else String.Join("; ", f0))
    ok <- ok && f0.IsEmpty
    // DL skip: from D840F (sil != 0, r11 != 0 established) rdi/rcx/r8/r9/r10 must be written before read on every path
    let f1 = walk [ 0xD840FUL ] [ Register.RDI; Register.RCX; Register.R8; Register.R9; Register.R10 ] false false 0
    printfn "DL: rdi, rcx, r8, r9, r10 (the registers the skipped call would have set) dead from D840F: %s" (if f1.IsEmpty then "[ok]" else String.Join("; ", f1))
    ok <- ok && f1.IsEmpty
    // [rbp-51h] (local_b0) is read only at D84E7; both paths to it pass a store (D83FC first call, D8427 second call)
    let reads51 = decodeAll img.[0xD8152 .. 0xD86E1] 0xD8152UL |> List.filter (fun x -> (fmtIns x).Contains "rbp-51h") |> List.map (fun x -> sprintf "%X %s" x.IP (fmtIns x))
    printfn "  [rbp-51h] uses in D80B0: %s" (String.Join(" | ", reads51))
    // flags
    for st in [ 0xD8682UL; 0xD86ABUL ] do
        let f = walk [ st ] [] true false 0
        printfn "CS: status flags dead at %X: %s" st (if f.IsEmpty then "[ok]" else String.Join("; ", f))
        ok <- ok && f.IsEmpty
    // cave discipline
    printDis c.Bytes.[0x40 ..] c.DL B
    let ins = decodeAll c.Bytes.[0x40 ..] c.DL
    let mutable st = []
    let wr = HashSet<Register>()
    for x in ins do
        let info = infoFactory.GetInfo(&x)
        for m in info.GetUsedMemory() do
            if m.Access = OpAccess.Write || m.Access = OpAccess.ReadWrite then st <- fmtIns x :: st
        for u in info.GetUsedRegisters() do
            if u.Access = OpAccess.Write || u.Access = OpAccess.ReadWrite then wr.Add(u.Register.GetFullRegister()) |> ignore
    printfn "cave discipline: stores = %A; registers written = %A" (List.rev st) (wr |> Seq.map string |> List.ofSeq)
    if st.Length <> 2 || wr.Count <> 1 || not (wr.Contains Register.RSP) then (ok <- false; printfn "  !! unexpected writes")
    ok

// ---- offline: harness ---------------------------------------------------------------------------------------------
type CbH = { H: Harness; C: CbCave }
let mkCb () =
    let h = mkHarness DEFAULT
    let mem = allocNear (h.R + 0xD8400n) 0x1000
    let c = buildCB (uint64 mem) (uint64 h.R) 1u 1u
    Marshal.Copy(c.Bytes, 0, mem, c.Bytes.Length)
    { H = h; C = c }
let setCB (x: CbH) (on: bool) =
    for (s, o, nb) in cbSites (uint64 x.H.R) x.C do
        Marshal.Copy((if on then nb else o), 0, x.H.R + nativeint s, o.Length)
    FlushInstructionCache(GetCurrentProcess(), x.H.R + 0xD8000n, 0x1000un) |> ignore
let setSw (x: CbH) (dl: bool) (cs: bool) =
    Marshal.WriteInt32(nativeint x.C.EnDL, (if dl then 1 else 0)); Marshal.WriteInt32(nativeint x.C.EnCS, (if cs then 1 else 0))

/// the 5 phase jobs on 5 threads at once (as in the game); returns ms from the release to the last join
let runJobsMT (h: Harness) (w: World) =
    use bar = new Barrier(6)
    let ths = [| for j in 0 .. 4 -> Thread((fun () -> bar.SignalAndWait(); h.Fn.Invoke(w.Mgr, w.Outs.[j], int64 j)), 1 <<< 20) |]
    for t in ths do t.Start()
    bar.SignalAndWait()
    let sw = Stopwatch.StartNew()
    for t in ths do t.Join()
    let ms = sw.Elapsed.TotalMilliseconds
    maskRecords w
    ms

let cbEquivalence (x: CbH) =
    let h = x.H
    let cases = if quick then 40 else 300
    let rnd = Random(20261008)
    let mutable bad, recs, worlds, mt = 0, 0L, 0, 0
    let same (a1: byte[], b1: byte[]) (a2: byte[], b2: byte[]) = Linq.Enumerable.SequenceEqual(a1, a2) && Linq.Enumerable.SequenceEqual(b1, b2)
    for k in 1 .. cases do
        let n = [| 0; 1; 3; 7; 16; 40; 200; 1000; 3000 |].[rnd.Next 9]
        let w = mkWorld h (5000 + k) n (rnd.Next 101)
        bind h w
        let snap = snapshot w
        let run (cb: bool) (dl: bool) (cs: bool) (cp: bool) (threads: bool) =
            restore w snap
            setCB x cb; setSw x dl cs; setSite h cp; setEn h true
            (if threads then runJobsMT h w |> ignore else runJobs h w)
            snapshot w
        let stock = run false false false false false
        recs <- recs + records w
        let check name r = if not (same stock r) then (bad <- bad + 1; if bad <= 6 then printfn "  MISMATCH (%s) world %d n=%d" name k n)
        check "CB on" (run true true true false false)
        check "CB installed, switches off" (run true false false false false)
        check "DL only" (run true true false false false)
        check "CB + CP" (run true true true true false)
        if n >= 40 then
            check "stock, 5 threads" (run false false false false true)
            check "CB + CP, 5 threads" (run true true true true true)
            mt <- mt + 1
        worlds <- worlds + 1
        freeWorld w
    setCB x false; setSite h false
    printfn "  %d synthetic worlds (0..3000 colliders; %d also run as 5 concurrent jobs): %d mismatches vs stock (CB on, CB off, DL only, CB+CP, MT); %d change records" worlds mt bad recs
    bad = 0

let cbTiming (x: CbH) =
    let h = x.H
    let reps = if quick then 5 else 11
    let med (s: ResizeArray<float>) = (let a = s |> Seq.sort |> Array.ofSeq in a.[a.Length / 2])
    printfn "  single thread (5 jobs back to back), median of %d:" reps
    for (n, cold) in [ 20000, true; 4000, false; 4000, true ] do
        let w = mkWorld h 777 n 100
        bind h w
        let snap = snapshot w
        let t (cb: bool) (dl: bool) (cs: bool) =
            let ts = ResizeArray<float>()
            for _ in 1 .. reps do
                restore w snap
                setCB x cb; setSw x dl cs
                if cold then flush () else (runJobs h w; resetState w)
                let sw = Stopwatch.StartNew()
                runJobs h w
                ts.Add sw.Elapsed.TotalMilliseconds
            med ts
        let st, dl, all = t false false false, t true true false, t true true true
        printfn "    %5d %-5s stock %7.2f ms  DL %7.2f (%+5.1f %%)  DL+CS %7.2f (%+5.1f %%)" n (if cold then "cold" else "warm") st dl ((dl - st) / st * 100.0) all ((all - st) / st * 100.0)
        freeWorld w
    printfn "  5 concurrent jobs (5 threads), median of %d:" (reps * 2 + 1)
    for (n, cold) in [ 20000, false; 4000, false; 20000, true ] do
        let w = mkWorld h 778 n 100
        bind h w
        let snap = snapshot w
        let t (cb: bool) (dl: bool) (cs: bool) =
            let ts = ResizeArray<float>()
            for _ in 1 .. reps * 2 + 1 do
                restore w snap
                setCB x cb; setSw x dl cs
                if cold then flush () else (runJobsMT h w |> ignore; resetState w)
                ts.Add(runJobsMT h w)
            med ts
        let st, cs, all = t false false false, t true false true, t true true true
        printfn "    %5d %-5s stock %7.2f ms  CS %7.2f (%+5.1f %%)  DL+CS %7.2f (%+5.1f %%)" n (if cold then "cold" else "warm") st cs ((cs - st) / st * 100.0) all ((all - st) / st * 100.0)
        freeWorld w
    setCB x false

let cbOffline () =
    let mutable ok = cbStatic ()
    printfn "real-code harness (collprefetch_live.fsx: dump image mapped privately, D80B0 + real callees):"
    let x = mkCb ()
    ok <- cbEquivalence x && ok
    printfn "timing (this PC):"
    cbTiming x
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live -----------------------------------------------------------------------------------------------------------
let cbLive (m: string) =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readState () =
        if not (File.Exists cbState) then None
        else
            let v = File.ReadAllText(cbState).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let caveOrProbe () = match readState () with Some a -> buildCB a B 0u 0u | None -> buildCB (B + 0x5E20000UL) B 0u 0u
    let classify (c: CbCave) =
        [ for (s, o, nb) in cbSites B c ->
            let cur = t.Read(nativeint (B + uint64 s), o.Length)
            if cur = o then "original" elif cur = nb then "patched" else "UNKNOWN" ]
    let fnOk () = t.Read(nativeint (B + 0xD83F4UL), 0x2C0) = img.[0xD83F4 .. 0xD83F4 + 0x2BF] || classify (caveOrProbe ()) |> List.forall ((=) "patched")
    let which = if args.Length > 1 then args.[1].ToLowerInvariant() else "all"
    match m with
    | "dry" | "status" ->
        let c = caveOrProbe ()
        printfn "base=%X  sites D83F4/D867B/D86A4: %A" B (classify c)
        match readState () with
        | Some a -> printfn "cave %X  DL=%d CS=%d" a (BitConverter.ToInt32(t.Read(nativeint c.EnDL, 4), 0)) (BitConverter.ToInt32(t.Read(nativeint c.EnCS, 4), 0))
        | None -> printfn "no cave in this process"
        if m = "dry" then printDis c.Bytes.[0x40 ..] c.DL B
    | "apply" ->
        writeGuard ()
        if t.Read(nativeint (B + 0xD83F4UL), 0x2C0) <> img.[0xD83F4 .. 0xD83F4 + 0x2BF] then failwith "D80B0 body differs from the dump (or already patched) - aborting"
        let c =
            match readState () with
            | Some a -> buildCB a B 0u 0u
            | None ->
                let mem = t.AllocNear(nativeint (B + 0xD8400UL), 0x1000)
                let c = buildCB (uint64 mem) B 0u 0u
                t.Write(mem, c.Bytes)
                File.WriteAllText(cbState, sprintf "%X %X" (uint64 mem) t.Process.Id)
                c
        let n = safeWrite t [ for (s, o, nb) in cbSites B c -> nativeint (B + uint64 s), nb, o ]
        printfn "CB applied (attempt %d), switches OFF; use `on [dl|cs]`" n
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some a ->
            let c = buildCB a B 0u 0u
            let v = BitConverter.GetBytes(if m = "on" then 1u else 0u)
            if which <> "cs" then t.Write(nativeint c.EnDL, v)
            if which <> "dl" then t.Write(nativeint c.EnCS, v)
            printfn "CB %s %s" which m
        | None -> printfn "CB is not applied in this process - use apply"
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        if classify c |> List.forall ((=) "patched") then
            safeWrite t [ for (s, o, nb) in cbSites B c -> nativeint (B + uint64 s), o, nb ] |> ignore
            printfn "CB reverted (cave left allocated)"
        elif classify c |> List.forall ((=) "original") then printfn "already original"
        else failwith "unexpected bytes at the sites - aborting"
    | m -> printfn "unknown mode %s" m
    ignore fnOk

if mode = "offline" then cbOffline () else cbLive mode
