// mq_live.fsx - candidate MQ from notes\streaming-anim.md: bit-exact SSE MatrixFromScaleQuatTrans (Nioh3+88780).
//
//   dotnet fsi mq_live.fsx [offline] [N]        NO game access: site/byte/.pdata checks, branch-target sweep, native
//                                               equivalence of the cave vs the original bytes copied from the dump
//                                               (N random cases per MXCSR mode, default 1,000,000), timing.
//   dotnet fsi mq_live.fsx dry                  READ-ONLY attach: live bytes vs dump, site state, cave preview.
//   dotnet fsi mq_live.fsx status               READ-ONLY: site state, switch, call counter (if counting is on).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi mq_live.fsx apply     WRITES: allocates the cave (once per process), jmp at 88780.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi mq_live.fsx off|on|count   WRITES 1 dword in the cave only (A/B without code writes).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi mq_live.fsx revert    WRITES the original 10 bytes back (cave stays allocated).
// Nothing is applied unless you run apply (default off).
//
// 88780 = float* MatrixFromScaleQuatTrans(float out[16] /*rcx*/, const float s[3] /*rdx*/, const float q[4] /*r8*/,
//                                          const float t[4] /*r9, movaps*/), returns out. 238 callers; ~1.7 % of each
//   KIDSTaskScheduler thread on the PC (SkelBuildBoneMatrices CC940 per bone).
//   Stock: 25 scalar SSE ops + 8 xmm6-13 spills/reloads (it keeps every product in its own register):
//     x2=x+x y2=y+y z2=z+z; products y2*y y2*w y2*z z2*w x2*x x2*w z2*z x2*y z2*x (dest first);
//     m0=((1-y2y)-z2z)*s0 m5=((1-z2z)-x2x)*s1 m10=((1-x2x)-y2y)*s2
//     m1=(x2y+z2w)*s0 m4=(x2y-z2w)*s1 m6=(y2z+x2w)*s1 m9=(y2z-x2w)*s2 m8=(z2x+y2w)*s2 m2=(z2x-y2w)*s0
//     m3=m7=m11=0, m12..14=t.xyz, m15=1.0.
//   Cave: the same IEEE ops with the same operand order (dest/src kept, so NaN payload selection is identical), 3 lanes
//   at once; lane 3 of every vector op repeats lane 0's operands, so even the MXCSR status flags are identical. No FMA,
//   SSE4.1 (insertps). Reads all inputs before the first store; falls back to the stock code when out overlaps s
//   (the stock code re-reads s between its stores).
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
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "mq_live_state.txt")
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"

let SITE = 0x88780
let SITE_ORIG = [| 0x4Cuy; 0x8Buy; 0xDCuy; 0x48uy; 0x81uy; 0xECuy; 0x88uy; 0x00uy; 0x00uy; 0x00uy |] // mov r11,rsp ; sub rsp,88h
let RESUME = 0x8878A
let FN_END = 0x88938
let OFF_EN, OFF_CNT, OFF_DATA, OFF_CODE = 0x00, 0x04, 0x40, 0x100
let CAVE_SIZE = 0x400

/// Whole cave image for address `cave`; `resume` = absolute address of the original 8878A.
let buildCave (cave: uint64) (resume: uint64) : byte[] =
    let a = Assembler(64)
    let mutable lEn = a.CreateLabel()
    let mutable lCnt = a.CreateLabel()
    let mutable lOne = a.CreateLabel()
    let mutable fallback = a.CreateLabel()
    let mutable noAlias = a.CreateLabel()
    let mutable noCount = a.CreateLabel()
    a.Label(&lEn); a.dd(1u)
    a.Label(&lCnt); a.dd(0u)
    for _ in 1 .. (OFF_DATA - 8) / 4 do a.dd(0u)
    a.Label(&lOne); a.dd(0x3F800000u); a.dd(0x3F800000u); a.dd(0x3F800000u); a.dd(0x3F800000u)   // @+40h, 16-aligned
    for _ in 1 .. (OFF_CODE - OFF_DATA - 0x10) / 4 do a.dd(0xCCCCCCCCu)
    // code @+100h
    a.test(__byte_ptr.[lEn], 1u)
    a.jz(fallback)
    a.test(__byte_ptr.[lEn], 2u)
    a.jz(noCount)
    a.inc(__dword_ptr.[lCnt])                    // plain inc, status only
    a.Label(&noCount)
    // out [rcx, rcx+40h) must not overlap s [rdx, rdx+0Ch)
    a.lea(rax, __[rcx + 0x40L])
    a.cmp(rdx, rax)
    a.jae(noAlias)
    a.lea(rax, __[rdx + 0x0CL])
    a.cmp(rcx, rax)
    a.jb(fallback)
    a.Label(&noAlias)
    a.movups(xmm0, __xmmword_ptr.[r8])           // (x,y,z,w)
    a.movaps(xmm5, xmm0); a.shufps(xmm5, xmm5, 0xFFuy)           // wb = (w,w,w,w)
    a.shufps(xmm0, xmm0, 0x24uy)                                 // q4 = (x,y,z,x)
    a.movaps(xmm1, xmm0); a.addps(xmm1, xmm0)                    // q2 = (x2,y2,z2,x2)
    a.movaps(xmm2, xmm1); a.mulps(xmm2, xmm0)                    // D  = (x2*x, y2*y, z2*z, x2*x)
    a.movaps(xmm3, xmm0); a.shufps(xmm3, xmm3, 0x49uy)           // (y,z,x,y)
    a.movaps(xmm4, xmm1); a.mulps(xmm4, xmm3)                    // A  = (x2*y, y2*z, z2*x, x2*y)
    a.mulps(xmm1, xmm5)                                          // W  = (x2*w, y2*w, z2*w, x2*w)
    a.shufps(xmm1, xmm1, 0x92uy)                                 // Ws = (z2w, x2w, y2w, z2w)
    a.movaps(xmm5, xmm4); a.addps(xmm5, xmm1)                    // P  = A + Ws = (m1, m6, m8, m1) unscaled
    a.subps(xmm4, xmm1)                                          // M  = A - Ws = (m4, m9, m2, m4) unscaled
    a.movaps(xmm0, __xmmword_ptr.[lOne])
    a.movaps(xmm3, xmm2); a.shufps(xmm3, xmm3, 0x49uy)           // (y2y, z2z, x2x, y2y)
    a.subps(xmm0, xmm3)
    a.shufps(xmm2, xmm2, 0x92uy)                                 // (z2z, x2x, y2y, z2z)
    a.subps(xmm0, xmm2)                                          // G = (m0, m5, m10, m0) unscaled
    a.movq(xmm1, __qword_ptr.[rdx])
    a.insertps(xmm1, __dword_ptr.[rdx + 8L], 0x20uy)             // (s0,s1,s2,0) - reads exactly 12 bytes
    a.shufps(xmm1, xmm1, 0x24uy)                                 // S = (s0,s1,s2,s0)
    a.mulps(xmm0, xmm1)                                          // (m0*s0, m5*s1, m10*s2)
    a.mulps(xmm5, xmm1)                                          // (m1*s0, m6*s1, m8*s2)
    a.shufps(xmm1, xmm1, 0x49uy)                                 // (s1,s2,s0,s1)
    a.mulps(xmm4, xmm1)                                          // (m4*s1, m9*s2, m2*s0)
    a.movaps(xmm2, xmm0); a.insertps(xmm2, xmm5, 0x18uy); a.insertps(xmm2, xmm4, 0xA8uy)   // row0 m0 m1 m2 0
    a.movaps(xmm3, xmm4); a.insertps(xmm3, xmm0, 0x58uy); a.insertps(xmm3, xmm5, 0x68uy)   // row1 m4 m5 m6 0
    a.insertps(xmm0, xmm5, 0x88uy); a.insertps(xmm0, xmm4, 0x58uy)                         // row2 m8 m9 m10 0
    a.movups(xmm1, __xmmword_ptr.[r9])
    a.insertps(xmm1, __dword_ptr.[lOne], 0x30uy)                                           // row3 t.x t.y t.z 1
    a.movups(__xmmword_ptr.[rcx], xmm2)
    a.movups(__xmmword_ptr.[rcx + 0x10L], xmm3)
    a.movups(__xmmword_ptr.[rcx + 0x20L], xmm0)
    a.movups(__xmmword_ptr.[rcx + 0x30L], xmm1)
    a.mov(rax, rcx)
    a.ret()
    a.Label(&fallback)                            // stock path: displaced prologue, then the original body
    a.mov(r11, rsp)
    a.sub(rsp, 0x88)
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
type MqFn = delegate of nativeint * nativeint * nativeint * nativeint -> nativeint

/// Test wrapper: loads MXCSR from [page+0], calls `target`, stores the resulting MXCSR to [page+4], restores MXCSR.
let buildWrapper (at: uint64) (page: uint64) (target: uint64) =
    assemble (nativeint at) (fun a ->
        a.sub(rsp, 0x38)
        a.stmxcsr(__dword_ptr.[rsp + 0x20L])
        a.mov(rax, page)
        a.ldmxcsr(__dword_ptr.[rax])
        a.mov(rax, target)
        a.call(rax)
        a.mov(r10, page)
        a.stmxcsr(__dword_ptr.[r10 + 4L])
        a.ldmxcsr(__dword_ptr.[rsp + 0x20L])
        a.add(rsp, 0x38)
        a.ret())

let offline () =
    let nRand = if args.Length > 1 then int args.[1] else 1000000
    let mutable ok = true
    let check name c = printfn "  [%s] %s" (if c then "ok" else "FAIL") name; ok <- ok && c
    printfn "== site checks (dump)"
    check "88780 bytes = 4C 8B DC 48 81 EC 88 00 00 00 (mov r11,rsp; sub rsp,88h)" (img.[SITE .. SITE + 9] = SITE_ORIG)
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, SITE, 16))
    dec.IP <- uint64 SITE
    let i0 = dec.Decode()
    let i1 = dec.Decode()
    check "two whole instructions (3 + 7 bytes), next starts at 8878A" (i0.Length = 3 && i1.Length = 7 && dec.IP = uint64 RESUME)
    let u32 o = BitConverter.ToUInt32(img, o)
    let pe = BitConverter.ToInt32(img, 0x3C)
    let dd = pe + 24 + 112
    let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
    let n = excSz / 12
    let pB = Array.init n (fun i -> u32 (excRva + i * 12))
    let pE = Array.init n (fun i -> u32 (excRva + i * 12 + 4))
    let idx = Array.IndexOf(pB, uint32 SITE)
    check "88780 is a .pdata function start, end 88938" (idx >= 0 && pE.[idx] = uint32 FN_END)
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
    printfn "  direct branches/calls to 88780: %d" entries
    check "no branch into 88781..88789" (hits = 0)
    // the original body must not use anything outside the copied ranges
    let d2 = Decoder.Create(64, ByteArrayCodeReader(img, SITE, FN_END - SITE))
    d2.IP <- uint64 SITE
    let mutable refsOk = true
    let mutable fma = false
    while d2.IP < uint64 FN_END do
        d2.Decode(&ins)
        let m = ins.Mnemonic.ToString().ToLowerInvariant()
        if m.StartsWith "vfm" || m.StartsWith "vfnm" then fma <- true
        if ins.Op0Kind = OpKind.NearBranch64 then refsOk <- false; printfn "  !! %X branch %X" ins.IP ins.NearBranchTarget
        if ins.IsIPRelativeMemoryOperand && ins.IPRelativeMemoryAddress <> 0x3BD41E0UL then refsOk <- false; printfn "  !! %X rip-rel %X" ins.IP ins.IPRelativeMemoryAddress
    check "original body: no branches/calls, only rip-rel operand is 3BD41E0 (1.0f)" refsOk
    check "original body has no FMA (cave has none either)" (not fma)
    check "3BD41E0 = 1.0f" (u32 0x3BD41E0 = 0x3F800000u)

    printfn "== native equivalence (original bytes from the dump vs cave), own process only"
    let REGION = 0x3BD5000
    let region = VirtualAlloc(0n, unativeint REGION, 0x3000u, 0x40u)
    if region = 0n then failwith "VirtualAlloc region failed"
    Marshal.Copy(img, SITE, region + nativeint SITE, FN_END - SITE)
    Marshal.Copy(img, 0x3BD41C0, region + 0x3BD41C0n, 0x40)
    let cave = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    let caveBytes = buildCave (uint64 cave) (uint64 region + uint64 RESUME)
    Marshal.Copy(caveBytes, 0, cave, caveBytes.Length)
    printfn "  cave %d bytes (code %d)" caveBytes.Length (caveBytes.Length - OFF_CODE)
    let wpage = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    let wA = buildWrapper (uint64 wpage + 0x100UL) (uint64 wpage) (uint64 region + uint64 SITE)
    let wB = buildWrapper (uint64 wpage + 0x200UL) (uint64 wpage + 8UL) (uint64 cave + uint64 OFF_CODE)
    Marshal.Copy(wA, 0, wpage + 0x100n, wA.Length)
    Marshal.Copy(wB, 0, wpage + 0x200n, wB.Length)
    let fOrig = Marshal.GetDelegateForFunctionPointer<MqFn>(wpage + 0x100n)
    let fCave = Marshal.GetDelegateForFunctionPointer<MqFn>(wpage + 0x200n)
    let setMx (v: int) = Marshal.WriteInt32(wpage, v); Marshal.WriteInt32(wpage + 8n, v)
    // buffers: inputs 16-aligned like the game (t is read with movaps)
    let buf = Marshal.AllocHGlobal(0x600)
    let bas = nativeint ((int64 buf + 15L) &&& ~~~15L)
    let pS, pQ, pT, outA, outB = bas, bas + 0x10n, bas + 0x20n, bas + 0x40n, bas + 0x80n
    let rnd = Random(20261008)
    let special =
        [| 0x00000000; 0x80000000; 0x3F800000; 0xBF800000; 0x00000001; 0x80000001; 0x007FFFFF; 0x807FFFFF; 0x00800000
           0x80800000; 0x7F7FFFFF; 0xFF7FFFFF; 0x7F800000; 0xFF800000; 0x7FC00000; 0xFFC00000; 0x7F800001; 0xFF812345
           0x7FC12345; 0x3F000000; 0x40000000; 0x5F800000; 0x1F800000; 0x20000000; 0x3F3504F3; 0x33800000; 0x4B800000 |]
    let bits (f: float32) = BitConverter.SingleToInt32Bits f
    let rbits () = (rnd.Next() <<< 1) ^^^ rnd.Next()          // any 32-bit pattern (NaN/inf/denormal/-0 included)
    let rf kind =
        match kind with
        | 0 -> rbits ()
        | 1 -> special.[rnd.Next special.Length]
        | 2 -> bits (float32 (rnd.NextDouble() * 2.0 - 1.0))
        | 3 -> bits (float32 ((rnd.NextDouble() - 0.5) * 2e4))
        | _ -> rnd.Next() &&& 0x80FFFFFF                        // denormal / tiny exponents
    let fill () =
        let kind = rnd.Next 8
        if kind = 7 then   // realistic: unit quaternion, scale ~1, translation in metres
            let q = Array.init 4 (fun _ -> rnd.NextDouble() * 2.0 - 1.0)
            let l = sqrt (q |> Array.sumBy (fun x -> x * x))
            for j in 0 .. 3 do Marshal.WriteInt32(pQ + nativeint (4 * j), bits (float32 (q.[j] / l)))
            for j in 0 .. 2 do Marshal.WriteInt32(pS + nativeint (4 * j), bits (if rnd.Next 3 = 0 then 1.0f else float32 (rnd.NextDouble() * 2.0)))
            for j in 0 .. 3 do Marshal.WriteInt32(pT + nativeint (4 * j), bits (float32 ((rnd.NextDouble() - 0.5) * 200.0)))
        else
            let pick () = if kind = 6 then rf (rnd.Next 5) else rf (kind % 5)
            for j in 0 .. 3 do Marshal.WriteInt32(pQ + nativeint (4 * j), pick ())
            for j in 0 .. 2 do Marshal.WriteInt32(pS + nativeint (4 * j), pick ())
            for j in 0 .. 3 do Marshal.WriteInt32(pT + nativeint (4 * j), pick ())
        Marshal.WriteInt32(pS + 12n, rbits ())      // never read by either
    let mutable tests, bad, flagBad = 0L, 0L, 0L
    let run (o1: nativeint) (o2: nativeint) (s: nativeint) (q: nativeint) (t: nativeint) =
        for j in 0 .. 15 do Marshal.WriteInt32(outA + nativeint (4 * j), 0x7F81234 + j); Marshal.WriteInt32(outB + nativeint (4 * j), 0x7F81234 + j)
        let ra = fOrig.Invoke(o1, s, q, t)
        let rb = fCave.Invoke(o2, s, q, t)
        tests <- tests + 1L
        let mutable same = (ra = o1 && rb = o2)
        for j in 0 .. 15 do if Marshal.ReadInt32(outA + nativeint (4 * j)) <> Marshal.ReadInt32(outB + nativeint (4 * j)) then same <- false
        if Marshal.ReadInt32(wpage + 4n) <> Marshal.ReadInt32(wpage + 12n) then flagBad <- flagBad + 1L
        if not same then
            bad <- bad + 1L
            if bad <= 6L then
                let hx (p: nativeint) k = String.Join(" ", [| for j in 0 .. k - 1 -> sprintf "%08X" (Marshal.ReadInt32(p + nativeint (4 * j))) |])
                printfn "  MISMATCH: s %s | q %s | t %s" (hx s 3) (hx q 4) (hx t 4)
                printfn "    org %s" (hx outA 16)
                printfn "    new %s" (hx outB 16)
    // MXCSR: all exceptions masked + flags clear; default RN, FTZ+DAZ, and the three other rounding modes
    let mxModes = [ "RN", 0x1F80; "RN+FTZ+DAZ", 0x9FC0; "RD", 0x3F80; "RU", 0x5F80; "RZ", 0x7F80 ]
    for (name, mx) in mxModes do
        setMx mx
        let t0, b0, f0 = tests, bad, flagBad
        let cnt = if name.StartsWith "RN" then nRand else nRand / 4
        for _ in 1 .. cnt do
            fill ()
            run outA outB pS pQ pT
        // exhaustive-ish special grid: every special in every single input lane, others random-unit
        for v in special do
            for lane in 0 .. 10 do
                fill ()
                let p = if lane < 4 then pQ + nativeint (4 * lane) elif lane < 7 then pS + nativeint (4 * (lane - 4)) else pT + nativeint (4 * (lane - 7))
                Marshal.WriteInt32(p, v)
                run outA outB pS pQ pT
        // all-special pairs in the quaternion (x,w), (y,z) etc.
        for v1 in special do
            for v2 in special do
                fill ()
                Marshal.WriteInt32(pQ + nativeint (4 * rnd.Next 4), v1)
                Marshal.WriteInt32(pQ + nativeint (4 * rnd.Next 4), v2)
                run outA outB pS pQ pT
        printfn "  MXCSR %-11s %9d tests, %d bitwise mismatches (16 dwords + return value), %d MXCSR status-flag mismatches" name (tests - t0) (bad - b0) (flagBad - f0)
    check (sprintf "bit-identical on %d inputs (random bits, specials, NaN/inf/denormal/-0, 5 MXCSR modes)" tests) (bad = 0L)
    check "MXCSR status flags identical after every call" (flagBad = 0L)
    setMx 0x1F80
    // aliasing: out overlapping s (fallback path), q or t (cave path; all reads happen before the stores in both)
    let mutable aliasBad = 0
    let mutable aliasN = 0
    let arena = bas + 0x100n
    let arenaB = bas + 0x280n
    for k in 0 .. 3000 do
        fill ()
        let raw = Array.init 0x180 (fun _ -> byte (rnd.Next 256))
        // place s/q/t at fixed spots in the arena, out at an offset that overlaps one of them
        let sOff, qOff, tOff = 0x40, 0x80, 0xC0
        for j in 0 .. 11 do raw.[sOff + j] <- Marshal.ReadByte(pS + nativeint j)
        for j in 0 .. 15 do raw.[qOff + j] <- Marshal.ReadByte(pQ + nativeint j); raw.[tOff + j] <- Marshal.ReadByte(pT + nativeint j)
        let target = [| sOff; qOff; tOff |].[k % 3]
        let oOff = target - 0x3C + 4 * (rnd.Next 0x1A)           // out (64 B) overlaps the chosen input
        Marshal.Copy(raw, 0, arena, raw.Length)
        Marshal.Copy(raw, 0, arenaB, raw.Length)
        fOrig.Invoke(arena + nativeint oOff, arena + nativeint sOff, arena + nativeint qOff, arena + nativeint tOff) |> ignore
        fCave.Invoke(arenaB + nativeint oOff, arenaB + nativeint sOff, arenaB + nativeint qOff, arenaB + nativeint tOff) |> ignore
        aliasN <- aliasN + 1
        for j in 0 .. raw.Length - 1 do if Marshal.ReadByte(arena + nativeint j) <> Marshal.ReadByte(arenaB + nativeint j) then aliasBad <- aliasBad + 1
    check (sprintf "aliasing (out overlaps s / q / t): %d cases, %d byte diffs" aliasN aliasBad) (aliasBad = 0)
    // switch off -> stock path
    Marshal.WriteInt32(cave, 0)
    let tb, bb = tests, bad
    for _ in 1 .. 2000 do fill (); run outA outB pS pQ pT
    check "switch off = stock path" (bad = bb && tests = tb + 2000L)
    Marshal.WriteInt32(cave, 3)
    for _ in 1 .. 1000 do fill (); run outA outB pS pQ pT
    check "switch 3 (on + count): counter = 1000, results identical" (Marshal.ReadInt32(cave + 4n) = 1000 && bad = bb)
    Marshal.WriteInt32(cave, 1)
    printfn "== timing: native loop over 256 bones (like SkelBuildBoneMatrices), 4000 passes, best of 5"
    let bones = VirtualAlloc(0n, 0x10000un, 0x3000u, 0x04u)
    let oBase, sBase, qBase, tBase = bones, bones + 0x4000n, bones + 0x5000n, bones + 0x6000n
    for i in 0 .. 255 do
        let q = Array.init 4 (fun _ -> rnd.NextDouble() * 2.0 - 1.0)
        let l = sqrt (q |> Array.sumBy (fun x -> x * x))
        for j in 0 .. 3 do Marshal.WriteInt32(qBase + nativeint (16 * i + 4 * j), bits (float32 (q.[j] / l)))
        for j in 0 .. 2 do Marshal.WriteInt32(sBase + nativeint (16 * i + 4 * j), bits 1.0f)
        for j in 0 .. 3 do Marshal.WriteInt32(tBase + nativeint (16 * i + 4 * j), bits (float32 (rnd.NextDouble() - 0.5)))
    let buildLoop (at: nativeint) (target: uint64) =
        assemble at (fun a ->
            let mutable outer = a.CreateLabel()
            let mutable inner = a.CreateLabel()
            a.push(rbx); a.push(rsi); a.push(rdi); a.sub(rsp, 0x20)
            a.mov(rbx, rcx)
            a.Label(&outer)
            a.xor(esi, esi)
            a.Label(&inner)
            a.mov(rdi, rsi); a.shl(rdi, 6uy)
            a.mov(rcx, uint64 oBase); a.add(rcx, rdi)
            a.mov(rax, rsi); a.shl(rax, 4uy)
            a.mov(rdx, uint64 sBase); a.add(rdx, rax)
            a.mov(r8, uint64 qBase); a.add(r8, rax)
            a.mov(r9, uint64 tBase); a.add(r9, rax)
            a.mov(rax, target); a.call(rax)
            a.inc(esi); a.cmp(esi, 256); a.jb(inner)
            a.dec(rbx); a.jnz(outer)
            a.add(rsp, 0x20); a.pop(rdi); a.pop(rsi); a.pop(rbx); a.ret())
    let empty = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    Marshal.WriteByte(empty, 0xC3uy)
    let loops = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    let mk (off: int) (target: uint64) =
        let b = buildLoop (loops + nativeint off) target
        Marshal.Copy(b, 0, loops + nativeint off, b.Length)
        Marshal.GetDelegateForFunctionPointer<MqFn>(loops + nativeint off)
    let lE, lO, lC = mk 0 (uint64 empty), mk 0x200 (uint64 region + uint64 SITE), mk 0x400 (uint64 cave + uint64 OFF_CODE)
    let time (f: MqFn) =
        f.Invoke(200n, 0n, 0n, 0n) |> ignore
        let sw = Stopwatch.StartNew()
        f.Invoke(4000n, 0n, 0n, 0n) |> ignore
        sw.Elapsed.TotalMilliseconds * 1e6 / (4000.0 * 256.0)
    let mutable best = (1e9, 1e9, 1e9)
    for _ in 1 .. 5 do
        let te, to1, tc = time lE, time lO, time lC
        let (a, b, c) = best
        best <- (min a te, min b to1, min c tc)
    let (te, to1, tc) = best
    printfn "  per call: empty %.2f ns, stock %.2f ns, cave %.2f ns  => stock body %.2f ns, cave body %.2f ns (%.0f%% less)" te to1 tc (to1 - te) (tc - te) (100.0 * (1.0 - (tc - te) / (to1 - te)))
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
    let jmpTo (cave: uint64) = jmpPatch site (nativeint (cave + uint64 OFF_CODE)) 10
    let siteState () =
        let cur = t.Read(site, 10)
        if cur = SITE_ORIG then "original"
        else
            match readState () with
            | Some c when cur = jmpTo c -> "patched"
            | _ -> sprintf "UNKNOWN (%s)" (BitConverter.ToString cur)
    let bodyMatches () = t.Read(nativeint (B + uint64 RESUME), FN_END - RESUME) = img.[RESUME .. FN_END - 1]
    match mode with
    | "dry" ->
        printfn "base=%X  site %X: %s  body 8878A..88938 matches dump: %b" B site (siteState ()) (bodyMatches ())
        let prev = buildCave (B + 0x5E00000UL) (B + uint64 RESUME)
        printfn "cave preview (%d bytes) at a dummy address:" prev.Length
        printDis prev.[OFF_CODE ..] (B + 0x5E00000UL + uint64 OFF_CODE)
    | "apply" ->
        writeGuard ()
        if siteState () <> "original" then failwithf "site is %s - aborting (revert first)" (siteState ())
        if not (bodyMatches ()) then failwith "88780 body differs from the dump - wrong game version, aborting"
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
        printfn "MQ applied on attempt %d; cave %X; switch=%d" n cave (BitConverter.ToInt32(t.Read(nativeint cave, 4), 0))
    | "revert" ->
        writeGuard ()
        match readState () with
        | Some c when siteState () = "patched" ->
            let n = safeWrite t [ (site, SITE_ORIG, jmpTo c) ]
            printfn "MQ reverted on attempt %d (cave left allocated)" n
        | _ -> printfn "site is %s - nothing to revert" (siteState ())
    | "on" | "off" | "count" ->
        writeGuard ()
        match readState () with
        | Some c when siteState () = "patched" ->
            let v = match mode with "on" -> 1u | "off" -> 0u | _ -> 3u
            t.Write(nativeint c, BitConverter.GetBytes v)
            printfn "MQ switch = %d" v
        | _ -> printfn "MQ is not applied in this process - use apply"
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
                printfn "  cave calls/s %9d  (counts only with switch=3; plain inc, approximate)" ((q - p) * 5u)
                p <- q
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
