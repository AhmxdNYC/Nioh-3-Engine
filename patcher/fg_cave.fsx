// fg_cave.fsx — F1: FSR3 frame-generation presenter pacing wait (Nioh 3 v2.02). See notes\cpu-profile.md (W5/S5).
//
//   dotnet fsi fg_cave.fsx dry       READ-ONLY: check the site + IAT, build the cave at a dummy address, print it.
//   dotnet fsi fg_cave.fsx apply     WRITES (FT_ALLOW_WRITE=1): alloc cave near the module, write it, patch the call.
//   dotnet fsi fg_cave.fsx status    READ-ONLY: enable flag + timer sleeps per second.
//   dotnet fsi fg_cave.fsx on|off    WRITES 4 bytes: runtime switch (off = the stock QPC spin).
//   dotnet fsi fg_cave.fsx revert    WRITES: original call back. Cave stays allocated.
//
// Stock: the "AMD FSR Presenter Thread" (FidelityFX FrameInterpolationSwapChain, statically linked) paces the
// interpolated vs real present with 370F578 waitUntil(t) = `do QueryPerformanceCounter(&now) while (now < t)`,
// a pure spin (~0.4 core with FG on; on a 6c/12t CPU it can share a physical core with the main thread).
// Its single caller: 3777A40 `call 370F578` (rcx = target QPC).
// F1: that call -> cave: if more than MARGIN+MIN remain, block on a high-resolution waitable timer for
// (remaining - MARGIN), then tail-jump to the original spin for the last MARGIN (pacing accuracy unchanged).
#load "Mem.fsx"
open System
open System.IO
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "dry"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "fg_cave_state.txt")

let MARGIN_US = 1000UL      // last 1 ms stays the original spin
let MIN_US = 500UL          // don't sleep for less than this
let SITE, SITE_ORIG = 0x3777A40, "E8 33 7B F9 FF"         // call 370F578
let CALLEE = 0x370F578UL
let IAT_QPC  = 0x38E2420UL  // KERNEL32!QueryPerformanceCounter
let IAT_GMHW = 0x38E2528UL  // KERNEL32!GetModuleHandleW
let IAT_GPA  = 0x38E2358UL  // KERNEL32!GetProcAddress
let IAT_SWT  = 0x38E26B0UL  // KERNEL32!SetWaitableTimer
let IAT_WFSO = 0x38E2340UL  // KERNEL32!WaitForSingleObject

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")
let freq = Diagnostics.Stopwatch.Frequency |> uint64       // QPC frequency (same for every process on this boot)

type Cave = { Bytes: byte[]; Entry: uint64; En: uint64; Cnt: uint64; Fail: uint64 }

let build (at: uint64) (B: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, lCnt, lFail, lMarginT, lThreshT, lFreq, lPCwt, lHTimer = L(), L(), L(), L(), L(), L(), L(), L()
    let mutable lWKern, lSCwt, entry, spin, haveT, skip, mk = L(), L(), L(), L(), L(), L(), L()
    a.Label(&lEn);     a.dd(1u)
    a.Label(&lCnt);    a.dd(0u)
    a.Label(&lFail);   a.dd(0u)
    a.dd(0u)
    a.Label(&lMarginT); a.dq(MARGIN_US * freq / 1000000UL)
    a.Label(&lThreshT); a.dq((MARGIN_US + MIN_US) * freq / 1000000UL)
    a.Label(&lFreq);    a.dq(freq)
    a.Label(&lPCwt);    a.dq(0UL)
    a.Label(&lHTimer);  a.dq(0UL)
    a.Label(&lWKern); a.db(Text.Encoding.Unicode.GetBytes("kernel32.dll\000"))
    a.Label(&lSCwt);  a.db(Text.Encoding.ASCII.GetBytes("CreateWaitableTimerExW\000"))
    a.int3(); a.int3(); a.int3(); a.int3()
    let callIat (rva: uint64) = a.mov(rax, B + rva); a.call(__qword_ptr.[rax])

    // entry: rcx = target QPC. rsp = 8 mod 16. Frame: push rbx (0 mod 16) + 0x40 -> [rsp+30h] now, [rsp+38h] due.
    a.Label(&entry)
    a.push(rbx); a.sub(rsp, 0x40)
    a.mov(rbx, rcx)
    a.cmp(__dword_ptr.[lEn], 0); a.je(spin)
    a.test(__dword_ptr.[lFail], 1); a.jne(spin)
    a.lea(rcx, __qword_ptr.[rsp + 0x30L]); callIat IAT_QPC
    a.mov(rax, rbx); a.sub(rax, __qword_ptr.[rsp + 0x30L])          // remaining ticks (signed)
    a.cmp(rax, __qword_ptr.[lThreshT]); a.jle(spin)
    a.sub(rax, __qword_ptr.[lMarginT])                               // ticks to sleep
    a.mov(rcx, 10000000UL); a.mul(rcx); a.div(__qword_ptr.[lFreq])   // -> 100 ns units
    a.neg(rax); a.mov(__qword_ptr.[rsp + 0x38L], rax)                // relative due time
    a.mov(rcx, __qword_ptr.[lHTimer]); a.test(rcx, rcx); a.jne(haveT)
    a.mov(rax, __qword_ptr.[lPCwt]); a.test(rax, rax); a.jne(skip)
    a.lea(rcx, __qword_ptr.[lWKern]); callIat IAT_GMHW
    a.test(rax, rax); a.je(mk)
    a.mov(rcx, rax); a.lea(rdx, __qword_ptr.[lSCwt]); callIat IAT_GPA
    a.test(rax, rax); a.je(mk)
    a.mov(__qword_ptr.[lPCwt], rax)
    a.Label(&skip)
    // CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)
    a.xor(ecx, ecx); a.xor(edx, edx); a.mov(r8d, 2); a.mov(r9d, 0x1F0003)
    a.call(rax)
    a.test(rax, rax); a.je(mk)
    a.mov(__qword_ptr.[lHTimer], rax); a.mov(rcx, rax)
    a.Label(&haveT)
    // SetWaitableTimer(h, &due, 0, NULL, NULL, FALSE); WaitForSingleObject(h, 100)
    a.lea(rdx, __qword_ptr.[rsp + 0x38L]); a.xor(r8d, r8d); a.xor(r9d, r9d)
    a.mov(__qword_ptr.[rsp + 0x20L], 0); a.mov(__dword_ptr.[rsp + 0x28L], 0)
    callIat IAT_SWT
    a.test(eax, eax); a.je(mk)
    a.mov(rcx, __qword_ptr.[lHTimer]); a.mov(edx, 100); callIat IAT_WFSO
    a.inc(__dword_ptr.[lCnt])
    a.jmp(spin)
    a.Label(&mk)                                       // timer unavailable: stock spin from now on
    a.``or``(__dword_ptr.[lFail], 1)
    a.Label(&spin)                                     // original wait for the rest (returns to 3777A45)
    a.mov(rcx, rbx); a.add(rsp, 0x40); a.pop(rbx)
    a.mov(rax, B + CALLEE); a.jmp(rax)

    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    { Bytes = ms.ToArray(); Entry = addr entry; En = addr lEn; Cnt = addr lCnt; Fail = addr lFail }

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let siteBytes (B: uint64) (c: Cave) = Array.append [| 0xE8uy |] (rel32 (B + uint64 SITE + 5UL) c.Entry)

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
let cur () = t.Read(nativeint (B + uint64 SITE), 5)
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let readState () =
    if not (File.Exists stateFile) then failwith "no fg_cave_state.txt - apply first"
    let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
    if v.Length < 5 || int v.[4] <> t.Process.Id then failwith "fg_cave_state.txt belongs to another game process"
    v   // cave entry en cnt pid

match mode with
| "dry" ->
    printfn "base=%X freq=%d site=%s (%s)" B freq (hexs (cur ())) (if cur () = hexb SITE_ORIG then "original" else "NOT original")
    let at = B + 0x6000000UL
    let c = build at B
    printfn "cave %d bytes, entry %X" c.Bytes.Length c.Entry
    printDis c.Bytes.[int (c.Entry - at) ..] c.Entry
| "apply" ->
    writeGuard ()
    if cur () <> hexb SITE_ORIG then failwithf "site not original (%s) - revert first" (hexs (cur ()))
    let caveMem = t.AllocNear(nativeint (B + uint64 SITE), 0x1000)
    let c = build (uint64 caveMem) B
    t.Write(caveMem, c.Bytes)
    File.WriteAllText(stateFile, String.Join(" ", [ uint64 caveMem; c.Entry; c.En; c.Cnt; uint64 t.Process.Id ] |> List.map (sprintf "%X")))
    let n = safeWrite t [ nativeint (B + uint64 SITE), siteBytes B c, hexb SITE_ORIG ]
    printfn "F1 applied on attempt %d; cave at %X" n (uint64 caveMem)
| "revert" ->
    writeGuard ()
    let v = readState ()
    let c = build v.[0] B
    if cur () = siteBytes B c then
        safeWrite t [ nativeint (B + uint64 SITE), hexb SITE_ORIG, siteBytes B c ] |> ignore
        printfn "F1 reverted (cave left allocated)"
    else printfn "F1 not installed (site %s)" (hexs (cur ()))
| "on" | "off" ->
    writeGuard ()
    let v = readState ()
    t.Write(nativeint v.[2], BitConverter.GetBytes(if mode = "on" then 1u else 0u))
    printfn "F1 %s" mode
| "status" ->
    let v = readState ()
    let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
    let mutable p = rd v.[3]
    printfn "F1 en=%d fail=%d site=%s" (rd v.[2]) (rd (v.[2] + 8UL)) (hexs (cur ()))
    for _ in 1 .. 5 do
        Threading.Thread.Sleep 200
        let n = rd v.[3]
        printfn "  timer sleeps/s %d" ((n - p) * 5u)
        p <- n
| m -> printfn "unknown mode %s" m
