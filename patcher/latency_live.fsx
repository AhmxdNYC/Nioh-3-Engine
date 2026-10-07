// latency_live.fsx - input-latency agent (notes\input-latency.md). PC / Windows only (attaches to Nioh3.exe).
//
// Two things in one cave, both DEFAULT OFF after `apply`:
//   IK  "late input kick": the [App]InputUpdateWorker pad poll is kicked at the START of the next logic frame
//       (FrameEarlyUpdate 5ACAAA, right before the engine's own pad-stage wait) instead of at the end of the
//       current one (UpdateDispatchB 5AB6CF, i.e. BEFORE the QPC frame limiter). Still exactly one poll per logic
//       frame, so button edges (pressed/released) are computed between consecutive polls as in stock.
//       Expected: input age at consumption drops from ~(limiter slack + FrameSync wait) to the poll time
//       (~0.05-0.3 ms). Rendering is untouched.
//   TS  timestamp probes (measurement only, no behaviour change): QPC of K (pad kick), C (pad consumed by the
//       game update), L (limiter exit), W (render-frame begin, before the waitable wait), B (after the waitable
//       wait), P (present handed to the present thread) into a 4096-entry ring.
//
//   dotnet fsi latency_live.fsx offline         no game needed: check every site against the dump, assemble the
//                                                cave at a dummy address and print it
//   dotnet fsi latency_live.fsx dry             READ-ONLY on the running game: site bytes live vs dump
//   dotnet fsi latency_live.fsx apply           WRITES (FT_ALLOW_WRITE=1): cave + 6 call-site redirects, all
//                                                switches OFF (stock behaviour; the probes cost one cmp each)
//   dotnet fsi latency_live.fsx on IK|TS        WRITES 1 byte: switch on    (toggle IK at a shrine / menu)
//   dotnet fsi latency_live.fsx off IK|TS       WRITES 1 byte: switch off
//   dotnet fsi latency_live.fsx cap <ms>        WRITES: pad-stage wait cap used by IK (default 2, stock = 1)
//   dotnet fsi latency_live.fsx status          READ-ONLY: switches, input-age counters, wait timeouts
//   dotnet fsi latency_live.fsx stats [secs]    resets the age counters, waits, prints the per-frame budget from
//                                                the ring (needs TS on; IK on or off)
//   dotnet fsi latency_live.fsx timeline [n]    READ-ONLY: last n ring events with times
//   dotnet fsi latency_live.fsx revert          WRITES: original bytes back at all 6 sites (cave left allocated)
//
// A/B recipe: apply; on TS; stats 20 (stock); on IK; stats 20 (IK). Compare "K->C input age" and the chained
// "K->P sample-to-present" median. Then PresentMon for "P -> displayed" (MsUntilDisplayed).
#load "Mem.fsx"
open System
open System.IO
open System.Diagnostics
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "latency_state.txt")
let dump = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"

// ---- engine RVAs (v2.02) ---------------------------------------------------------------------------------
let QpcNow        = 0x205530UL   // returns QPC ticks in rax (calls the QueryPerformanceCounter IAT)
let InputKick     = 0x5AB7B0UL   // join previous run, reset pad-stage event, set worker event, pending=1
let PadStageWait1 = 0x86C018UL   // stock: wait_for(padStageEvent, 1)
let PadStageWaitN = 0x86C040UL   // (event*, duration*) -> al = 1 signalled / 0 timeout
let PadStageEvent = 0x45C39B0UL
let LatMarker     = 0x6B8D34UL   // first call after the frame-latency wait in RenderFrameBegin
let IatSetEvent   = 0x38E2348UL

// site rva, length, expected original bytes (also checked against the dump)
let SK = 0x5AB6CF, "E8 DC 00 00 00"          // UpdateDispatchB: call InputKick           (end of logic frame)
let SC = 0x5ACAAA, "E8 69 F5 2B 00"          // FrameEarlyUpdate: call PadStageWait1      (start of logic frame)
let SL = 0x5AD2BB, "49 89 17 0F 57 C9"       // FrameEnd: mov [r15],rdx ; xorps xmm1,xmm1  (limiter exit QPC)
let SW = 0x4A52D4, "E8 57 02 D6 FF"          // RenderFrameBegin: call QpcNow             (render frame begin)
let SB = 0x4A535E, "E8 D1 39 21 00"          // RenderFrameBegin: call 6B8D34             (after waitable wait)
let SP = 0x3D6552, "FF 15 F0 BD 50 03"       // GfxDevicePresent: call [SetEvent]         (hand Present over)
let sites = [ "K", SK; "C", SC; "L", SL; "W", SW; "B", SB; "P", SP ]
let hex (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun b -> Convert.ToByte(b, 16))

// ---- cave layout -----------------------------------------------------------------------------------------
let CODE_SZ = 0x1000
let D_IK, D_TS, D_IKLATCH = 0x00, 0x01, 0x02   // bytes: switches; IKLATCH = IK state latched at the last frame start
let D_LASTKICK = 0x08                   // qword QPC of the last kick
let D_AGESUM, D_AGECNT, D_AGEMAX = 0x10, 0x18, 0x20
let D_TIMEOUTS = 0x28                   // IK pad-stage waits that hit the cap
let D_CAP = 0x30                        // qword duration for 86C040 (same unit as stock's 1 = 1 ms)
let D_RINGIDX = 0x38
let D_KSTOCK, D_KIK = 0x40, 0x48        // kick counters
let D_RING = 0x100
let RING_N = 4096
let ALLOC = CODE_SZ + D_RING + RING_N * 16 + 0x100
let evName = [| "?"; "K"; "C"; "W"; "B"; "P"; "L" |]

type CaveAddrs = { K: uint64; C: uint64; L: uint64; W: uint64; B: uint64; P: uint64 }

let buildCave (caveBase: uint64) (imgBase: uint64) =
    let D = caveBase + uint64 CODE_SZ
    let a = Assembler(64)
    let abs (rva: uint64) = imgBase + rva
    let mutable stamp = a.CreateLabel()
    let mutable lK = a.CreateLabel()
    let mutable lC = a.CreateLabel()
    let mutable lL = a.CreateLabel()
    let mutable lW = a.CreateLabel()
    let mutable lB = a.CreateLabel()
    let mutable lP = a.CreateLabel()
    // stamp: rax = QPC, ecx = event type. Clobbers r10, r11, flags. Preserves rax, rcx, rdx, everything else.
    let mutable stOut = a.CreateLabel()
    a.Label(&stamp)
    a.mov(r11, int64 D); a.cmp(__byte_ptr.[r11 + int64 D_TS], 0); a.je(stOut)
    a.mov(r10d, 1); a.``lock``.xadd(__qword_ptr.[r11 + int64 D_RINGIDX], r10)
    a.``and``(r10d, RING_N - 1); a.shl(r10, 4uy); a.add(r10, r11)
    a.mov(__qword_ptr.[r10 + int64 D_RING], rax); a.mov(__qword_ptr.[r10 + int64 (D_RING + 8)], rcx)
    a.Label(&stOut); a.ret()
    // K: replaces `call InputKick` at the end of the logic frame. IK on -> skip (the kick moves to C).
    let mutable kSkip = a.CreateLabel()
    a.Label(&lK)
    a.mov(r11, int64 D); a.cmp(__byte_ptr.[r11 + int64 D_IKLATCH], 0); a.jne(kSkip)
    a.sub(rsp, 0x28); a.call(abs QpcNow)
    a.mov(r11, int64 D); a.mov(__qword_ptr.[r11 + int64 D_LASTKICK], rax); a.inc(__qword_ptr.[r11 + int64 D_KSTOCK])
    a.mov(ecx, 1); a.call(stamp); a.add(rsp, 0x28); a.jmp(abs InputKick)
    a.Label(&kSkip); a.ret()
    // C: replaces `call PadStageWait1` at the start of the logic frame (FrameEarlyUpdate, only when +646h set,
    //    which is the same flag that gates the stock kick).
    let mutable cStock = a.CreateLabel()
    let mutable cMeas = a.CreateLabel()
    let mutable cNoAge = a.CreateLabel()
    a.Label(&lC)
    a.sub(rsp, 0x28)
    // latch the switch once per frame: IK path if IK is on now OR was on last frame (so switching off still
    // does one fresh poll here, because last frame's end-of-frame kick was skipped)
    a.mov(r11, int64 D); a.movzx(eax, __byte_ptr.[r11 + int64 D_IK]); a.mov(cl, __byte_ptr.[r11 + int64 D_IKLATCH])
    a.mov(__byte_ptr.[r11 + int64 D_IKLATCH], al); a.``or``(al, cl); a.je(cStock)
    a.call(abs QpcNow)
    a.mov(r11, int64 D); a.mov(__qword_ptr.[r11 + int64 D_LASTKICK], rax); a.inc(__qword_ptr.[r11 + int64 D_KIK])
    a.mov(ecx, 1); a.call(stamp)
    a.call(abs InputKick)
    a.mov(rcx, int64 (abs PadStageEvent)); a.mov(rdx, int64 (D + uint64 D_CAP)); a.call(abs PadStageWaitN)
    a.test(al, al); a.jne(cMeas)
    a.mov(r11, int64 D); a.``lock``.inc(__qword_ptr.[r11 + int64 D_TIMEOUTS]); a.jmp(cMeas)
    a.Label(&cStock); a.call(abs PadStageWait1)
    a.Label(&cMeas)
    a.call(abs QpcNow)
    a.mov(r11, int64 D); a.mov(rdx, __qword_ptr.[r11 + int64 D_LASTKICK]); a.test(rdx, rdx); a.je(cNoAge)
    a.mov(rcx, rax); a.sub(rcx, rdx); a.js(cNoAge)
    a.add(__qword_ptr.[r11 + int64 D_AGESUM], rcx); a.inc(__qword_ptr.[r11 + int64 D_AGECNT])
    a.cmp(rcx, __qword_ptr.[r11 + int64 D_AGEMAX]); a.jbe(cNoAge); a.mov(__qword_ptr.[r11 + int64 D_AGEMAX], rcx)
    a.Label(&cNoAge)
    a.mov(ecx, 2); a.call(stamp)
    a.add(rsp, 0x28); a.ret()
    // L: replaces `mov [r15],rdx ; xorps xmm1,xmm1` (6 bytes, call + nop). rdx = limiter exit QPC.
    a.Label(&lL)
    a.mov(__qword_ptr.[r15], rdx); a.xorps(xmm1, xmm1)
    a.push(rax); a.push(rcx); a.push(rdx); a.push(r10); a.push(r11); a.sub(rsp, 0x20)
    a.mov(rax, rdx); a.mov(ecx, 6); a.call(stamp)
    a.add(rsp, 0x20); a.pop(r11); a.pop(r10); a.pop(rdx); a.pop(rcx); a.pop(rax); a.ret()
    // W: replaces `call QpcNow` at RenderFrameBegin entry; must return the QPC in rax.
    a.Label(&lW)
    a.sub(rsp, 0x28); a.call(abs QpcNow); a.mov(ecx, 3); a.call(stamp); a.add(rsp, 0x28); a.ret()
    // B: replaces `call 6B8D34` right after the frame-latency (waitable) wait; keep rcx/rdx/r8/r9/xmm0-3.
    let mutable bGo = a.CreateLabel()
    a.Label(&lB)
    a.mov(r11, int64 D); a.cmp(__byte_ptr.[r11 + int64 D_TS], 0); a.je(bGo)
    a.push(rcx); a.push(rdx); a.push(r8); a.push(r9); a.sub(rsp, 0x68)
    a.movdqu(__xmmword_ptr.[rsp + 0x20L], xmm0); a.movdqu(__xmmword_ptr.[rsp + 0x30L], xmm1)
    a.movdqu(__xmmword_ptr.[rsp + 0x40L], xmm2); a.movdqu(__xmmword_ptr.[rsp + 0x50L], xmm3)
    a.call(abs QpcNow); a.mov(ecx, 4); a.call(stamp)
    a.movdqu(xmm0, __xmmword_ptr.[rsp + 0x20L]); a.movdqu(xmm1, __xmmword_ptr.[rsp + 0x30L])
    a.movdqu(xmm2, __xmmword_ptr.[rsp + 0x40L]); a.movdqu(xmm3, __xmmword_ptr.[rsp + 0x50L])
    a.add(rsp, 0x68); a.pop(r9); a.pop(r8); a.pop(rdx); a.pop(rcx)
    a.Label(&bGo); a.jmp(abs LatMarker)
    // P: replaces `call [IAT SetEvent]` (6 bytes, call + nop) in GfxDevicePresent; rcx = event handle.
    let mutable pGo = a.CreateLabel()
    a.Label(&lP)
    a.mov(r11, int64 D); a.cmp(__byte_ptr.[r11 + int64 D_TS], 0); a.je(pGo)
    a.push(rcx); a.sub(rsp, 0x20); a.call(abs QpcNow); a.mov(ecx, 5); a.call(stamp); a.add(rsp, 0x20); a.pop(rcx)
    a.Label(&pGo); a.mov(rax, int64 (abs IatSetEvent)); a.jmp(__qword_ptr.[rax])
    use ms = new MemoryStream()
    let r = a.Assemble(StreamCodeWriter(ms), caveBase, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let code = ms.ToArray()
    if code.Length > CODE_SZ then failwith "cave code too large"
    code, { K = r.GetLabelRIP &lK; C = r.GetLabelRIP &lC; L = r.GetLabelRIP &lL
            W = r.GetLabelRIP &lW; B = r.GetLabelRIP &lB; P = r.GetLabelRIP &lP }

let callBytes (site: uint64) (target: uint64) (len: int) =
    let rel = int64 target - int64 (site + 5UL)
    if rel > int64 Int32.MaxValue || rel < int64 Int32.MinValue then failwith "cave out of rel32 range"
    Array.concat [ [| 0xE8uy |]; BitConverter.GetBytes(int32 rel); Array.create (len - 5) 0x90uy ]

let patchList (imgBase: uint64) (c: CaveAddrs) =
    [ for (n, (rva, orig)) in sites do
        let o = hex orig
        let tgt = match n with "K" -> c.K | "C" -> c.C | "L" -> c.L | "W" -> c.W | "B" -> c.B | _ -> c.P
        yield n, imgBase + uint64 rva, callBytes (imgBase + uint64 rva) tgt o.Length, o ]

let printDis (bytes: byte[]) (ip: uint64) =
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- ip
    let fmt = NasmFormatter()
    let out = StringOutput()
    while dec.IP < ip + uint64 bytes.Length do
        let ins = dec.Decode()
        fmt.Format(&ins, out)
        printfn "  %08X  %s" (ins.IP - ip) (out.ToStringAndReset())

let checkDump () =
    let mutable ok = true
    for (n, (rva, orig)) in sites do
        let o = hex orig
        let d = dump.[rva .. rva + o.Length - 1]
        let m = d = o
        if not m then ok <- false
        printfn "site %s %X: dump %s %s" n rva (BitConverter.ToString d) (if m then "OK" else "MISMATCH (expected " + orig + ")")
    // call targets encoded in the originals
    let callTarget rva = uint64 (int64 rva + 5L + int64 (BitConverter.ToInt32(dump, rva + 1)))
    printfn "K calls %X (InputKick %X)  C calls %X (%X)  W calls %X (QpcNow %X)  B calls %X (%X)"
        (callTarget (fst SK)) InputKick (callTarget (fst SC)) PadStageWait1 (callTarget (fst SW)) QpcNow (callTarget (fst SB)) LatMarker
    let pIat = uint64 (int64 (fst SP) + 6L + int64 (BitConverter.ToInt32(dump, fst SP + 2)))
    printfn "P IAT slot %X (SetEvent %X)" pIat IatSetEvent
    ok && callTarget (fst SK) = InputKick && callTarget (fst SC) = PadStageWait1 && callTarget (fst SW) = QpcNow
       && callTarget (fst SB) = LatMarker && pIat = IatSetEvent

let guard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let loadState (t: Target) =
    if not (File.Exists stateFile) then failwith "no state file: run apply first"
    let v = File.ReadAllText(stateFile).Split(' ')
    if int v.[1] <> t.Process.Id then failwith "state file belongs to another game process (restart = patches gone)"
    nativeint (Convert.ToInt64(v.[0], 16))

let qpf = float Stopwatch.Frequency
let ms (ticks: int64) = float ticks * 1000.0 / qpf

let readRing (t: Target) (cave: nativeint) =
    let D = cave + nativeint CODE_SZ
    let idx = t.ReadI64(D + nativeint D_RINGIDX)
    let raw = t.Read(D + nativeint D_RING, RING_N * 16)
    let n = int (min idx (int64 RING_N))
    [| for k in idx - int64 n .. idx - 1L do
         let s = int (k % int64 RING_N) * 16
         let q = BitConverter.ToInt64(raw, s)
         let ty = int (BitConverter.ToInt64(raw, s + 8) &&& 0xFFL)
         if q > 0L && ty >= 1 && ty <= 6 then yield q, ty |] |> Array.sortBy fst, idx

let pct (xs: float[]) (p: float) =
    if xs.Length = 0 then nan else
    let s = Array.sort xs
    s.[min (s.Length - 1) (int (p / 100.0 * float s.Length))]

let analyse (ev: (int64 * int)[]) =
    let next ty (from: int64) = ev |> Array.tryFind (fun (q, t) -> t = ty && q > from) |> Option.map fst
    let prev ty (before: int64) = ev |> Array.tryFindBack (fun (q, t) -> t = ty && q < before) |> Option.map fst
    let times ty = ev |> Array.filter (fun (_, t) -> t = ty) |> Array.map fst
    let row name (xs: float[]) =
        if xs.Length > 0 then printfn "  %-44s n=%4d  median %7.2f  p5 %7.2f  p95 %7.2f ms" name xs.Length (pct xs 50.0) (pct xs 5.0) (pct xs 95.0)
        else printfn "  %-44s (no samples)" name
    let gaps a b = [| for q in times a do match next b q with Some q2 -> yield ms (q2 - q) | None -> () |]
    let diffs ty = let x = times ty in [| for i in 1 .. x.Length - 1 -> ms (x.[i] - x.[i - 1]) |]
    printfn "per-frame budget (QPC, this machine):"
    row "K->C  input age at consumption (pad kick -> update)" [| for q in times 2 do match prev 1 q with Some k -> yield ms (q - k) | None -> () |]
    row "C->L  update + scene steps + limiter" (gaps 2 6)
    row "L->W  limiter exit -> next render-frame begin" (gaps 6 3)
    row "W->B  frame-latency (waitable) wait" (gaps 3 4)
    row "B->P  render recording -> Present handed over" (gaps 4 5)
    row "C->C  logic frame interval" (diffs 2)
    row "P->P  present interval" (diffs 5)
    // chain: K(k) -> C -> L -> first W after L -> B -> P
    let chain =
        [| for c in times 2 do
             match prev 1 c, next 6 c with
             | Some k, Some l ->
                 match next 3 l with
                 | Some w -> match next 5 w with Some p -> yield ms (p - k), ms (p - c) | None -> ()
                 | None -> ()
             | _ -> () |]
    row "K->P  sample -> Present (assumes W after L renders it)" (chain |> Array.map fst)
    row "C->P  consume -> Present (same assumption)" (chain |> Array.map snd)
    printfn "  (add PresentMon MsUntilDisplayed for Present -> photons; see notes\\input-latency.md)"

let runOffline () =
    let ok = checkDump ()
    let imgBase = 0x7FF74F410000UL
    let caveBase = imgBase - 0x10000000UL
    let code, c = buildCave caveBase imgBase
    printfn "cave: %d bytes code, entries K+%X C+%X L+%X W+%X B+%X P+%X" code.Length
        (c.K - caveBase) (c.C - caveBase) (c.L - caveBase) (c.W - caveBase) (c.B - caveBase) (c.P - caveBase)
    printDis code caveBase
    for (n, addr, nw, old) in patchList imgBase c do
        printfn "site %s %X: %s -> %s" n (addr - imgBase) (BitConverter.ToString old) (BitConverter.ToString nw)
    printfn (if ok then "OFFLINE: ALL PASSED" else "OFFLINE: FAILED")
let runOnline () =
    let t = attach "Nioh3"
    let imgBase = uint64 t.Base
    match mode with
    | "dry" ->
        checkDump () |> ignore
        for (n, (rva, orig)) in sites do
            let live = t.Read(t.Base + nativeint rva, (hex orig).Length)
            printfn "live %s %X: %s %s" n rva (BitConverter.ToString live)
                (if live = hex orig then "original" elif live.[0] = 0xE8uy then "patched (or foreign hook)" else "UNKNOWN")
    | "apply" ->
        guard ()
        for (n, (rva, orig)) in sites do
            if t.Read(t.Base + nativeint rva, (hex orig).Length) <> hex orig then failwithf "site %s %X not original: nothing written" n rva
        let cave = t.AllocNear(t.Base + nativeint (fst SC), ALLOC)
        let code, c = buildCave (uint64 cave) imgBase
        t.Write(cave, code)
        let D = cave + nativeint CODE_SZ
        t.Write(D, Array.zeroCreate 0x100)
        t.Write(D + nativeint D_CAP, BitConverter.GetBytes 2L)
        File.WriteAllText(stateFile, sprintf "%X %d" (int64 cave) t.Process.Id)
        let n = safeWrite t [ for (_, addr, nw, old) in patchList imgBase c -> nativeint addr, nw, old ]
        printfn "applied on attempt %d, cave %X; IK and TS are OFF (stock). Use: on TS / on IK" n (int64 cave)
    | "on" | "off" ->
        guard ()
        let cave = loadState t
        let off = match (if args.Length > 1 then args.[1].ToUpper() else "") with "IK" -> D_IK | "TS" -> D_TS | x -> failwithf "on|off IK|TS (got %s)" x
        t.Write(cave + nativeint (CODE_SZ + off), [| (if mode = "on" then 1uy else 0uy) |])
        printfn "%s %s" args.[1] mode
    | "cap" ->
        guard ()
        let cave = loadState t
        let v = if args.Length > 1 then int64 args.[1] else 2L
        t.Write(cave + nativeint (CODE_SZ + D_CAP), BitConverter.GetBytes v)
        printfn "IK pad-stage wait cap = %d" v
    | "status" ->
        let cave = loadState t
        let D = cave + nativeint CODE_SZ
        let b = t.Read(D, 0x50)
        let q o = BitConverter.ToInt64(b, o)
        for (n, (rva, orig)) in sites do
            let live = t.Read(t.Base + nativeint rva, (hex orig).Length)
            printfn "site %s %X: %s" n rva (if live = hex orig then "original" else "patched")
        printfn "IK=%d TS=%d cap=%d  kicks stock=%d IK=%d  IK wait timeouts=%d  ring=%d" b.[D_IK] b.[D_TS] (q D_CAP) (q D_KSTOCK) (q D_KIK) (q D_TIMEOUTS) (q D_RINGIDX)
        let cnt = q D_AGECNT
        if cnt > 0L then printfn "input age K->C: mean %.3f ms, max %.3f ms over %d frames (since apply/last stats)" (ms (q D_AGESUM) / float cnt) (ms (q D_AGEMAX)) cnt
    | "stats" ->
        let cave = loadState t
        let D = cave + nativeint CODE_SZ
        let secs = if args.Length > 1 then int args.[1] else 10
        if t.Read(D + nativeint D_TS, 1).[0] = 0uy then printfn "warning: TS is off, only the K->C counters will be filled"
        // counters are only written by the logic thread; resetting them races at most one frame
        t.Write(D + nativeint D_AGESUM, Array.zeroCreate 24)
        t.Write(D + nativeint D_TIMEOUTS, Array.zeroCreate 8)
        Threading.Thread.Sleep(secs * 1000)
        let b = t.Read(D, 0x50)
        let q o = BitConverter.ToInt64(b, o)
        printfn "IK=%d TS=%d  frames %d  input age mean %.3f ms max %.3f ms  IK timeouts %d" b.[D_IK] b.[D_TS] (q D_AGECNT)
            (if q D_AGECNT > 0L then ms (q D_AGESUM) / float (q D_AGECNT) else nan) (ms (q D_AGEMAX)) (q D_TIMEOUTS)
        let ev, _ = readRing t cave
        if ev.Length > 0 then analyse ev
    | "timeline" ->
        let cave = loadState t
        let n = if args.Length > 1 then int args.[1] else 60
        let ev, idx = readRing t cave
        let ev = ev.[max 0 (ev.Length - n) ..]
        if ev.Length > 0 then
            let t0 = fst ev.[0]
            let mutable last = t0
            for (q, ty) in ev do
                printfn "%9.3f ms  (+%7.3f)  %s" (ms (q - t0)) (ms (q - last)) evName.[ty]
                last <- q
        printfn "ring index %d" idx
    | "revert" ->
        guard ()
        let cave = loadState t
        let _, c = buildCave (uint64 cave) imgBase
        let pl = patchList imgBase c
        let todo = [ for (n, addr, nw, old) in pl do
                       let live = t.Read(nativeint addr, nw.Length)
                       if live = nw then yield nativeint addr, old, nw
                       elif live <> old then printfn "site %s: unexpected bytes %s, left alone" n (BitConverter.ToString live) ]
        if todo.IsEmpty then printfn "nothing to revert"
        else
            // switch everything off first so no thread is mid-way through IK when the sites flip back
            t.Write(cave + nativeint CODE_SZ, [| 0uy; 0uy |])   // IK off; the latch clears itself at the next frame start
            Threading.Thread.Sleep 200
            let n = safeWrite t todo
            printfn "reverted %d site(s) on attempt %d (cave left allocated)" todo.Length n
    | m -> printfn "unknown mode %s" m

if mode = "offline" then runOffline () else runOnline ()
