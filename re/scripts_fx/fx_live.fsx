// fx_live.fsx - effect-update CPU patch for Nioh 3 (Steam v2.02). See notes\effects-animation.md.
//
//   dotnet fsi fx_live.fsx offline            NO GAME ACCESS: verify both sites against re\Nioh3.dump.exe, sweep .text for
//                                             branch targets inside the patched bytes, prove the clobbered registers and
//                                             flags dead at the return points, check overlap with every other patch set,
//                                             and run the cave natively (own process) against a recording stub.
//   dotnet fsi fx_live.fsx dry                READ-ONLY on the live game: site bytes + cave preview.
//   dotnet fsi fx_live.fsx apply              WRITES (needs FT_ALLOW_WRITE=1): allocates one cave per game process,
//                                             writes both sites with Mem.safeWrite (process frozen, RIP boundary check).
//   dotnet fsi fx_live.fsx status             READ-ONLY: site states, switch, N, updates/skips/passes per second.
//   dotnet fsi fx_live.fsx on|off             WRITES (FT_ALLOW_WRITE=1): 4-byte cave switch only, no code touched.
//   dotnet fsi fx_live.fsx rate <1|2|4|8>     WRITES (FT_ALLOW_WRITE=1): N (mask + float scale) in the cave data.
//   dotnet fsi fx_live.fsx revert             WRITES: original bytes back (the cave stays allocated).
//
// E1 - "NoDisp" effect throttle.
//   The effect manager pass 15FC80 (on KIDSTaskScheduler0, inside the game update 39E4E4 -> 39E910) walks EVERY live
//   effect instance each frame. Instances that are not displayed this frame (beyond their display distance, owner
//   hidden/faded, etc.; local "disp" flag == 0) are queued with 73A6F8 onto the 9 [App]EffectUpdateNoDispThread lists
//   (round robin), and those threads run the full KTGL effect update 3D9F64(sys, dt) on them: track timers, emitters,
//   particle simulation (C7A7C / C8498 type switch) - for effects nobody can see. Displayed ones go to the 4
//   EffectUpdateDisp threads via 73A784 and are NOT touched by this patch.
//   Patch: update each invisible instance only on every N-th pass (fixed per-instance phase from its pointer), with
//   dt*N, so its clock keeps up (exactly when dt is constant, i.e. at a fixed fps tier).
//   Sites:
//     A 7E82B4 `33 D2 44 8B CA` (xor edx,edx; mov r9d,edx; entry of the per-pass list reset, called once per update
//       pass from 39E9B0, and from the clear-all 20AAA34) -> `jmp caveA`: ++pass; xor edx,edx; mov r9d,edx; jmp 7E82B9.
//     B 160B83 `E8 <73A6F8>` (the only NoDisp enqueue call) -> `E8 <caveB>`:
//         if (!en) tail-jmp 73A6F8                                   ; stock
//         ph = (u32)((sys>>7) ^ (sys>>13)) + pass
//         if (ph & mask) { ++skips; ret }                            ; not this instance's turn: not queued
//         xmm3 *= scale(N); ++updates; tail-jmp 73A6F8               ; queued with N passes' worth of dt
//   rcx/rdx/r8 (args) are untouched; the cave clobbers rax, r9, r10, xmm3, flags = volatile at a call (MS x64 ABI);
//   offline proves rax/r9/r10/flags dead at the return 160B88.
//   Bounded differences vs stock (invisible effects only):
//     - state of an invisible effect is up to N-1 passes old; when it becomes visible again its clock can lag by up
//       to (N-1)*dt once per NoDisp->Disp transition (16 ms at N=2 / 60 fps);
//     - large-step simulation (dt*N) for invisible emitters; finished invisible effects end/release up to N-1 passes
//       later.
//     - VISIBLE effects, the Disp threads and anything with the engine's own "always full update" bit (sys+0xCC bit 8,
//       checked by the caller before 73A6F8) are untouched.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "fx_live_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let img = File.ReadAllBytes DUMP
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

let A_SITE, A_ORIG, A_BACK = 0x7E82B4, "33 D2 44 8B CA", 0x7E82B9UL     // EffectThreadListsReset entry
let B_SITE, B_ORIG, B_RET = 0x160B83, "E8 70 9B 5D 00", 0x160B88UL      // call EffectNoDispEnqueue
let ENQ = 0x73A6F8UL                                                     // EffectNoDispEnqueue(mgr, idx, sys, dt)
let DEFAULT_N = 2

/// sites of every other patch set (workers_live, cpu_caves, fg_cave, ft_vrr, mainthread, W1/W2, meshlet_caves,
/// pool_patch) + workers_live M1/M2/J1 rows - must not overlap
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4
      0x419A8; 0x4ABD2; 0x1E72C6; 0x1E71CD; 0x3777A40; 0x1FE30D5
      0x49131; 0x48FB9; 0x49046; 0x49261; 0x4933D; 0x493DC; 0x4122D; 0x41265; 0x41397; 0x414B6; 0x40899; 0x40481
      0x3E7F5; 0x3E821 ]

// ---- cave ---------------------------------------------------------------------------------------------------
// data (one line, written by the update thread only): en @+0, mask @+4, scale f32 @+8, pass @+0Ch,
// updates u64 @+10h, skips u64 @+18h. code: caveA @+40h, caveB after it.
type Cave = { Bytes: byte[]; A: uint64; B: uint64; En: uint64; Mask: uint64; Scale: uint64; Pass: uint64; Upd: uint64; Skip: uint64 }

/// `enq` = address of 73A6F8 (the native test passes a recording stub), `aBack` = 7E82B9.
let build (at: uint64) (enq: uint64) (aBack: uint64) (n: int) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lEn, lMask, lScale, lPass, lUpd, lSkip = L(), L(), L(), L(), L(), L()
    let mutable cA, cB, stock, skip = L(), L(), L(), L()
    a.Label(&lEn); a.dd(1u)
    a.Label(&lMask); a.dd(uint32 (n - 1))
    a.Label(&lScale); a.dd(BitConverter.SingleToUInt32Bits(float32 n))
    a.Label(&lPass); a.dd(0u)
    a.Label(&lUpd); a.dq(0UL)
    a.Label(&lSkip); a.dq(0UL)
    for _ in 1 .. 8 do a.dd(0u)                         // pad to 40h
    // ===== caveA: entered by jmp from 7E82B4 (function entry, rcx = effect thread manager)
    a.Label(&cA)
    a.inc(__dword_ptr.[lPass])                         // one per update pass (statistics + phase)
    a.xor(edx, edx)                                    // the original two instructions (flags = xor's, as stock)
    a.mov(r9d, edx)
    a.jmp(aBack)
    // ===== caveB: entered by `call` from 160B83. rcx = mgr, edx = NoDisp list index, r8 = effect sys, xmm3 = dt
    a.Label(&cB)
    a.cmp(__dword_ptr.[lEn], 0); a.je(stock)
    a.mov(r9, r8); a.shr(r9, 7uy)
    a.mov(r10, r8); a.shr(r10, 13uy)
    a.xor(r9, r10)
    a.mov(eax, __dword_ptr.[lPass])
    a.add(eax, r9d)
    a.test(__dword_ptr.[lMask], eax); a.jne(skip)
    a.mulss(xmm3, __dword_ptr.[lScale])               // dt * N
    a.inc(__qword_ptr.[lUpd])
    a.Label(&stock)
    a.jmp(enq)                                         // tail jump: 73A6F8 returns to 160B88
    a.Label(&skip)
    a.inc(__qword_ptr.[lSkip])
    a.ret()
    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    let c = { Bytes = ms.ToArray(); A = addr cA; B = addr cB; En = addr lEn; Mask = addr lMask; Scale = addr lScale
              Pass = addr lPass; Upd = addr lUpd; Skip = addr lSkip }
    if c.A - at <> 0x40UL || c.Pass - at <> 0xCUL || c.Upd - at <> 0x10UL || c.Skip - at <> 0x18UL then failwith "cave layout"
    c

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let aNew (B: uint64) (c: Cave) = Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 A_SITE + 5UL) c.A ]
let bNew (B: uint64) (c: Cave) = Array.concat [ [| 0xE8uy |]; rel32 (B + uint64 B_SITE + 5UL) c.B ]
let writes (B: uint64) (c: Cave) = [ A_SITE, aNew B c, hexb A_ORIG; B_SITE, bNew B c, hexb B_ORIG ]

// ---- disassembly / static helpers ---------------------------------------------------------------------------
let decodeAt (rva: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(img, int rva, 16))
    d.IP <- rva
    d.Decode()
let decodeAll (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    [ while d.IP < ip + uint64 bytes.Length do yield d.Decode() ]
let fmtIns =
    let fmt = NasmFormatter()
    fmt.Options.RipRelativeAddresses <- true
    let so = StringOutput()
    fun (ins: Instruction) -> (let mutable i = ins in fmt.Format(&i, so); so.ToStringAndReset())
let printDis (bytes: byte[]) (at: uint64) =
    for ins in decodeAll bytes at do printfn "  %016X  %s" ins.IP (fmtIns ins)

let infoFactory = InstructionInfoFactory()
let volatileRegs = [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ]
let argRegs = [ Register.RCX; Register.RDX; Register.R8; Register.R9 ]
let allStatus = RflagsBits.OF ||| RflagsBits.SF ||| RflagsBits.ZF ||| RflagsBits.AF ||| RflagsBits.CF ||| RflagsBits.PF
let isShiftByCl (ins: Instruction) =
    ins.OpCount = 2 && ins.Op1Kind = OpKind.Register && ins.Op1Register = Register.CL &&
    (match ins.Mnemonic with
     | Mnemonic.Shl | Mnemonic.Shr | Mnemonic.Sar | Mnemonic.Sal | Mnemonic.Rol | Mnemonic.Ror | Mnemonic.Rcl | Mnemonic.Rcr -> true
     | _ -> false)
/// Same walker as re\scripts_workers\workers_live.fsx: every path from `starts` until each tracked register is fully
/// written (or killed by a call) and all status flags are written; fails on any read before that.
let rec walk (starts: uint64 list) (regs: Register list) (flags: bool) (depth: int) : string list =
    let regIdx = regs |> List.mapi (fun i r -> r, i) |> dict
    let flagBit = 1 <<< regs.Length
    let full0 = (1 <<< regs.Length) - 1 ||| (if flags then flagBit else 0)
    let seen = HashSet<struct (uint64 * int)>()
    let fails = ResizeArray<string>()
    let stack = Stack<uint64 * int>()
    for s in starts do stack.Push((s, full0))
    while stack.Count > 0 && fails.Count < 8 do
        let ip, m0 = stack.Pop()
        if m0 <> 0 && seen.Add(struct (ip, m0)) then
            let ins = decodeAt ip
            let mutable m = m0
            let fail why = fails.Add(sprintf "%s%X  %s  : %s" (String(' ', 2 * depth)) ip (fmtIns ins) why)
            let tracked (m: int) r = match regIdx.TryGetValue r with | true, i -> (m &&& (1 <<< i)) <> 0 | _ -> false
            let isZeroIdiom =
                (ins.Mnemonic = Mnemonic.Xor || ins.Mnemonic = Mnemonic.Sub) && ins.OpCount = 2 &&
                ins.Op0Kind = OpKind.Register && ins.Op1Kind = OpKind.Register && ins.Op0Register = ins.Op1Register
            let info = infoFactory.GetInfo(&ins)
            if flags && (m &&& flagBit) <> 0 && ins.RflagsRead <> RflagsBits.None then fail "reads flags"
            if not isZeroIdiom then
                for u in info.GetUsedRegisters() do
                    if tracked m (u.Register.GetFullRegister()) then
                        match u.Access with
                        | OpAccess.Read | OpAccess.CondRead | OpAccess.ReadWrite | OpAccess.ReadCondWrite -> fail (sprintf "reads %O" u.Register)
                        | _ -> ()
            match ins.FlowControl with
            | FlowControl.Call ->
                let live = argRegs |> List.filter (tracked m)
                if not live.IsEmpty then
                    if depth >= 4 then fail (sprintf "call depth limit with %A tracked" live)
                    else for f in walk [ ins.NearBranchTarget ] live false (depth + 1) do fail (sprintf "callee %X: %s" ins.NearBranchTarget f)
            | FlowControl.IndirectCall ->
                let live = argRegs |> List.filter (tracked m)
                if not live.IsEmpty then fail (sprintf "virtual call with %A tracked" live)
            | FlowControl.IndirectBranch -> fail "indirect branch (cannot follow)"
            | _ -> ()
            for u in info.GetUsedRegisters() do
                if u.Access = OpAccess.Write && u.Register.GetSize() >= 4 then
                    match regIdx.TryGetValue (u.Register.GetFullRegister()) with
                    | true, i -> m <- m &&& ~~~(1 <<< i)
                    | _ -> ()
            if isZeroIdiom && ins.Op0Register.GetSize() >= 4 then
                match regIdx.TryGetValue (ins.Op0Register.GetFullRegister()) with
                | true, i -> m <- m &&& ~~~(1 <<< i)
                | _ -> ()
            if flags && not (isShiftByCl ins) && (ins.RflagsModified &&& allStatus) = allStatus then m <- m &&& ~~~flagBit
            match ins.FlowControl with
            | FlowControl.Call | FlowControl.IndirectCall ->
                for r in volatileRegs do
                    match regIdx.TryGetValue r with
                    | true, i -> m <- m &&& ~~~(1 <<< i)
                    | _ -> ()
                m <- m &&& ~~~flagBit
                stack.Push((ins.NextIP, m))
            | FlowControl.Next | FlowControl.Exception -> stack.Push((ins.NextIP, m))
            | FlowControl.UnconditionalBranch -> stack.Push((ins.NearBranchTarget, m))
            | FlowControl.ConditionalBranch -> stack.Push((ins.NearBranchTarget, m)); stack.Push((ins.NextIP, m))
            | _ -> ()
    List.ofSeq fails

/// direct branch/call targets anywhere in .text that land strictly inside a patched range; also counts calls to `fn`
let scanText (ranges: (string * uint64 * uint64) list) (fns: uint64 list) =
    let pe = BitConverter.ToInt32(img, 0x3C)
    let nsec = int (BitConverter.ToUInt16(img, pe + 6))
    let optsz = int (BitConverter.ToUInt16(img, pe + 20))
    let hits = ResizeArray<string>()
    let callers = Dictionary<uint64, ResizeArray<uint64>>()
    for f in fns do callers.[f] <- ResizeArray()
    for i in 0 .. nsec - 1 do
        let sh = pe + 24 + optsz + i * 40
        let vs = BitConverter.ToInt32(img, sh + 8)
        let va = BitConverter.ToInt32(img, sh + 12)
        let ch = BitConverter.ToUInt32(img, sh + 36)
        if ch &&& 0x20000000u <> 0u then
            let dec = Decoder.Create(64, ByteArrayCodeReader(img, va, vs))
            dec.IP <- uint64 va
            let fin = uint64 (va + vs)
            while dec.IP < fin do
                let ins = dec.Decode()
                match ins.FlowControl with
                | FlowControl.UnconditionalBranch | FlowControl.ConditionalBranch | FlowControl.Call ->
                    let t = ins.NearBranchTarget
                    for (id, lo, hi) in ranges do
                        if t > lo && t < hi then hits.Add(sprintf "%X -> %X (inside %s @%X)" ins.IP t id lo)
                    match callers.TryGetValue t with
                    | true, l -> l.Add ins.IP
                    | _ -> ()
                | _ -> ()
    hits, callers

// ---- native harness: the cave against a recording stub (this process only) ---------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type CallFn = delegate of nativeint * int * nativeint * int -> int
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type ResetFn = delegate of nativeint -> int64

let nativeTest () =
    let exec = VirtualAlloc(0n, 0x3000un, 0x3000u, 0x40u)
    let STUB, THB, THA, BACK, REC, CAVE = exec, exec + 0x100n, exec + 0x200n, exec + 0x300n, exec + 0x800n, exec + 0x1000n
    // recording stub = "73A6F8": rec = {rcx, rdx(u32), r8, xmm3(f32), count}; returns 0x5A
    let stub = assemble STUB (fun a ->
        a.mov(rax, uint64 REC)
        a.mov(__qword_ptr.[rax], rcx); a.mov(__dword_ptr.[rax + 8L], edx); a.mov(__qword_ptr.[rax + 0x10L], r8)
        a.movss(__dword_ptr.[rax + 0x18L], xmm3); a.inc(__dword_ptr.[rax + 0x1CL])
        a.mov(eax, 0x5A); a.ret())
    Marshal.Copy(stub, 0, STUB, stub.Length)
    // "7E82B9": return rdx | (r9 << 32) | flags so the test sees edx, r9d and ZF after caveA
    let back = assemble BACK (fun a ->
        a.pushfq(); a.pop(rax); a.``and``(eax, 0x8C5)        // OF SF ZF PF CF (AF is undefined after xor)
        a.shl(rax, 40uy); a.mov(r10d, r9d); a.shl(r10, 20uy); a.``or``(rax, r10); a.``or``(rax, rdx); a.ret())
    Marshal.Copy(back, 0, BACK, back.Length)
    let mutable cave = build (uint64 CAVE) (uint64 STUB) (uint64 BACK) DEFAULT_N
    let load (c: Cave) = Marshal.Copy(c.Bytes, 0, CAVE, c.Bytes.Length)
    load cave
    // thunkB(mgr, idx, sys, dtBits): movd xmm3, r9d ; call caveB ; return eax (0x5A when the stub ran, else the skip
    // path's eax), with rsp aligned like a real call site
    let thb = assemble THB (fun a ->
        a.sub(rsp, 0x28); a.movd(xmm3, r9d); a.mov(r9d, 0x77777777); a.mov(r10, 0x1234UL)
        a.call(cave.B); a.add(rsp, 0x28); a.ret())
    Marshal.Copy(thb, 0, THB, thb.Length)
    // thunkA(mgr): r9 = garbage, edx = garbage, flags = garbage; jmp caveA (BACK returns to our caller)
    let tha = assemble THA (fun a -> a.mov(edx, 0xDEAD); a.mov(r9, 0xBEEFUL); a.cmp(rcx, 0); a.jmp(cave.A))
    Marshal.Copy(tha, 0, THA, tha.Length)
    let callB = Marshal.GetDelegateForFunctionPointer<CallFn>(THB)
    let callA = Marshal.GetDelegateForFunctionPointer<ResetFn>(THA)
    let rd32 (a: uint64) = Marshal.ReadInt32(nativeint a)
    let wr32 (a: uint64) (v: int) = Marshal.WriteInt32(nativeint a, v)
    let rec_ () = Marshal.ReadInt64(REC), Marshal.ReadInt32(REC + 8n), Marshal.ReadInt64(REC + 0x10n), BitConverter.Int32BitsToSingle(Marshal.ReadInt32(REC + 0x18n)), Marshal.ReadInt32(REC + 0x1Cn)
    let setN n = wr32 cave.Mask (n - 1); wr32 cave.Scale (BitConverter.SingleToInt32Bits(float32 n))
    let mutable bad = 0
    let report (s: string) = (bad <- bad + 1; if bad <= 10 then printfn "  MISMATCH %s" s)
    // (1) caveA: ++pass, edx = r9d = 0, flags = those of `xor edx,edx` (ZF=1 PF=1, others 0)
    let p0 = rd32 cave.Pass
    let r = callA.Invoke(0x1000n)
    let edxv, r9v, fl = r &&& 0xFFFFFL, (r >>> 20) &&& 0xFFFFFL, r >>> 40
    if rd32 cave.Pass <> p0 + 1 || edxv <> 0L || r9v <> 0L || fl <> 0x44L then report (sprintf "caveA pass %d->%d edx %X r9 %X flags %X" p0 (rd32 cave.Pass) edxv r9v fl)
    // (2) switch off: every call reaches the stub with identical args, dt untouched
    let rnd = Random(20261006)
    wr32 cave.En 0
    for i in 1 .. 2000 do
        let sys = nativeint (0x200000000L + int64 (rnd.Next()) * 16L)
        let dt = float32 (rnd.NextDouble())
        let idx = rnd.Next 9
        let before = Marshal.ReadInt32(REC + 0x1Cn)
        let ret = callB.Invoke(0x7777n, idx, sys, BitConverter.SingleToInt32Bits dt)
        let (m, ix, s, d, cnt) = rec_ ()
        if ret <> 0x5A || cnt <> before + 1 || m <> 0x7777L || ix <> idx || s <> int64 sys || d <> dt then report (sprintf "off: call %d" i)
    // (3) switch on, N = 1/2/4/8: over N consecutive passes every instance is queued exactly once, with dt*N; args intact
    wr32 cave.En 1
    let mutable cases = 0
    for n in [ 1; 2; 4; 8 ] do
        setN n
        for round in 1 .. 300 do
            let objs = [| for _ in 1 .. rnd.Next(1, 60) -> nativeint (0x200000000L + int64 (rnd.Next()) * 16L) |] |> Array.distinct
            let dt = float32 (rnd.NextDouble() * 0.05)
            let got = Dictionary<nativeint, ResizeArray<float32>>()
            for o in objs do got.[o] <- ResizeArray()
            for _ in 1 .. n do
                callA.Invoke(0n) |> ignore                    // a new pass
                for o in objs do
                    let idx = rnd.Next 9
                    let before = Marshal.ReadInt32(REC + 0x1Cn)
                    let ret = callB.Invoke(0x9999n, idx, o, BitConverter.SingleToInt32Bits dt)
                    let (m, ix, s, d, cnt) = rec_ ()
                    if cnt = before + 1 then
                        if ret <> 0x5A || m <> 0x9999L || ix <> idx || s <> int64 o then report (sprintf "on N=%d: args" n)
                        got.[o].Add d
            for o in objs do
                cases <- cases + 1
                let l = got.[o]
                if l.Count <> 1 then report (sprintf "N=%d: instance %X queued %d times in %d passes" n o l.Count n)
                elif l.[0] <> dt * float32 n then report (sprintf "N=%d: dt %g expected %g" n l.[0] (dt * float32 n))
    let upd, skp = Marshal.ReadInt64(nativeint cave.Upd), Marshal.ReadInt64(nativeint cave.Skip)
    printfn "  caveA: pass++, edx=r9d=0, flags as `xor edx,edx`; switch off: 2000 calls forwarded unchanged"
    printfn "  switch on: %d instance windows (N=1,2,4,8), each queued exactly once per N passes with dt*N; cave counters upd=%d skip=%d; %d mismatches" cases upd skp bad
    bad = 0

// ---- offline ------------------------------------------------------------------------------------------------
let offline () =
    printfn "dump: %s (raw offset == RVA)" DUMP
    let mutable ok = true
    let B = 0x7FF74F410000UL
    let c = build (B + 0x5E00000UL) (B + ENQ) (B + A_BACK) DEFAULT_N
    printfn "site bytes vs dump:"
    for (rva, o, id) in [ A_SITE, hexb A_ORIG, "A"; B_SITE, hexb B_ORIG, "B" ] do
        let cur = img.[rva .. rva + o.Length - 1]
        let m = cur = o
        ok <- ok && m
        printfn "  %s %8X  %-9s %s" id rva (if m then "MATCH" else "DIFFERENT") (hexs cur)
        for s in OTHER_SITES do
            if s < rva + o.Length && rva < s + 10 then (printfn "  !! %s %X overlaps another patch site %X" id rva s; ok <- false)
    let callIns = decodeAt (uint64 B_SITE)
    if not (callIns.FlowControl = FlowControl.Call && callIns.NearBranchTarget = ENQ) then (printfn "  !! 160B83 is not call 73A6F8"; ok <- false)
    printfn "cave @dummy %X: %d bytes (data 40h)" (B + 0x5E00000UL) c.Bytes.Length
    printDis c.Bytes.[0x40 ..] c.A
    printfn "patched sites:"
    for (rva, nw, o) in writes B c do
        printfn "  %X  %s -> %s" rva (hexs o) (hexs nw)
        printDis nw (B + uint64 rva)
        let both = Set.intersect (Set.ofSeq (boundaries nw (B + uint64 rva))) (Set.ofSeq (boundaries o (B + uint64 rva)))
        printfn "    instruction starts valid in both encodings: %A" (both |> Set.map (fun x -> sprintf "%X" (x - B)) |> Set.toList)
    printfn "branch targets / callers (whole .text linear sweep):"
    let hits, callers = scanText [ "A", uint64 A_SITE, uint64 A_SITE + 5UL; "B", uint64 B_SITE, uint64 B_SITE + 5UL ] [ ENQ; uint64 A_SITE ]
    if hits.Count = 0 then printfn "  no direct branch lands inside a patched range" else (for h in hits do printfn "  !! %s" h); ok <- ok && hits.Count = 0
    let enqCallers = callers.[ENQ] |> Seq.toList
    printfn "  callers of 73A6F8: %s" (enqCallers |> List.map (sprintf "%X") |> String.concat ", ")
    if enqCallers <> [ uint64 B_SITE ] then (printfn "  !! expected exactly one caller (160B83)"; ok <- false)
    printfn "  callers of 7E82B4: %s  (39E9B0 = per-pass in 39E910, 20AAA8E = clear-all 20AAA34)" (callers.[uint64 A_SITE] |> Seq.map (sprintf "%X") |> String.concat ", ")
    printfn "dead-register proofs:"
    let f1 = walk [ B_RET ] [ Register.RAX; Register.R9; Register.R10 ] true 0
    if f1.IsEmpty then printfn "  [ok] rax, r9, r10, flags dead at 160B88 (caveB clobbers them; xmm3 is volatile across a call by the ABI)"
    else (ok <- false; for x in f1 do printfn "  [FAIL] %s" x)
    // caveA: the only extra effect vs stock is the cave's own counter; flags/edx/r9d at 7E82B9 equal stock (xor last)
    printfn "  [ok] caveA leaves edx/r9d/flags exactly as `xor edx,edx; mov r9d,edx` (checked natively below)"
    printfn "native test (cave vs a recording stub for 73A6F8 / 7E82B9):"
    ok <- nativeTest () && ok
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live modes ---------------------------------------------------------------------------------------------
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readState () =
        if not (File.Exists stateFile) then None
        else
            let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
            if v.Length < 2 || int v.[1] <> t.Process.Id then None else Some v.[0]
    let caveOrProbe () = match readState () with Some a -> build a (B + ENQ) (B + A_BACK) DEFAULT_N | None -> build (B + 0x5E00000UL) (B + ENQ) (B + A_BACK) DEFAULT_N
    let classify (c: Cave) =
        let st =
            [ for (rva, nw, o) in writes B c do
                let cur = t.Read(nativeint (B + uint64 rva), o.Length)
                let s = if cur = o then "original" elif cur = nw then "patched" else "UNKNOWN"
                printfn "  %8X  %-8s  %s" rva s (hexs cur)
                yield s ]
        match List.distinct st with [ s ] -> s | l -> if List.contains "UNKNOWN" l then "UNKNOWN" else "MIXED"
    let rd32 (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
    let rd64 (a: uint64) = BitConverter.ToUInt64(t.Read(nativeint a, 8), 0)
    match mode with
    | "dry" ->
        let c = caveOrProbe ()
        printfn "base=%X cave=%s" B (match readState () with Some a -> sprintf "%X" a | None -> "(none, preview)")
        printfn "state: %s" (classify c)
        printDis c.Bytes.[0x40 ..] c.A
    | "apply" ->
        writeGuard ()
        if classify (caveOrProbe ()) <> "original" then failwith "sites not original - aborting (revert first)"
        let c =
            match readState () with
            | Some a -> build a (B + ENQ) (B + A_BACK) DEFAULT_N
            | None ->
                let mem = t.AllocNear(nativeint (B + uint64 B_SITE), 0x1000)
                let c = build (uint64 mem) (B + ENQ) (B + A_BACK) DEFAULT_N
                t.Write(mem, c.Bytes)
                File.WriteAllText(stateFile, sprintf "%X %X" (uint64 mem) t.Process.Id)
                c
        // A before B in one frozen write: B's phase needs passes counted, but either order is safe
        let n = safeWrite t [ for (rva, nw, o) in writes B c -> nativeint (B + uint64 rva), nw, o ]
        printfn "applied E1 on attempt %d; cave %X; switch=%d N=%d" n (c.A - 0x40UL) (rd32 c.En) (rd32 c.Mask + 1u)
    | "revert" ->
        writeGuard ()
        let c = caveOrProbe ()
        match classify c with
        | "UNKNOWN" -> failwith "unexpected bytes - aborting"
        | "original" -> printfn "already original"
        | _ ->
            let ws = [ for (rva, nw, o) in writes B c do
                         let cur = t.Read(nativeint (B + uint64 rva), o.Length)
                         if cur = nw then yield nativeint (B + uint64 rva), o, nw ]
            safeWrite t ws |> ignore
            printfn "reverted (cave left allocated)"
    | "on" | "off" ->
        writeGuard ()
        match readState () with
        | Some a ->
            let c = build a (B + ENQ) (B + A_BACK) DEFAULT_N
            t.Write(nativeint c.En, BitConverter.GetBytes(if mode = "on" then 1u else 0u))
            printfn "E1 switch %s" mode
        | None -> printfn "E1 is not applied in this process - use apply"
    | "rate" ->
        writeGuard ()
        let n = if args.Length > 1 then int args.[1] else DEFAULT_N
        if not (List.contains n [ 1; 2; 4; 8 ]) then failwith "N must be 1, 2, 4 or 8"
        match readState () with
        | Some a ->
            let c = build a (B + ENQ) (B + A_BACK) n
            t.Write(nativeint c.Mask, BitConverter.GetBytes(uint32 (n - 1)))
            t.Write(nativeint c.Scale, BitConverter.GetBytes(float32 n))
            printfn "E1 N=%d" n
        | None -> printfn "E1 is not applied in this process - use apply"
    | "status" ->
        let c = caveOrProbe ()
        printfn "E1: %s" (classify c)
        match readState () with
        | None -> printfn "no cave in this process"
        | Some _ ->
            printfn "switch=%d N=%d" (rd32 c.En) (rd32 c.Mask + 1u)
            let mutable u, s, p = rd64 c.Upd, rd64 c.Skip, rd32 c.Pass
            for _ in 1 .. 10 do
                Threading.Thread.Sleep 1000
                let u2, s2, p2 = rd64 c.Upd, rd64 c.Skip, rd32 c.Pass
                let per = if p2 > p then float (u2 - u + s2 - s) / float (p2 - p) else 0.0
                printfn "  passes/s %4d   invisible effects queued/s %7d   skipped/s %7d   (~%.0f invisible instances per pass)" (p2 - p) (u2 - u) (s2 - s) per
                u <- u2; s <- s2; p <- p2
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
