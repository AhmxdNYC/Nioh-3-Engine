// input_live.fsx - [App]InputUpdateWorker patch for Nioh 3 (Steam v2.02). See notes\input-thread.md.
//
//   dotnet fsi input_live.fsx offline         NO GAME ACCESS: verify the site + its context against re\Nioh3.dump.exe
//                                             (raw offset == RVA), pattern uniqueness, branch targets inside the patched
//                                             bytes (whole .text sweep), dead registers/flags after the call, overlap with
//                                             every other patch set, and run the cave natively (own process) against a
//                                             model of the original call.
//   dotnet fsi input_live.fsx dry             READ-ONLY on the live game: site bytes + cave preview.
//   dotnet fsi input_live.fsx status          READ-ONLY: site state, switch, skipped/passed XInputGetState calls per s.
//   dotnet fsi input_live.fsx apply           WRITES (needs FT_ALLOW_WRITE=1): allocate the cave, patch the site.
//   dotnet fsi input_live.fsx on|off          WRITES (FT_ALLOW_WRITE=1): 4-byte cave switch (off = every call passes
//                                             straight to XInputGetState = stock behaviour). No code touched.
//   dotnet fsi input_live.fsx revert          WRITES (FT_ALLOW_WRITE=1): original 6 bytes back (cave stays allocated).
//
// Finding (static): the thread is NOT a polling loop. Its entry 1A7B8B4 is `KtglEventWait(start); vf[2](obj);
// KtglEventSet(done)` and it is kicked once per game frame by 5AB7B0 (UpdateDispatchB, from FrameDtSetup), joined by
// 5AB7E0 (GameModeUpdateMain), and the pad stage is awaited by FrameEarlyUpdate (86C018: KtglEventWait <= 1 ms).
// So it already runs at the frame rate; pacing it to 1 kHz would ADD wake-ups. No pacing patch is proposed.
//
// X1  (the one safe saving that exists in the body) - per-frame "which pad is active" scan 435B8C calls
//     XInputGetState(0..3) on EVERY slot every frame, connected or not, then once more for the active slot.
//     On Windows XInput 1.4 re-enumerates devices on each call for an empty slot (profile: CM_Get_Device_Interface_
//     List / DeviceIoControl under XInputGetState = 0.5-2.7 % of the thread's wall time, most of its CPU).
//     434E18 (runs earlier in the same update) already keeps a per-slot "connected" byte at padMgr+69850h+i*14h
//     (set by its every-2-s probe of all 4 slots, cleared the moment a per-frame poll fails).
//     Site 435BB7 `call [XInputGetState]` (scan loop only, FF 15 rel32, 6 B) -> `nop; call cave`.
//     Cave: if switch on && idx<4 && padMgr && !connected[idx] -> return ERROR_DEVICE_NOT_CONNECTED (0x48F) without
//     calling the API; else tail-jmp through the (current) IAT slot. The final call for the active slot (435BD7) is
//     left stock, so a pad keeps working even before the first 2-s probe.
//     Clobbers rax, r11, flags only (r11: volatile, never an argument). padMgr = [Nioh3+45BA1E0] (the rcx of 435B8C).
//
// Safety model = re\scripts_workers\workers_live.fsx: original-byte check, Mem.fsx safeWrite (process frozen,
// thread-RIP boundary check), revert leaves the cave allocated, state file per game pid.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "input_live_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let img = File.ReadAllBytes DUMP
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- constants (RVAs, v2.02) ----------------------------------------------------------------------------------
let X1_SITE = 0x435BB7                   // call [XInputGetState] inside the 435B8C scan loop
let X1_ORIG = "FF 15 93 D0 4A 03"        // FF 15 disp32 -> IAT 38E2C50 (verified below)
let X1_LEN = 6
let IAT_XINPUTGETSTATE = 0x38E2C50UL     // XINPUT1_4.dll ord 2 (XInputGetState)
let G_PADMGR = 0x45BA1E0UL               // global: pad manager pointer (rcx of 434E18 and 435B8C)
let CONNECTED_OFF = 0x69850              // padMgr + 69850h + i*14h : u8 connected (434E18)
let SLOT_STRIDE = 0x14
let ERROR_DEVICE_NOT_CONNECTED = 0x48Fu
// pattern for the mod (exec sections, must match once); offset of the patched instruction inside it
let X1_PATTERN = "48 8B FA 48 8B F1 33 DB 48 8B D7 8B CB FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? FF C3 83 FB 04 7C E6"
let X1_PAT_OFF = 13
// aux pattern the mod uses to derive G_PADMGR (435B48 `mov rcx,[rip+disp]` -> call 435B8C)
let PADMGR_PATTERN = "48 8B 0D ?? ?? ?? ?? 48 8D 54 24 30 0F 57 C0 0F 11 44 24 30 E8 ?? ?? ?? ?? 84 C0"
let PADMGR_PAT_OFF = 0

/// sites of the other patch sets (cpu_caves W3/R1/L1, fg_cave F1, ft_vrr, mainthread, W1/W2, meshlet, pool, workers
/// M1/M2/J1, mod patches.json) - must not overlap
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4
      0x419A8; 0x4ABD2; 0x1E72C6; 0x1E71CD; 0x3777A40; 0x1FE30D5; 0x1975BAA; 0x20E77B; 0x5AF526; 0x5AF682; 0x5AF69F
      0x5AD89B; 0x43312B; 0x49131; 0x48FB9; 0x49046; 0x49261; 0x4933D; 0x493DC; 0x4122D; 0x41265; 0x41397; 0x414B6
      0x40899; 0x40481; 0x3E7F5; 0x3E821 ]

// ---- cave -----------------------------------------------------------------------------------------------------
/// Data line 0 (read-mostly): en dword @+0, &padMgr slot @+8, &IAT slot @+10h. Line 1: skipped @+40h, passed @+48h.
/// Code @+80h. `B` = module base (the native test passes a fake base).
type Cave = { Bytes: byte[]; Code: uint64; En: uint64; Skip: uint64; Pass: uint64 }
let build (at: uint64) (B: uint64) =
    let a = Assembler(64)
    let mutable lEn, lPMgr, lIat, lSkip, lPass, lCode, pass = a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel(), a.CreateLabel()
    a.Label(&lEn); a.dd(1u); a.dd(0u)
    a.Label(&lPMgr); a.dq(B + G_PADMGR)
    a.Label(&lIat); a.dq(B + IAT_XINPUTGETSTATE)
    for _ in 1 .. 10 do a.dd(0u)                         // pad line 0 to 40h
    a.Label(&lSkip); a.dd(0u); a.dd(0u)
    a.Label(&lPass); a.dd(0u); a.dd(0u)
    for _ in 1 .. 12 do a.dd(0u)                         // pad line 1 to 80h
    // ===== entered by `call` from 435BB8 with the XInputGetState arguments: ecx = slot, rdx = XINPUT_STATE*
    a.Label(&lCode)
    a.cmp(__dword_ptr.[lEn], 0); a.je(pass)
    a.cmp(ecx, 3); a.ja(pass)                            // out-of-range slot: let the API answer
    a.mov(rax, __qword_ptr.[lPMgr]); a.mov(rax, __qword_ptr.[rax])
    a.test(rax, rax); a.je(pass)
    a.mov(r11d, ecx); a.lea(r11, __[r11 + r11 * 4])      // r11 = slot*5
    a.cmp(__byte_ptr.[rax + r11 * 4 + int64 CONNECTED_OFF], 0) // padMgr + 69850h + slot*14h
    a.jne(pass)
    a.inc(__dword_ptr.[lSkip])
    a.mov(eax, ERROR_DEVICE_NOT_CONNECTED)
    a.ret()
    a.Label(&pass)
    a.inc(__dword_ptr.[lPass])
    a.mov(r11, __qword_ptr.[lIat])
    a.jmp(__qword_ptr.[r11])                             // tail call through the current IAT entry (hooks kept)
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); Code = addr lCode; En = addr lEn; Skip = addr lSkip; Pass = addr lPass }
    if c.Code - at <> 0x80UL || c.Skip - at <> 0x40UL || c.Pass - at <> 0x48UL then failwith "cave layout"
    c

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
/// `nop; call cave` (call_at = 1 so the return address stays 435BBD)
let x1New (B: uint64) (c: Cave) = Array.concat [ [| 0x90uy; 0xE8uy |]; rel32 (B + uint64 X1_SITE + 6UL) c.Code ]

// ---- static helpers -------------------------------------------------------------------------------------------
let decodeAll (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    [ while d.IP < ip + uint64 bytes.Length do yield d.Decode() ]
let decodeAt (rva: int) = (decodeAll img.[rva .. rva + 15] (uint64 rva)).Head
let fmtIns =
    let fmt = NasmFormatter()
    let so = StringOutput()
    fun (ins: Instruction) -> (let mutable i = ins in fmt.Format(&i, so); so.ToStringAndReset())
let printDis (bytes: byte[]) (at: uint64) = for ins in decodeAll bytes at do printfn "    %X  %s" ins.IP (fmtIns ins)
let execSections () =
    let pe = BitConverter.ToInt32(img, 0x3C)
    let nsec = int (BitConverter.ToUInt16(img, pe + 6))
    let optsz = int (BitConverter.ToUInt16(img, pe + 20))
    [ for i in 0 .. nsec - 1 do
        let sh = pe + 24 + optsz + i * 40
        let vs, va, ch = BitConverter.ToInt32(img, sh + 8), BitConverter.ToInt32(img, sh + 12), BitConverter.ToUInt32(img, sh + 36)
        if ch &&& 0x20000000u <> 0u then yield va, vs ]
let findPattern (pat: string) =
    let toks = pat.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun s -> if s.StartsWith "?" then -1 else Convert.ToInt32(s, 16))
    [ for (va, vs) in execSections () do
        for i in va .. va + vs - toks.Length do
            let mutable j = 0
            while j < toks.Length && (toks.[j] = -1 || int img.[i + j] = toks.[j]) do j <- j + 1
            if j = toks.Length then yield i ]
let scanTargets (lo: uint64) (hi: uint64) =
    [ for (va, vs) in execSections () do
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, va, vs))
        dec.IP <- uint64 va
        let fin = uint64 (va + vs)
        while dec.IP < fin do
            let ins = dec.Decode()
            match ins.FlowControl with
            | FlowControl.UnconditionalBranch | FlowControl.ConditionalBranch | FlowControl.Call ->
                let t = ins.NearBranchTarget
                if t > lo && t < hi then yield sprintf "%X -> %X" ins.IP t
            | _ -> () ]
let infoFactory = InstructionInfoFactory()
let full64 (r: Register) = r.GetFullRegister()
/// From each start, every path must write `reg` (full or 32-bit) before reading it, or leave via call/ret/indirect
/// jump (r11 / flags are volatile across those). Flags: must be written before any read.
let rec deadAfter (starts: uint64 list) (reg: Register) (checkFlags: bool) (depth: int) (seen: Set<uint64>) : string list =
    [ for s in starts do
        if not (seen.Contains s) then
            let mutable ip = s
            let mutable fin = false
            let mutable n = 0
            let mutable regDead, flagsDead = false, not checkFlags
            let errs = ResizeArray<string>()
            while not fin && n < 64 do
                let ins = decodeAt (int ip)
                let info = infoFactory.GetInfo(&ins)
                let regs = info.GetUsedRegisters() |> Seq.toList
                let reads = regs |> List.exists (fun u -> full64 u.Register = full64 reg && (u.Access = OpAccess.Read || u.Access = OpAccess.ReadWrite || u.Access = OpAccess.CondRead || u.Access = OpAccess.ReadCondWrite))
                let mems = info.GetUsedMemory() |> Seq.toList
                let memReads = mems |> List.exists (fun m -> full64 m.Base = full64 reg || full64 m.Index = full64 reg)
                if not regDead && (reads || memReads) then errs.Add(sprintf "%X reads %O before it is written: %s" ins.IP reg (fmtIns ins))
                let writes = regs |> List.exists (fun u -> full64 u.Register = full64 reg && u.Access = OpAccess.Write && (u.Register = full64 reg || u.Register.IsGPR32()))
                if writes then regDead <- true
                if not flagsDead then
                    if ins.RflagsRead <> RflagsBits.None then errs.Add(sprintf "%X reads flags: %s" ins.IP (fmtIns ins))
                    if ins.RflagsWritten ||| ins.RflagsCleared ||| ins.RflagsSet ||| ins.RflagsUndefined <> RflagsBits.None && ins.RflagsRead = RflagsBits.None then flagsDead <- true
                match ins.FlowControl with
                | FlowControl.Return | FlowControl.Call | FlowControl.IndirectCall | FlowControl.IndirectBranch -> fin <- true
                | FlowControl.UnconditionalBranch -> ip <- ins.NearBranchTarget
                | FlowControl.ConditionalBranch ->
                    if not (regDead && flagsDead) && depth < 8 then
                        // continue both ways with the current knowledge
                        for e in deadAfter [ ins.NearBranchTarget ] (if regDead then Register.None else reg) (not flagsDead) (depth + 1) (seen.Add s) do errs.Add e
                    ip <- ins.NextIP
                | _ -> ip <- ins.NextIP
                if regDead && flagsDead then fin <- true
                n <- n + 1
            if not fin then errs.Add(sprintf "path from %X not resolved in 64 instructions" s)
            yield! errs ]

// ---- native test: the cave in this process vs a model of the original call --------------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type XigsFn = delegate of uint32 * nativeint -> uint32

let testX1 () =
    // fake module: B .. B+4600000h reserved; commit the padMgr slot page, the IAT page and the cave page
    let B = VirtualAlloc(0n, 0x4700000un, 0x2000u, 0x04u)                    // MEM_RESERVE, RW
    if B = 0n then failwith "reserve failed"
    let commit (rva: uint64) (prot: uint32) = VirtualAlloc(B + nativeint (rva &&& ~~~0xFFFUL), 0x1000un, 0x1000u, prot) |> ignore
    commit G_PADMGR 0x04u; commit IAT_XINPUTGETSTATE 0x04u; commit 0x4600000UL 0x40u
    let caveAt = uint64 B + 0x4600000UL
    let c = build caveAt (uint64 B)
    Marshal.Copy(c.Bytes, 0, nativeint caveAt, c.Bytes.Length)
    // stub "XInputGetState": records its arguments, returns a scripted value
    let mutable lastIdx, lastPtr, calls, nextRet = 0u, 0n, 0, 0u
    let stub = XigsFn(fun i p -> calls <- calls + 1; lastIdx <- i; lastPtr <- p; nextRet)
    let stubPtr = Marshal.GetFunctionPointerForDelegate stub
    Marshal.WriteIntPtr(B + nativeint IAT_XINPUTGETSTATE, stubPtr)
    let viaCave = Marshal.GetDelegateForFunctionPointer<XigsFn>(nativeint c.Code)
    let mgr = Marshal.AllocHGlobal(0x6A000)
    let rnd = Random(20261007)
    let mutable cases, bad = 0, 0
    let report (s: string) = (bad <- bad + 1; if bad <= 8 then printfn "  MISMATCH %s" s)
    for round in 1 .. 20000 do
        let en = rnd.Next 8 <> 0
        Marshal.WriteInt32(nativeint c.En, if en then 1 else 0)
        let mgrNull = rnd.Next 20 = 0
        Marshal.WriteIntPtr(B + nativeint G_PADMGR, if mgrNull then 0n else mgr)
        let conn = Array.init 4 (fun _ -> if rnd.Next 3 = 0 then byte (rnd.Next(1, 256)) else 0uy)
        for i in 0 .. 3 do
            Marshal.WriteByte(mgr + nativeint (CONNECTED_OFF + i * SLOT_STRIDE), conn.[i])
            // neighbours must not matter
            Marshal.WriteByte(mgr + nativeint (CONNECTED_OFF + i * SLOT_STRIDE + 1), byte (rnd.Next 256))
        let idx = uint32 (match rnd.Next 6 with 4 -> 4 + rnd.Next 3 | 5 -> rnd.Next() | k -> k)
        let p = nativeint (rnd.NextInt64(1L, 0x7FFFFFFFFFFFL))
        nextRet <- (match rnd.Next 3 with 0 -> 0u | 1 -> 0x48Fu | _ -> 0xA0u)
        let callsBefore = calls
        let r = viaCave.Invoke(idx, p)
        let shouldSkip = en && idx < 4u && not mgrNull && conn.[int idx] = 0uy
        if shouldSkip then
            if r <> 0x48Fu then report (sprintf "skip path returned %X" r)
            if calls <> callsBefore then report "skip path called the API"
        else
            if calls <> callsBefore + 1 then report (sprintf "pass path: API calls %d" (calls - callsBefore))
            elif lastIdx <> idx || lastPtr <> p then report (sprintf "pass path args %X/%X vs %X/%X" lastIdx lastPtr idx p)
            elif r <> nextRet then report (sprintf "pass path returned %X expected %X" r nextRet)
        cases <- cases + 1
    let sk, ps = Marshal.ReadInt32(nativeint c.Skip), Marshal.ReadInt32(nativeint c.Pass)
    Marshal.FreeHGlobal mgr
    GC.KeepAlive stub
    printfn "  X1 native test: %d cases (1/8 switch off, 1/20 null padMgr, out-of-range slots), %d mismatches; cave counters skip=%d pass=%d" cases bad sk ps
    bad = 0 && sk + ps = cases

// ---- offline --------------------------------------------------------------------------------------------------
let offline () =
    printfn "dump: %s (raw offset == RVA)" DUMP
    let mutable ok = true
    let check (name: string) (cond: bool) (detail: string) =
        printfn "  [%s] %s%s" (if cond then "ok" else "FAIL") name (if detail = "" then "" else "  -- " + detail)
        ok <- ok && cond
    let B = 0x7FF74F410000UL
    printfn "site:"
    let cur = img.[X1_SITE .. X1_SITE + X1_LEN - 1]
    check (sprintf "X1 %X bytes %s" X1_SITE (hexs cur)) (cur = hexb X1_ORIG) X1_ORIG
    let ins = decodeAt X1_SITE
    check "X1 is `call qword [rip+disp]` to the XInputGetState IAT slot" (ins.FlowControl = FlowControl.IndirectCall && ins.IsIPRelativeMemoryOperand && ins.IPRelativeMemoryAddress = IAT_XINPUTGETSTATE) (fmtIns ins)
    // IAT slot name
    let pe = BitConverter.ToInt32(img, 0x3C)
    let imp = BitConverter.ToInt32(img, pe + 24 + 112 + 8)
    let mutable iatName = "?"
    let mutable d = imp
    while BitConverter.ToUInt32(img, d + 12) <> 0u do
        let cstr o = (let e = Array.IndexOf(img, 0uy, o) in Text.Encoding.ASCII.GetString(img, o, e - o))
        let dll = cstr (BitConverter.ToInt32(img, d + 12))
        let oft, ft = BitConverter.ToInt32(img, d), BitConverter.ToInt32(img, d + 16)
        let th = if oft <> 0 then oft else ft
        let mutable i = 0
        while BitConverter.ToUInt64(img, th + i * 8) <> 0UL do
            let ent = BitConverter.ToUInt64(img, th + i * 8)
            if uint64 (ft + i * 8) = IAT_XINPUTGETSTATE then
                iatName <- dll + "!" + (if ent &&& (1UL <<< 63) <> 0UL then sprintf "ord%d" (ent &&& 0xFFFFUL) else cstr (int ent + 2))
            i <- i + 1
        d <- d + 20
    check (sprintf "IAT %X = %s (XInputGetState is ordinal 2 of xinput1_4)" IAT_XINPUTGETSTATE iatName) (iatName.ToUpperInvariant() = "XINPUT1_4.DLL!ORD2") ""
    printfn "context (435B8C scan loop and its caller):"
    let expect (rva: int) (txt: string) =
        let i = decodeAt rva
        check (sprintf "%X  %s" rva (fmtIns i)) ((fmtIns i).Replace(" ", "") = txt.Replace(" ", "")) ("expected " + txt)
    expect 0x435BAD "mov rsi,rcx"
    expect 0x435BB0 "xor ebx,ebx"
    expect 0x435BB2 "mov rdx,rdi"
    expect 0x435BB5 "mov ecx,ebx"
    expect 0x435BBD "test eax,eax"
    expect 0x435BC5 "inc ebx"
    expect 0x435BC7 "cmp ebx,4"
    expect 0x435BCC "mov ebx,[rsi+69B20h]"
    let l = decodeAt 0x435B48
    check "435B48 loads rcx = [45BA1E0] (padMgr) before calling 435B8C" (l.IsIPRelativeMemoryOperand && l.IPRelativeMemoryAddress = G_PADMGR && l.Op0Register = Register.RCX) (fmtIns l)
    let cl = decodeAt 0x435B5C
    check "435B5C calls 435B8C" (cl.FlowControl = FlowControl.Call && cl.NearBranchTarget = 0x435B8CUL) (fmtIns cl)
    // 43671C: 434E18(padMgr) runs before 434D24 -> 435B1C -> 435B8C in the same update
    let body = decodeAll img.[0x43671C .. 0x43671C + 0x80] 0x43671CUL
    let iLoad = body |> List.tryFindIndex (fun i -> i.IsIPRelativeMemoryOperand && i.IPRelativeMemoryAddress = G_PADMGR && i.Op0Register = Register.RCX)
    let iCall = body |> List.tryFindIndex (fun i -> i.FlowControl = FlowControl.Call && i.NearBranchTarget = 0x434E18UL)
    let iD24 = body |> List.tryFindIndex (fun i -> i.FlowControl = FlowControl.Call && i.NearBranchTarget = 0x434D24UL)
    check "43671C: rcx=[45BA1E0]; call 434E18 ... later call 434D24" (match iLoad, iCall, iD24 with Some a, Some b, Some c -> a < b && b < c | _ -> false) (sprintf "%A %A %A" iLoad iCall iD24)
    // 434E18 maintains the connected bytes: lea rsi,[rbx+69850h] / rbp,[rbx+69854h] / XInputGetState / mov byte [rsi],1
    let probe = decodeAll img.[0x43502C .. 0x43502C + 0x40] 0x43502CUL |> List.map fmtIns
    for p in probe do printfn "      %s" p
    check "434E18 2-s probe writes connected bytes at +69850h+i*14h" (probe |> List.exists (fun s -> s.Replace(" ", "") = "learsi,[rbx+69850h]") && probe |> List.exists (fun s -> s.Contains "[rel 38E2C50h]" || s.Contains "38E2C50")) ""
    printfn "pattern (mod):"
    let hits = findPattern X1_PATTERN
    check (sprintf "X1 pattern unique, site = hit+%d" X1_PAT_OFF) (hits = [ X1_SITE - X1_PAT_OFF ]) (sprintf "%d hit(s): %s" hits.Length (String.Join(",", hits |> List.map (sprintf "%X"))))
    let h2 = findPattern PADMGR_PATTERN
    check "padMgr pattern unique at 435B48" (h2 = [ 0x435B48 ]) (sprintf "%d hit(s)" h2.Length)
    printfn "overlap with other patch sets:"
    let ov = OTHER_SITES |> List.filter (fun s -> s < X1_SITE + X1_LEN && X1_SITE < s + 10)
    check "no overlap" ov.IsEmpty (String.Join(",", ov |> List.map (sprintf "%X")))
    printfn "branch/call targets strictly inside %X..%X (whole .text sweep):" X1_SITE (X1_SITE + X1_LEN)
    let t = scanTargets (uint64 X1_SITE) (uint64 (X1_SITE + X1_LEN))
    check "none" t.IsEmpty (String.Join("; ", t))
    printfn "dead state after the call returns to 435BBD (cave clobbers rax, r11, flags; rax is the result):"
    let e1 = deadAfter [ 0x435BBDUL ] Register.R11 true 0 Set.empty
    check "r11 and flags dead at 435BBD (test eax,eax rewrites flags; r11 never read)" e1.IsEmpty (String.Join("; ", e1))
    // the je target (cold block): must not read r11 either
    let e2 = deadAfter [ 0x19A9B2CUL ] Register.R11 false 0 Set.empty
    check "r11 dead in the 'pad active' cold block 19A9B2C" e2.IsEmpty (String.Join("; ", e2))
    printfn "cave @dummy %X:" (B + 0x5F00000UL)
    let c = build (B + 0x5F00000UL) B
    printDis c.Bytes.[0x80 ..] c.Code
    let nb = x1New B c
    printfn "patched site: %X  %s  ->  %s" X1_SITE X1_ORIG (hexs nb)
    printDis nb (B + uint64 X1_SITE)
    let both = Set.intersect (boundaries nb (B + uint64 X1_SITE) |> Set.ofSeq) (boundaries (hexb X1_ORIG) (B + uint64 X1_SITE) |> Set.ofSeq)
    check "only the site start is an instruction start in both encodings" (both = set [ B + uint64 X1_SITE ]) ""
    printfn "native test (this process):"
    ok <- testX1 () && ok
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live ----------------------------------------------------------------------------------------------------
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let caveOrProbe () = match readState () with Some a -> build a B | None -> build (B + 0x5F00000UL) B
    let classify (c: Cave) =
        let cur = t.Read(nativeint (B + uint64 X1_SITE), X1_LEN)
        let s = if cur = hexb X1_ORIG then "original" elif cur = x1New B c then "patched" else "UNKNOWN"
        printfn "  X1 %8X  %-8s  %s" X1_SITE s (hexs cur)
        s
    let write (c: Cave) (toNew: bool) =
        let nw, o = x1New B c, hexb X1_ORIG
        safeWrite t [ (nativeint (B + uint64 X1_SITE), (if toNew then nw else o), (if toNew then o else nw)) ]
    let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
    match mode with
    | "dry" ->
        let c = caveOrProbe ()
        printfn "base=%X cave=%s" B (match readState () with Some a -> sprintf "%X" a | None -> "(none, preview)")
        classify c |> ignore
        let mgr = BitConverter.ToUInt64(t.Read(nativeint (B + G_PADMGR), 8), 0)
        printfn "padMgr=[%X]=%X" (B + G_PADMGR) mgr
        if mgr <> 0UL then
            for i in 0 .. 3 do printfn "  slot %d connected=%d" i (t.Read(nativeint (mgr + uint64 (CONNECTED_OFF + i * SLOT_STRIDE)), 1)).[0]
        printDis (x1New B c) (B + uint64 X1_SITE)
    | "apply" ->
        writeGuard ()
        if classify (caveOrProbe ()) <> "original" then failwith "site not original - aborting (revert first)"
        let c =
            match readState () with
            | Some a -> build a B
            | None ->
                let mem = t.AllocNear(nativeint (B + uint64 X1_SITE), 0x1000)
                let c = build (uint64 mem) B
                t.Write(mem, c.Bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" (uint64 mem) t.Process.Id)
                c
        let n = write c true
        printfn "X1 applied on attempt %d; cave %X switch=%d" n (c.Code - 0x80UL) (rd c.En)
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        match classify c with
        | "patched" -> write c false |> ignore; printfn "X1 reverted (cave left allocated)"
        | "original" -> printfn "X1 already original"
        | _ -> failwith "unexpected bytes - aborting"
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some _ ->
            let c = caveOrProbe ()
            if classify c <> "patched" then printfn "X1 not applied in this process - use apply"
            else (t.Write(nativeint c.En, BitConverter.GetBytes(if mode = "on" then 1u else 0u)); printfn "X1 switch %s" mode)
        | None -> printfn "no cave in this process - use apply"
    | "status" ->
        let c = caveOrProbe ()
        classify c |> ignore
        match readState () with
        | None -> printfn "no cave in this process"
        | Some _ ->
            printfn "switch=%d" (rd c.En)
            let mutable s0, p0 = rd c.Skip, rd c.Pass
            for _ in 1 .. 10 do
                Threading.Thread.Sleep 1000
                let s1, p1 = rd c.Skip, rd c.Pass
                printfn "  XInputGetState from the scan: skipped %5d/s  passed %5d/s  (stock = skipped+passed)" (s1 - s0) (p1 - p0)
                s0 <- s1; p0 <- p1
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
