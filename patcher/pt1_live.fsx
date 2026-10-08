// pt1_live.fsx - candidate PT1 from notes\streaming-anim.md: take the page-pool SRW lock (pool+D08h, shared) once per
// texture in TextureStreamRequestTiles phase 2 (Nioh3+1290B8) instead of once per tile x mip touch (PagePoolTouchTile
// 114C38 acquires + releases it on every call).
//
//   dotnet fsi pt1_live.fsx [offline]           NO game access: site bytes, branch sweeps, phase-2 closure (what runs
//                                               while the lock is held), 114C38 callers, lock-user census, native
//                                               functional + 2-thread stress test of the caves on a real SRWLOCK.
//   dotnet fsi pt1_live.fsx dry                 READ-ONLY attach: live bytes vs dump, site state, cave preview.
//   dotnet fsi pt1_live.fsx status              READ-ONLY: site state, switch, owner/held, textures claimed, locks saved.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi pt1_live.fsx apply    WRITES: cave (once per process) + 5 sites, all at once.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi pt1_live.fsx off|on   WRITES the switch dword only (off = stock from the next texture).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi pt1_live.fsx revert   WRITES: switch off, waits until no texture holds the lock,
//                                               then restores the 5 sites (cave stays allocated).
// Nothing is applied unless you run apply (default off).
//
// Sites (all in the dump, v2.02):
//   S_ACQ 114CA8  call [AcquireSRWLockShared] (FF 15, 6 B) -> call capAcq ; nop
//   S_REL 114CEE  call [ReleaseSRWLockShared] (FF 15, 6 B) -> call capRel ; nop
//   S_B   129651  add [r14+7590h],r10d (7 B)          phase 2 begins: claim owner = this thread id (lock cmpxchg 0 -> tid)
//   S_C   12982E  mov rax,[rsi]; lea r8,[rbp+60h] (7)  touch failed, before vfunc +28h: release the held lock (if any)
//   S_D   1298B3  cmp byte [rbp+0Ch],0; je 129943 (10) phase 2 ends (single join point): release held lock, owner = 0
// capAcq: owner == this thread ? (held == lock ? skip : [release other held] held = lock, acquire) : acquire.
// capRel: owner == this thread && held == lock ? skip : release.
// Other threads (TS_MESHLET_REQ workers etc.) and every other 114C38 path are untouched (owner test).
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
open System
open System.IO
open System.Threading
open System.Diagnostics
open System.Runtime.InteropServices
open Iced.Intel
open type Iced.Intel.AssemblerRegisters
open Mem

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "pt1_live_state.txt")
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"

let S_ACQ, S_REL, S_B, S_C, S_D = 0x114CA8, 0x114CEE, 0x129651, 0x12982E, 0x1298B3
let IAT_ACQ, IAT_REL = 0x38E2628, 0x38E2630
let hx (s: string) = Convert.FromHexString s
let ORIG =
    [ S_ACQ, hx "FF157AD97C03"; S_REL, hx "FF153CD97C03"; S_B, hx "45019690750000"
      S_C, hx "488B064C8D4560"; S_D, hx "807D0C000F8486000000" ] |> dict
let B_RESUME, C_RESUME, D_RESUME, D_JE = 0x129658, 0x129835, 0x1298BD, 0x129943
let PH2_LO, PH2_HI = 0x129651, 0x1298B3          // phase 2: [129651, 1298B3)
let F114_LO, F114_HI = 0x114C38, 0x114D15
// cave layout
let OFF_EN, OFF_OWNER, OFF_HELD, OFF_CLAIMS, OFF_SAVED, OFF_PACQ, OFF_PREL, OFF_RET, OFF_CODE = 0x00, 0x04, 0x08, 0x10, 0x14, 0x20, 0x28, 0x30, 0x100
let TOUCH_RET = 0x1297FE                          // return address of the phase-2 call to 114C38
let RET_DEPTH = 0x70L                             // [rsp+70h] inside capAcq/capRel = 114C38's return address (checked offline)
let CAVE_SIZE = 0x800

type Entries = { acq: uint64; rel: uint64; b: uint64; c: uint64; d: uint64 }

/// Whole cave image for address `cave`. `res` maps the resume points (absolute); pAcq/pRel are the function
/// pointers stored in the cave (live: the values of the game's IAT slots).
let buildCave (cave: uint64) (bRes: uint64) (cRes: uint64) (dRes: uint64) (dJe: uint64) (pAcq: uint64) (pRel: uint64) (retSite: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, lOwner, lHeld, lClaims, lSaved, lPAcq, lPRel, lRet = L (), L (), L (), L (), L (), L (), L (), L ()
    let mutable eAcq, eRel, eB, eC, eD = L (), L (), L (), L (), L ()
    let tidEax () = a.db([| 0x65uy; 0x8Buy; 0x04uy; 0x25uy; 0x48uy; 0uy; 0uy; 0uy |])   // mov eax,gs:[48h] (TEB ClientId.UniqueThread)
    let tidEcx () = a.db([| 0x65uy; 0x8Buy; 0x0Cuy; 0x25uy; 0x48uy; 0uy; 0uy; 0uy |])   // mov ecx,gs:[48h]
    a.Label(&lEn); a.dd(1u)
    a.Label(&lOwner); a.dd(0u)
    a.Label(&lHeld); a.dq(0UL)
    a.Label(&lClaims); a.dd(0u)
    a.Label(&lSaved); a.dd(0u)
    a.dq(0UL)
    a.Label(&lPAcq); a.dq(pAcq)
    a.Label(&lPRel); a.dq(pRel)
    a.Label(&lRet); a.dq(retSite)
    for _ in 1 .. (OFF_CODE - 0x38) / 4 do a.dd(0xCCCCCCCCu)
    // ---- capAcq (rcx = lock), replaces call [AcquireSRWLockShared] in 114C38
    let mutable stockA, skipA, takeA = L (), L (), L ()
    a.Label(&eAcq)
    tidEax ()
    a.cmp(eax, __dword_ptr.[lOwner]); a.jne(stockA)
    a.mov(rax, __qword_ptr.[rsp + RET_DEPTH]); a.cmp(rax, __qword_ptr.[lRet]); a.jne(stockA)   // only the phase-2 call (1297F9)
    a.mov(rax, __qword_ptr.[lHeld])
    a.cmp(rax, rcx); a.je(skipA)
    a.test(rax, rax); a.jz(takeA)
    a.push(rcx); a.sub(rsp, 0x20)                    // another lock is held (never seen; defensive): release it first
    a.mov(rcx, rax); a.call(__qword_ptr.[lPRel])
    a.add(rsp, 0x20); a.pop(rcx)
    a.Label(&takeA)
    a.mov(__qword_ptr.[lHeld], rcx)
    a.Label(&stockA)
    a.jmp(__qword_ptr.[lPAcq])
    a.Label(&skipA)
    a.inc(__dword_ptr.[lSaved])                      // owner thread only: single writer
    a.ret()
    // ---- capRel (rcx = lock), replaces call [ReleaseSRWLockShared] in 114C38
    let mutable stockR = L ()
    a.Label(&eRel)
    tidEax ()
    a.cmp(eax, __dword_ptr.[lOwner]); a.jne(stockR)
    a.mov(rax, __qword_ptr.[rsp + RET_DEPTH]); a.cmp(rax, __qword_ptr.[lRet]); a.jne(stockR)
    a.cmp(rcx, __qword_ptr.[lHeld]); a.jne(stockR)
    a.ret()                                          // keep it for the next touch of this texture
    a.Label(&stockR)
    a.jmp(__qword_ptr.[lPRel])
    // ---- S_B: phase 2 begins (rax dead: 12965B reloads eax)
    let mutable skipB = L ()
    a.Label(&eB)
    a.test(__byte_ptr.[lEn], 1u); a.jz(skipB)
    a.push(rcx)
    tidEcx ()
    a.xor(eax, eax)
    a.``lock``.cmpxchg(__dword_ptr.[lOwner], ecx)   // free -> this thread; already owned (recursion) -> unchanged
    let mutable noClaim = L ()
    a.jne(noClaim)
    a.inc(__dword_ptr.[lClaims])                     // claiming thread only
    a.Label(&noClaim)
    a.pop(rcx)
    a.Label(&skipB)
    a.add(__dword_ptr.[r14 + 0x7590L], r10d)         // displaced
    a.jmp(bRes)
    // ---- S_C: touch failed; release before vfunc +28h (all volatile GPRs are dead here: the call follows)
    let mutable outC = L ()
    a.Label(&eC)
    tidEax ()
    a.cmp(eax, __dword_ptr.[lOwner]); a.jne(outC)
    a.mov(rcx, __qword_ptr.[lHeld])
    a.test(rcx, rcx); a.jz(outC)
    a.mov(__qword_ptr.[lHeld], 0)
    a.sub(rsp, 0x20); a.call(__qword_ptr.[lPRel]); a.add(rsp, 0x20)
    a.Label(&outC)
    a.mov(rax, __qword_ptr.[rsi])                    // displaced
    a.lea(r8, __[rbp + 0x60L])
    a.jmp(cRes)
    // ---- S_D: phase 2 ends (single join point); preserve every volatile GPR
    let mutable restD, clearD, neD = L (), L (), L ()
    a.Label(&eD)
    a.push(rax); a.push(rcx); a.push(rdx); a.push(r8); a.push(r9); a.push(r10); a.push(r11)
    a.sub(rsp, 0x28)
    tidEax ()
    a.cmp(eax, __dword_ptr.[lOwner]); a.jne(restD)
    a.mov(rcx, __qword_ptr.[lHeld])
    a.mov(__qword_ptr.[lHeld], 0)
    a.test(rcx, rcx); a.jz(clearD)
    a.call(__qword_ptr.[lPRel])
    a.Label(&clearD)
    a.mov(__dword_ptr.[lOwner], 0)
    a.Label(&restD)
    a.add(rsp, 0x28)
    a.pop(r11); a.pop(r10); a.pop(r9); a.pop(r8); a.pop(rdx); a.pop(rcx); a.pop(rax)
    a.cmp(__byte_ptr.[rbp + 0x0CL], 0)               // displaced
    a.jne(neD)
    a.jmp(dJe)
    a.Label(&neD)
    a.jmp(dRes)
    use ms = new MemoryStream()
    let r = a.Assemble(StreamCodeWriter(ms), cave, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let b = ms.ToArray()
    if b.Length > CAVE_SIZE then failwith "cave too big"
    b, { acq = r.GetLabelRIP(&eAcq); rel = r.GetLabelRIP(&eRel); b = r.GetLabelRIP(&eB); c = r.GetLabelRIP(&eC); d = r.GetLabelRIP(&eD) }

/// The 5 site patches for a cave whose entries are `e`; `B` = module base (0 offline).
let sitePatches (B: uint64) (e: Entries) =
    let callRel (src: int) (dst: uint64) =
        let rel = int (int64 dst - int64 (B + uint64 src + 5UL))
        Array.concat [ [| 0xE8uy |]; BitConverter.GetBytes rel; [| 0x90uy |] ]
    [ S_ACQ, callRel S_ACQ e.acq
      S_REL, callRel S_REL e.rel
      S_B, jmpPatch (nativeint (B + uint64 S_B)) (nativeint e.b) 7
      S_C, jmpPatch (nativeint (B + uint64 S_C)) (nativeint e.c) 7
      S_D, jmpPatch (nativeint (B + uint64 S_D)) (nativeint e.d) 10 ]

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
[<DllImport("kernel32.dll")>] extern nativeint GetModuleHandleW([<MarshalAs(UnmanagedType.LPWStr)>] string name)
[<DllImport("kernel32.dll")>] extern nativeint GetProcAddress(nativeint h, [<MarshalAs(UnmanagedType.LPStr)>] string name)
[<DllImport("kernel32.dll")>] extern uint32 GetCurrentThreadId()
[<DllImport("kernel32.dll")>] extern void AcquireSRWLockExclusive(nativeint l)
[<DllImport("kernel32.dll")>] extern void ReleaseSRWLockExclusive(nativeint l)
[<DllImport("kernel32.dll")>] extern byte TryAcquireSRWLockExclusive(nativeint l)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type Fn1 = delegate of nativeint -> unit
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type Tramp = delegate of nativeint * nativeint * nativeint * nativeint -> int64
type Touch(f: Tramp, m: int64, acq: uint64, rel: uint64) =
    member _.Invoke(l: nativeint) = f.Invoke(l, nativeint m, nativeint acq, nativeint rel) |> ignore

let offline () =
    let mutable ok = true
    let check name c = printfn "  [%s] %s" (if c then "ok" else "FAIL") name; ok <- ok && c
    let u32 o = BitConverter.ToUInt32(img, o)
    printfn "== site checks (dump)"
    for KeyValue(s, b) in ORIG do check (sprintf "%X bytes = %s" s (BitConverter.ToString b)) (img.[s .. s + b.Length - 1] = b)
    let decodeAt (s: int) (len: int) =
        let d = Decoder.Create(64, ByteArrayCodeReader(img, s, len + 16))
        d.IP <- uint64 s
        [ while d.IP < uint64 (s + len) do yield d.Decode() ]
    let ends s len = let l = decodeAt s len in (l |> List.last).NextIP = uint64 (s + len)
    check "every site is whole instructions" ([ S_ACQ, 6; S_REL, 6; S_B, 7; S_C, 7; S_D, 10 ] |> List.forall (fun (s, l) -> ends s l))
    let iatOf (s: int) = let i = (decodeAt s 6).Head in i.IPRelativeMemoryAddress
    check "114CA8 calls [38E2628] AcquireSRWLockShared, 114CEE calls [38E2630] ReleaseSRWLockShared" (iatOf S_ACQ = uint64 IAT_ACQ && iatOf S_REL = uint64 IAT_REL)
    let dj = (decodeAt S_D 10) |> List.item 1
    check "1298B7 is je 129943" (dj.Mnemonic = Mnemonic.Je && dj.NearBranchTarget = uint64 D_JE)
    // .pdata + full decode of every function
    let pe = BitConverter.ToInt32(img, 0x3C)
    let dd = pe + 24 + 112
    let imageBase = BitConverter.ToUInt64(img, pe + 24 + 24)
    let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
    let n = excSz / 12
    let pB = Array.init n (fun i -> u32 (excRva + i * 12))
    let pE = Array.init n (fun i -> u32 (excRva + i * 12 + 4))
    let interior = [ S_ACQ, 6; S_REL, 6; S_B, 7; S_C, 7; S_D, 10 ]
    let mutable intoSite = 0
    let mutable callers114 = []
    let mutable ripRef114 = 0
    let mutable intoPh2 = []
    let mutable ins = Instruction()
    // lock census: SRW calls whose lock argument is [reg+0D08h] / [reg+6EA8h] (pool) - via lea rcx or lea reg; mov rcx,reg
    let census = Collections.Generic.List<string>()
    let srwName = dict [ 0x38E2628UL, "AcqShared"; 0x38E2630UL, "RelShared" ]
    let srwAll = Collections.Generic.Dictionary<uint64, string>()
    // resolve all SRW imports by name from the import table
    let cstr o = let e = Array.IndexOf(img, 0uy, o) in Text.Encoding.ASCII.GetString(img, o, e - o)
    let mutable di = int (u32 (dd + 8))
    while u32 (di + 12) <> 0u do
        let oft, ft = u32 di, u32 (di + 16)
        let th = if oft <> 0u then oft else ft
        let mutable i = 0
        while BitConverter.ToUInt64(img, int th + i * 8) <> 0UL do
            let ent = BitConverter.ToUInt64(img, int th + i * 8)
            if ent &&& (1UL <<< 63) = 0UL && ent < uint64 img.Length then
                let nm = cstr (int ent + 2)
                if nm.Contains "SRWLock" then srwAll.[uint64 ft + uint64 (i * 8)] <- nm
            i <- i + 1
        di <- di + 20
    for i in 0 .. n - 1 do
        let s, e = pB.[i], pE.[i]
        if e > s && int e <= img.Length then
            let d = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
            d.IP <- uint64 s
            let leaD08 = Collections.Generic.Dictionary<Register, uint64>()
            while d.IP < uint64 e do
                d.Decode(&ins)
                if not ins.IsInvalid then
                    if ins.Op0Kind = OpKind.NearBranch64 then
                        let t = ins.NearBranchTarget
                        for (ss, l) in interior do
                            if t > uint64 ss && t < uint64 (ss + l) then intoSite <- intoSite + 1; printfn "  !! %X -> %X (inside site %X)" ins.IP t ss
                        if t = uint64 F114_LO then callers114 <- ins.IP :: callers114
                        if t > uint64 PH2_LO && t < uint64 PH2_HI && (ins.IP < uint64 PH2_LO || ins.IP >= uint64 PH2_HI) then intoPh2 <- (ins.IP, t) :: intoPh2
                    if ins.IsIPRelativeMemoryOperand && ins.IPRelativeMemoryAddress = uint64 F114_LO then ripRef114 <- ripRef114 + 1
                    if ins.Mnemonic = Mnemonic.Lea && (ins.MemoryDisplacement64 = 0xD08UL || ins.MemoryDisplacement64 = 0x6EA8UL) then
                        leaD08.[ins.Op0Register] <- ins.MemoryDisplacement64
                    elif ins.Mnemonic = Mnemonic.Mov && ins.Op0Kind = OpKind.Register && ins.Op1Kind = OpKind.Register && leaD08.ContainsKey ins.Op1Register then
                        leaD08.[ins.Op0Register] <- leaD08.[ins.Op1Register]
                    elif ins.Op0Kind = OpKind.Register && ins.Op0Register <> Register.None && ins.Mnemonic <> Mnemonic.Call && ins.Mnemonic <> Mnemonic.Cmp && ins.Mnemonic <> Mnemonic.Test && ins.Mnemonic <> Mnemonic.Push then
                        leaD08.Remove(ins.Op0Register) |> ignore
                    if ins.IsIPRelativeMemoryOperand && srwAll.ContainsKey ins.IPRelativeMemoryAddress && (ins.Mnemonic = Mnemonic.Call || ins.Mnemonic = Mnemonic.Jmp) then
                        if leaD08.ContainsKey Register.RCX then
                            census.Add(sprintf "%X in fn %X  %s  [x+%Xh]" ins.IP s srwAll.[ins.IPRelativeMemoryAddress] leaD08.[Register.RCX])
    check "no branch lands inside any patched site" (intoSite = 0)
    printfn "  direct calls/jumps to 114C38: %s" (String.Join(", ", callers114 |> List.map (sprintf "%X")))
    check "114C38 direct callers = 1297F9 (phase 2) + 7C4BCC (7C463C; stays stock via the return-address gate)" (List.sort callers114 = [ 0x1297F9UL; 0x7C4BCCUL ])
    // rsp depth inside 114C38 at both IAT calls: 5 pushes + sub rsp,40h, nothing else touches rsp before 114CEE
    let mutable depth = 0L
    let mutable dAcq, dRel = -1L, -1L
    for i in decodeAt F114_LO (S_REL + 6 - F114_LO) do
        if i.Mnemonic = Mnemonic.Push then depth <- depth + 8L
        elif i.Mnemonic = Mnemonic.Pop then depth <- depth - 8L
        elif i.Op0Kind = OpKind.Register && i.Op0Register = Register.RSP then
            if i.Mnemonic = Mnemonic.Sub && i.Op1Kind <> OpKind.Register then depth <- depth + int64 (i.GetImmediate(1))
            else depth <- -100000L
        if uint64 S_ACQ = i.IP then dAcq <- depth
        if uint64 S_REL = i.IP then dRel <- depth
    check (sprintf "114C38 frame at both IAT calls = %Xh -> its return address is at [rsp+%Xh] inside capAcq/capRel" dAcq (dAcq + 8L)) (dAcq = 0x68L && dRel = 0x68L && dAcq + 8L = RET_DEPTH)
    let absRefs =
        let target = imageBase + uint64 F114_LO
        [ for o in 0 .. 8 .. img.Length - 8 do if BitConverter.ToUInt64(img, o) = target then yield o ]
    check (sprintf "114C38 is never address-taken (rip-relative refs %d, absolute qwords %d)" ripRef114 absRefs.Length) (ripRef114 = 0 && absRefs.IsEmpty)
    check "phase 2 [129651,1298B3) is entered from outside only at 129651" intoPh2.IsEmpty
    // phase-2 closure: branches stay inside, and the only calls are 114C38 and vfunc +28h
    let ph2 = decodeAt PH2_LO (PH2_HI - PH2_LO)
    let calls = ph2 |> List.filter (fun i -> i.Mnemonic = Mnemonic.Call)
    let callTxt = calls |> List.map (fun i -> if i.Op0Kind = OpKind.NearBranch64 then sprintf "%X: call %X" i.IP i.NearBranchTarget else sprintf "%X: call [%A+%Xh]" i.IP i.MemoryBase i.MemoryDisplacement64)
    printfn "  calls inside phase 2: %s" (String.Join("; ", callTxt))
    check "phase 2 calls only 114C38 (1297F9) and [rax+28h] (129841, right after S_C)" (callTxt = [ "1297F9: call 114C38"; "129841: call [RAX+28h]" ])
    let outward = ph2 |> List.filter (fun i -> (i.FlowControl = FlowControl.ConditionalBranch || i.FlowControl = FlowControl.UnconditionalBranch) && (i.NearBranchTarget < uint64 PH2_LO || i.NearBranchTarget > uint64 PH2_HI))
    let retOrInd = ph2 |> List.filter (fun i -> i.FlowControl = FlowControl.Return || i.FlowControl = FlowControl.IndirectBranch)
    check "every phase-2 branch stays in [129651,1298B3] (exit only via 1298B3 = S_D)" (outward.IsEmpty && retOrInd.IsEmpty)
    // 114C38: calls are 117B44 (before the lock), Acquire, 1195E8, Release; lock = [r15+D08h] for both
    let f114 = decodeAt F114_LO (F114_HI - F114_LO)
    let c114 = f114 |> List.filter (fun i -> i.Mnemonic = Mnemonic.Call) |> List.map (fun i -> if i.Op0Kind = OpKind.NearBranch64 then sprintf "%X" i.NearBranchTarget else sprintf "[%X]" i.IPRelativeMemoryAddress)
    check (sprintf "114C38 calls: %s" (String.Join(" ", c114))) (c114 = [ "117B44"; "[38E2628]"; "1195E8"; "[38E2630]" ])
    let f117 = decodeAt 0x117B44 0x65 |> List.filter (fun i -> i.Mnemonic = Mnemonic.Call)
    printfn "  117B44 (runs before the lock in stock, under the held lock with PT1) calls: %s" (String.Join("; ", f117 |> List.map (fun i -> if i.Op0Kind = OpKind.NearBranch64 then sprintf "%X" i.NearBranchTarget else sprintf "[%A+%Xh]" i.MemoryBase i.MemoryDisplacement64)))
    printfn "    = vtable+50h of the texture's [rsi+10h] (ID3D12Resource::GetDesc: 1/2/3/4 = D3D12_RESOURCE_DIMENSION, Flags & 3 = RT|DS)"
    printfn "== lock census: every SRW call on [x+D08h] / [x+6EA8h] (pool lock; +6EA8h = device+61A0h+D08h)"
    for c in census do printfn "  %s" c
    // ---------------- native test of the caves on a real SRWLOCK
    printfn "== native functional test (caves vs a real SRWLOCK, own process only)"
    let page = VirtualAlloc(0n, 0x10000un, 0x3000u, 0x40u)
    let caveA = page
    let stubs = page + 0x1000n
    // resume stubs: each returns a tag in eax (rax for C: rax already = [rsi])
    let stub (off: int) (code: byte[]) = Marshal.Copy(code, 0, stubs + nativeint off, code.Length); uint64 stubs + uint64 off
    let sB = stub 0x00 [| 0xB8uy; 0x0Buy; 0uy; 0uy; 0uy; 0xC3uy |]       // mov eax,0Bh ; ret
    let sC = stub 0x10 [| 0xC3uy |]                                        // ret (rax = [rsi] from the displaced mov)
    let sD = stub 0x20 [| 0xB8uy; 0x0Duy; 0uy; 0uy; 0uy; 0xC3uy |]       // jne path
    let sJ = stub 0x30 [| 0xB8uy; 0x0Euy; 0uy; 0uy; 0uy; 0xC3uy |]       // je path
    let k32 = GetModuleHandleW "kernel32.dll"
    let pA, pR = GetProcAddress(k32, "AcquireSRWLockShared"), GetProcAddress(k32, "ReleaseSRWLockShared")
    // fake 114C38 frame (same 5 pushes + sub rsp,40h) calling capAcq (mode 1) / capRel (mode 2) via r8 / r9,
    // reached from two call sites: callerP2 (its return address plays 1297FE) and callerOther (plays 7C4BCC)
    let fake114 = uint64 stubs + 0x600UL
    let fb = assemble (nativeint fake114) (fun a ->
        let mutable s1 = a.CreateLabel()
        let mutable s2 = a.CreateLabel()
        a.push(rdi); a.push(r12); a.push(r13); a.push(r14); a.push(r15); a.sub(rsp, 0x40)
        a.mov(rdi, rcx); a.mov(r12, rdx); a.mov(r13, r8); a.mov(r14, r9)
        a.test(r12b, 1uy); a.jz(s1); a.mov(rcx, rdi); a.call(r13); a.Label(&s1)
        a.test(r12b, 2uy); a.jz(s2); a.mov(rcx, rdi); a.call(r14); a.Label(&s2)
        a.add(rsp, 0x40); a.pop(r15); a.pop(r14); a.pop(r13); a.pop(r12); a.pop(rdi); a.ret())
    Marshal.Copy(fb, 0, nativeint fake114, fb.Length)
    let mkCaller (at: uint64) =
        let b = assemble (nativeint at) (fun a -> a.sub(rsp, 0x28); a.call(fake114); a.add(rsp, 0x28); a.ret())
        Marshal.Copy(b, 0, nativeint at, b.Length)
        let d = Decoder.Create(64, ByteArrayCodeReader(b))
        d.IP <- at
        d.Decode() |> ignore; d.Decode() |> ignore
        d.IP                                                         // return address of the call to fake114
    let callerP2, callerOther = uint64 stubs + 0x700UL, uint64 stubs + 0x780UL
    let retP2 = mkCaller callerP2
    mkCaller callerOther |> ignore
    let bytes, e = buildCave (uint64 caveA) sB sC sD sJ (uint64 pA) (uint64 pR) retP2
    Marshal.Copy(bytes, 0, caveA, bytes.Length)
    printfn "  cave %d bytes (code %d)" bytes.Length (bytes.Length - OFF_CODE)
    // trampoline: (r14v, rbpv, rsiv, r10v) -> sets r14/rbp/rsi/r10, calls the entry with rsp = 0 mod 16 (like the game body)
    let tramp (entry: uint64) (at: nativeint) =
        let b = assemble at (fun a ->
            a.push(r14); a.push(rbp); a.push(rsi); a.push(rbx)        // entry rsp 8 mod 16 -> 8 after 4 pushes
            a.mov(r14, rcx); a.mov(rbp, rdx); a.mov(rsi, r8); a.mov(r10, r9)
            a.mov(rbx, entry); a.call(rbx)                           // pushes ret -> 0 mod 16 at the entry
            a.pop(rbx); a.pop(rsi); a.pop(rbp); a.pop(r14); a.ret())
        Marshal.Copy(b, 0, at, b.Length)
        Marshal.GetDelegateForFunctionPointer<Tramp>(at)
    let tB, tC, tD = tramp e.b (stubs + 0x100n), tramp e.c (stubs + 0x200n), tramp e.d (stubs + 0x300n)
    let dP2 = Marshal.GetDelegateForFunctionPointer<Tramp>(nativeint callerP2)
    let dOther = Marshal.GetDelegateForFunctionPointer<Tramp>(nativeint callerOther)
    let fAcq = Touch(dP2, 1L, e.acq, e.rel)          // 114C38 called from phase 2
    let fRel = Touch(dP2, 2L, e.acq, e.rel)
    let oAcq = Touch(dOther, 1L, e.acq, e.rel)       // 114C38 called from 7C4BCC
    let oRel = Touch(dOther, 2L, e.acq, e.rel)
    let mem = Marshal.AllocHGlobal(0x10000)
    for j in 0 .. 0x10000 - 1 do Marshal.WriteByte(mem + nativeint j, 0uy)
    let lock = mem                                                   // SRWLOCK (8 bytes)
    let lock2 = mem + 0x40n
    let r14b = mem + 0x100n                                          // [r14+7590h] counter
    let rbpb = mem + 0x8000n                                         // [rbp+0Ch] flag byte
    let rsib = mem + 0x9000n                                         // [rsi] qword
    Marshal.WriteInt64(rsib, 0x1122334455667788L)
    let owner () = Marshal.ReadInt32(caveA + nativeint OFF_OWNER)
    let held () = Marshal.ReadInt64(caveA + nativeint OFF_HELD)
    let isFree (l: nativeint) = if TryAcquireSRWLockExclusive l <> 0uy then ReleaseSRWLockExclusive l; true else false
    let me = int (GetCurrentThreadId())
    // (the test thread must stay on one OS thread: fsi runs this on the main thread, delegates call natively)
    let callB () = tB.Invoke(r14b, rbpb, rsib, 5n)
    let callC () = tC.Invoke(r14b, rbpb, rsib, 0n)
    let callD () = tD.Invoke(r14b, rbpb, rsib, 0n)
    let r1 = callB ()
    check "S_B: claims owner = this thread, resumes, displaced add done ([r14+7590h] += r10d)" (owner () = me && r1 = 0xBL && Marshal.ReadInt32(r14b + 0x7590n) = 5)
    fAcq.Invoke(lock)
    check "capAcq (owner): acquires once, held = lock" (held () = int64 lock && not (isFree lock))
    fRel.Invoke(lock)
    check "capRel (owner): lock stays held" (not (isFree lock) && held () = int64 lock)
    for _ in 1 .. 50 do fAcq.Invoke(lock); fRel.Invoke(lock)
    check "50 more touches: no re-acquire (saved counter 50), lock still held once" (Marshal.ReadInt32(caveA + nativeint OFF_SAVED) = 50 && not (isFree lock))
    let rc = callC ()
    check "S_C: releases the held lock, held = 0, displaced mov rax,[rsi] done" (isFree lock && held () = 0L && rc = 0x1122334455667788L)
    fAcq.Invoke(lock); fRel.Invoke(lock)
    check "touch after S_C re-acquires and keeps it" (not (isFree lock) && held () = int64 lock)
    fAcq.Invoke(lock2)
    check "capAcq on a different lock: releases the first, holds the second" (isFree lock && not (isFree lock2) && held () = int64 lock2)
    Marshal.WriteByte(rbpb + 0x0Cn, 0uy)
    let rd = callD ()
    check "S_D: releases, owner = 0, held = 0; [rbp+0Ch] = 0 -> je path (129943)" (isFree lock2 && owner () = 0 && held () = 0L && rd = 0xEL)
    Marshal.WriteByte(rbpb + 0x0Cn, 1uy)
    check "S_D with nothing owned: no-op; [rbp+0Ch] = 1 -> jne path (1298BD)" (callD () = 0xDL && owner () = 0)
    fAcq.Invoke(lock)
    check "not owner: capAcq = plain AcquireSRWLockShared" (not (isFree lock) && held () = 0L)
    fRel.Invoke(lock)
    check "not owner: capRel = plain ReleaseSRWLockShared" (isFree lock)
    callB () |> ignore
    oAcq.Invoke(lock)
    check "owner, but 114C38 called from the other caller (7C4BCC): capAcq is stock" (not (isFree lock) && held () = 0L)
    oRel.Invoke(lock)
    check "... and capRel is stock (lock free again)" (isFree lock)
    callD () |> ignore
    let addBefore = Marshal.ReadInt32(r14b + 0x7590n)
    Marshal.WriteInt32(caveA + nativeint OFF_EN, 0)
    callB () |> ignore
    check "switch off: S_B does not claim (stock from the next texture), displaced add still done" (owner () = 0 && Marshal.ReadInt32(r14b + 0x7590n) = addBefore + 5)
    Marshal.WriteInt32(caveA + nativeint OFF_EN, 1)
    // owned by another thread: this thread's touches are stock, and S_B does not steal
    Marshal.WriteInt32(caveA + nativeint OFF_OWNER, me + 4)
    callB () |> ignore
    fAcq.Invoke(lock)
    check "another thread owns: S_B does not steal, capAcq is stock" (owner () = me + 4 && held () = 0L && not (isFree lock))
    fRel.Invoke(lock)
    check "... and capRel is stock" (isFree lock)
    callD () |> ignore
    check "S_D on a non-owner thread leaves the owner alone" (owner () = me + 4)
    Marshal.WriteInt32(caveA + nativeint OFF_OWNER, 0)
    // recursion: phase 2 entered again on the same thread while it owns (S_B no-op, the inner S_D ends the hold)
    callB () |> ignore; fAcq.Invoke(lock); fRel.Invoke(lock); callC () |> ignore; callB () |> ignore
    check "re-entered S_B while owning: owner unchanged, nothing held after S_C" (owner () = me && held () = 0L && isFree lock)
    callD () |> ignore; callD () |> ignore
    check "inner + outer S_D: owner = 0, lock free" (owner () = 0 && isFree lock)
    // ---------------- 2-thread stress: an exclusive taker vs textures of k touches each
    printfn "== stress: exclusive taker thread vs 200,000 textures x 1-40 touches (+ ~3 %% failed touches -> S_C)"
    let inside = mem + 0x200n                                        // set by the exclusive taker while it holds the lock
    let viol = ref 0
    let stop = ref false
    let exclusiveCount = ref 0
    let ex = Thread((fun () ->
        while not stop.Value do
            AcquireSRWLockExclusive lock
            Marshal.WriteInt32(inside, 1)
            Thread.SpinWait 20
            Marshal.WriteInt32(inside, 0)
            ReleaseSRWLockExclusive lock
            exclusiveCount.Value <- exclusiveCount.Value + 1
            Thread.SpinWait 50), 256 * 1024)
    // shared readers on a third thread (stand-in for TS_MESHLET_REQ workers): plain acquire/release via the caps (not owner)
    let sharedCount = ref 0
    let sh = Thread((fun () ->
        while not stop.Value do
            oAcq.Invoke(lock)
            if Marshal.ReadInt32(inside) <> 0 then Interlocked.Increment(viol) |> ignore
            oRel.Invoke(lock)
            sharedCount.Value <- sharedCount.Value + 1), 256 * 1024)
    ex.Start(); sh.Start()
    let rnd = Random(7)
    let sw = Stopwatch.StartNew()
    let mutable touches = 0L
    for _ in 1 .. 200000 do
        callB () |> ignore
        for _ in 1 .. 1 + rnd.Next 40 do
            fAcq.Invoke(lock)
            if Marshal.ReadInt32(inside) <> 0 then Interlocked.Increment(viol) |> ignore
            fRel.Invoke(lock)
            touches <- touches + 1L
            if rnd.Next 33 = 0 then callC () |> ignore
        callD () |> ignore
        if held () <> 0L || owner () <> 0 then Interlocked.Increment(viol) |> ignore
    stop.Value <- true
    ex.Join(); sh.Join()
    printfn "  %.0f ms, %d touches, %d exclusive sections, %d foreign shared sections" sw.Elapsed.TotalMilliseconds touches exclusiveCount.Value sharedCount.Value
    check "no deadlock, no overlap of the held window with an exclusive section, lock free at the end" (viol.Value = 0 && isFree lock && exclusiveCount.Value > 0)
    // ---------------- cost per touch: native loop of N fake-114C38 calls (acquire + release) from the phase-2 call site
    printfn "== cost per touch (native loop, 4M touches; 'busy' = another thread hammering the same lock shared, like TS_MESHLET_REQ)"
    let loopAt = stubs + 0x800n
    let lb = assemble loopAt (fun a ->
        let mutable top = a.CreateLabel()
        a.push(rbx); a.push(rsi); a.push(r12); a.push(r13); a.sub(rsp, 0x28)
        a.mov(rbx, rcx); a.mov(rsi, rdx); a.mov(r12, r8); a.mov(r13, r9)
        a.Label(&top)
        a.mov(rcx, rbx); a.mov(edx, 3); a.mov(r8, r12); a.mov(r9, r13)
        a.mov(rax, callerP2); a.call(rax)
        a.dec(rsi); a.jnz(top)
        a.add(rsp, 0x28); a.pop(r13); a.pop(r12); a.pop(rsi); a.pop(rbx); a.ret())
    Marshal.Copy(lb, 0, loopAt, lb.Length)
    let dLoop = Marshal.GetDelegateForFunctionPointer<Tramp>(loopAt)
    let N = 4000000
    let timeLoop () =
        let sw = Stopwatch.StartNew()
        dLoop.Invoke(lock, nativeint N, nativeint e.acq, nativeint e.rel) |> ignore
        sw.Elapsed.TotalMilliseconds * 1e6 / float N
    let measure () =
        let ts = Array.init 3 (fun _ -> timeLoop ()) |> Array.min                  // owner = 0 -> stock pair
        callB () |> ignore; fAcq.Invoke(lock)
        let tp = Array.init 3 (fun _ -> timeLoop ()) |> Array.min                  // owner -> PT1
        callD () |> ignore
        ts, tp
    let s0, p0 = measure ()
    printfn "  idle lock: stock %.1f ns/touch, PT1 %.1f ns/touch" s0 p0
    let stop2 = ref false
    let bg = Thread((fun () -> while not stop2.Value do oAcq.Invoke(lock); oRel.Invoke(lock)), 256 * 1024)
    bg.Start()
    Thread.Sleep 50
    let s1, p1 = measure ()
    stop2.Value <- true
    bg.Join()
    printfn "  busy lock: stock %.1f ns/touch, PT1 %.1f ns/touch" s1 p1
    printfn "== cave disassembly"
    printDis bytes.[OFF_CODE ..] (uint64 caveA + uint64 OFF_CODE)
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ================================================================== live
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let at (rva: int) = nativeint (B + uint64 rva)
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let rd64 (a: nativeint) = BitConverter.ToUInt64(t.Read(a, 8), 0)
    let build (c: uint64) = buildCave c (B + uint64 B_RESUME) (B + uint64 C_RESUME) (B + uint64 D_RESUME) (B + uint64 D_JE) (rd64 (at IAT_ACQ)) (rd64 (at IAT_REL)) (B + uint64 TOUCH_RET)
    let siteState () =
        let cur = [ for KeyValue(s, o) in ORIG -> s, t.Read(at s, o.Length) ]
        if cur |> List.forall (fun (s, b) -> b = ORIG.[s]) then "original"
        else
            match readState () with
            | Some c when (let _, e = build c in sitePatches B e |> List.forall (fun (s, p) -> t.Read(at s, p.Length) = p)) -> "patched"
            | _ -> sprintf "UNKNOWN/MIXED (%s)" (String.Join(" ", cur |> List.map (fun (s, b) -> sprintf "%X=%s" s (BitConverter.ToString b))))
    let bodiesMatch () =
        t.Read(at F114_LO, F114_HI - F114_LO) = img.[F114_LO .. F114_HI - 1] && t.Read(at 0x1290B8, 0x1299A0 - 0x1290B8) = img.[0x1290B8 .. 0x1299A0 - 1]
    match mode with
    | "dry" ->
        printfn "base=%X  sites: %s  114C38 + 1290B8 bodies match dump: %b" B (siteState ()) (bodiesMatch ())
        let bytes, _ = build (B + 0x5E00000UL)
        printfn "cave preview (%d bytes) at a dummy address:" bytes.Length
        printDis bytes.[OFF_CODE ..] (B + 0x5E00000UL + uint64 OFF_CODE)
    | "apply" ->
        writeGuard ()
        if siteState () <> "original" then failwithf "sites are %s - aborting (revert first)" (siteState ())
        if not (bodiesMatch ()) then failwith "114C38/1290B8 differ from the dump - wrong game version, aborting"
        let c =
            match readState () with
            | Some c -> c
            | None ->
                let mem = t.AllocNear(at S_B, 0x1000)
                let c = uint64 mem
                let bytes, _ = build c
                t.Write(mem, bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" c t.Process.Id)
                c
        let _, e = build c
        let n = safeWrite t [ for (s, p) in sitePatches B e -> at s, p, ORIG.[s] ]
        printfn "PT1 applied on attempt %d; cave %X; switch=%d" n c (BitConverter.ToInt32(t.Read(nativeint c, 4), 0))
    | "revert" ->
        writeGuard ()
        match readState () with
        | Some c when siteState () = "patched" ->
            t.Write(nativeint c + nativeint OFF_EN, BitConverter.GetBytes 0u)          // no new claims
            let sw = Stopwatch.StartNew()
            while BitConverter.ToInt32(t.Read(nativeint c + nativeint OFF_OWNER, 4), 0) <> 0 && sw.ElapsedMilliseconds < 3000L do Threading.Thread.Sleep 1
            if BitConverter.ToInt32(t.Read(nativeint c + nativeint OFF_OWNER, 4), 0) <> 0 then failwith "a texture still owns the lock after 3 s - not reverting (switch is off; retry)"
            let _, e = build c
            let n = safeWrite t [ for (s, p) in sitePatches B e -> at s, ORIG.[s], p ]
            printfn "PT1 reverted on attempt %d (cave left allocated)" n
        | _ -> printfn "sites are %s - nothing to revert" (siteState ())
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some c when siteState () = "patched" ->
            let v = if mode = "on" then 1u else 0u
            t.Write(nativeint c, BitConverter.GetBytes v)
            printfn "PT1 switch = %d" v
        | _ -> printfn "PT1 is not applied in this process - use apply"
    | "status" ->
        printfn "sites: %s" (siteState ())
        match readState () with
        | None -> printfn "no cave in this process"
        | Some c ->
            let rd (o: int) = BitConverter.ToUInt32(t.Read(nativeint c + nativeint o, 4), 0)
            printfn "switch=%d owner tid=%d held=%X" (rd OFF_EN) (rd OFF_OWNER) (rd64 (nativeint c + nativeint OFF_HELD))
            let mutable p = rd OFF_SAVED
            for _ in 1 .. 5 do
                Threading.Thread.Sleep 200
                let q = rd OFF_SAVED
                printfn "  lock pairs saved/s %8d" ((q - p) * 5u)
                p <- q
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
