// Nioh 3 "variable-dt mode" v2 (frame-timing agent). See notes\frame-timing.md §5-final / §8 (v2).
// v1 (120-tier only) is kept as ft_vrr_patch_v1.fsx. v2 sites are a superset of v1 with identical originals,
// so `revert` here also reverts a v1 install. To upgrade a live v1 install: `revert`, then `apply`.
//
//   dotnet fsi ft_vrr_patch.fsx dry      READ-ONLY: check original bytes, assemble cave at a dummy address, print it.
//   dotnet fsi ft_vrr_patch.fsx verify [secs]
//                                        READ-ONLY: sample G (+ cave if installed): real fps, tier, armed %,
//                                        G[614] histogram, implied game-time/wall-time, interval stats.
//   dotnet fsi ft_vrr_patch.fsx apply    WRITES (FT_ALLOW_WRITE=1): cave + sites under t.Frozen.
//   dotnet fsi ft_vrr_patch.fsx status   READ-ONLY: cave diagnostics.
//   dotnet fsi ft_vrr_patch.fsx on|off   WRITES 1 dword: runtime kill switch (off = exact stock behaviour).
//   dotnet fsi ft_vrr_patch.fsx revert   WRITES: restore original bytes at every v1/v2 site.
//
// Config: ft_vrr.cfg (key=value; missing keys use the defaults below).
#load "Mem.fsx"
open System
open System.IO
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let mode = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else "dry"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "ft_vrr_state.txt")
// DYNAMIC_FPS (config int, 1 = on). Its controller drops the tier to 60 whenever the reported fps is below the
// tier; with real fps reported it sat in the 60 tier most of the time (72.7 vs 102.7 fps, 2026-10-06). The
// variable-dt patch already removes the judder DYNAMIC_FPS exists to avoid, so force it off while applied and
// restore the user's value on revert.
let DynFpsRva = 0x45BAF58
let dynFile = Path.Combine(__SOURCE_DIRECTORY__, "ft_vrr_dynfps.txt")
let cfgFile = (match Environment.GetEnvironmentVariable "FT_VRR_CFG" with null | "" -> Path.Combine(__SOURCE_DIRECTORY__, "ft_vrr.cfg") | p -> p)

// ---- configuration ---------------------------------------------------------------------------------
let cfg =
    let d = Collections.Generic.Dictionary<string, int>()
    for k, v in [ "arm30",1; "arm60",1; "arm120",1
                  "min30",20; "max30",30; "min60",24; "max60",60; "min120",48; "max120",120
                  "menuQuantize",0; "maxFps",120; "coopSafe",1 ] do d.[k] <- v
    if File.Exists cfgFile then
        for line in File.ReadAllLines cfgFile do
            let l = line.Split('#').[0].Trim()
            if l.Contains "=" then
                let p = l.Split('=')
                d.[p.[0].Trim()] <- int (p.[1].Trim())
    d
let tiers = [| 30; 60; 120 |]
let arm i = cfg.["arm" + string tiers.[i]]
let minF i = cfg.["min" + string tiers.[i]]
// 30/60 tiers: cap <= tier. 120 tier: cap = maxFps (120 = stock cap, 240 = 240 Hz, 0 = unlocked -> 1000);
// above 120 the internal tier stays 120 (all fixed-step/mode logic unchanged), P9 keeps the event ratio on the tier.
let maxFpsCfg = (let v = cfg.["maxFps"] in if v <= 0 then 1000 else max 120 v)
let maxF i = if tiers.[i] = 120 then (if maxFpsCfg > 120 then maxFpsCfg else min 120 cfg.["max120"]) else min tiers.[i] cfg.["max" + string tiers.[i]]
let menuQ = cfg.["menuQuantize"] <> 0
let coopSafe = cfg.["coopSafe"] <> 0
// Online session object (Online::IsOnlinePlay 0x85834: [[0x4709B50]+0x34] == 1 || == 2). 0 in solo (verified live).
let NetSessionPtrRva = 0x4709B50UL

// ---- sites (RVA, original bytes) ---------------------------------------------------------------------
let P1 = 0x5AD677, "44 89 87 14 06 00 00"        // mov [rdi+614h],r8d      -> call caveFps ; 66 90
let P2 = 0x5AD6AA, "F3 0F 11 97 24 06 00 00"     // movss [rdi+624h],xmm2   -> call caveScale ; 0F 1F 00
let P4 = 0x5AD860, "8B 87 14 06 00 00"           // mov eax,[rdi+614h] -> SetLimiterFps arg -> call caveLim ; 90
let P3 = [ 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4 ]
let P3orig = "8D 42 78 41 F7 F0"                 // lea eax,[rdx+78h]; div r8d -> mov eax,[ticks120]
let P6 = 0x1975BAA, "8D 42 3C 41 F7 F0"          // lea eax,[rdx+3Ch]; div r8d -> mov eax,[ticks60]   (fn 0x281470)
// calls to GetFps (0x46594) whose integer semantics must stay the TIER: fn 0x20E540 (fps/60 int),
// dynamic-FPS controller 0x5AF4E4 / 0x5AF670 (load fraction, ms budget)
let P8 = [ 0x20E77B, "E8 14 7E E3 FF"; 0x5AF526, "E8 69 70 A9 FF"; 0x5AF682, "E8 0D 6F A9 FF"; 0x5AF69F, "E8 F0 6E A9 FF" ]
let P7 = 0x43312B, "E8 54 33 C1 FF"              // menu-input hold timers: call GetDelta60 -> call caveMenu (optional)
let P9 = 0x5AD89B, "8B 87 14 06 00 00"           // mov eax,[rdi+614h] feeding ctx[1E6]=max(1,fps/G[61C]) -> call caveTier ; 90
let GetDelta60 = 0x46484UL
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- cave ------------------------------------------------------------------------------------------------
type Cave = { Bytes: byte[]; Fps: uint64; Scale: uint64; Lim: uint64; Tier: uint64; Menu: uint64
              Ticks120: uint64; Ticks60: uint64; Enable: uint64; FpsOut: uint64; Last: uint64; ScaleV: uint64; TierV: uint64; NetMode: uint64 }

let build (at: uint64) (imageBase: uint64) (initTier: int) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lPrev = L()
    let mutable lAcc120 = L()
    let mutable lAcc60 = L()
    let mutable lLast = L()
    let mutable l1e9 = L()
    let mutable l120 = L()
    let mutable l60 = L()
    let mutable lMinNs = L()
    let mutable lMaxNs = L()
    let mutable lEnable = L()
    let mutable lScale = L()
    let mutable lT120 = L()
    let mutable lT60 = L()
    let mutable lFpsOut = L()
    let mutable lTier = L()
    let mutable lCapCur = L()
    let mutable lArm = L()
    let mutable lCap = L()
    let mutable lC4 = L()
    let mutable lC025 = L()
    let mutable lMCarry = L()
    let mutable lCoopSafe = L()
    let mutable lNetMode = L()
    let mutable lNoNet = L()
    let mutable cFps = L()
    let mutable cScale = L()
    let mutable cLim = L()
    let mutable cTier = L()
    let mutable cMenu = L()
    let mutable lStock = L()
    let mutable lDone = L()
    let mutable lIdx = L()
    let mutable a1 = L()
    let mutable a2 = L()
    let mutable a3 = L()
    let mutable l60z = L()
    let mutable l60d = L()
    let mutable lR = L()
    let mutable lM = L()
    let d64 (x: float) = uint64 (BitConverter.DoubleToInt64Bits x)
    let f32 (x: float32) = uint32 (BitConverter.SingleToInt32Bits x)
    // 8-byte data
    a.Label(&lPrev);   a.dq(0UL)
    a.Label(&lAcc120); a.dq(d64 0.0)
    a.Label(&lAcc60);  a.dq(d64 0.0)
    a.Label(&lLast);   a.dq(0UL)
    a.Label(&l1e9);    a.dq(d64 1e9)
    a.Label(&l120);    a.dq(d64 120.0)
    a.Label(&l60);     a.dq(d64 60.0)
    a.Label(&lMinNs);  for i in 0..2 do a.dq(uint64 (1_000_000_000L / int64 (maxF i)))   // shortest interval
    a.Label(&lMaxNs);  for i in 0..2 do a.dq(uint64 (1_000_000_000L / int64 (minF i)))   // longest interval
    // 4-byte data
    a.Label(&lEnable); a.dd(1u)
    a.Label(&lScale);  a.dd(f32 1.0f)
    a.Label(&lT120);   a.dd(uint32 (120 / max 1 initTier))
    a.Label(&lT60);    a.dd(uint32 (60 / max 1 initTier))
    a.Label(&lFpsOut); a.dd(0u)                                   // 0 = not armed this frame
    a.Label(&lTier);   a.dd(uint32 initTier)
    a.Label(&lCapCur); a.dd(uint32 initTier)
    a.Label(&lArm);    for i in 0..2 do a.dd(uint32 (arm i))
    a.Label(&lCap);    for i in 0..2 do a.dd(uint32 (maxF i))
    a.Label(&lC4);     a.dd(f32 4.0f)
    a.Label(&lC025);   a.dd(f32 0.25f)
    a.Label(&lMCarry); a.dd(f32 0.0f)
    a.Label(&lCoopSafe); a.dd(if coopSafe then 1u else 0u)
    a.Label(&lNetMode); a.dd(0u)                                   // last read session mode (diagnostics)
    a.int3(); a.int3(); a.int3(); a.int3()

    // ---- caveFps: replaces `mov [rdi+614h],r8d` (0x5AD677). rdi=G, r8d=tier fps (stays the tier: it is
    //      passed on to 0x947250 -> 0x1334980, the 30/60/120 mode switch behind Is30Fps), eax=old G[620].
    a.Label(&cFps)
    a.push(rax); a.push(rcx); a.push(rdx); a.push(r9)
    a.sub(rsp, 0x38)
    a.movdqu(__xmmword_ptr.[rsp], xmm0)
    a.movdqu(__xmmword_ptr.[rsp + 0x10L], xmm1)
    a.movdqu(__xmmword_ptr.[rsp + 0x20L], xmm2)
    a.mov(rcx, __qword_ptr.[rdi + 0x7D8L])            // this frame's start (ns), written in 0x5AC664
    a.xchg(__qword_ptr.[lPrev], rcx)                  // rcx = previous frame start
    a.mov(__dword_ptr.[lTier], r8d)
    a.cmp(__dword_ptr.[lEnable], 0); a.je(lStock)
    // coopSafe: stock path while in an online session (mode != 0 at [[base+0x4709B50]+0x34])
    a.mov(__dword_ptr.[lNetMode], 0)
    a.cmp(__dword_ptr.[lCoopSafe], 0); a.je(lNoNet)
    a.mov(rdx, imageBase + NetSessionPtrRva); a.mov(rdx, __qword_ptr.[rdx])
    a.test(rdx, rdx); a.je(lNoNet)
    a.mov(edx, __dword_ptr.[rdx + 0x34L]); a.mov(__dword_ptr.[lNetMode], edx)
    a.test(edx, edx); a.jne(lStock)
    a.Label(&lNoNet)
    a.xor(r9d, r9d); a.cmp(r8d, 30); a.je(lIdx)
    a.mov(r9d, 1); a.cmp(r8d, 60); a.je(lIdx)
    a.mov(r9d, 2); a.cmp(r8d, 120); a.jne(lStock)
    a.Label(&lIdx)
    a.lea(rdx, __[lArm]); a.cmp(__dword_ptr.[rdx + r9 * 4], 0); a.je(lStock)
    a.cmp(eax, r8d); a.jne(lStock)                    // event / time-effect override (G[620] != tier) -> stock
    a.test(rcx, rcx); a.je(lStock)
    a.mov(rax, __qword_ptr.[rdi + 0x7D8L]); a.sub(rax, rcx)
    a.lea(rdx, __[lMinNs]); a.cmp(rax, __qword_ptr.[rdx + r9 * 8]); a.jge(a1); a.mov(rax, __qword_ptr.[rdx + r9 * 8]); a.Label(&a1)
    a.lea(rdx, __[lMaxNs]); a.cmp(rax, __qword_ptr.[rdx + r9 * 8]); a.jle(a2); a.mov(rax, __qword_ptr.[rdx + r9 * 8]); a.Label(&a2)
    a.mov(__qword_ptr.[lLast], rax)
    a.lea(rdx, __[lCap]); a.mov(edx, __dword_ptr.[rdx + r9 * 4]); a.mov(__dword_ptr.[lCapCur], edx)
    a.cvtsi2sd(xmm1, rax); a.divsd(xmm1, __qword_ptr.[l1e9])           // xmm1 = interval seconds
    a.movsd(xmm0, __qword_ptr.[l1e9]); a.cvtsi2sd(xmm2, rax); a.divsd(xmm0, xmm2)
    a.cvtsd2si(ecx, xmm0)                                              // fps_i = round(1/interval)
    a.cmp(ecx, 1); a.jge(a3); a.mov(ecx, 1); a.Label(&a3)
    a.mov(__dword_ptr.[lFpsOut], ecx)
    a.mov(__dword_ptr.[rdi + 0x614L], ecx)                             // measured fps -> G[614]
    a.cvtsi2sd(xmm0, ecx); a.mulsd(xmm0, xmm1); a.cvtsd2ss(xmm0, xmm0)
    a.movss(__dword_ptr.[lScale], xmm0)                                // scaleCorr = fps_i * interval
    a.movapd(xmm0, xmm1); a.mulsd(xmm0, __qword_ptr.[l120]); a.addsd(xmm0, __qword_ptr.[lAcc120])
    a.cvttsd2si(ecx, xmm0); a.cvtsi2sd(xmm2, ecx); a.subsd(xmm0, xmm2)
    a.movsd(__qword_ptr.[lAcc120], xmm0); a.mov(__dword_ptr.[lT120], ecx)   // 120 Hz ticks with carry
    a.cmp(r9d, 2); a.je(l60z)                                          // stock 60/120 == 0 in the 120 tier
    a.movapd(xmm0, xmm1); a.mulsd(xmm0, __qword_ptr.[l60]); a.addsd(xmm0, __qword_ptr.[lAcc60])
    a.cvttsd2si(ecx, xmm0); a.cvtsi2sd(xmm2, ecx); a.subsd(xmm0, xmm2)
    a.movsd(__qword_ptr.[lAcc60], xmm0); a.mov(__dword_ptr.[lT60], ecx)     // 60 Hz ticks with carry
    a.jmp(lDone)
    a.Label(&l60z)
    a.mov(__dword_ptr.[lT60], 0)
    a.jmp(lDone)
    a.Label(&lStock)
    a.mov(__dword_ptr.[lScale], 0x3F800000)
    a.mov(__dword_ptr.[lFpsOut], 0)
    a.mov(__dword_ptr.[lCapCur], r8d)
    a.mov(__dword_ptr.[rdi + 0x614L], r8d)                             // the replaced instruction
    a.mov(ecx, r8d); a.test(ecx, ecx); a.je(lDone)
    a.mov(eax, 120); a.xor(edx, edx); a.div(ecx); a.mov(__dword_ptr.[lT120], eax)   // stock 120/fps
    a.mov(eax, 60); a.xor(edx, edx); a.div(ecx); a.mov(__dword_ptr.[lT60], eax)     // stock 60/fps
    a.Label(&lDone)
    a.movdqu(xmm2, __xmmword_ptr.[rsp + 0x20L])
    a.movdqu(xmm1, __xmmword_ptr.[rsp + 0x10L])
    a.movdqu(xmm0, __xmmword_ptr.[rsp])
    a.add(rsp, 0x38)
    a.pop(r9); a.pop(rdx); a.pop(rcx); a.pop(rax)
    a.ret()
    // ---- caveScale: replaces `movss [rdi+624h],xmm2` (0x5AD6AA)
    a.Label(&cScale)
    a.mulss(xmm2, __dword_ptr.[lScale])
    a.movss(__dword_ptr.[rdi + 0x624L], xmm2)
    a.ret()
    // ---- caveLim: replaces `mov eax,[rdi+614h]` feeding SetLimiterFps (0x5AD860): limiter keeps the tier cap
    a.Label(&cLim)
    a.mov(eax, __dword_ptr.[rdi + 0x614L])
    a.cmp(__dword_ptr.[lFpsOut], 0); a.je(lR)
    a.mov(eax, __dword_ptr.[lCapCur])
    a.Label(&lR)
    a.ret()
    // ---- caveTier: replaces `call GetFps` where the integer value must stay the tier
    a.Label(&cTier)
    a.mov(eax, __dword_ptr.[lTier])
    a.ret()
    // ---- caveMenu (optional): GetDelta60 for menu hold timers, quantized to 0.25 with carry while armed
    a.Label(&cMenu)
    a.call(imageBase + GetDelta60)
    a.cmp(__dword_ptr.[lFpsOut], 0); a.je(lM)
    a.movaps(xmm1, xmm0); a.addss(xmm1, __dword_ptr.[lMCarry])
    a.movaps(xmm0, xmm1); a.mulss(xmm0, __dword_ptr.[lC4]); a.roundss(xmm0, xmm0, 0uy); a.mulss(xmm0, __dword_ptr.[lC025])
    a.subss(xmm1, xmm0); a.movss(__dword_ptr.[lMCarry], xmm1)
    a.Label(&lM)
    a.ret()
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    { Bytes = ms.ToArray(); Fps = addr cFps; Scale = addr cScale; Lim = addr cLim; Tier = addr cTier; Menu = addr cMenu
      Ticks120 = addr lT120; Ticks60 = addr lT60; Enable = addr lEnable; FpsOut = addr lFpsOut; Last = addr lLast
      ScaleV = addr lScale; TierV = addr lTier; NetMode = addr lNetMode }

let callPatch (site: uint64) (target: uint64) (len: int) =
    let rel = int (int64 target - int64 (site + 5UL))
    let nops = match len - 5 with 0 -> [||] | 1 -> [| 0x90uy |] | 2 -> [| 0x66uy; 0x90uy |]
                                  | 3 -> [| 0x0Fuy; 0x1Fuy; 0x00uy |] | n -> failwithf "bad len %d" n
    Array.concat [ [| 0xE8uy |]; BitConverter.GetBytes rel; nops ]
let movEaxRip (site: uint64) (target: uint64) =
    let rel = int (int64 target - int64 (site + 6UL))
    Array.concat [ [| 0x8Buy; 0x05uy |]; BitConverter.GetBytes rel ]

let printDis (bytes: byte[]) (at: uint64) =
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- at
    let fmt = NasmFormatter()
    let out = StringOutput()
    while dec.IP < at + uint64 bytes.Length do
        let ins = dec.Decode()
        fmt.Format(&ins, out)
        printfn "  %016X  %s" ins.IP (out.ToStringAndReset())

let t = attach "Nioh3"
let B = uint64 t.Base
let s (rva: int) = B + uint64 rva
let readAt (rva: int) n = t.Read(nativeint (s rva), n)
let G () = t.ReadI64(t.Base + 0x4759910n)
let allSites = [ P1; P2; P4; P6; P7; P9 ] @ (P3 |> List.map (fun r -> r, P3orig)) @ P8
let activeSites = [ P1; P2; P4; P6; P9 ] @ (P3 |> List.map (fun r -> r, P3orig)) @ P8 @ (if menuQ then [ P7 ] else [])
let checkSite (rva, orig: string) =
    let o = hexb orig
    let cur = readAt rva o.Length
    let ok = cur = o
    printfn "  %-9X %s  %s" rva (if ok then "OK      " else "differs ") (hexs cur)
    ok
// site bytes for a given cave (in a safe write order: data-only users first, P1 last)
let sitePlan (c: Cave) =
    [ for r in P3 -> r, movEaxRip (s r) c.Ticks120
      yield fst P6, movEaxRip (s (fst P6)) c.Ticks60
      for (r, _) in P8 -> r, callPatch (s r) c.Tier 5
      if menuQ then yield fst P7, callPatch (s (fst P7)) c.Menu 5
      yield fst P9, callPatch (s (fst P9)) c.Tier 6
      yield fst P2, callPatch (s (fst P2)) c.Scale 8
      yield fst P4, callPatch (s (fst P4)) c.Lim 6
      yield fst P1, callPatch (s (fst P1)) c.Fps 7 ]

let verify (secs: float) =
    let g = nativeint (G ())
    let cave = if File.Exists stateFile then Some (File.ReadAllText(stateFile).Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))) else None
    let sw = Diagnostics.Stopwatch.StartNew()
    let fr = Collections.Generic.List<int * int * int * int * float32 * int64 * float>()
    let mutable last = -1
    while sw.Elapsed.TotalSeconds < secs do
        let b = t.Read(g + 0x610n, 0x18)
        let ctr = BitConverter.ToInt32(b, 0)
        if ctr <> last then
            last <- ctr
            fr.Add((ctr, BitConverter.ToInt32(b, 4), BitConverter.ToInt32(b, 8), BitConverter.ToInt32(b, 0x10),
                    BitConverter.ToSingle(b, 0x14), t.ReadI64(g + 0x7D8n), sw.Elapsed.TotalMilliseconds))
        Threading.Thread.SpinWait 2000
    let f = fr.ToArray()
    let (c0,_,_,_,_,_,t0), (c1,_,_,_,_,_,t1) = f.[0], f.[f.Length-1]
    printfn "real fps %.2f  (%d frames / %.0f ms)" (float (c1-c0) * 1000.0 / (t1-t0)) (c1-c0) (t1-t0)
    printfn "tier [618]: %A   [620]: %A" (f |> Array.countBy (fun (_,_,x,_,_,_,_) -> x)) (f |> Array.countBy (fun (_,_,_,x,_,_,_) -> x))
    let h = f |> Array.countBy (fun (_,x,_,_,_,_,_) -> (x / 5) * 5) |> Array.sortBy fst
    printfn "G[614] histogram (5-fps bins): %A" h
    let armed = f |> Array.filter (fun (_,fps,tier,_,_,_,_) -> fps <> tier) |> Array.length
    printfn "frames with G[614] != tier (armed, approx): %.1f%%" (100.0 * float armed / float f.Length)
    let gameSec = f |> Array.pairwise |> Array.sumBy (fun ((ca,_,_,_,_,_,_),(cb,fps,_,_,sc,_,_)) -> float (cb-ca) * float sc / float (max fps 1))
    printfn "implied game-time / wall-time = %.3f   (1.000 = real time)" (gameSec * 1000.0 / (t1 - t0))
    let iv = [| for i in 1..f.Length-1 do
                  let (ca,_,_,_,_,ta,_), (cb,_,_,_,_,tb,_) = f.[i-1], f.[i]
                  if cb = ca + 1 && tb > ta then yield float (tb - ta) / 1e6 |] |> Array.sort
    if iv.Length > 0 then
        let pct p = iv.[min (iv.Length-1) (int (float iv.Length * p))]
        printfn "frame interval ms: mean %.2f p5 %.2f p50 %.2f p95 %.2f p99 %.2f" (Array.average iv) (pct 0.05) (pct 0.5) (pct 0.95) (pct 0.99)
    let np = t.ReadI64(t.Base + nativeint NetSessionPtrRva)
    let nm = if np <> 0L then BitConverter.ToInt32(t.Read(nativeint np + 0x34n, 4), 0) else 0
    printfn "online session mode [[0x4709B50]+0x34] = %d (%s)" nm (if nm <> 0 then "ONLINE" else "solo")
    let installed = readAt (fst P1) 7 <> hexb (snd P1)
    printfn "P1 site: %s" (if installed then "PATCHED" else "original (patch not installed)")
    if installed then
      try
        match cave with
        | Some v when v.Length >= 6 ->
            let rd (i: int) = if v.Length > i then BitConverter.ToInt32(t.Read(nativeint v.[i], 4), 0) else -1
            printfn "cave: enable=%d fpsOut=%d tier=%d sessionModeSeenByCave=%d" (rd 1) (rd 2) (rd 5) (rd 6)
        | _ -> ()
      with _ -> printfn "cave state file is stale (cave from another process)"

match mode with
| "dry" ->
    printfn "base=%X  config: %s" B (String.Join(", ", [ for kv in cfg -> sprintf "%s=%d" kv.Key kv.Value ]))
    printfn "checking original bytes:"
    let ok = activeSites |> List.map checkSite |> List.forall id
    let at = B + 0x7000000UL
    let tier = BitConverter.ToInt32(t.Read(nativeint (G ()) + 0x618n, 4), 0)
    let c = build at B tier
    printfn "cave (dummy @%X, %d bytes, initTier=%d): fps=%X scale=%X lim=%X tier=%X menu=%X" at c.Bytes.Length tier c.Fps c.Scale c.Lim c.Tier c.Menu
    printDis c.Bytes.[int (c.Fps - at) ..] c.Fps
    printfn "site patches:"
    for (r, b) in sitePlan c do printfn "  %-9X %s" r (hexs b)
    printfn "originals %s" (if ok then "ALL MATCH" else "DIFFER (v1 installed? -> run revert first) - DO NOT APPLY")
| "verify" ->
    verify (if fsi.CommandLineArgs.Length > 2 then float fsi.CommandLineArgs.[2] else 5.0)
| "apply" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    if not (activeSites |> List.map checkSite |> List.forall id) then failwith "original bytes differ - run revert first"
    let caveMem = t.AllocNear(nativeint (s 0x5AD000), 0x1000)
    let tier = BitConverter.ToInt32(t.Read(nativeint (G ()) + 0x618n, 4), 0)
    let c = build (uint64 caveMem) B tier
    t.Write(caveMem, c.Bytes)
    File.WriteAllText(stateFile, sprintf "%X %X %X %X %X %X %X" (uint64 caveMem) c.Enable c.FpsOut c.Last c.ScaleV c.TierV c.NetMode)
    // safeWrite: never rewrite a site while a thread is paused mid-way through it (P3's lea/div pair has an
    // old-code boundary at +3 that is mid-instruction in the new `mov eax,[rip+x]`).
    let plan = sitePlan c |> List.map (fun (r, b) -> nativeint (s r), b, t.Read(nativeint (s r), b.Length))
    let attempts = safeWrite t plan
    printfn "sites written on attempt %d" attempts
    let dynA = t.Base + nativeint DynFpsRva
    let dynOrig = BitConverter.ToInt32(t.Read(dynA, 4), 0)
    File.WriteAllText(dynFile, sprintf "%d %d" t.Process.Id dynOrig)
    t.Write(dynA, BitConverter.GetBytes 0)
    printfn "DYNAMIC_FPS %d -> 0 while applied" dynOrig
    printfn "applied v2; cave at %X (state in %s)" (uint64 caveMem) stateFile
| "status" ->
    if readAt (fst P1) 7 = hexb (snd P1) then failwith "patch not installed in this process (P1 original)"
    let v = File.ReadAllText(stateFile).Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
    for _ in 1..10 do
        let en = BitConverter.ToInt32(t.Read(nativeint v.[1], 4), 0)
        let fps = BitConverter.ToInt32(t.Read(nativeint v.[2], 4), 0)
        let last = t.ReadI64(nativeint v.[3])
        let sc = t.ReadF32(nativeint v.[4])
        let tier = if v.Length > 5 then BitConverter.ToInt32(t.Read(nativeint v.[5], 4), 0) else -1
        let netCave = if v.Length > 6 then BitConverter.ToInt32(t.Read(nativeint v.[6], 4), 0) else -1
        let np = t.ReadI64(t.Base + nativeint NetSessionPtrRva)
        let netLive = if np <> 0L then BitConverter.ToInt32(t.Read(nativeint np + 0x34n, 4), 0) else 0
        printfn "enable=%d tier=%d armed fps_i=%d interval=%.3f ms scaleCorr=%.4f  session mode live=%d cave=%d (%s)" en tier fps (float last / 1e6) sc netLive netCave (if netLive <> 0 then "ONLINE -> stock path" else "solo")
        Threading.Thread.Sleep 50
| "off" | "on" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    let v = File.ReadAllText(stateFile).Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
    t.Write(nativeint v.[1], [| (if mode = "on" then 1uy else 0uy) |])
    printfn "enable=%s" mode
| "revert" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    // P1 first so the cave stops being entered, then the rest
    let order = [ P1; P4; P2 ] @ (allSites |> List.filter (fun (r, _) -> r <> fst P1 && r <> fst P4 && r <> fst P2))
    let plan = order |> List.map (fun (rva, orig) ->
        let o = hexb orig in nativeint (s rva), o, t.Read(nativeint (s rva), o.Length))
    safeWrite t plan |> ignore
    if File.Exists dynFile then
        let d = File.ReadAllText(dynFile).Trim().Split(' ') |> Array.map int
        if d.[0] = t.Process.Id then
            t.Write(t.Base + nativeint DynFpsRva, BitConverter.GetBytes d.[1])
            printfn "DYNAMIC_FPS restored to %d" d.[1]
    printfn "reverted all v1/v2 sites (cave left allocated)"
| m -> printfn "unknown mode %s" m
