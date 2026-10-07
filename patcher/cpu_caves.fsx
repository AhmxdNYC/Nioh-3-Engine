// cpu_caves.fsx — code-cave CPU/power fixes for Nioh 3 (v2.02). See notes\cpu-profile.md ("Caves").
//
//   dotnet fsi cpu_caves.fsx dry   [W3,R1,L1]   READ-ONLY: check original bytes + IAT slots in the live game,
//                                               build the cave at a dummy address, print cave + site disassembly.
//   dotnet fsi cpu_caves.fsx apply [W3,R1,L1]   WRITES (needs env FT_ALLOW_WRITE=1): alloc cave near the module,
//                                               write it, then patch the sites with the process frozen.
//   dotnet fsi cpu_caves.fsx status             READ-ONLY: flags + counters (10 x 100 ms).
//   dotnet fsi cpu_caves.fsx on|off [W3,R1,L1]  WRITES 4 bytes per patch: runtime switch (no code is touched).
//   dotnet fsi cpu_caves.fsx revert [W3,R1,L1]  WRITES: original bytes back (process frozen). Cave stays allocated.
//
// Patches (all default ON when applied):
//   W3  WinMain pump 1A6BF18..: PeekMessageA loop never waits. Call site 1A6BF2D -> cave: PeekMessageA, and if
//       the queue was empty: MsgWaitForMultipleObjectsEx(0,NULL,W3_MS,QS_ALLINPUT,MWMO_INPUTAVAILABLE).
//       MsgWait is not imported -> resolved once at runtime via GetModuleHandleW+GetProcAddress (both from the IAT).
//   R1  render-begin wait 4A5346..4A5359: `do { pause; } while (WaitForSingleObject(h,0))` (h = vtbl+108h object,
//       frame-latency waitable) -> `while (WaitForSingleObject(h, R1_MS))`. In place, no code in the cave: the
//       timeout is read from the cave (`mov edx,[rip+x]`), so off = timeout 0 = the stock poll (minus PAUSE).
//   L1  frame limiter slow path 19DF57E (`xor edx,edx; mov rcx,rsi; call 916EBC` = run a job or Sleep(0)) ->
//       cave: if more than L1_MARGIN_US + L1_MIN_US remain until the frame deadline, block on a HIGH-RESOLUTION
//       waitable timer for (remaining - margin); otherwise the original 916EBC(sched,0) fine spin.
//       Registers at the site (5ACF80): rdx=now, r12=last frame QPC, rdi=ticks/frame, rbx=QPC frequency, rsi=sched.
//   (W6 7D9E90 `while(flag) Sleep(0)` is deliberately NOT patched: ~0.4 ms waits on the bottleneck thread, no
//    waker exists, anything coarser than Sleep(0) adds main-thread latency.)
//
// Safety: original-byte check on every site; writes inside Mem.fsx t.Frozen; while frozen every thread's RIP is
// checked not to sit inside a patched range except at an instruction boundary valid in BOTH old and new code
// (retries up to 100x). Return addresses were chosen to be boundaries in both encodings, so threads blocked
// inside the caves (MsgWait / timer wait) survive a revert, and threads inside the original callees survive apply.
#load "Mem.fsx"
open System
open System.IO
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "dry"
let selected = (if args.Length > 1 then args.[1] else "W3,R1,L1").Split(',') |> Set.ofArray
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "cpu_caves_state.txt")
let IMPORTS = Path.Combine(__SOURCE_DIRECTORY__, @"..\re\out\imports.tsv")

// ---- tunables (also live-editable in the cave: see status output) -----------------------------------
let W3_MS = 1u            // MsgWait timeout: 47990 (cursor/IME state) still runs >= ~1 kHz (or at timer res)
let R1_MS = 2u            // WaitForSingleObject timeout per round (signal wakes immediately; only re-poll cadence)
let L1_MARGIN_US = 1000UL // keep the last 1 ms as the original fine spin
let L1_MIN_US = 500UL     // don't bother sleeping for less than this

// ---- sites -------------------------------------------------------------------------------------------
let W3_SITE, W3_ORIG = 0x1A6BF2D, "FF 15 95 6A E7 01"                       // call [PeekMessageA]
let L1_SITE, L1_ORIG = 0x19DF57E, "33 D2 48 8B CE E8 34 79 F3 FE"           // xor edx,edx; mov rcx,rsi; call 916EBC
let L1_CALLEE = 0x916EBCUL
let R1_SITE, R1_ORIG = 0x4A5346, "EB 02 F3 90 33 D2"                        // jmp +2; pause; xor edx,edx
let R1_JNE, R1_JNE_ORIG, R1_JNE_NEW = 0x4A5357, "75 EF", "75 ED"            // jne 4A5348 -> jne 4A5346
let FT_SITES = [ 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4 ]

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- IAT slots from re\out\imports.tsv (library, name, iat_rva, ...) --------------------------------------
let iat =
    File.ReadAllLines IMPORTS |> Array.skip 1
    |> Array.choose (fun l ->
        let f = l.Split('\t')
        if f.Length >= 3 && f.[2].StartsWith "0x" then Some ((f.[0].ToUpperInvariant() + "!" + f.[1]), Convert.ToUInt64(f.[2], 16)) else None)
    |> Array.filter (fun (_, r) -> r >= 0x38E2000UL && r < 0x38E3000UL)    // real IAT slots only (tsv also lists thunks)
    |> dict
let iatRva (name: string) = match iat.TryGetValue name with | true, v -> v | _ -> failwithf "import %s not in imports.tsv" name
let IAT_PEEK  = iatRva "USER32.DLL!PeekMessageA"
let IAT_GMHW  = iatRva "KERNEL32.DLL!GetModuleHandleW"
let IAT_GPA   = iatRva "KERNEL32.DLL!GetProcAddress"
let IAT_SWT   = iatRva "KERNEL32.DLL!SetWaitableTimer"
let IAT_WFSO  = iatRva "KERNEL32.DLL!WaitForSingleObject"

// ---- cave ------------------------------------------------------------------------------------------
type Cave = { Bytes: byte[]; W3: uint64; L1: uint64; EnW3: uint64; EnL1: uint64; R1Tmo: uint64; W3Ms: uint64
              CntW3: uint64; CntL1: uint64; MarginUs: uint64; ResolveFail: uint64; HTimer: uint64 }

let build (at: uint64) (B: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEnW3, lEnL1, lR1Tmo, lW3Ms, lCntW3, lCntL1, lFail = L(), L(), L(), L(), L(), L(), L()
    let mutable lMargin, lMin, lPMsg, lPCwt, lHTimer, lDue = L(), L(), L(), L(), L(), L()
    let mutable lWUser, lWKern, lSMsg, lSCwt = L(), L(), L(), L()
    let mutable cW3, cL1 = L(), L()
    let mutable w3done, w3have, w3fail, w3wait = L(), L(), L(), L()
    let mutable l1orig, l1haveT, l1skip, l1mk = L(), L(), L(), L()
    // ---- data (dword block first so the status reader has fixed offsets)
    a.Label(&lEnW3);  a.dd(1u)
    a.Label(&lEnL1);  a.dd(1u)
    a.Label(&lR1Tmo); a.dd(R1_MS)
    a.Label(&lW3Ms);  a.dd(W3_MS)
    a.Label(&lCntW3); a.dd(0u)
    a.Label(&lCntL1); a.dd(0u)
    a.Label(&lFail);  a.dd(0u)           // bit0: MsgWait unresolved, bit1: timer creation failed
    a.dd(0u)
    a.Label(&lMargin); a.dq(L1_MARGIN_US)
    a.Label(&lMin);    a.dq(L1_MIN_US)
    a.Label(&lPMsg);   a.dq(0UL)
    a.Label(&lPCwt);   a.dq(0UL)
    a.Label(&lHTimer); a.dq(0UL)
    a.Label(&lDue);    a.dq(0UL)
    let wstr (s: string) = a.db(Text.Encoding.Unicode.GetBytes(s + "\000"))
    let astr (s: string) = a.db(Text.Encoding.ASCII.GetBytes(s + "\000"))
    a.Label(&lWUser); wstr "user32.dll"
    a.Label(&lWKern); wstr "kernel32.dll"
    a.Label(&lSMsg);  astr "MsgWaitForMultipleObjectsEx"
    a.Label(&lSCwt);  astr "CreateWaitableTimerExW"
    a.int3(); a.int3(); a.int3(); a.int3()
    let callIat (rva: uint64) = a.mov(rax, B + rva); a.call(__qword_ptr.[rax])

    // ---- W3: replaces `call [PeekMessageA]` at 1A6BF2D (site = 90 E8 rel32 -> returns to 1A6BF33)
    // entry: rcx,rdx,r8,r9 = Peek args; caller's 5th arg at [rsp+8+20h]
    a.Label(&cW3)
    a.sub(rsp, 0x38)
    a.mov(rax, __qword_ptr.[rsp + 0x60L]); a.mov(__qword_ptr.[rsp + 0x20L], rax)
    callIat IAT_PEEK
    a.test(eax, eax); a.jne(w3done)
    a.cmp(__dword_ptr.[lEnW3], 0); a.je(w3done)
    a.mov(rax, __qword_ptr.[lPMsg]); a.test(rax, rax); a.jne(w3have)
    a.test(__dword_ptr.[lFail], 1); a.jne(w3fail)
    a.lea(rcx, __qword_ptr.[lWUser]); callIat IAT_GMHW
    a.test(rax, rax); a.je(w3fail)
    a.mov(rcx, rax); a.lea(rdx, __qword_ptr.[lSMsg]); callIat IAT_GPA
    a.test(rax, rax); a.je(w3fail)
    a.mov(__qword_ptr.[lPMsg], rax)
    a.Label(&w3have)
    a.xor(ecx, ecx); a.xor(edx, edx); a.mov(r8d, __dword_ptr.[lW3Ms]); a.mov(r9d, 0x4FF)   // QS_ALLINPUT
    a.mov(__dword_ptr.[rsp + 0x20L], 4)                                                    // MWMO_INPUTAVAILABLE
    a.call(rax)
    a.inc(__dword_ptr.[lCntW3])
    a.jmp(w3wait)
    a.Label(&w3fail)
    a.``or``(__dword_ptr.[lFail], 1)
    a.Label(&w3wait)
    a.xor(eax, eax)                       // PeekMessage result stays "no message"
    a.Label(&w3done)
    a.add(rsp, 0x38)
    a.ret()

    // ---- L1: replaces `xor edx,edx; mov rcx,rsi; call 916EBC` at 19DF57E (site = E8 rel32 + nop5; ret -> 19DF583)
    a.Label(&cL1)
    a.cmp(__dword_ptr.[lEnL1], 0); a.je(l1orig)
    a.mov(rax, rdx); a.sub(rax, r12)                 // elapsed ticks (caller guarantees now > last)
    a.mov(rcx, rdi); a.sub(rcx, rax); a.jbe(l1orig)  // remaining ticks
    a.mov(r8, rcx)                                   // r8 = remaining
    a.mov(rax, __qword_ptr.[lMargin]); a.add(rax, __qword_ptr.[lMin])
    a.imul(rax, rbx); a.xor(edx, edx); a.mov(r9, 1000000UL); a.div(r9)       // (margin+min) in ticks
    a.cmp(r8, rax); a.jbe(l1orig)
    a.mov(rax, __qword_ptr.[lMargin]); a.imul(rax, rbx); a.xor(edx, edx); a.div(r9)   // margin ticks
    a.sub(r8, rax)                                   // ticks to sleep
    a.mov(rax, r8); a.mov(r9, 10000000UL); a.mul(r9); a.div(rbx)               // -> 100 ns units
    a.neg(rax); a.mov(__qword_ptr.[lDue], rax)
    a.sub(rsp, 0x38)
    a.mov(rcx, __qword_ptr.[lHTimer]); a.test(rcx, rcx); a.jne(l1haveT)
    a.test(__dword_ptr.[lFail], 2); a.jne(l1mk)
    a.mov(rax, __qword_ptr.[lPCwt]); a.test(rax, rax); a.jne(l1skip)
    a.lea(rcx, __qword_ptr.[lWKern]); callIat IAT_GMHW
    a.test(rax, rax); a.je(l1mk)
    a.mov(rcx, rax); a.lea(rdx, __qword_ptr.[lSCwt]); callIat IAT_GPA
    a.test(rax, rax); a.je(l1mk)
    a.mov(__qword_ptr.[lPCwt], rax)
    a.Label(&l1skip)
    // CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)
    a.xor(ecx, ecx); a.xor(edx, edx); a.mov(r8d, 2); a.mov(r9d, 0x1F0003)
    a.call(rax)
    a.test(rax, rax); a.je(l1mk)
    a.mov(__qword_ptr.[lHTimer], rax); a.mov(rcx, rax)
    a.Label(&l1haveT)
    // SetWaitableTimer(h, &due, 0, NULL, NULL, FALSE); WaitForSingleObject(h, 100)
    a.lea(rdx, __qword_ptr.[lDue]); a.xor(r8d, r8d); a.xor(r9d, r9d)
    a.mov(__qword_ptr.[rsp + 0x20L], 0); a.mov(__dword_ptr.[rsp + 0x28L], 0)
    callIat IAT_SWT
    a.test(eax, eax); a.je(l1mk)
    a.mov(rcx, __qword_ptr.[lHTimer]); a.mov(edx, 100); callIat IAT_WFSO
    a.inc(__dword_ptr.[lCntL1])
    a.add(rsp, 0x38)
    a.ret()
    a.Label(&l1mk)                                   // timer unavailable: disable L1 for good, fall back to stock
    a.``or``(__dword_ptr.[lFail], 2); a.mov(__dword_ptr.[lEnL1], 0)
    a.add(rsp, 0x38)
    a.Label(&l1orig)
    a.xor(edx, edx); a.mov(rcx, rsi)
    a.mov(rax, B + L1_CALLEE); a.jmp(rax)            // tail call: 916EBC returns straight to 19DF583

    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    { Bytes = ms.ToArray(); W3 = addr cW3; L1 = addr cL1; EnW3 = addr lEnW3; EnL1 = addr lEnL1; R1Tmo = addr lR1Tmo
      W3Ms = addr lW3Ms; CntW3 = addr lCntW3; CntL1 = addr lCntL1; MarginUs = addr lMargin; ResolveFail = addr lFail
      HTimer = addr lHTimer }

// ---- site encodings -----------------------------------------------------------------------------------
let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let siteW3 (B: uint64) (c: Cave) = Array.concat [ [| 0x90uy; 0xE8uy |]; rel32 (B + uint64 W3_SITE + 6UL) c.W3 ]
let siteL1 (B: uint64) (c: Cave) = Array.concat [ [| 0xE8uy |]; rel32 (B + uint64 L1_SITE + 5UL) c.L1; [| 0x0Fuy; 0x1Fuy; 0x44uy; 0x00uy; 0x00uy |] ]
let siteR1 (B: uint64) (c: Cave) = Array.concat [ [| 0x8Buy; 0x15uy |]; rel32 (B + uint64 R1_SITE + 6UL) c.R1Tmo ]

/// (name, rva, original, new) for the selected patches
let edits (B: uint64) (c: Cave) =
    [ if selected.Contains "W3" then yield "W3", W3_SITE, hexb W3_ORIG, siteW3 B c
      if selected.Contains "L1" then yield "L1", L1_SITE, hexb L1_ORIG, siteL1 B c
      if selected.Contains "R1" then
        yield "R1", R1_SITE, hexb R1_ORIG, siteR1 B c
        yield "R1j", R1_JNE, hexb R1_JNE_ORIG, hexb R1_JNE_NEW ]

let boundaries (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    let s = Collections.Generic.HashSet<uint64>()
    while d.IP < ip + uint64 bytes.Length do s.Add d.IP |> ignore; d.Decode() |> ignore
    s
let printDis (bytes: byte[]) (at: uint64) (B: uint64) =
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- at
    let fmt = NasmFormatter()
    let out = StringOutput()
    while dec.IP < at + uint64 bytes.Length do
        let ins = dec.Decode()
        fmt.Format(&ins, out)
        let txt = out.ToStringAndReset()
        let note =
            if ins.Op1Kind = OpKind.Immediate64 && ins.Immediate64 >= B && ins.Immediate64 < B + 0x6000000UL then
                let r = ins.Immediate64 - B
                let nm = iat |> Seq.tryFind (fun kv -> kv.Value = r) |> Option.map (fun kv -> kv.Key) |> Option.defaultValue ""
                sprintf "   ; Nioh3+%X %s" r nm
            else ""
        printfn "  %016X  %-44s%s" ins.IP txt note

// ---- thread RIP check while frozen ----------------------------------------------------------------------
module TN =
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenThread(uint32 access, bool inh, int tid)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool GetThreadContext(nativeint h, nativeint ctx)
    [<DllImport("kernel32.dll")>]
    extern bool CloseHandle(nativeint h)
let threadRips (t: Target) =
    let mem = Marshal.AllocHGlobal(0x4D0 + 64)
    let ctx = nativeint ((int64 mem + 15L) &&& ~~~15L)
    t.Process.Refresh()
    let rips =
        [ for th in t.Process.Threads |> Seq.cast<Diagnostics.ProcessThread> do
            let h = TN.OpenThread(0x0008u, false, th.Id)
            if h <> 0n then
                Marshal.WriteInt32(ctx, 0x30, 0x100001)
                if TN.GetThreadContext(h, ctx) then yield th.Id, uint64 (Marshal.ReadInt64(ctx, 0xF8))
                TN.CloseHandle h |> ignore ]
    Marshal.FreeHGlobal mem
    rips
/// write `pairs` (rva, bytes-to-write, other-encoding) frozen, only when no thread RIP is inside a range at a
/// position that is not an instruction start in both encodings
let safeWrite (t: Target) (B: uint64) (pairs: (int * byte[] * byte[]) list) =
    let ok () =
        let rips = threadRips t
        pairs |> List.forall (fun (rva, nw, old) ->
            let s = B + uint64 rva
            let bn, bo = boundaries nw s, boundaries old s
            rips |> List.forall (fun (tid, rip) ->
                let inside = rip > s && rip < s + uint64 nw.Length
                if inside && not (bn.Contains rip && bo.Contains rip) then
                    printfn "  thread %d at %X inside site %X - retry" tid (rip - B) rva; false
                else true))
    let mutable attempt = 0
    let mutable fin = false
    while not fin do
        attempt <- attempt + 1
        if attempt > 100 then failwith "a thread kept sitting inside a patch site - nothing written"
        fin <- t.Frozen(fun () ->
            if ok () then
                for (rva, nw, _) in pairs do t.Write(nativeint (B + uint64 rva), nw)
                true
            else false)
        if not fin then Threading.Thread.Sleep 3

// ---- main ---------------------------------------------------------------------------------------------
let t = attach "Nioh3"
let B = uint64 t.Base
let readAt (rva: int) n = t.Read(nativeint (B + uint64 rva), n)
let classify (name, rva, o: byte[], n: byte[]) =
    let cur = readAt rva o.Length
    let st = if cur = o then "original" elif cur = n then "patched" else "UNKNOWN"
    printfn "  %-4s %8X  %-8s  %s" name rva st (hexs cur)
    st
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let overlap () =
    for (rva, len) in [ (W3_SITE, 6); (L1_SITE, 10); (R1_SITE, 6); (R1_JNE, 2) ] do
        for f in FT_SITES do
            if f < rva + len && rva < f + 8 then failwithf "site %X overlaps ft_vrr site %X" rva f
let readState () =
    if not (File.Exists stateFile) then failwith "no cpu_caves_state.txt - apply first"
    let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
    // cave EnW3 EnL1 R1Tmo W3Ms CntW3 CntL1 Margin Fail HTimer pid
    if v.Length < 11 || int v.[10] <> t.Process.Id then failwith "cpu_caves_state.txt belongs to another game process (relaunched?)"
    v

overlap ()
match mode with
| "dry" ->
    printfn "base=%X  selected=%A" B (Set.toList selected)
    printfn "IAT slots (live):"
    for (nm, r) in [ ("PeekMessageA", IAT_PEEK); ("GetModuleHandleW", IAT_GMHW); ("GetProcAddress", IAT_GPA)
                     ("SetWaitableTimer", IAT_SWT); ("WaitForSingleObject", IAT_WFSO) ] do
        let p = BitConverter.ToUInt64(readAt (int r) 8, 0)
        let m = t.Process.Modules |> Seq.cast<Diagnostics.ProcessModule>
                |> Seq.tryFind (fun m -> p >= uint64 (m.BaseAddress.ToInt64()) && p < uint64 (m.BaseAddress.ToInt64()) + uint64 m.ModuleMemorySize)
        printfn "  %-20s IAT Nioh3+%X -> %X (%s)" nm r p (match m with Some m -> m.ModuleName | None -> "?? not in a module")
    let at = B + 0x6000000UL
    let c = build at B
    printfn "cave (dummy @%X, %d bytes): W3=%X L1=%X data: enW3=%X enL1=%X r1Tmo=%X" at c.Bytes.Length c.W3 c.L1 c.EnW3 c.EnL1 c.R1Tmo
    printDis c.Bytes.[int (c.W3 - at) ..] c.W3 B
    printfn "sites (state | current bytes) and new code:"
    let es = edits B c
    let states = es |> List.map classify
    for (name, rva, o, n) in es do
        printfn "  %s new %s" name (hexs n)
    // show the full new instruction stream around each site, decoded
    let showNew (rva: int) (len: int) (patches: (int * byte[]) list) =
        let buf = readAt rva len
        for (pr, pb) in patches do Array.blit pb 0 buf (pr - rva) pb.Length
        printfn " -- after patch @%X:" rva
        printDis buf (B + uint64 rva) B
    for (name, rva, o, n) in es do
        match name with
        | "W3" -> showNew (W3_SITE - 0x15) 0x30 [ W3_SITE, n ]
        | "L1" -> showNew L1_SITE 0x1B [ L1_SITE, n ]
        | "R1" -> showNew 0x4A533E 0x1B [ R1_SITE, n; R1_JNE, hexb R1_JNE_NEW ]
        | _ -> ()
    printfn "originals: %s" (if states |> List.forall (fun s -> s = "original") then "ALL MATCH" else sprintf "%A" states)
    let rips = threadRips t
    printfn "live thread RIPs inside site ranges right now: %d (informational)"
        (rips |> List.filter (fun (_, r) -> es |> List.exists (fun (_, rva, o, _) -> r >= B + uint64 rva && r < B + uint64 rva + uint64 o.Length)) |> List.length)
| "apply" ->
    writeGuard ()
    let probe = build (B + 0x6000000UL) B
    let es0 = edits B probe
    if es0 |> List.exists (fun e -> classify e <> "original") then failwith "sites not original - aborting"
    // one cave per process: all three call sites must still be stock, otherwise use on/off or revert first
    for (rva, o) in [ W3_SITE, W3_ORIG; L1_SITE, L1_ORIG; R1_SITE, R1_ORIG ] do
        if readAt rva (hexb o).Length <> hexb o then failwithf "site %X already patched - apply all patches in ONE call; use on/off to A/B" rva
    let caveMem = t.AllocNear(nativeint (B + 0x1A00000UL), 0x1000)
    let c = build (uint64 caveMem) B
    let es = edits B c
    t.Write(caveMem, c.Bytes)
    File.WriteAllText(stateFile, String.Join(" ", [ uint64 caveMem; c.EnW3; c.EnL1; c.R1Tmo; c.W3Ms; c.CntW3; c.CntL1; c.MarginUs; c.ResolveFail; c.HTimer; uint64 t.Process.Id ] |> List.map (sprintf "%X")))
    safeWrite t B (es |> List.map (fun (_, rva, o, n) -> rva, n, o))
    printfn "applied %A; cave at %X (state %s)" (Set.toList selected) (uint64 caveMem) stateFile
| "revert" ->
    writeGuard ()
    let v = readState ()
    // rebuild the cave image at its real address to know the patched bytes
    let c = build v.[0] B
    let es = edits B c
    let st = es |> List.map classify
    if st |> List.exists (fun s -> s = "UNKNOWN") then failwith "unexpected bytes - aborting"
    safeWrite t B (es |> List.filter (fun (_, rva, o, n) -> readAt rva o.Length = n) |> List.map (fun (_, rva, o, n) -> rva, o, n))
    printfn "reverted %A (cave left allocated)" (Set.toList selected)
| "on" | "off" ->
    writeGuard ()
    let v = readState ()
    let on = mode = "on"
    if selected.Contains "W3" then t.Write(nativeint v.[1], BitConverter.GetBytes(if on then 1u else 0u))
    if selected.Contains "L1" then t.Write(nativeint v.[2], BitConverter.GetBytes(if on then 1u else 0u))
    if selected.Contains "R1" then t.Write(nativeint v.[3], BitConverter.GetBytes(if on then R1_MS else 0u))   // 0 = stock poll
    printfn "%s %A" mode (Set.toList selected)
| "status" ->
    let v = readState ()
    let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
    let mutable pw, pl = rd v.[5], rd v.[6]
    printfn "cave %X  enW3=%d enL1=%d r1Timeout=%dms w3Timeout=%dms marginUs=%d fail=%d timer=%X" v.[0] (rd v.[1]) (rd v.[2]) (rd v.[3]) (rd v.[4])
        (BitConverter.ToUInt64(t.Read(nativeint v.[7], 8), 0)) (rd v.[8]) (BitConverter.ToUInt64(t.Read(nativeint v.[9], 8), 0))
    for _ in 1 .. 10 do
        Threading.Thread.Sleep 100
        let w, l = rd v.[5], rd v.[6]
        printfn "  W3 waits/s %6d   L1 timer sleeps/s %5d" ((w - pw) * 10u) ((l - pl) * 10u)
        pw <- w; pl <- l
| m -> printfn "unknown mode %s" m
