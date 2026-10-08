// collprefetch_live.fsx - CP: software prefetch pipeline for the collision worker loop (notes\worker-jobs-opt.md).
//
//   dotnet fsi collprefetch_live.fsx offline [quick]   NO GAME ACCESS. Site bytes vs dump, branch-target sweep, dead-register
//                                                      proof at the site, cave register/memory discipline test, and the
//                                                      REAL-CODE harness: the whole dump image is mapped into this process,
//                                                      CollisionObjectUpdate (D80B0) and its real callees (D8D90, D8740,
//                                                      D89E0, D8B70, D6E9C, 2D9EE0, 168E80, 3CD840) run on synthetic
//                                                      collision worlds, stock vs cave: outputs + every byte of the world
//                                                      must be identical. Then a timing comparison (cold / L3-warm).
//   dotnet fsi collprefetch_live.fsx dry               READ-ONLY on the game: site state, cave preview.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi collprefetch_live.fsx apply     one cave per game process, jmp at D8180 (safeWrite).
//   dotnet fsi collprefetch_live.fsx status            READ-ONLY: site state + switch.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi collprefetch_live.fsx on|off    4-byte cave switch (no code write).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi collprefetch_live.fsx revert    original 7 bytes back (the cave stays allocated).
//
// What it does. KIDSTS[s9:CollisionUpdate] runs CollisionObjectUpdate D80B0(mgr, out, start) as 5 strided jobs
// (i = start, start+5, ...) over the per-frame work vector mgr+150h (filled by 43CEC with every collider in range).
// Each iteration chases: collider lines (+08/+0C, +88..+C8), its part array [+88h], count [+C8h], radii [+90h], every
// part object (+62h type, +68h, +80h/+B0h position via D8B70), and lookups. All of these are first-touch cache misses
// (hot RIPs D8291 / D81BD / D83E0 / D8B7E in every PC capture) - the loop is latency-bound, ~0.9 core on the PC and
// 72 % CPU on the Ally (7 threads). The out-of-order window cannot overlap misses of different iterations because each
// iteration is hundreds of instructions long.
// Patch: at the loop head D8180 (`mov rcx,[r10+150h]`, 7 B) -> `jmp cave; nop2`. The cave issues, for the SAME job's
// future colliders (same residue class, so nothing another job touches):
//   stage A  collider i+3*5 : prefetcht0 its 3 lines (+08h, +88h, +C8h)
//   stage B  collider i+2*5 : (lines are in cache now) if A5 set: prefetcht0 [ [+88h] ], [ [+C8h] ], [ [+90h] ]
//   stage C  collider i+1*5 : if A5 set and count>0: for its first K=4 parts: prefetcht0 part+62h, +88h, +B8h
// then executes the original `mov rcx,[r10+150h]` and jumps to D8187.
// Equivalence: the cave writes no memory (only push/pop of rdx/r8 below rsp, restored), restores rdx/r8, and clobbers
// only rax, rcx and the flags, which are proven dead at D8180 (D8180 writes rcx, D8187 writes rax, D818E writes all
// flags before any read). Its loads are exactly loads the stock loop performs later for the same collider under the
// same guards (vector index < size, collider non-null, A5 != 0, [+C8h]/[+88h] non-null, part index < count), and a
// prefetch never faults. So architectural state at D8187 is identical to stock; only cache contents differ.
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
let quick = args |> Array.exists (fun a -> a.ToLowerInvariant() = "quick")
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "collprefetch_state.txt")

let SITE, SITE_ORIG = 0xD8180, "49 8B 8A 50 01 00 00"       // mov rcx,[r10+150h]
let RESUME = 0xD8187UL                                      // mov rax,[r10+158h]
let FN = 0xD80B0UL

// ---- cave -----------------------------------------------------------------------------------------------------
type Prm = { DA: int; DB: int; DC: int; K: int }
let DEFAULT = { DA = 15; DB = 10; DC = 5; K = 4 }
type Cave = { Bytes: byte[]; Code: uint64; En: uint64 }
/// data: en dword @+0 (1 = on). code @+40h. `B` = module base the cave returns into.
let build (at: uint64) (B: uint64) (p: Prm) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, code, stock, skA, skB, skC, lp, nx, kOk = L(), L(), L(), L(), L(), L(), L(), L(), L()
    a.Label(&lEn); a.dd(1u)
    for _ in 1 .. 15 do a.dd(0u)
    a.Label(&code)
    a.cmp(__dword_ptr.[lEn], 0); a.je(stock)
    a.push(rdx); a.push(r8)
    a.mov(rcx, __qword_ptr.[r10 + 0x150L])                   // vector begin
    a.mov(rax, __qword_ptr.[r10 + 0x158L])
    a.sub(rax, rcx); a.sar(rax, 3uy)                         // size (elements)
    // stage A: collider lines
    a.lea(rdx, __[r13 + int64 p.DA]); a.cmp(rdx, rax); a.jae(skA)
    a.mov(rdx, __qword_ptr.[rcx + rdx * 8]); a.test(rdx, rdx); a.je(skA)
    a.prefetcht0(__byte_ptr.[rdx + 0x08L]); a.prefetcht0(__byte_ptr.[rdx + 0x88L]); a.prefetcht0(__byte_ptr.[rdx + 0xC8L])
    a.Label(&skA)
    // stage B: the collider's arrays (its lines were requested one iteration ago)
    a.lea(rdx, __[r13 + int64 p.DB]); a.cmp(rdx, rax); a.jae(skB)
    a.mov(rdx, __qword_ptr.[rcx + rdx * 8]); a.test(rdx, rdx); a.je(skB)
    a.cmp(__byte_ptr.[rdx + 0xA5L], 0); a.je(skB)
    a.mov(r8, __qword_ptr.[rdx + 0x88L]); a.prefetcht0(__byte_ptr.[r8])
    a.mov(r8, __qword_ptr.[rdx + 0xC8L]); a.prefetcht0(__byte_ptr.[r8])
    a.mov(r8, __qword_ptr.[rdx + 0x90L]); a.prefetcht0(__byte_ptr.[r8])
    a.Label(&skB)
    // stage C: the first K part objects (part array requested one iteration ago)
    a.lea(rdx, __[r13 + int64 p.DC]); a.cmp(rdx, rax); a.jae(skC)
    a.mov(rdx, __qword_ptr.[rcx + rdx * 8]); a.test(rdx, rdx); a.je(skC)
    a.cmp(__byte_ptr.[rdx + 0xA5L], 0); a.je(skC)
    a.mov(r8, __qword_ptr.[rdx + 0xC8L]); a.test(r8, r8); a.je(skC)
    a.mov(eax, __dword_ptr.[r8])                              // part count
    a.mov(r8, __qword_ptr.[rdx + 0x88L]); a.test(r8, r8); a.je(skC)
    a.cmp(eax, p.K); a.jbe(kOk); a.mov(eax, p.K)
    a.Label(&kOk)
    a.test(eax, eax); a.je(skC)
    a.Label(&lp)
    a.mov(rdx, __qword_ptr.[r8]); a.test(rdx, rdx); a.je(nx)
    a.prefetcht0(__byte_ptr.[rdx + 0x62L]); a.prefetcht0(__byte_ptr.[rdx + 0x88L]); a.prefetcht0(__byte_ptr.[rdx + 0xB8L])
    a.Label(&nx)
    a.add(r8, 8); a.dec(eax); a.jne(lp)
    a.Label(&skC)
    a.pop(r8); a.pop(rdx)
    a.Label(&stock)
    a.mov(rcx, __qword_ptr.[r10 + 0x150L])                   // the original instruction
    a.jmp(B + RESUME)
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); Code = addr code; En = addr lEn }
    if c.Code - at <> 0x40UL || c.En <> at then failwith "cave layout"
    c

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let siteNew (B: uint64) (c: Cave) = Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 SITE + 5UL) c.Code; hexb "66 90" ]

// ---- native helpers -------------------------------------------------------------------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<DllImport("kernel32.dll")>] extern bool VirtualFree(nativeint addr, unativeint size, uint32 typ)
[<DllImport("kernel32.dll")>] extern bool FlushInstructionCache(nativeint h, nativeint addr, unativeint size)
[<DllImport("kernel32.dll")>] extern nativeint GetCurrentProcess()
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type CollFn = delegate of nativeint * nativeint * int64 -> unit
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type ThunkFn = delegate of nativeint * nativeint -> unit

/// allocate RWX within +-2 GB of `near`
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

// ---- offline: static checks -----------------------------------------------------------------------------------
let staticChecks () =
    let mutable ok = true
    let o = hexb SITE_ORIG
    let cur = img.[SITE .. SITE + o.Length - 1]
    printfn "site %X: %s  %s" SITE (hexs cur) (if cur = o then "MATCH" else "DIFFERENT")
    ok <- ok && cur = o
    let i0 = decodeAt (uint64 SITE)
    printfn "  = %s (len %d); next %X = %s" (fmtIns i0) i0.Length RESUME (fmtIns (decodeAt RESUME))
    if i0.Length <> 7 then (ok <- false; printfn "  !! not one 7-byte instruction")
    for s in OTHER_SITES do
        if s < SITE + 7 && SITE < s + 10 then (printfn "  !! overlaps patch site %X" s; ok <- false)
    printfn "dead-register/flags proof at %X (the cave clobbers rax, rcx, flags; rdx/r8 are saved/restored):" SITE
    let f = walk [ uint64 SITE ] [ Register.RAX; Register.RCX ] true false 0
    if f.IsEmpty then printfn "  [ok] rax, rcx and all status flags are written before any read on every path from %X" SITE
    else (ok <- false; for x in f do printfn "  [FAIL] %s" x)
    printfn "direct branch/call targets strictly inside %X..%X (whole executable sweep):" SITE (SITE + 7)
    let hits = scanTargets [ "CP", uint64 SITE, uint64 SITE + 7UL ]
    if hits.Count = 0 then printfn "  none" else (ok <- false; for h in hits do printfn "  !! %s" h)
    let B = DUMP_BASE
    let c = build (B + 0x5E10000UL) B DEFAULT
    printfn "cave @dummy %X: %d bytes (code at +40h)" (B + 0x5E10000UL) c.Bytes.Length
    printDis c.Bytes.[0x40 ..] c.Code B
    let nb = siteNew B c
    printfn "patched site: %s -> %s" SITE_ORIG (hexs nb)
    printDis nb (B + uint64 SITE) B
    let both = Set.intersect (Set.ofSeq (boundariesOf nb (B + uint64 SITE))) (Set.ofSeq (boundariesOf o (B + uint64 SITE)))
    printfn "  instruction starts valid in both encodings: %A (a thread parked at D8180 is safe in either state)" (both |> Set.map (fun x -> sprintf "%X" (x - B)) |> Set.toList)
    // the cave's own discipline: no stores except push/pop, only rax/rcx/rdx/r8 + flags written, rdx/r8 popped
    let ins = decodeAll c.Bytes.[0x40 ..] c.Code
    let mutable stores = 0
    let written = HashSet<Register>()
    for x in ins do
        let info = infoFactory.GetInfo(&x)
        for m in info.GetUsedMemory() do
            if (m.Access = OpAccess.Write || m.Access = OpAccess.ReadWrite) && x.Mnemonic <> Mnemonic.Push then stores <- stores + 1
        for u in info.GetUsedRegisters() do
            if u.Access = OpAccess.Write || u.Access = OpAccess.ReadWrite then written.Add(u.Register.GetFullRegister()) |> ignore
    let allowed = set [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.RSP ]
    let extra = written |> Seq.filter (fun r -> not (allowed.Contains r)) |> List.ofSeq
    printfn "cave discipline: memory writes other than push = %d; registers written = %A" stores (written |> Seq.map string |> List.ofSeq)
    if stores <> 0 || not extra.IsEmpty then (ok <- false; printfn "  !! cave writes memory or an unexpected register")
    ok

// ---- offline: real-code harness ---------------------------------------------------------------------------------
/// Maps the dump image (raw == RVA) into RWX memory so D80B0 and callees run with their real bytes, then patches
/// the few external dependencies with stubs (identical in both runs).
type Harness =
    { R: nativeint; Stub: nativeint; Cave: Cave; CaveMem: nativeint; Fn: CollFn }
let mkHarness (p: Prm) =
    let size = img.Length
    let R = VirtualAlloc(0n, unativeint size, 0x3000u, 0x40u)
    if R = 0n then failwith "VirtualAlloc image failed"
    Marshal.Copy(img, 0, R, size)
    let stub = allocNear (R + 0x100000n) 0x1000
    let w64 (a: nativeint) (v: int64) = Marshal.WriteInt64(a, v)
    let put (a: nativeint) (b: byte[]) = Marshal.Copy(b, 0, a, b.Length)
    // 19CED0 (world object lookup by id) -> mov rax, [stub+F00h]; ret   (objA is set per world)
    put (R + 0x19CED0n) (assemble (R + 0x19CED0n) (fun a -> a.mov(rax, __qword_ptr.[uint64 (stub + 0xF00n)]); a.ret()))
    // D8B24 (TLS-guarded singleton getter; D89E0 ignores its result) -> lea rax,[R+46D7AD0]; ret
    put (R + 0xD8B24n) (assemble (R + 0xD8B24n) (fun a -> a.mov(rax, uint64 (R + 0x46D7AD0n)); a.ret()))
    // never expected: vector growth 4B807C (capacity is pre-sized) and the out_of_range throw 1FB3AD8
    Marshal.WriteByte(R + 0x4B807Cn, 0xCCuy); Marshal.WriteByte(R + 0x1FB3AD8n, 0xCCuy)
    // generic part-position function for type 2 (D8B70's indirect call): out = part+40h, w = 1.0
    put stub (assemble stub (fun a ->
        a.movups(xmm0, __xmmword_ptr.[rdx + 0x40L]); a.movups(__xmmword_ptr.[rcx], xmm0)
        a.mov(__dword_ptr.[rcx + 0xCL], 0x3F800000u); a.mov(rax, rcx); a.ret()))
    // position-function table 3BE3780 (type byte +62h): 0 -> inline copy path (A948C0), 1 -> 2D9EE0 (168E80), 2 -> stub
    w64 (R + 0x3BE3780n) (int64 (R + 0xA948C0n)); w64 (R + 0x3BE3788n) (int64 (R + 0x2D9EE0n)); w64 (R + 0x3BE3790n) (int64 stub)
    let caveMem = allocNear (R + nativeint SITE) 0x1000
    let c = build (uint64 caveMem) (uint64 R) p
    Marshal.Copy(c.Bytes, 0, caveMem, c.Bytes.Length)
    FlushInstructionCache(GetCurrentProcess(), R, unativeint size) |> ignore
    { R = R; Stub = stub; Cave = c; CaveMem = caveMem; Fn = Marshal.GetDelegateForFunctionPointer<CollFn>(R + nativeint FN) }
let setSite (h: Harness) (patched: bool) =
    let b = if patched then siteNew (uint64 h.R) h.Cave else hexb SITE_ORIG
    Marshal.Copy(b, 0, h.R + nativeint SITE, b.Length)
    FlushInstructionCache(GetCurrentProcess(), h.R + nativeint SITE, 7un) |> ignore
let setEn (h: Harness) (on: bool) = Marshal.WriteInt32(nativeint h.Cave.En, if on then 1 else 0)

/// Synthetic collision world. Objects live in a "scatter" arena (random 0x300-byte slots, so neighbouring objects
/// are far apart like heap objects); arrays live in a "linear" arena. Both arenas are snapshotted/restored as a whole.
type World =
    { Scatter: nativeint; ScatterSize: int; Linear: nativeint; LinearSize: int
      Mgr: nativeint; Outs: nativeint[]; OutCap: int; ObjA: nativeint; WorldObj: nativeint; GMap: nativeint; NColl: int; Fixups: (nativeint * byte)[] }
let SLOT = 0x300
let mkWorld (h: Harness) (seed: int) (nColl: int) (spread: int) =
    let rnd = Random(seed)
    let nSlots = nColl * 16 + 4096
    let scatterSize = nSlots * SLOT
    let outCap = (nColl * 7 / 5 + 64) * 7 * 0x30
    let linearSize = 0x100000 + nColl * 0x400 + 5 * (outCap + 0x80)
    let S = VirtualAlloc(0n, unativeint scatterSize, 0x3000u, 0x04u)
    let Lb = VirtualAlloc(0n, unativeint linearSize, 0x3000u, 0x04u)
    if S = 0n || Lb = 0n then failwith "arena alloc failed"
    // slot permutation (only a `spread` fraction is randomised: spread=0 -> sequential, 100 -> fully random)
    let order = Array.init nSlots id
    for i in nSlots - 1 .. -1 .. 1 do
        if rnd.Next 100 < spread then (let j = rnd.Next(i + 1) in let t = order.[i] in order.[i] <- order.[j]; order.[j] <- t)
    let mutable nextSlot = 0
    let obj () = (let s = order.[nextSlot] in nextSlot <- nextSlot + 1; S + nativeint (s * SLOT))
    let mutable lin = 0
    let arr (bytes: int) = (let p = Lb + nativeint lin in lin <- (lin + bytes + 63) &&& ~~~63; (if lin > linearSize then failwith "linear arena full"); p)
    let w64 (a: nativeint) (v: int64) = Marshal.WriteInt64(a, v)
    let w32 (a: nativeint) (v: int) = Marshal.WriteInt32(a, v)
    let w8 (a: nativeint) (v: int) = Marshal.WriteByte(a, byte v)
    let wf (a: nativeint) (v: float32) = Marshal.WriteInt32(a, BitConverter.SingleToInt32Bits v)
    let rb () = rnd.Next 2
    let fix = ResizeArray<nativeint>()        // bytes D80B0 writes (restored by resetState for warm timing)
    // ---- world (g_CollisionWorld) ----
    let W = arr 0x2000
    let px, py, pz = float32 (rnd.Next(-5000, 5000)), float32 (rnd.Next(-500, 500)), float32 (rnd.Next(-5000, 5000))
    wf (W + 0x1B10n) px; wf (W + 0x1B14n) py; wf (W + 0x1B18n) pz
    w8 (W + 0x11F6n) (rb ())
    // ---- owners ----
    let nOwn = max 4 (nColl / 3)
    let keys = HashSet<uint32>()
    while keys.Count < nOwn do keys.Add(uint32 (rnd.Next(1, Int32.MaxValue))) |> ignore
    let keyArr = keys |> Seq.sort |> Array.ofSeq
    // D8D90 array at H+180h: sorted {u32 key, ptr} for ~80 % of the owners
    let H = obj ()
    let oaKeys = keyArr |> Array.filter (fun _ -> rnd.Next 10 < 8)
    let OA = arr (oaKeys.Length * 16 + 16)
    oaKeys |> Array.iteri (fun i k ->
        let o = obj ()
        w32 (o + 0xF8n) (rnd.Next()); w8 (o + 0x106n) (rb ()); w8 (o + 0x10Fn) (rnd.Next 4); w8 (o + 0x110n) (rnd.Next 4); fix.Add(o + 0x110n)
        w32 (OA + nativeint (i * 16)) (int k); w64 (OA + nativeint (i * 16 + 8)) (int64 o))
    w64 (H + 0x180n) (int64 OA); w64 (H + 0x188n) (int64 oaKeys.Length)
    // std::map (MSVC _Tree): header = nil node (+19h isnil = 1), [hdr+8] = root; node {+0 left, +8 parent, +10h right,
    // +19h isnil, +20h key u32, +28h value}. Built as a balanced BST from sorted keys.
    let mkMap (ks: uint32[]) (mkVal: uint32 -> nativeint) =
        let hdr = obj ()
        w8 (hdr + 0x19n) 1
        let rec bst (lo: int) (hi: int) (parent: nativeint) : nativeint =
            if lo > hi then hdr
            else
                let mid = (lo + hi) / 2
                let n = obj ()
                w8 (n + 0x19n) 0; w32 (n + 0x20n) (int ks.[mid]); w64 (n + 0x28n) (int64 (mkVal ks.[mid])); w64 (n + 8n) (int64 parent)
                w64 n (int64 (bst lo (mid - 1) n)); w64 (n + 0x10n) (int64 (bst (mid + 1) hi n))
                n
        w64 (hdr + 8n) (int64 (bst 0 (ks.Length - 1) hdr))
        hdr
    let omKeys = keyArr |> Array.filter (fun _ -> rnd.Next 10 < 5)
    let OM = mkMap omKeys (fun _ -> (let v = obj () in w8 (v + 0x126n) 1; fix.Add(v + 0x126n); v))
    w64 (H + 0x28n) (int64 OM)
    // ---- parts + the part map (D8740: sorted {u64 part, rec}) ----
    let colliders = Array.init nColl (fun _ -> obj ())
    let allParts = ResizeArray<nativeint>()
    let partLists =
        colliders |> Array.map (fun _ ->
            let n = [| 0; 1; 1; 1; 2; 2; 3; 4; 6 |].[rnd.Next 9]
            Array.init n (fun _ ->
                if rnd.Next 20 = 0 then 0n
                else
                    let p = obj ()                                    // slots are 0x300-aligned -> +B0h is 16-aligned
                    let t = rnd.Next 3
                    w8 (p + 0x62n) t
                    let pos (o: int) = wf (p + nativeint o) (px + float32 (rnd.Next(-30000, 30000))); wf (p + nativeint (o + 4)) (py + float32 (rnd.Next(-3000, 3000))); wf (p + nativeint (o + 8)) (pz + float32 (rnd.Next(-30000, 30000))); wf (p + nativeint (o + 12)) 1.0f
                    pos 0x80; pos 0xB0; pos 0x40
                    if rnd.Next 6 = 0 then
                        let par = obj ()
                        for k in 0 .. 2 do Marshal.WriteInt64(par + nativeint (0x210 + 8 * k), BitConverter.DoubleToInt64Bits(float (rnd.Next(-2000, 2000))))
                        w64 (p + 0x68n) (int64 par)
                    allParts.Add p
                    p))
    let mkPartMap () =
        let ps = allParts |> Seq.filter (fun _ -> rnd.Next 10 < 9) |> Seq.sortBy uint64 |> Array.ofSeq
        let m = obj ()
        let a = arr (ps.Length * 16 + 16)
        ps |> Array.iteri (fun i p ->
            let r = obj ()
            w8 (r + 1n) (rb ()); w8 (r + 5n) (rnd.Next 256)
            w64 (a + nativeint (i * 16)) (int64 p); w64 (a + nativeint (i * 16 + 8)) (int64 r))
        w64 (m + 0x10n) (int64 a); w64 (m + 0x18n) (int64 ps.Length)
        m
    let PM = mkPartMap ()
    // global map [4759FC0]+28h (owner fallback when the collider has no OM entry and +A0 != 0)
    let gmKeys = keyArr |> Array.filter (fun _ -> rnd.Next 10 < 4)
    let pm2 = mkPartMap ()
    let GMh = mkMap gmKeys (fun _ ->
        let v = obj ()
        w8 (v + 0x26Dn) (rb ()); w8 (v + 0x278n) (rb ())
        w64 (v + 0x60n) (if rnd.Next 4 = 0 then 0L else int64 pm2)
        v)
    let GM = obj ()
    w64 (GM + 0x28n) (int64 GMh)
    // world +1A68h index table (sometimes absent)
    if rnd.Next 3 <> 0 then
        let T = obj ()
        let U = obj ()
        let n = 64
        let a = arr (n * 8)
        for i in 0 .. n - 1 do
            if rnd.Next 4 <> 0 then
                let X = obj ()
                if rnd.Next 3 <> 0 then (let Y = obj () in w32 (Y + 8n) (rnd.Next 2); w64 (X + 0x28n) (int64 Y))
                w64 (a + nativeint (i * 8)) (int64 X)
        w64 (U + 8n) (int64 a); w32 (U + 0x10n) n
        w64 (T + 0x28n) (int64 U)
        w64 (W + 0x1A68n) (int64 T)
    // ---- mgr: grid cells (0xD0 each; +B0h/+B8h vector of 16-byte points) ----
    let mgr = arr 0x400
    w32 (mgr + 4n) 7
    let gx, gy = 4, 4
    w32 (mgr + 0x140n) gx; w32 (mgr + 0x144n) gy
    let cells = arr (gx * gy * 0xD0)
    for c in 0 .. gx * gy - 1 do
        let n = rnd.Next 4
        let pts = arr (n * 16 + 16)
        for k in 0 .. n - 1 do
            wf (pts + nativeint (k * 16)) (px + float32 (rnd.Next(-20000, 20000))); wf (pts + nativeint (k * 16 + 4)) py
            wf (pts + nativeint (k * 16 + 8)) (pz + float32 (rnd.Next(-20000, 20000))); wf (pts + nativeint (k * 16 + 12)) 0.0f
        let cell = cells + nativeint (c * 0xD0)
        w64 (cell + 0xB0n) (int64 pts); w64 (cell + 0xB8n) (int64 (pts + nativeint (n * 16)))
    w64 (mgr + 0x148n) (int64 cells)
    w64 (mgr + 0x180n) 0x1234L
    // ---- colliders ----
    let pick () = keyArr.[rnd.Next keyArr.Length]
    colliders |> Array.iteri (fun i C ->
        let parts = partLists.[i]
        w32 (C + 8n) (int (if rnd.Next 20 = 0 then uint32 (rnd.Next()) else pick ()))
        w32 (C + 0xCn) (if rnd.Next 2 = 0 then 0 else int (pick ()))
        w32 (C + 0x9Cn) (if rnd.Next 3 = 0 then -1 else rnd.Next 80)
        w32 (C + 0xA0n) (rb ())
        w8 (C + 0xA5n) (if rnd.Next 25 = 0 then 0 else 1)
        w8 (C + 0xA6n) (if rnd.Next 5 = 0 then 1 else 0)
        w8 (C + 0xC0n) (rnd.Next 4)
        fix.Add(C + 0xA5n); fix.Add(C + 0xA6n); fix.Add(C + 0xC0n)
        wf (C + 0x98n) (float32 (rnd.Next(0, 30000)))
        if parts.Length > 0 || rnd.Next 4 <> 0 then
            let pa = arr (parts.Length * 8 + 8)
            parts |> Array.iteri (fun k p -> w64 (pa + nativeint (k * 8)) (int64 p))
            w64 (C + 0x88n) (int64 pa)
            let cnt = arr 8
            w32 cnt (if rnd.Next 15 = 0 then max 0 (parts.Length - 1) else parts.Length)
            if rnd.Next 30 <> 0 then w64 (C + 0xC8n) (int64 cnt)
            if rnd.Next 3 <> 0 then
                let rad = arr (parts.Length * 4 + 4)
                for k in 0 .. parts.Length - 1 do wf (rad + nativeint (k * 4)) (float32 (rnd.Next(0, 5000)))
                w64 (C + 0x90n) (int64 rad)
        let ng = rnd.Next 3
        let gl = arr (ng * 4 + 4)
        for k in 0 .. ng - 1 do w32 (gl + nativeint (k * 4)) (rnd.Next(gx * gy + 2))
        w64 (C + 0xA8n) (int64 gl); w64 (C + 0xB0n) (int64 (gl + nativeint (ng * 4))))
    // work vector mgr+150h (with a few null entries)
    let vec = arr (nColl * 8 + 8)
    colliders |> Array.iteri (fun i C -> w64 (vec + nativeint (i * 8)) (if rnd.Next 200 = 0 then 0L else int64 C))
    w64 (mgr + 0x150n) (int64 vec); w64 (mgr + 0x158n) (int64 (vec + nativeint (nColl * 8)))
    // per-job output vectors {+0 begin, +8 end, +10h cap}
    let outs = Array.init 5 (fun _ ->
        let ov = arr 0x40
        let buf = arr outCap
        w64 ov (int64 buf); w64 (ov + 8n) (int64 buf); w64 (ov + 0x10n) (int64 (buf + nativeint outCap))
        ov)
    // objA (the 19CED0 result): +30h = H, +60h = part map
    let objA = obj ()
    w64 (objA + 0x30n) (int64 H); w64 (objA + 0x60n) (int64 PM)
    { Scatter = S; ScatterSize = scatterSize; Linear = Lb; LinearSize = linearSize; Mgr = mgr; Outs = outs; OutCap = outCap
      ObjA = objA; WorldObj = W; GMap = GM; NColl = nColl
      Fixups = [| for a in fix -> a, Marshal.ReadByte a |] }
let bind (h: Harness) (w: World) =
    Marshal.WriteInt64(h.Stub + 0xF00n, int64 w.ObjA)
    Marshal.WriteInt64(h.R + 0x45B9E58n, int64 w.WorldObj)
    Marshal.WriteInt64(h.R + 0x4759FC0n, int64 w.GMap)
let freeWorld (w: World) = VirtualFree(w.Scatter, 0un, 0x8000u) |> ignore; VirtualFree(w.Linear, 0un, 0x8000u) |> ignore
let snapshot (w: World) =
    let a = Array.zeroCreate<byte> w.ScatterSize
    let b = Array.zeroCreate<byte> w.LinearSize
    Marshal.Copy(w.Scatter, a, 0, a.Length); Marshal.Copy(w.Linear, b, 0, b.Length)
    a, b
let restore (w: World) (a: byte[], b: byte[]) = Marshal.Copy(a, 0, w.Scatter, a.Length); Marshal.Copy(b, 0, w.Linear, b.Length)
/// The 0x30-byte change records are built in D80B0 locals of which 16 bytes are never written (rec+12h..13h,
/// +19h..1Fh, +29h..2Fh: padding / uStack_96 / uStack_8f / uStack_7c) - they carry stale stack bytes in the stock game
/// too. They are zeroed after each run so the comparison sees only defined bytes.
let maskRecords (w: World) =
    for ov in w.Outs do
        let b, e = Marshal.ReadInt64 ov, Marshal.ReadInt64(ov + 8n)
        let mutable r = b
        while r < e do
            for o in [ 0x12; 0x13; 0x19; 0x1A; 0x1B; 0x1C; 0x1D; 0x1E; 0x1F; 0x29; 0x2A; 0x2B; 0x2C; 0x2D; 0x2E; 0x2F ] do
                Marshal.WriteByte(nativeint (r + int64 o), 0uy)
            r <- r + 0x30L
let resetState (w: World) =
    for (a, v) in w.Fixups do Marshal.WriteByte(a, v)
    for ov in w.Outs do Marshal.WriteInt64(ov + 8n, Marshal.ReadInt64 ov)
let runJobs (h: Harness) (w: World) =
    for j in 0 .. 4 do h.Fn.Invoke(w.Mgr, w.Outs.[j], int64 j)
    maskRecords w
let records (w: World) = [ for ov in w.Outs -> (Marshal.ReadInt64(ov + 8n) - Marshal.ReadInt64 ov) / 0x30L ] |> List.sum

let equivalence (h: Harness) =
    let cases = if quick then 40 else 300
    let rnd = Random(20261007)
    let mutable bad, recs, worlds = 0, 0L, 0
    for k in 1 .. cases do
        let n = [| 0; 1; 3; 7; 16; 40; 200; 1000; 3000 |].[rnd.Next 9]
        let w = mkWorld h (1000 + k) n (rnd.Next 101)
        bind h w
        let snap = snapshot w
        // A: stock bytes at the site
        setSite h false
        runJobs h w
        let a1, b1 = snapshot w
        recs <- recs + records w
        // B: cave on
        restore w snap
        setSite h true; setEn h true
        runJobs h w
        let a2, b2 = snapshot w
        // C: cave installed, switch off
        restore w snap
        setEn h false
        runJobs h w
        let a3, b3 = snapshot w
        setEn h true
        let same (x: byte[]) (y: byte[]) = Linq.Enumerable.SequenceEqual(x, y)
        if not (same a1 a2 && same b1 b2) then (bad <- bad + 1; if bad <= 5 then printfn "  MISMATCH (cave on) world %d n=%d" k n)
        if not (same a1 a3 && same b1 b3) then (bad <- bad + 1; if bad <= 5 then printfn "  MISMATCH (switch off) world %d n=%d" k n)
        worlds <- worlds + 1
        freeWorld w
    setSite h false
    printfn "  %d synthetic worlds (0..3000 colliders, 5 jobs each, cave on + switch off vs stock): %d mismatches; %d change records emitted by stock (all compared byte for byte, plus every world byte)" worlds bad recs
    bad = 0

let flushBuf = Array.zeroCreate<byte> (128 <<< 20)
let flush () = for i in 0 .. 64 .. flushBuf.Length - 1 do flushBuf.[i] <- flushBuf.[i] + 1uy
/// median ms of `reps` timed runs. cold: world restored + 128 MB written before each run (nothing of the world in
/// L1/L2/L3). warm: the same world is run once untimed, its written bytes are reset, then timed (L3-resident data).
let timeRuns (h: Harness) (w: World) (snap: byte[] * byte[]) (patched: bool) (cold: bool) (reps: int) =
    setSite h patched
    let ts = ResizeArray<float>()
    for _ in 1 .. reps do
        restore w snap
        if cold then flush () else (runJobs h w; resetState w)
        let sw = Stopwatch.StartNew()
        runJobs h w
        ts.Add(sw.Elapsed.TotalMilliseconds)
    setSite h false
    let s = ts |> Seq.sort |> Array.ofSeq
    s.[s.Length / 2]
let timing (h: Harness) =
    let reps = if quick then 5 else 11
    for (n, cold) in [ (if quick then 20000 else 60000), true; 4000, false; 4000, true ] do
        let w = mkWorld h 777 n 100
        bind h w
        let snap = snapshot w
        let st = timeRuns h w snap false cold reps
        let cp = timeRuns h w snap true cold reps
        printfn "  %5d colliders %-5s stock %7.2f ms   CP %7.2f ms   %+5.1f %%   (%4.0f -> %4.0f ns per collider)" n (if cold then "cold" else "warm") st cp ((cp - st) / st * 100.0) (st * 1e6 / float n) (cp * 1e6 / float n)
        freeWorld w

let tune () =
    printfn "  distance / K sweep (20000 colliders, cold, median of 7):"
    for p in [ { DA = 15; DB = 10; DC = 5; K = 4 }; { DA = 10; DB = 10; DC = 5; K = 4 }; { DA = 20; DB = 10; DC = 5; K = 4 }
               { DA = 25; DB = 15; DC = 5; K = 4 }; { DA = 20; DB = 15; DC = 10; K = 4 }; { DA = 15; DB = 10; DC = 5; K = 8 }; { DA = 15; DB = 10; DC = 5; K = 2 } ] do
        let h = mkHarness p
        let w = mkWorld h 4242 20000 100
        bind h w
        let snap = snapshot w
        let st = timeRuns h w snap false true 7
        let cp = timeRuns h w snap true true 7
        printfn "    A=%2d B=%2d C=%2d K=%d   stock %6.2f ms  CP %6.2f ms  %+5.1f %%" p.DA p.DB p.DC p.K st cp ((cp - st) / st * 100.0)
        freeWorld w
        VirtualFree(h.R, 0un, 0x8000u) |> ignore

// cave register discipline, natively: run the cave with random registers through a thunk; everything except
// rax/rcx/flags must come back unchanged, and memory must be unchanged
let cavePreserves (h: Harness) =
    // thunk(regsIn*, regsOut*): load r8..r15/rbx/rsi/rdi/rbp/rdx from regsIn, jmp-call the cave via a fake "return"
    let T = allocNear (h.R + nativeint SITE) 0x1000
    // the cave returns by `jmp R+D8187`; place a trampoline there? We cannot touch R+D8187 (real code). Instead build a
    // private cave copy that jumps back into the thunk.
    let thunkAt = uint64 T
    // private cave copy whose "return" (base + D8187) lands in this page at +800h
    let baseFake = (thunkAt + 0x800UL) - RESUME
    let c = build (thunkAt + 0x400UL) baseFake DEFAULT
    Marshal.Copy(c.Bytes, 0, nativeint (thunkAt + 0x400UL), c.Bytes.Length)
    let regs = [| Register.RBX; Register.RBP; Register.RSI; Register.RDI; Register.R8; Register.R9; Register.R10; Register.R11; Register.R12; Register.R13; Register.R14; Register.R15; Register.RDX |]
    let entry = assemble T (fun a ->
        // rcx = in*, rdx = out*
        a.push(rbx); a.push(rbp); a.push(rsi); a.push(rdi); a.push(r12); a.push(r13); a.push(r14); a.push(r15)
        a.sub(rsp, 0x28)
        a.mov(__qword_ptr.[rsp + 0x20L], rdx)
        a.mov(rax, rcx)
        for i in 0 .. regs.Length - 1 do
            a.mov(AssemblerRegister64(regs.[i]), __qword_ptr.[rax + int64 (i * 8)])
        a.jmp(c.Code))
    Marshal.Copy(entry, 0, T, entry.Length)
    let exitc = assemble (nativeint (thunkAt + 0x800UL)) (fun a ->
        a.mov(rax, __qword_ptr.[rsp + 0x20L])
        for i in 0 .. regs.Length - 1 do
            a.mov(__qword_ptr.[rax + int64 (i * 8)], AssemblerRegister64(regs.[i]))
        a.mov(__qword_ptr.[rax + int64 (regs.Length * 8)], rcx)
        a.add(rsp, 0x28)
        a.pop(r15); a.pop(r14); a.pop(r13); a.pop(r12); a.pop(rdi); a.pop(rsi); a.pop(rbp); a.pop(rbx)
        a.ret())
    Marshal.Copy(exitc, 0, nativeint (thunkAt + 0x800UL), exitc.Length)
    let f = Marshal.GetDelegateForFunctionPointer<ThunkFn>(T)
    let rnd = Random(5)
    let inp = Marshal.AllocHGlobal(0x100)
    let outp = Marshal.AllocHGlobal(0x100)
    let mutable bad, calls = 0, 0
    for k in 0 .. 49 do
        // a world of k colliders; r10 = mgr, r13 = random index (in or out of range)
        let w = mkWorld h (90000 + k) k 100
        let snap = snapshot w
        for j in 1 .. 40 do
            let vals = regs |> Array.map (fun _ -> rnd.NextInt64())
            vals.[Array.IndexOf(regs, Register.R10)] <- int64 w.Mgr
            vals.[Array.IndexOf(regs, Register.R13)] <- int64 (rnd.Next(0, 60))
            vals |> Array.iteri (fun i v -> Marshal.WriteInt64(inp + nativeint (i * 8), v))
            Marshal.WriteInt32(nativeint c.En, if j % 10 = 0 then 0 else 1)
            f.Invoke(inp, outp)
            calls <- calls + 1
            for i in 0 .. regs.Length - 1 do
                if Marshal.ReadInt64(outp + nativeint (i * 8)) <> vals.[i] then (bad <- bad + 1; if bad < 5 then printfn "  !! %O changed" regs.[i])
            if Marshal.ReadInt64(outp + nativeint (regs.Length * 8)) <> Marshal.ReadInt64(w.Mgr + 0x150n) then (bad <- bad + 1; printfn "  !! rcx != [r10+150h]")
        let a, b = snapshot w
        if not (Linq.Enumerable.SequenceEqual(a, fst snap) && Linq.Enumerable.SequenceEqual(b, snd snap)) then (bad <- bad + 1; printfn "  !! memory changed")
        freeWorld w
    printfn "  cave alone, %d calls (r10 = mgr of 0..49 colliders, random r13, 10 %% switch off): %d register/memory differences (13 GPRs compared; rcx = [r10+150h] as the original)" calls bad
    bad = 0

let offline () =
    let mutable ok = staticChecks ()
    printfn "real-code harness (dump image mapped at a private address, stubs: 19CED0 world lookup, D8B24 TLS getter, type-2 position fn):"
    let h = mkHarness DEFAULT
    ok <- cavePreserves h && ok
    ok <- equivalence h && ok
    printfn "timing (this PC; median of runs; all 5 jobs run back to back on one thread):"
    timing h
    if not quick then tune ()
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live modes -----------------------------------------------------------------------------------------------
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let caveOrProbe () = match readState () with Some a -> build a B DEFAULT | None -> build (B + 0x5E10000UL) B DEFAULT
    let classify (c: Cave) =
        let cur = t.Read(nativeint (B + uint64 SITE), 7)
        (if cur = hexb SITE_ORIG then "original" elif cur = siteNew B c then "patched" else "UNKNOWN"), cur
    let fnOk () = t.Read(nativeint (B + FN), 0x40) = img.[int FN .. int FN + 0x3F]
    match mode with
    | "dry" | "status" ->
        let c = caveOrProbe ()
        let s, cur = classify c
        printfn "base=%X  D80B0 body matches dump: %b  site D8180: %s (%s)" B (fnOk ()) s (hexs cur)
        match readState () with
        | Some a -> printfn "cave %X  switch=%d" a (BitConverter.ToInt32(t.Read(nativeint c.En, 4), 0))
        | None -> printfn "no cave in this process"
        if mode = "dry" then (printfn "cave preview:"; printDis c.Bytes.[0x40 ..] c.Code B)
    | "apply" ->
        writeGuard ()
        if not (fnOk ()) then failwith "D80B0 differs from the dump - wrong build, aborting"
        let probe = caveOrProbe ()
        if fst (classify probe) <> "original" then failwith "site not original - revert first"
        let c =
            match readState () with
            | Some a -> build a B DEFAULT
            | None ->
                let mem = t.AllocNear(nativeint (B + uint64 SITE), 0x1000)
                let c = build (uint64 mem) B DEFAULT
                t.Write(mem, c.Bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" (uint64 mem) t.Process.Id)
                c
        let n = safeWrite t [ nativeint (B + uint64 SITE), siteNew B c, hexb SITE_ORIG ]
        printfn "CP applied (attempt %d); cave %X switch=%d" n c.En (BitConverter.ToInt32(t.Read(nativeint c.En, 4), 0))
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some a ->
            let c = build a B DEFAULT
            t.Write(nativeint c.En, BitConverter.GetBytes(if mode = "on" then 1u else 0u))
            printfn "CP switch %s" mode
        | None -> printfn "CP is not applied in this process - use apply"
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        match classify c with
        | "patched", _ -> (safeWrite t [ nativeint (B + uint64 SITE), hexb SITE_ORIG, siteNew B c ] |> ignore; printfn "CP reverted (cave left allocated)")
        | "original", _ -> printfn "already original"
        | _ -> failwith "unexpected bytes at the site - aborting"
    | m -> printfn "unknown mode %s" m

// main only when run directly (collbook_live.fsx #loads this file for its harness)
if Path.GetFileName(fsi.CommandLineArgs.[0]).StartsWith("collprefetch", StringComparison.OrdinalIgnoreCase) then
    (if mode = "offline" then offline () else live ())
