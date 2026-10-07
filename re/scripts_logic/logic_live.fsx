// logic_live.fsx - game-logic-thread patches found by the mainthread-logic analysis (notes\mainthread-logic.md).
//
//   dotnet fsi logic_live.fsx [offline]        NO game access: site/byte checks, branch-target sweep, native equivalence
//                                              test of the cave against the original code copied from the dump, timing.
//   dotnet fsi logic_live.fsx dry              READ-ONLY attach: live bytes vs dump, site state, cave preview.
//   dotnet fsi logic_live.fsx status           READ-ONLY: site state, switch, call counter (if counting is on).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx apply      WRITES: allocates the cave (once per process), jmp at 6EA50.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx off|on     WRITES 1 dword in the cave only (A/B without code writes).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx count      WRITES switch=3 (on + call counter, for status; adds a shared inc).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx revert     WRITES original 5 bytes back (cave stays allocated).
//
// Patch "AABB" (only one so far): Nioh3+6EA50 = ObbToAabb(const Obb* o /*rcx*/, float out[8] /*rdx*/).
//   Obb: center @+0 (4 f32), axes @+10h/+20h/+30h (4 f32 each), half extents @+40h/+44h/+48h.
//   out: [0..2] = max xyz, [4..6] = min xyz; out[3] / out[7] are NOT written.
//   Stock code: for each of the 6 directions +-x/+-y/+-z and each of the 3 axes it calls dot (6F060), scale (6F010) and
//   add (6EFD0) through memory: 54 calls + 6 store/reload chains per box. It is ~6-8 % of KIDSTaskScheduler0 (the game
//   update thread) in every capture, mostly from 71C60 (per-bone OBBs -> character AABB), 38 callers in total.
//   Cave: the same IEEE operations in the same order, done for the 3 output lanes at once in SSE registers
//   (dot -> sign select -> f*e -> *A -> +P), so outputs are bit-identical (checked natively below, incl. +-0 / denormals).
//   Falls back to the original code when out overlaps the input (the stock code re-reads the input after each store).
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
open System
open System.IO
open System.Diagnostics
open System.Runtime.InteropServices
open Iced.Intel
open type Iced.Intel.AssemblerRegisters
open Mem

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "logic_live_state.txt")
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"

let SITE = 0x6EA50
let SITE_ORIG = [| 0x48uy; 0x89uy; 0x5Cuy; 0x24uy; 0x08uy |]      // mov [rsp+8],rbx
let RESUME = 0x6EA55                                                 // push rbp (next original instruction)
let FN_END = 0x6EFCB                                                 // end of 6EA50 incl. its cold stubs
// cave layout
let OFF_EN, OFF_CNT, OFF_DATA, OFF_CODE = 0x00, 0x04, 0x40, 0x100
let CAVE_SIZE = 0x400

/// Builds the whole cave image for address `cave`; `resume` = absolute address of the original 6EA55.
let buildCave (cave: uint64) (resume: uint64) : byte[] =
    let a = Assembler(64)
    let mutable lOne = a.CreateLabel()
    let mutable lMone = a.CreateLabel()
    let mutable lS0 = a.CreateLabel()
    let mutable lS1 = a.CreateLabel()
    let mutable lS2 = a.CreateLabel()
    let mutable lEn = a.CreateLabel()
    let mutable lCnt = a.CreateLabel()
    let mutable fallback = a.CreateLabel()
    let mutable noAlias = a.CreateLabel()
    let mutable noCount = a.CreateLabel()
    // header
    a.Label(&lEn); a.dd(1u)
    a.Label(&lCnt); a.dd(0u)
    for _ in 1 .. (OFF_DATA - 8) / 4 do a.dd(0u)
    // data @+40h (16-aligned because the cave base is page aligned)
    a.Label(&lOne); a.dd(0x3F800000u); a.dd(0x3F800000u); a.dd(0x3F800000u); a.dd(0x3F800000u)
    a.Label(&lMone); a.dd(0xBF800000u); a.dd(0xBF800000u); a.dd(0xBF800000u); a.dd(0xBF800000u)
    a.Label(&lS0); a.dd(0x80000000u); a.dd(0u); a.dd(0u); a.dd(0u)
    a.Label(&lS1); a.dd(0u); a.dd(0x80000000u); a.dd(0u); a.dd(0u)
    a.Label(&lS2); a.dd(0u); a.dd(0u); a.dd(0x80000000u); a.dd(0u)
    for _ in 1 .. (OFF_CODE - OFF_DATA - 0x50) / 4 do a.dd(0xCCCCCCCCu)
    // code @+100h
    a.test(__byte_ptr.[lEn], 1u)
    a.jz(fallback)
    a.test(__byte_ptr.[lEn], 2u)
    a.jz(noCount)
    a.inc(__dword_ptr.[lCnt])                     // optional, plain inc (status only)
    a.Label(&noCount)
    // out [rdx, rdx+1Ch) must not overlap obb [rcx, rcx+4Ch)
    a.lea(rax, __[rcx + 0x4CL])
    a.cmp(rdx, rax)
    a.jae(noAlias)
    a.lea(rax, __[rdx + 0x1CL])
    a.cmp(rcx, rax)
    a.jb(fallback)
    a.Label(&noAlias)
    a.movups(xmm5, __xmmword_ptr.[rcx])           // Pmin = C
    a.movaps(xmm4, xmm5)                          // Pmax = C
    for i in 0 .. 2 do
        let axOff = int64 (0x10 + 0x10 * i)
        let eOff = int64 (0x40 + 4 * i)
        a.movups(xmm0, __xmmword_ptr.[rcx + axOff])   // A
        a.xorps(xmm1, xmm1)
        a.mulps(xmm1, xmm0)                           // Z = 0*A (the zero lanes of d times A)
        for neg in [ false; true ] do
            let P = if neg then xmm5 else xmm4
            // D = ((d.y*A.y) + (d.x*A.x)) + (d.z*A.z) for d = +-e_k, lane k
            a.movaps(xmm2, xmm1); a.shufps(xmm2, xmm2, 0x55uy); a.blendps(xmm2, xmm0, 2uy)   // (Zy, Ay, Zy)
            if neg then a.xorps(xmm2, __xmmword_ptr.[lS1])                                    // lane1: -Ay
            a.movaps(xmm3, xmm1); a.shufps(xmm3, xmm3, 0x00uy); a.blendps(xmm3, xmm0, 1uy)   // (Ax, Zx, Zx)
            if neg then a.xorps(xmm3, __xmmword_ptr.[lS0])                                    // lane0: -Ax
            a.addps(xmm2, xmm3)
            a.movaps(xmm3, xmm1); a.shufps(xmm3, xmm3, 0xAAuy); a.blendps(xmm3, xmm0, 4uy)   // (Zz, Zz, Az)
            if neg then a.xorps(xmm3, __xmmword_ptr.[lS2])                                    // lane2: -Az
            a.addps(xmm2, xmm3)                                                               // D
            // f = (D > 0, ordered) ? 1.0 : -1.0      (stock: comiss dot,0 ; ja)
            a.xorps(xmm3, xmm3)
            a.cmpltps(xmm3, xmm2)
            a.movaps(xmm2, xmm3)
            a.andps(xmm3, __xmmword_ptr.[lOne])
            a.andnps(xmm2, __xmmword_ptr.[lMone])
            a.orps(xmm3, xmm2)
            // S = f*e ; T = S*A ; P = T + P   (same operand order as stock mulss/mulss/addss)
            a.movss(xmm2, __dword_ptr.[rcx + eOff]); a.shufps(xmm2, xmm2, 0uy)
            a.mulps(xmm3, xmm2)
            a.mulps(xmm3, xmm0)
            a.addps(xmm3, P)
            a.movaps(P, xmm3)
    a.movq(__qword_ptr.[rdx], xmm4)
    a.extractps(__dword_ptr.[rdx + 8L], xmm4, 2uy)
    a.movq(__qword_ptr.[rdx + 0x10L], xmm5)
    a.extractps(__dword_ptr.[rdx + 0x18L], xmm5, 2uy)
    a.ret()
    a.Label(&fallback)                              // stock path: displaced instruction, then original body
    a.mov(__qword_ptr.[rsp + 8L], rbx)
    a.jmp(resume)
    use ms = new MemoryStream()
    a.Assemble(StreamCodeWriter(ms), cave) |> ignore
    let b = ms.ToArray()
    if b.Length > CAVE_SIZE then failwith "cave too big"
    b

let printDis (bytes: byte[]) (ip: uint64) =
    let fmt = NasmFormatter()
    let so = StringOutput()
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- ip
    while dec.IP < ip + uint64 bytes.Length do
        let ins = dec.Decode()
        fmt.Format(&ins, so)
        printfn "    %X  %s" ins.IP (so.ToStringAndReset())

// ================================================================== offline
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type AabbFn = delegate of nativeint * nativeint -> unit

let offline () =
    let mutable ok = true
    let check name c = printfn "  [%s] %s" (if c then "ok" else "FAIL") name; ok <- ok && c
    printfn "== site checks (dump)"
    check "6EA50 bytes = 48 89 5C 24 08 (mov [rsp+8],rbx)" (img.[SITE .. SITE + 4] = SITE_ORIG)
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, SITE, 16))
    dec.IP <- uint64 SITE
    let i0 = dec.Decode()
    check "first instruction is exactly 5 bytes, next starts at 6EA55" (i0.Length = 5 && dec.IP = uint64 RESUME)
    // .pdata: 6EA50 is a function start and the 5 patched bytes are inside the prologue
    let u32 o = BitConverter.ToUInt32(img, o)
    let pe = BitConverter.ToInt32(img, 0x3C)
    let dd = pe + 24 + 112
    let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
    let n = excSz / 12
    let pB = Array.init n (fun i -> u32 (excRva + i * 12))
    let pE = Array.init n (fun i -> u32 (excRva + i * 12 + 4))
    check "6EA50 is a .pdata function start" (Array.contains (uint32 SITE) pB)
    // branch-target sweep over every .pdata range: no direct branch may land in 6EA51..6EA54
    printfn "== branch-target sweep (all .pdata ranges)"
    let mutable hits = 0
    let mutable entries = 0
    let mutable ins = Instruction()
    for i in 0 .. n - 1 do
        let s, e = pB.[i], pE.[i]
        if e > s && int e <= img.Length then
            let d = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
            d.IP <- uint64 s
            while d.IP < uint64 e do
                d.Decode(&ins)
                if not ins.IsInvalid && (ins.Op0Kind = OpKind.NearBranch64) then
                    let t = ins.NearBranchTarget
                    if t = uint64 SITE then entries <- entries + 1
                    if t > uint64 SITE && t < uint64 RESUME then
                        hits <- hits + 1
                        printfn "  !! %X -> %X" ins.IP t
    printfn "  direct branches/calls to 6EA50: %d" entries
    check "no branch into the middle of the 5 patched bytes" (hits = 0)
    // native equivalence: original code + callees + constants at their RVAs inside one private region
    printfn "== native equivalence (original code copied from the dump vs cave), own process only"
    let REGION = 0x3BD5000
    let region = VirtualAlloc(0n, unativeint REGION, 0x3000u, 0x40u)
    if region = 0n then failwith "VirtualAlloc region failed"
    let copy (lo: int) (hi: int) = Marshal.Copy(img, lo, region + nativeint lo, hi - lo)
    copy 0x6EA50 0x6F0A0            // 6EA50 body + cold stubs, 6EFD0 add, 6F010 scale, 6F060 dot
    copy 0x167B30 0x167B50          // vec4 set (w = 0)
    copy 0x3BD41C0 0x3BD4200        // constants 3BD41C0 sign mask, 3BD41D8 -1.0, 3BD41E0 1.0
    // verify every rel32 call/jmp and RIP-relative operand of the copied 6EA50 resolves inside the copied ranges
    let inCopied (t: uint64) = (t >= 0x6EA50UL && t < 0x6F0A0UL) || (t >= 0x167B30UL && t < 0x167B50UL) || (t >= 0x3BD41C0UL && t < 0x3BD4200UL)
    let d2 = Decoder.Create(64, ByteArrayCodeReader(img, SITE, FN_END - SITE))
    d2.IP <- uint64 SITE
    let mutable refsOk = true
    while d2.IP < uint64 FN_END do
        d2.Decode(&ins)
        if ins.Op0Kind = OpKind.NearBranch64 && not (inCopied ins.NearBranchTarget) then refsOk <- false; printfn "  !! %X branch out %X" ins.IP ins.NearBranchTarget
        if ins.IsIPRelativeMemoryOperand && not (inCopied ins.IPRelativeMemoryAddress) then refsOk <- false; printfn "  !! %X rip-rel out %X" ins.IP ins.IPRelativeMemoryAddress
    check "copied original only references copied code/constants" refsOk
    let cave = VirtualAlloc(0n, unativeint 0x1000, 0x3000u, 0x40u)
    let caveBytes = buildCave (uint64 cave) (uint64 region + uint64 RESUME)
    Marshal.Copy(caveBytes, 0, cave, caveBytes.Length)
    printfn "  cave %d bytes (code %d)" caveBytes.Length (caveBytes.Length - OFF_CODE)
    let fOrig = Marshal.GetDelegateForFunctionPointer<AabbFn>(region + nativeint SITE)
    let fCave = Marshal.GetDelegateForFunctionPointer<AabbFn>(cave + nativeint OFF_CODE)
    let buf = Marshal.AllocHGlobal(0x200)
    let obb, outA, outB = buf, buf + 0x80n, buf + 0x100n
    let rnd = Random(20261006)
    let bits (f: float32) = BitConverter.SingleToInt32Bits f
    let special = [| 0.0f; -0.0f; 1.0f; -1.0f; 1e-40f; -1e-40f; 1e-38f; 3.0e38f; -3.0e38f; 0.5f |]
    let rf (kind: int) =
        match kind with
        | 0 -> float32 (rnd.NextDouble() * 2.0 - 1.0)
        | 1 -> float32 ((rnd.NextDouble() * 2.0 - 1.0) * 2000.0)
        | 2 -> special.[rnd.Next special.Length]
        | 3 -> BitConverter.Int32BitsToSingle(rnd.Next() &&& 0x7F7FFFFF ||| (if rnd.Next 2 = 0 then 0 else Int32.MinValue))   // any finite
        | _ -> if rnd.Next 3 = 0 then 0.0f else float32 (rnd.NextDouble() * 2.0 - 1.0)
    let fillObb () =
        let kind = rnd.Next 6
        for j in 0 .. 18 do
            let v =
                match kind with
                | 5 ->   // proper rotation matrix rows, centre and extents like the game's
                    0.0f
                | k -> rf (if k = 4 then 4 elif rnd.Next 8 = 0 then 2 else k)
            Marshal.WriteInt32(obb + nativeint (4 * j), bits v)
        if kind = 5 then
            let q = Array.init 4 (fun _ -> rnd.NextDouble() * 2.0 - 1.0)
            let l = sqrt (q |> Array.sumBy (fun x -> x * x))
            let x, y, z, w = q.[0] / l, q.[1] / l, q.[2] / l, q.[3] / l
            let m = [| 1.0 - 2.0*(y*y+z*z); 2.0*(x*y+w*z); 2.0*(x*z-w*y)
                       2.0*(x*y-w*z); 1.0 - 2.0*(x*x+z*z); 2.0*(y*z+w*x)
                       2.0*(x*z+w*y); 2.0*(y*z-w*x); 1.0 - 2.0*(x*x+y*y) |]
            for r in 0 .. 2 do
                for c in 0 .. 2 do Marshal.WriteInt32(obb + nativeint (0x10 + 0x10 * r + 4 * c), bits (float32 m.[3 * r + c]))
                Marshal.WriteInt32(obb + nativeint (0x1C + 0x10 * r), 0)
            for c in 0 .. 2 do Marshal.WriteInt32(obb + nativeint (4 * c), bits (float32 ((rnd.NextDouble() - 0.5) * 20000.0)))
            for c in 0 .. 2 do Marshal.WriteInt32(obb + nativeint (0x40 + 4 * c), bits (float32 (rnd.NextDouble() * 50.0)))
    let mutable tests, bad = 0, 0
    let run (o: nativeint) (oa: nativeint) (ob: nativeint) =
        for j in 0 .. 7 do Marshal.WriteInt32(oa + nativeint (4 * j), 0x7F81234 + j); Marshal.WriteInt32(ob + nativeint (4 * j), 0x7F81234 + j)
        fOrig.Invoke(o, oa)
        fCave.Invoke(o, ob)
        tests <- tests + 1
        let mutable same = true
        for j in 0 .. 7 do if Marshal.ReadInt32(oa + nativeint (4 * j)) <> Marshal.ReadInt32(ob + nativeint (4 * j)) then same <- false
        if not same then
            bad <- bad + 1
            if bad <= 8 then
                printfn "  MISMATCH:"
                printfn "    in  %s" (String.Join(" ", [| for j in 0 .. 18 -> sprintf "%08X" (Marshal.ReadInt32(o + nativeint (4 * j))) |]))
                printfn "    org %s" (String.Join(" ", [| for j in 0 .. 7 -> sprintf "%08X" (Marshal.ReadInt32(oa + nativeint (4 * j))) |]))
                printfn "    new %s" (String.Join(" ", [| for j in 0 .. 7 -> sprintf "%08X" (Marshal.ReadInt32(ob + nativeint (4 * j))) |]))
    for _ in 1 .. 300000 do
        fillObb ()
        run obb outA outB
    printfn "  finite inputs: %d tests, %d bitwise mismatches (all 8 output dwords, incl. untouched [3]/[7])" tests bad
    check "bit-identical on finite inputs" (bad = 0)
    // aliasing: out inside the obb -> the cave must take the stock path; compare against stock on a separate copy
    let mutable aliasBad = 0
    let obb2 = buf + 0x180n
    for k in 0 .. 2000 do
        fillObb ()
        let off = nativeint (4 * (k % 13))
        let raw = Array.init 0x4C (fun j -> Marshal.ReadByte(obb + nativeint j))
        Marshal.Copy(raw, 0, obb2, raw.Length)
        fOrig.Invoke(obb, obb + off)
        fCave.Invoke(obb2, obb2 + off)
        for j in 0 .. 0x4B do if Marshal.ReadByte(obb + nativeint j) <> Marshal.ReadByte(obb2 + nativeint j) then aliasBad <- aliasBad + 1
    check (sprintf "aliasing (out overlaps obb) falls back to the stock code: 2001 cases, %d byte diffs" aliasBad) (aliasBad = 0)
    // switch off -> stock path
    Marshal.WriteInt32(cave, 0)
    let tb = tests
    for _ in 1 .. 2000 do fillObb (); run obb outA outB
    check "switch off = stock path" (bad = 0 && tests = tb + 2000)
    Marshal.WriteInt32(cave, 1)
    // non-finite inputs (reported, not required)
    let b0, t0 = bad, tests
    for _ in 1 .. 20000 do
        fillObb ()
        let j = rnd.Next 19
        let v = [| Single.NaN; Single.PositiveInfinity; Single.NegativeInfinity |].[rnd.Next 3]
        Marshal.WriteInt32(obb + nativeint (4 * j), bits v)
        run obb outA outB
    printfn "  non-finite inputs (info only): %d tests, %d differ bitwise (NaN payload/sign only expected)" (tests - t0) (bad - b0)
    bad <- b0
    // timing
    printfn "== timing (delegate call overhead included in both)"
    fillObb ()
    let time (f: AabbFn) =
        for _ in 1 .. 20000 do f.Invoke(obb, outA)
        let sw = Stopwatch.StartNew()
        for _ in 1 .. 2000000 do f.Invoke(obb, outA)
        sw.Elapsed.TotalMilliseconds * 1e6 / 2e6
    let empty = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    Marshal.WriteByte(empty, 0xC3uy)
    let fEmpty = Marshal.GetDelegateForFunctionPointer<AabbFn>(empty)
    let te = time fEmpty
    let to1 = time fOrig
    let tc = time fCave
    printfn "  empty %.1f ns, stock %.1f ns, cave %.1f ns  => stock body %.1f ns, cave body %.1f ns (%.0f%% less)" te to1 tc (to1 - te) (tc - te) (100.0 * (1.0 - (tc - te) / (to1 - te)))
    printfn "== cave disassembly (at its test address)"
    printDis caveBytes.[OFF_CODE ..] (uint64 cave + uint64 OFF_CODE)
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ================================================================== live
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let site = nativeint (B + uint64 SITE)
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let jmpTo (cave: uint64) = jmpPatch site (nativeint (cave + uint64 OFF_CODE)) 5
    let siteState () =
        let cur = t.Read(site, 5)
        if cur = SITE_ORIG then "original"
        else
            match readState () with
            | Some c when cur = jmpTo c -> "patched"
            | _ -> sprintf "UNKNOWN (%s)" (BitConverter.ToString cur)
    let bodyMatches () =
        let live = t.Read(nativeint (B + uint64 RESUME), FN_END - RESUME)
        live = img.[RESUME .. FN_END - 1]
    match mode with
    | "dry" ->
        printfn "base=%X  site %X: %s  body 6EA55..6EFCB matches dump: %b" B site (siteState ()) (bodyMatches ())
        let prev = buildCave (B + 0x5E00000UL) (B + uint64 RESUME)
        printfn "cave preview (%d bytes) at a dummy address:" prev.Length
        printDis prev.[OFF_CODE ..] (B + 0x5E00000UL + uint64 OFF_CODE)
    | "apply" ->
        writeGuard ()
        if siteState () <> "original" then failwithf "site is %s - aborting (revert first)" (siteState ())
        if not (bodyMatches ()) then failwith "6EA50 body differs from the dump - wrong game version, aborting"
        let cave =
            match readState () with
            | Some c -> c
            | None ->
                let mem = t.AllocNear(site, 0x1000)
                let c = uint64 mem
                t.Write(mem, buildCave c (B + uint64 RESUME))
                File.WriteAllText(stateFile, sprintf "%X %X" c t.Process.Id)
                c
        let n = safeWrite t [ (site, jmpTo cave, SITE_ORIG) ]
        printfn "AABB applied on attempt %d; cave %X; switch=%d" n cave (BitConverter.ToInt32(t.Read(nativeint cave, 4), 0))
    | "revert" ->
        writeGuard ()
        match readState () with
        | Some c when siteState () = "patched" ->
            let n = safeWrite t [ (site, SITE_ORIG, jmpTo c) ]
            printfn "AABB reverted on attempt %d (cave left allocated)" n
        | _ -> printfn "site is %s - nothing to revert" (siteState ())
    | "on" | "off" | "count" ->
        writeGuard ()
        match readState () with
        | Some c when siteState () = "patched" ->
            let v = match mode with "on" -> 1u | "off" -> 0u | _ -> 3u
            t.Write(nativeint c, BitConverter.GetBytes v)
            printfn "AABB switch = %d" v
        | _ -> printfn "AABB is not applied in this process - use apply"
    | "status" ->
        printfn "site %X: %s" site (siteState ())
        match readState () with
        | None -> printfn "no cave in this process"
        | Some c ->
            let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
            printfn "switch=%d (1=on, 3=on+count, 0=stock)" (rd c)
            let mutable p = rd (c + uint64 OFF_CNT)
            for _ in 1 .. 5 do
                Threading.Thread.Sleep 200
                let q = rd (c + uint64 OFF_CNT)
                printfn "  cave calls/s %8d  (counts only with switch=3)" ((q - p) * 5u)
                p <- q
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
