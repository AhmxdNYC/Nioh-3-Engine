// Nioh 3 "variable-dt mode" patch (frame-timing agent). See notes\frame-timing.md §5.
//
//   dotnet fsi ft_vrr_patch.fsx dry            -> READ-ONLY: verify original bytes at every site, assemble the
//                                                 cave at a dummy address and print the disassembly. No writes.
//   dotnet fsi ft_vrr_patch.fsx apply          -> WRITES (requires env FT_ALLOW_WRITE=1). Allocates the cave,
//                                                 writes it, then patches sites in a safe order (P3,P2,P4,P1).
//                                                 Saves the cave address to ft_vrr_state.txt.
//   dotnet fsi ft_vrr_patch.fsx status         -> READ-ONLY: reads the cave diagnostics (fps_i, interval, scale).
//   dotnet fsi ft_vrr_patch.fsx off|on         -> WRITES 1 byte: runtime kill switch (cave enable flag).
//   dotnet fsi ft_vrr_patch.fsx revert         -> WRITES: restores original bytes (P1 first). Cave stays allocated.
//
// All sites are RVAs in the build whose image matches re\Nioh3.dump.exe; every write is preceded by an
// original-bytes check and aborts on mismatch.
#load "Mem.fsx"
open System
open System.IO
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let mode = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else "dry"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "ft_vrr_state.txt")

// ---- configuration --------------------------------------------------------------------------------
let ARM_FPS   = 120      // only act when the tier fps (G+618 at frame end) is this value
let MAX_FPS   = 120      // clamp: shortest interval accepted
let MIN_FPS   = 48       // clamp: longest interval accepted (hitches beyond this lose time, like stock)
let CAP_FPS   = 120      // limiter cap while armed (set e.g. 116 to stay inside a 120 Hz VRR window)

// ---- sites (RVA, original bytes) ----------------------------------------------------------------------
let P1 = 0x5AD677, "44 89 87 14 06 00 00"        // mov [rdi+614h],r8d      -> call caveFps ; 2-byte nop
let P2 = 0x5AD6AA, "F3 0F 11 97 24 06 00 00"     // movss [rdi+624h],xmm2   -> call caveScale ; 3-byte nop
let P4 = 0x5AD860, "8B 87 14 06 00 00"           // mov eax,[rdi+614h] (limiter fps arg) -> call caveLim ; nop
let P3 = [ 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4 ]
let P3orig = "8D 42 78 41 F7 F0"                 // lea eax,[rdx+78h]; div r8d -> mov eax,[ticks120]
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- cave ------------------------------------------------------------------------------------------
type Cave = { Bytes: byte[]; Fps: uint64; Scale: uint64; Lim: uint64; Ticks: uint64; Enable: uint64
              FpsOut: uint64; Last: uint64; ScaleV: uint64 }

let build (at: uint64) =
    let a = Assembler(64)
    let mutable lPrev = a.CreateLabel()
    let mutable lMinNs = a.CreateLabel()
    let mutable lMaxNs = a.CreateLabel()
    let mutable lAcc = a.CreateLabel()
    let mutable lLast = a.CreateLabel()
    let mutable l1e9 = a.CreateLabel()
    let mutable l120 = a.CreateLabel()
    let mutable lEnable = a.CreateLabel()
    let mutable lArm = a.CreateLabel()
    let mutable lScale = a.CreateLabel()
    let mutable lTicks = a.CreateLabel()
    let mutable lFpsOut = a.CreateLabel()
    let mutable lCap = a.CreateLabel()
    let mutable cFps = a.CreateLabel()
    let mutable cScale = a.CreateLabel()
    let mutable cLim = a.CreateLabel()
    let mutable lStock = a.CreateLabel()
    let mutable lDone = a.CreateLabel()
    let mutable l1 = a.CreateLabel()
    let mutable l2 = a.CreateLabel()
    let mutable lR = a.CreateLabel()
    // data (8-byte fields first)
    a.Label(&lPrev);  a.dq(0UL)
    a.Label(&lMinNs); a.dq(uint64 (1_000_000_000L / int64 MAX_FPS))
    a.Label(&lMaxNs); a.dq(uint64 (1_000_000_000L / int64 MIN_FPS))
    a.Label(&lAcc);   a.dq(uint64 (BitConverter.DoubleToInt64Bits 0.0))
    a.Label(&lLast);  a.dq(0UL)
    a.Label(&l1e9);   a.dq(uint64 (BitConverter.DoubleToInt64Bits 1e9))
    a.Label(&l120);   a.dq(uint64 (BitConverter.DoubleToInt64Bits 120.0))
    a.Label(&lEnable); a.dd(1u)
    a.Label(&lArm);    a.dd(uint32 ARM_FPS)
    a.Label(&lScale);  a.dd(uint32 (BitConverter.SingleToInt32Bits 1.0f))
    a.Label(&lTicks);  a.dd(1u)                       // 120/120: valid before P1 ever runs
    a.Label(&lFpsOut); a.dd(0u)                       // 0 = not armed this frame
    a.Label(&lCap);    a.dd(uint32 CAP_FPS)
    a.int3(); a.int3(); a.int3(); a.int3()
    // ---- caveFps: replaces `mov [rdi+614h],r8d` in 0x5AD5F8. rdi=G, r8d=tier fps, eax=old G[620] (live!)
    a.Label(&cFps)
    a.push(rax); a.push(rcx); a.push(rdx)
    a.sub(rsp, 0x20)
    a.movdqu(__xmmword_ptr.[rsp], xmm0)
    a.movdqu(__xmmword_ptr.[rsp + 0x10L], xmm1)
    a.mov(rcx, __qword_ptr.[rdi + 0x7D8L])            // this frame's start (ns), written in 0x5AC664
    a.xchg(__qword_ptr.[lPrev], rcx)                 // rcx = previous frame start
    a.cmp(__dword_ptr.[lEnable], 0); a.je(lStock)
    a.cmp(r8d, __dword_ptr.[lArm]); a.jne(lStock)    // only in the 120 tier
    a.cmp(eax, r8d); a.jne(lStock)                   // event/cutscene override (G[620] != tier) -> stock
    a.test(rcx, rcx); a.je(lStock)
    a.mov(rax, __qword_ptr.[rdi + 0x7D8L]); a.sub(rax, rcx)
    a.cmp(rax, __qword_ptr.[lMinNs]); a.jge(l1); a.mov(rax, __qword_ptr.[lMinNs]); a.Label(&l1)
    a.cmp(rax, __qword_ptr.[lMaxNs]); a.jle(l2); a.mov(rax, __qword_ptr.[lMaxNs]); a.Label(&l2)
    a.mov(__qword_ptr.[lLast], rax)
    a.cvtsi2sd(xmm1, rax)                            // interval ns
    a.movsd(xmm0, __qword_ptr.[l1e9]); a.divsd(xmm0, xmm1); a.cvtsd2si(ecx, xmm0)   // fps_i = round(1e9/interval)
    a.mov(r8d, ecx); a.mov(__dword_ptr.[lFpsOut], ecx)
    a.cvtsi2sd(xmm0, ecx); a.mulsd(xmm0, xmm1); a.divsd(xmm0, __qword_ptr.[l1e9])
    a.cvtsd2ss(xmm0, xmm0); a.movss(__dword_ptr.[lScale], xmm0)                       // scaleCorr = fps_i*interval
    a.movapd(xmm0, xmm1); a.mulsd(xmm0, __qword_ptr.[l120]); a.divsd(xmm0, __qword_ptr.[l1e9])
    a.addsd(xmm0, __qword_ptr.[lAcc]); a.cvttsd2si(ecx, xmm0); a.cvtsi2sd(xmm1, ecx)
    a.subsd(xmm0, xmm1); a.movsd(__qword_ptr.[lAcc], xmm0); a.mov(__dword_ptr.[lTicks], ecx) // 120 Hz ticks w/ carry
    a.jmp(lDone)
    a.Label(&lStock)
    a.mov(__dword_ptr.[lScale], 0x3F800000)
    a.mov(__dword_ptr.[lFpsOut], 0)
    a.mov(ecx, r8d); a.test(ecx, ecx); a.je(lDone)
    a.mov(eax, 120); a.xor(edx, edx); a.div(ecx); a.mov(__dword_ptr.[lTicks], eax)          // stock 120/fps
    a.Label(&lDone)
    a.mov(__dword_ptr.[rdi + 0x614L], r8d)            // the replaced instruction
    a.movdqu(xmm1, __xmmword_ptr.[rsp + 0x10L])
    a.movdqu(xmm0, __xmmword_ptr.[rsp])
    a.add(rsp, 0x20)
    a.pop(rdx); a.pop(rcx); a.pop(rax)
    a.ret()
    // ---- caveScale: replaces `movss [rdi+624h],xmm2`
    a.Label(&cScale)
    a.mulss(xmm2, __dword_ptr.[lScale])
    a.movss(__dword_ptr.[rdi + 0x624L], xmm2)
    a.ret()
    // ---- caveLim: replaces `mov eax,[rdi+614h]` that feeds SetLimiterFps (profile vf+0x20)
    a.Label(&cLim)
    a.mov(eax, __dword_ptr.[rdi + 0x614L])
    a.cmp(__dword_ptr.[lFpsOut], 0); a.je(lR)
    a.mov(eax, __dword_ptr.[lCap])
    a.Label(&lR)
    a.ret()
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    { Bytes = ms.ToArray(); Fps = addr cFps; Scale = addr cScale; Lim = addr cLim; Ticks = addr lTicks
      Enable = addr lEnable; FpsOut = addr lFpsOut; Last = addr lLast; ScaleV = addr lScale }

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
let readAt (rva: int) n = t.Read(nativeint (B + uint64 rva), n)
let checkSite (rva, orig: string) =
    let o = hexb orig
    let cur = readAt rva o.Length
    let ok = cur = o
    printfn "  %-9X %s  %s" rva (if ok then "OK      " else "MISMATCH") (hexs cur)
    ok
let sites = [ P1; P2; P4 ] @ (P3 |> List.map (fun r -> r, P3orig))

match mode with
| "dry" ->
    printfn "base=%X  checking original bytes:" B
    let allOk = sites |> List.forall checkSite
    let at = B + 0x7000000UL
    let c = build at
    printfn "cave (dummy @%X, %d bytes): fps=%X scale=%X lim=%X ticks=%X" at c.Bytes.Length c.Fps c.Scale c.Lim c.Ticks
    printDis c.Bytes.[int (c.Fps - at) ..] c.Fps
    printfn "site patches:"
    let s rva = B + uint64 rva
    printfn "  P1 %s" (hexs (callPatch (s (fst P1)) c.Fps 7))
    printfn "  P2 %s" (hexs (callPatch (s (fst P2)) c.Scale 8))
    printfn "  P4 %s" (hexs (callPatch (s (fst P4)) c.Lim 6))
    for r in P3 do printfn "  P3 %X %s" r (hexs (movEaxRip (s r) c.Ticks))
    printfn "originals %s" (if allOk then "ALL MATCH" else "MISMATCH - DO NOT APPLY")
| "apply" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    if not (sites |> List.forall checkSite) then failwith "original bytes mismatch - aborting"
    let caveMem = t.AllocNear(nativeint (B + 0x5AD000UL), 0x1000)
    let c = build (uint64 caveMem)
    t.Write(caveMem, c.Bytes)
    File.WriteAllText(stateFile, sprintf "%X %X %X %X %X" (uint64 caveMem) c.Enable c.FpsOut c.Last c.ScaleV)
    let s rva = B + uint64 rva
    // Process frozen for the site writes: they are 6-8 byte rewrites of hot game-thread code.
    t.Frozen(fun () ->
        for r in P3 do t.Write(nativeint (s r), movEaxRip (s r) c.Ticks)    // ticks preset to 1 (=120/120)
        t.Write(nativeint (s (fst P2)), callPatch (s (fst P2)) c.Scale 8)    // scale preset 1.0 -> no-op
        t.Write(nativeint (s (fst P4)), callPatch (s (fst P4)) c.Lim 6)      // fpsOut 0 -> no-op
        t.Write(nativeint (s (fst P1)), callPatch (s (fst P1)) c.Fps 7))     // activates
    printfn "applied; cave at %X (state in %s)" (uint64 caveMem) stateFile
| "status" ->
    let v = File.ReadAllText(stateFile).Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
    for _ in 1..10 do
        let en = t.Read(nativeint v.[1], 4).[0]
        let fps = BitConverter.ToInt32(t.Read(nativeint v.[2], 4), 0)
        let last = t.ReadI64(nativeint v.[3])
        let sc = t.ReadF32(nativeint v.[4])
        printfn "enable=%d armed fps_i=%d interval=%.3f ms scaleCorr=%.4f" en fps (float last / 1e6) sc
        Threading.Thread.Sleep 50
| "off" | "on" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    let v = File.ReadAllText(stateFile).Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
    t.Write(nativeint v.[1], [| (if mode = "on" then 1uy else 0uy) |])
    printfn "enable=%s" mode
| "revert" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    t.Frozen(fun () ->
        for (rva, orig) in [ P1; P4; P2 ] @ (P3 |> List.map (fun r -> r, P3orig)) do
            t.Write(nativeint (B + uint64 rva), hexb orig))
    printfn "reverted all sites (cave left allocated)"
| m -> printfn "unknown mode %s" m
