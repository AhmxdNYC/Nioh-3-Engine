// bounds_offline.fsx - BOUNDS patch group (notes\bounds-partobbs.md): offline proof + timing, NO game access.
//
//   dotnet fsi bounds_offline.fsx            all checks + timing (writes nothing outside this process)
//
// Target: Nioh3+71C60 BoundsFromPartObbs (per-part OBB -> AABB union, logic thread KIDSTaskScheduler0).
// Three independent parts, one switch dword (bit0 Q, bit1 L, bit2 V), same caves as mod\Nioh3EngineFix\src\caves.asm:
//   Q  per-part quaternion memo. The half-float path rebuilds every part's rotation quaternion each frame from 4 packed
//      halves (axis xyz + angle @part+8): 3 half unpacks, SinCosApprox 54360, sum of squares, sqrtss, divss, 4 mulss.
//      It is a pure function of those 8 bytes. Hook A (71EFE) looks the 8 bytes up in a seqlock table and on a hit
//      writes the stored quaternion and skips to 7206F; on a miss it runs the stock code, and hook B (72053) inserts
//      the stock result, keyed by the halves re-packed from the four unpacked floats the stock code actually used.
//   L  the two locked "atomic loads" per part of the object's ready word ([obj+3F8h]): 71DB2 (lock cmpxchg [rcx],r10d,
//      r10d = 0) and 1A7AE2 in ObjResTreeAllReady 1A7AAC (lock cmpxchg [rdx],ebx, ebx = 0) -> plain loads (eax identical).
//   V  the 6 Vec4CopyW0 (174610) calls in 71C60 -> a copy that reads the source with 4-byte loads (the sources were just
//      written with 4-byte stores, so the stock 16-byte load fails store-to-load forwarding; 174613 is the hot RIP)
//      and writes the destination with ONE 16-byte store (w already 0), so ObbToAabb's 16-byte loads forward too.
// Proof: the whole 71C60 (and everything it calls) is copied from the dump into two private regions at their RVAs; one
// stays stock, the other gets the patch bytes; both run on the same random inputs and the output OBB is compared bitwise.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Diagnostics
open System.Collections.Generic
open System.Runtime.InteropServices
open System.Threading
open Iced.Intel
open type Iced.Intel.AssemblerRegisters

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32at o = BitConverter.ToUInt32(img, o)
let mutable ok = true
let check name c = printfn "  [%s] %s" (if c then "ok" else "FAIL") name; ok <- ok && c
let hex (b: byte[]) = b |> Array.map (sprintf "%02X") |> String.concat " "

// ------------------------------------------------------------------ sites
let FN = 0x71C60
let QA, QA_LEN, QA_RES = 0x71EFE, 6, 0x71F04            // movzx eax,word [rbx+6] ; mov ecx,eax
let QB, QB_LEN, QB_RES = 0x72053, 7, 0x7206F            // movss [rsp+40h],xmm11 (jmp), cave redoes 72053..7206E
let Q_HIT = 0x7206F
let L1, L1_LEN, L1_RES = 0x71DB2, 5, 0x71DB7            // lock cmpxchg [rcx],r10d
let L2, L2_LEN, L2_RES = 0x1A7AE0, 6, 0x1A7AE6          // xor eax,eax ; lock cmpxchg [rdx],ebx
let VFN = 0x174610
let VCALLS = [| 0x72310; 0x7231C; 0x72346; 0x7250B; 0x72519; 0x72546 |]
let ORIG = dict [ QA, "0F B7 43 06 8B C8"; QB, "F3 44 0F 11 5C 24 40"; L1, "F0 44 0F B1 11"; L2, "33 C0 F0 0F B1 1A" ]
let QB_DISPLACED = 0x7206F - 0x72053                    // 28 bytes: 4 x movss [rsp+..],xmmN

// ------------------------------------------------------------------ pdata + code walker
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32at (dd + 24)), int (u32at (dd + 28))
let npd = excSz / 12
let pB = Array.init npd (fun i -> u32at (excRva + i * 12))
let pE = Array.init npd (fun i -> u32at (excRva + i * 12 + 4))

/// stubs: reached only on paths the harness never takes (not-ready object, negative sqrt, flag 0x100000 object)
let STUBS = set [ 0xBE0378; 0x17452C; 0x22276C; 0x21DC10; 0x190CE64 ]
let codeBytes = HashSet<int>()          // every byte of every reachable instruction
let dataRefs = Dictionary<int, int>()   // rip-relative address -> size
let insAt = SortedDictionary<int, Instruction>()
let walk (entries: int list) =
    let work = Stack<int>(entries)
    while work.Count > 0 do
        let mutable a = work.Pop()
        let mutable go = not (insAt.ContainsKey a) && not (STUBS.Contains a)
        while go do
            let d = Decoder.Create(64, ByteArrayCodeReader(img, a, 16))
            d.IP <- uint64 a
            let ins = d.Decode()
            if ins.IsInvalid then failwithf "invalid instruction at %X" a
            insAt.[a] <- ins
            for k in 0 .. ins.Length - 1 do codeBytes.Add(a + k) |> ignore
            if ins.IsIPRelativeMemoryOperand then
                let t = int ins.IPRelativeMemoryAddress
                let sz = max 4 (MemorySizeExtensions.GetSize ins.MemorySize)
                dataRefs.[t] <- max sz (match dataRefs.TryGetValue t with | true, v -> v | _ -> 0)
            let next = a + ins.Length
            match ins.FlowControl with
            | FlowControl.Return | FlowControl.Interrupt | FlowControl.IndirectBranch | FlowControl.Exception -> go <- false
            | FlowControl.UnconditionalBranch ->
                work.Push(int ins.NearBranchTarget); go <- false
            | FlowControl.ConditionalBranch ->
                work.Push(int ins.NearBranchTarget); a <- next
            | FlowControl.Call ->
                if ins.Op0Kind = OpKind.NearBranch64 then work.Push(int ins.NearBranchTarget)
                a <- next
            | _ -> a <- next
            if go && insAt.ContainsKey a then go <- false

printfn "== code walk from 71C60 (direct branches + calls; stubs %s)" (String.Join(", ", STUBS |> Seq.map (sprintf "%X")))
walk [ FN ]
// the stub targets must really be called from the walked code only on the paths described
let ranges =
    let s = codeBytes |> Seq.sort |> Array.ofSeq
    let r = List<int * int>()
    let mutable lo, hi = s.[0], s.[0] + 1
    for x in s.[1..] do
        if x = hi then hi <- x + 1 else (r.Add((lo, hi)); lo <- x; hi <- x + 1)
    r.Add((lo, hi)); r.ToArray()
printfn "  %d instructions, %d code bytes in %d ranges, %d rip-relative data refs" insAt.Count codeBytes.Count ranges.Length dataRefs.Count
let funcs = insAt.Keys |> Seq.choose (fun a -> let i = Array.BinarySearch(pB, uint32 a) in (if i >= 0 then Some a else None)) |> List.ofSeq
printfn "  functions entered: %s" (String.Join(" ", funcs |> List.map (sprintf "%X")))
for KeyValue(t, sz) in dataRefs do if t < 0x3000000 then printfn "  !! data ref into low image %X" t
check "every walked rip-relative operand is read-only data (>= 3000000h, no writes)" (
    insAt.Values |> Seq.forall (fun i ->
        not i.IsIPRelativeMemoryOperand ||
        (let f = InstructionInfoFactory()
         let info = f.GetInfo(&i)
         info.GetUsedMemory() |> Seq.forall (fun m -> m.Base <> Register.RIP || m.Access = OpAccess.Read || m.Access = OpAccess.CondRead))))

// ------------------------------------------------------------------ static checks on the sites
printfn "== site bytes / instruction boundaries / branch-target sweep"
let parseHex (s: string) = s.Split(' ') |> Array.map (fun x -> Convert.ToByte(x, 16))
for KeyValue(rva, o) in ORIG do
    let b = parseHex o
    check (sprintf "%X bytes = %s" rva o) (img.[rva .. rva + b.Length - 1] = b)
    check (sprintf "%X: site starts on an instruction and the next instruction starts at +%d" rva b.Length) (
        insAt.ContainsKey rva && (let mutable a = rva in (while a < rva + b.Length && insAt.ContainsKey a do a <- a + insAt.[a].Length); a = rva + b.Length))
check "QB displaced = 4 movss stores 72053..7206E" (
    [ 0x72053; 0x7205A; 0x72061; 0x72068 ] |> List.forall (fun a -> insAt.ContainsKey a && insAt.[a].Mnemonic = Mnemonic.Movss && insAt.[a].Op0Kind = OpKind.Memory && insAt.[a].MemoryBase = Register.RSP))
for c in VCALLS do
    check (sprintf "%X is call 174610" c) (insAt.ContainsKey c && insAt.[c].Mnemonic = Mnemonic.Call && int insAt.[c].NearBranchTarget = VFN && insAt.[c].Length = 5)
check "174610 = movaps xmm0,[rdx]; mov rax,rcx; movdqa [rcx],xmm0; mov dword [rcx+0Ch],0; ret" (img.[VFN .. VFN + 17] = parseHex "0F 28 02 48 8B C1 66 0F 7F 01 C7 41 0C 00 00 00 00 C3")
// whole-.text sweep: no direct branch may land strictly inside a patched range; count calls to 174610 for the report
let interiors = [ QA, QA_LEN; QB, QB_DISPLACED; L1, L1_LEN; L2, L2_LEN ] @ [ for c in VCALLS -> c, 5 ]
let mutable hits, calls174 = 0, 0
let mutable ins = Instruction()
for i in 0 .. npd - 1 do
    let s, e = pB.[i], pE.[i]
    if e > s && int e <= img.Length then
        let d = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        d.IP <- uint64 s
        while d.IP < uint64 e do
            d.Decode(&ins)
            if not ins.IsInvalid && ins.Op0Kind = OpKind.NearBranch64 then
                let t = int ins.NearBranchTarget
                if t = VFN && ins.Mnemonic = Mnemonic.Call then calls174 <- calls174 + 1
                for (a, n) in interiors do
                    if t > a && t < a + n then hits <- hits + 1; printfn "  !! %X -> %X (inside %X)" ins.IP t a
printfn "  (174610 has %d direct callers in .text; V redirects only the 6 in 71C60)" calls174
check "no direct branch into the interior of any patched range" (hits = 0)

// ------------------------------------------------------------------ liveness: what the caves change is dead
// For each resume point: registers / flags / stack slots whose value may differ from the stock run must be written
// before they are read on every path (followed through 71C60, its cold stubs and the loop back edge; calls are
// treated as reading their argument registers and as killing nothing, except the known leaf 174610 (writes rax, xmm0)).
let fac = InstructionInfoFactory()
let norm (r: Register) =
    if r.IsXMM() || r.IsYMM() || r.IsZMM() then enum<Register>(int Register.XMM0 + (int (r.GetFullRegister()) - int Register.ZMM0))
    elif r.IsGPR() then r.GetFullRegister()
    else r
let CALLREADS = dict [ 0x174610, [ Register.RCX; Register.RDX ]; 0x6D130, [ Register.RCX; Register.RDX ]; 0x6EA50, [ Register.RCX; Register.RDX ]
                       0x54360, [ Register.XMM0; Register.RDX; Register.R8 ]; 0x1A7AAC, [ Register.RCX ]; 0xBE0378, [ Register.XMM0 ]
                       0x74349C, [ Register.RCX; Register.RDX ] ]
type Slot = { b: Register; d: int64; n: int }
let deadCheck (name: string) (start: int) (regs: Register list) (flags: bool) (slots: Slot list) =
    let bad = List<string>()
    let seen = HashSet<string>()
    let rec go (a: int) (rs: Set<Register>) (fl: bool) (ss: Slot list) (depth: int) =
        if (rs.IsEmpty && not fl && ss.IsEmpty) || depth > 4000 then ()
        elif a < FN || (a >= 0x7262B && (a < 0x1907C32 || a >= 0x1907E95)) then ()   // left 71C60 (+ its cold stubs)
        elif not (insAt.ContainsKey a) then bad.Add(sprintf "%X not walked" a)
        else
            let key = sprintf "%X|%A|%b|%A" a rs fl ss
            if seen.Add key then
                let i = insAt.[a]
                let info = fac.GetInfo(&i)
                let mutable rs2 = rs
                let mutable fl2 = fl
                let mutable ss2 = ss
                // reads
                for u in info.GetUsedRegisters() do
                    let r = norm u.Register
                    let isRead = u.Access = OpAccess.Read || u.Access = OpAccess.CondRead || u.Access = OpAccess.ReadWrite || u.Access = OpAccess.ReadCondWrite
                    if isRead && rs.Contains r then bad.Add(sprintf "%s: %A read at %X" name r a)
                if i.FlowControl = FlowControl.Call && i.Op0Kind = OpKind.NearBranch64 then
                    let t = int i.NearBranchTarget
                    let rd = match CALLREADS.TryGetValue t with | true, l -> l | _ -> [ Register.RCX; Register.RDX; Register.R8; Register.R9; Register.XMM0; Register.XMM1; Register.XMM2; Register.XMM3 ]
                    for r in rd do if rs.Contains r then bad.Add(sprintf "%s: %A passed to call %X at %X" name r t a)
                if fl && i.RflagsRead <> RflagsBits.None then bad.Add(sprintf "%s: flags read at %X" name a)
                for m in info.GetUsedMemory() do
                    let b = norm m.Base
                    let disp = int64 m.Displacement
                    let sz = MemorySizeExtensions.GetSize m.MemorySize
                    if (b = Register.RSP || b = Register.RBP) && m.Index = Register.None then
                        for s in ss do
                            if s.b = b && disp < s.d + int64 s.n && s.d < disp + int64 sz then
                                if m.Access <> OpAccess.Write then bad.Add(sprintf "%s: [%A%+d] read at %X" name s.b s.d a)
                                elif disp <= s.d && disp + int64 sz >= s.d + int64 s.n then ss2 <- ss2 |> List.filter (fun x -> x <> s)
                // writes (full-register writes kill; 32-bit GPR writes zero-extend; xmm Write = whole register)
                for u in info.GetUsedRegisters() do
                    let r = norm u.Register
                    if u.Access = OpAccess.Write && rs2.Contains r then
                        let full = u.Register.IsXMM() || u.Register.IsGPR64() || u.Register.IsGPR32()
                        if full then rs2 <- rs2.Remove r
                if i.FlowControl = FlowControl.Call && i.Op0Kind = OpKind.NearBranch64 && int i.NearBranchTarget = VFN then
                    rs2 <- rs2.Remove(Register.RAX).Remove(Register.XMM0)
                // 71FBC call SinCosApprox(xmm0, rdx = rbp-30h, r8 = rbp-2Ch) writes both outputs on every path
                if a = 0x71FBC then ss2 <- ss2 |> List.filter (fun x -> not (x.b = Register.RBP && (x.d = -0x30L || x.d = -0x2CL)))
                let allArith = RflagsBits.OF ||| RflagsBits.SF ||| RflagsBits.ZF ||| RflagsBits.AF ||| RflagsBits.CF ||| RflagsBits.PF
                if fl2 && (i.RflagsModified &&& allArith) = allArith then fl2 <- false
                let next = a + i.Length
                match i.FlowControl with
                | FlowControl.Return -> ()      // 71C60's callers cannot rely on volatile registers: it makes an indirect vcall
                | FlowControl.UnconditionalBranch -> go (int i.NearBranchTarget) rs2 fl2 ss2 (depth + 1)
                | FlowControl.ConditionalBranch -> go (int i.NearBranchTarget) rs2 fl2 ss2 (depth + 1); go next rs2 fl2 ss2 (depth + 1)
                | FlowControl.Interrupt | FlowControl.IndirectBranch | FlowControl.Exception -> ()
                | _ -> go next rs2 fl2 ss2 (depth + 1)
    go start (Set.ofList regs) flags slots 0
    for b in bad |> Seq.distinct |> Seq.truncate 10 do printfn "    %s" b
    check (sprintf "dead after %s (%s%s%s)" name (String.Join(",", regs)) (if flags then ",flags" else "")
                (if slots.IsEmpty then "" else "," + String.Join(",", slots |> List.map (fun s -> sprintf "[%A%+X]" s.b s.d)))) (bad.Count = 0)
printfn "== liveness of everything the caves leave different from the stock run"
let X (n: int) = enum<Register>(int Register.XMM0 + n)
// Q hit at 7206F: stock wrote rcx/rdx/r8 (SinCos), xmm0 (sqrt), xmm2 (n^2), xmm3/xmm4 (SinCos), xmm7 (cos) and the stack
// slots [rbp-1Ch] [rbp-18h] [rsp+30h] [rbp-30h] [rbp-2Ch]; the hit path does not ([rbp-20h] is written before 71EFE).
deadCheck "QA-hit 7206F" Q_HIT [ Register.RCX; Register.RDX; Register.R8; X 0; X 2; X 3; X 4; X 7 ] true
    [ { b = Register.RBP; d = -0x1CL; n = 4 }; { b = Register.RBP; d = -0x18L; n = 4 }; { b = Register.RSP; d = 0x30L; n = 4 }
      { b = Register.RBP; d = -0x30L; n = 4 }; { b = Register.RBP; d = -0x2CL; n = 4 } ]
// Q miss/off at 71F04: the cave may have loaded xmm0 / xmm9..12 and changed flags before falling back
deadCheck "QA-stock 71F04" QA_RES [ Register.R8; X 0; X 9; X 10; X 11; X 12 ] true [ { b = Register.RSP; d = 0x30L; n = 4 } ]
// QB at 7206F: rcx rdx r8 r9 r11 + flags (rax is pushed/popped)
deadCheck "QB 7206F" QB_RES [ Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R11 ] true [ { b = Register.RBP; d = -0x30L; n = 4 } ]
deadCheck "L1 71DB7" L1_RES [] true []
for c in VCALLS do deadCheck (sprintf "V ret %X" (c + 5)) (c + 5) [] true []
// L2 lives in 1A7AAC: after 1A7AE6 lea edi,[rbx+1] (no flags) the next instruction test dil,al writes all flags
check "L2: 1A7AE6 lea (no flags), 1A7AE9 test dil,al (writes flags)" (img.[0x1A7AE6 .. 0x1A7AEB] = parseHex "8D 7B 01 40 84 C7")
check "L2: ebx = 0 (1A7AD9 xor ebx,ebx), no write to ebx/eax before 1A7AE0" (img.[0x1A7AD9 .. 0x1A7ADA] = parseHex "33 DB")
// L1: r10d is 0 at 71DB2 on every path: set at 71CDD and 72406, never written elsewhere in 71C60, and 1A7AAC (called at
// 71D93) does not write r10
let writesR10 (a0: int) (a1: int) =
    insAt |> Seq.exists (fun (KeyValue(a, i)) -> a >= a0 && a < a1 && (let info = fac.GetInfo(&i) in info.GetUsedRegisters() |> Seq.exists (fun u -> norm u.Register = Register.R10 && u.Access <> OpAccess.Read)))
let r10Writers = insAt |> Seq.filter (fun (KeyValue(a, i)) -> ((a >= FN && a < 0x7262B) || (a >= 0x1907C32 && a < 0x1907E95)) && (let info = fac.GetInfo(&i) in info.GetUsedRegisters() |> Seq.exists (fun u -> norm u.Register = Register.R10 && u.Access <> OpAccess.Read))) |> Seq.map (fun kv -> kv.Key) |> List.ofSeq
check (sprintf "L1: r10 writers in 71C60 = %s (expect 71CDD xor, 72406 mov 0)" (String.Join(" ", r10Writers |> List.map (sprintf "%X")))) (r10Writers = [ 0x71CDD; 0x72406 ])
check "L1: 1A7AAC..1A7B11 never writes r10" (not (writesR10 0x1A7AAC 0x1A7B11))

// ------------------------------------------------------------------ caves (mirror of caves.asm)
let K = 0x9E3779B97F4A7C15UL
let OFF_EN, OFF_MASK, OFF_TBL, OFF_ONE = 0x00, 0x08, 0x10, 0x18
let OFF_RQS, OFF_RQH, OFF_RQB, OFF_RL1, OFF_RL2, OFF_RVS = 0x20, 0x28, 0x30, 0x38, 0x40, 0x48
let CODE = 0x80
type Caves = { qa: uint64; qb: uint64; l1: uint64; l2: uint64; v: uint64; bytes: byte[] }
let buildCaves (cave: uint64) : Caves =
    let a = Assembler(64)
    let mk () = a.CreateLabel()
    let mutable lEn = mk ()
    let mutable lBits = mk ()
    let mutable lMask = mk ()
    let mutable lTbl = mk ()
    let mutable lOne = mk ()
    let lRes = Array.init 6 (fun _ -> a.CreateLabel())
    a.Label(&lEn); a.dd(0u)                           // +00 live switch (menu: 0/1)
    a.Label(&lBits); a.dd(0u)                         // +04 parts: 1 Q, 2 L, 4 V
    a.Label(&lMask); a.dq(0UL)                        // +08 index mask
    a.Label(&lTbl); a.dq(0UL)                         // +10 table
    a.Label(&lOne); a.dd(0x3F800000u); a.dd(0u)       // +18 1.0f
    for k in 0 .. 5 do (let mutable l = lRes.[k] in a.Label(&l); a.dq(0UL))   // +20.. resume addresses
    for _ in 1 .. (CODE - 0x50) / 8 do a.dq(0xCCCCCCCCCCCCCCCCUL)
    let enD = __dword_ptr.[lEn]
    let bits = __byte_ptr.[lBits]
    let mask = __qword_ptr.[lMask]
    let tbl = __qword_ptr.[lTbl]
    let one = __dword_ptr.[lOne]
    let res off = __qword_ptr.[lRes.[(off - OFF_RQS) / 8]]
    let labels = Dictionary<string, Label>()
    let L name = (match labels.TryGetValue name with | true, l -> l | _ -> (let l = a.CreateLabel(name) in labels.[name] <- l; l))
    let mark name = (let mutable l = L name in a.Label(&l); labels.[name] <- l)
    let gate (bit: int) (stock: string) = a.cmp(enD, 0); a.je(L stock); a.test(bits, bit); a.jz(L stock)
    // hash: rcx = tbl + ((key*K >> 32) & mask) * 32 ; key in kreg, uses rdx
    let hash (kreg: AssemblerRegister64) =
        a.mov(rcx, kreg); a.mov(rdx, K); a.imul(rcx, rdx); a.shr(rcx, 32uy); a.``and``(rcx, mask); a.shl(rcx, 5uy); a.add(rcx, tbl)
    // ---- QA @71EFE
    mark "qa"
    gate 1 "qa_stock"
    a.mov(rax, __qword_ptr.[rbx + 4L])
    a.stmxcsr(__dword_ptr.[rsp + 0x30L]); a.mov(r8d, __dword_ptr.[rsp + 0x30L]); a.``and``(r8d, 0xE040)
    hash rax
    a.mov(edx, __dword_ptr.[rcx])
    a.test(dl, 1uy); a.jnz(L "qa_stock")
    a.test(edx, edx); a.jz(L "qa_stock")
    a.cmp(rax, __qword_ptr.[rcx + 8L]); a.jne(L "qa_stock")
    a.cmp(r8d, __dword_ptr.[rcx + 4L]); a.jne(L "qa_stock")
    a.movups(xmm0, __xmmword_ptr.[rcx + 0x10L])
    a.movss(xmm11, __dword_ptr.[rcx + 0x10L]); a.movss(xmm12, __dword_ptr.[rcx + 0x14L])
    a.movss(xmm10, __dword_ptr.[rcx + 0x18L]); a.movss(xmm9, __dword_ptr.[rcx + 0x1CL])
    a.cmp(edx, __dword_ptr.[rcx]); a.jne(L "qa_stock")
    a.movups(__xmmword_ptr.[rsp + 0x40L], xmm0)
    a.movss(xmm1, one)
    a.mov(rax, __qword_ptr.[r14 + 0x3F8L])
    a.jmp(res OFF_RQH)
    mark "qa_stock"
    a.movzx(eax, __word_ptr.[rbx + 6L]); a.mov(ecx, eax)
    a.jmp(res OFF_RQS)
    // ---- QB @72053
    mark "qb"
    a.movss(__dword_ptr.[rsp + 0x40L], xmm11); a.movss(__dword_ptr.[rsp + 0x44L], xmm12)
    a.movss(__dword_ptr.[rsp + 0x4CL], xmm9); a.movss(__dword_ptr.[rsp + 0x48L], xmm10)
    gate 1 "qb_done"
    // r9 = key re-packed from the unpacked floats the stock code used: [rsp+30h] angle, [rbp-18h] z, [rbp-1Ch] y, [rbp-20h] x
    a.xor(r9d, r9d)
    for (src, last) in [ (__dword_ptr.[rsp + 0x30L], false); (__dword_ptr.[rbp - 0x18L], false); (__dword_ptr.[rbp - 0x1CL], false); (__dword_ptr.[rbp - 0x20L], true) ] do
        a.mov(ecx, src)
        a.mov(r8d, ecx); a.shr(r8d, 16uy); a.``and``(r8d, 0x8000)       // sign
        a.mov(edx, ecx); a.shr(edx, 13uy); a.``and``(edx, 0x3FF); a.``or``(r8d, edx)   // mantissa
        a.shr(ecx, 23uy); a.``and``(ecx, 0xFF)                          // float exponent: 0 or 113..143
        a.lea(edx, __.[rcx - 112L]); a.shl(edx, 10uy)
        a.test(ecx, ecx); a.cmovz(edx, ecx)
        a.``or``(r8d, edx)
        a.``or``(r9, r8)
        if not last then a.shl(r9, 16uy)
    hash r9
    a.mov(r8d, __dword_ptr.[rcx])
    a.test(r8b, 1uy); a.jnz(L "qb_done")                 // a writer is busy: skip
    a.test(r8d, r8d); a.jz(L "qb_try")                   // empty
    a.cmp(r9, __qword_ptr.[rcx + 8L]); a.je(L "qb_done")  // already there (same key -> same value)
    mark "qb_try"
    a.lea(r11d, __.[r8 + 1L])
    a.push(rax)
    a.mov(eax, r8d)
    a.``lock``.cmpxchg(__dword_ptr.[rcx], r11d)
    a.pop(rax)
    a.jnz(L "qb_done")
    a.stmxcsr(__dword_ptr.[rbp - 0x30L]); a.mov(edx, __dword_ptr.[rbp - 0x30L]); a.``and``(edx, 0xE040)
    a.mov(__dword_ptr.[rcx + 4L], edx)
    a.mov(__qword_ptr.[rcx + 8L], r9)
    a.movss(__dword_ptr.[rcx + 0x10L], xmm11); a.movss(__dword_ptr.[rcx + 0x14L], xmm12)
    a.movss(__dword_ptr.[rcx + 0x18L], xmm10); a.movss(__dword_ptr.[rcx + 0x1CL], xmm9)
    a.add(r11d, 1)
    a.mov(__dword_ptr.[rcx], r11d)
    mark "qb_done"
    a.jmp(res OFF_RQB)
    // ---- L1 @71DB2
    mark "l1"
    gate 2 "l1_stock"
    a.mov(eax, __dword_ptr.[rcx])
    a.jmp(res OFF_RL1)
    mark "l1_stock"
    a.``lock``.cmpxchg(__dword_ptr.[rcx], r10d)
    a.jmp(res OFF_RL1)
    // ---- L2 @1A7AE0
    mark "l2"
    a.xor(eax, eax)
    gate 2 "l2_stock"
    a.mov(eax, __dword_ptr.[rdx])
    a.jmp(res OFF_RL2)
    mark "l2_stock"
    a.``lock``.cmpxchg(__dword_ptr.[rdx], ebx)
    a.jmp(res OFF_RL2)
    // ---- V (call target replacing 174610 at the 6 sites)
    mark "v"
    gate 4 "v_stock"
    a.movss(xmm0, __dword_ptr.[rdx])
    a.insertps(xmm0, __dword_ptr.[rdx + 4L], 0x10uy)
    a.insertps(xmm0, __dword_ptr.[rdx + 8L], 0x20uy)
    a.movups(__xmmword_ptr.[rcx], xmm0)
    a.insertps(xmm0, __dword_ptr.[rdx + 12L], 0x30uy)
    a.mov(rax, rcx)
    a.ret()
    mark "v_stock"
    a.jmp(res OFF_RVS)
    use ms = new MemoryStream()
    let r = a.Assemble(StreamCodeWriter(ms), cave, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let at name = (let l = labels.[name] in r.GetLabelRIP(&l))
    { qa = at "qa"; qb = at "qb"; l1 = at "l1"; l2 = at "l2"; v = at "v"; bytes = ms.ToArray() }

// ------------------------------------------------------------------ native regions
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type BoundsFn = delegate of nativeint * nativeint * nativeint * nativeint * uint32 * nativeint -> unit
let RSIZE = 0x3C00000
let CAVE_AT, STUB_AT = 0x3BF0000, 0x3BF8000
let mkRegion () =
    let r = VirtualAlloc(0n, unativeint RSIZE, 0x3000u, 0x40u)
    if r = 0n then failwith "VirtualAlloc region"
    for (lo, hi) in ranges do Marshal.Copy(img, lo, r + nativeint lo, hi - lo)
    for KeyValue(t, sz) in dataRefs do Marshal.Copy(img, t, r + nativeint t, max sz 16)
    for s in STUBS do Marshal.WriteByte(r + nativeint s, 0xCCuy)   // int3: a stubbed path would crash the harness
    r
let R0 = mkRegion ()
let R1 = mkRegion ()
let TBL_BITS = 14
let table = VirtualAlloc(0n, unativeint ((1 <<< TBL_BITS) * 32), 0x3000u, 0x04u)
let caveBase = uint64 R1 + uint64 CAVE_AT
let cv = buildCaves caveBase
Marshal.Copy(cv.bytes, 0, nativeint caveBase, cv.bytes.Length)
let wq (off: int) (v: uint64) = Marshal.WriteInt64(nativeint caveBase + nativeint off, int64 v)
let setEn (v: int) = Marshal.WriteInt32(nativeint caveBase + nativeint OFF_EN, (if v <> 0 then 1 else 0)); Marshal.WriteInt32(nativeint caveBase + 4n, v)
wq OFF_MASK (uint64 ((1 <<< TBL_BITS) - 1)); wq OFF_TBL (uint64 table)
Marshal.WriteInt32(nativeint caveBase + nativeint OFF_ONE, 0x3F800000)
check "1.0 at 3BD41E0 (stock xmm1 at 72027) = cave constant 3F800000" (u32at 0x3BD41E0 = 0x3F800000u)
let reg1 (rva: int) = uint64 R1 + uint64 rva
wq OFF_RQS (reg1 QA_RES); wq OFF_RQH (reg1 Q_HIT); wq OFF_RQB (reg1 QB_RES); wq OFF_RL1 (reg1 L1_RES); wq OFF_RL2 (reg1 L2_RES); wq OFF_RVS (reg1 VFN)
let jmpBytes (fromRva: int) (len: int) (target: uint64) =
    let rel = int64 target - int64 (reg1 fromRva + 5UL)
    Array.append (Array.append [| 0xE9uy |] (BitConverter.GetBytes(int32 rel))) (Array.create (len - 5) 0x90uy)
let callBytes (fromRva: int) (target: uint64) =
    Array.append [| 0xE8uy |] (BitConverter.GetBytes(int32 (int64 target - int64 (reg1 fromRva + 5UL))))
let patch (rva: int) (b: byte[]) = Marshal.Copy(b, 0, nativeint (reg1 rva), b.Length)
patch QA (jmpBytes QA QA_LEN cv.qa); patch QB (jmpBytes QB QB_LEN cv.qb)
patch L1 (jmpBytes L1 L1_LEN cv.l1); patch L2 (jmpBytes L2 L2_LEN cv.l2)
for c in VCALLS do patch c (callBytes c cv.v)
printfn "  caves %d bytes (code %d) at region+%X; table %d entries x 32 B" cv.bytes.Length (cv.bytes.Length - CODE) CAVE_AT (1 <<< TBL_BITS)

// fake object: vtable +E8h -> stub that writes the local bounds (2 x 12 bytes from a data block) to [r8] and [r9]
let mem = Marshal.AllocHGlobal(0x40000)
let mbase = (mem + 0xFFn) &&& ~~~0xFFn
let OBJ, VT, BLK, OFFS, NODE, VB, DEQ, PARTS, OUT0, OUT1 = mbase, mbase + 0x800n, mbase + 0x900n, mbase + 0x980n, mbase + 0x9C0n, mbase + 0xA00n, mbase + 0xA40n, mbase + 0x1000n, mbase + 0x8000n, mbase + 0x8100n
let stubCode (r: nativeint) =
    let a = Assembler(64)
    a.mov(rax, uint64 VB)
    a.mov(r10, __qword_ptr.[rax]); a.mov(__qword_ptr.[r8], r10); a.mov(r10d, __dword_ptr.[rax + 8L]); a.mov(__dword_ptr.[r8 + 8L], r10d)
    a.mov(r10, __qword_ptr.[rax + 16L]); a.mov(__qword_ptr.[r9], r10); a.mov(r10d, __dword_ptr.[rax + 24L]); a.mov(__dword_ptr.[r9 + 8L], r10d)
    a.ret()
    use ms = new MemoryStream()
    a.Assemble(StreamCodeWriter(ms), uint64 (r + nativeint STUB_AT)) |> ignore
    Marshal.Copy(ms.ToArray(), 0, r + nativeint STUB_AT, int ms.Length)
stubCode R0; stubCode R1
let f0 = Marshal.GetDelegateForFunctionPointer<BoundsFn>(R0 + nativeint FN)
let f1 = Marshal.GetDelegateForFunctionPointer<BoundsFn>(R1 + nativeint FN)

let rnd = Random(20261008)
let w32 (p: nativeint) (v: int) = Marshal.WriteInt32(p, v)
let wf (p: nativeint) (v: float32) = Marshal.WriteInt32(p, BitConverter.SingleToInt32Bits v)
let w16 (p: nativeint) (v: int) = Marshal.WriteInt16(p, int16 v)
let toHalf (x: float32) = int (BitConverter.HalfToInt16Bits(Half.op_Explicit x)) &&& 0xFFFF
let special = [| 0.0f; -0.0f; 1.0f; -1.0f; 1e-40f; 1e-38f; 3.0e38f; -3.0e38f; 0.5f; 65504.0f |]
let rf kind =
    match kind with
    | 0 -> float32 (rnd.NextDouble() * 2.0 - 1.0)
    | 1 -> float32 ((rnd.NextDouble() * 2.0 - 1.0) * 5000.0)
    | 2 -> special.[rnd.Next special.Length]
    | _ -> BitConverter.Int32BitsToSingle(rnd.Next() &&& 0x7F7FFFFF ||| (if rnd.Next 2 = 0 then 0 else Int32.MinValue))
let rhalf kind =
    match kind with
    | 0 -> toHalf (float32 (rnd.NextDouble() * 2.0 - 1.0))
    | 1 -> rnd.Next 0x10000                                       // any bit pattern incl. inf/NaN/denormal codes
    | _ -> [| 0; 0x8000; 0x3C00; 0xBC00; 0x0001; 0x03FF; 0x7C00; 0x7FFF; 0xFC00 |].[rnd.Next 9]
/// one random call setup; returns (count, p6 or 0)
type Setup = { n: int; p6: nativeint; half: bool }
let setup (keyPool: uint64[] option) (realistic: bool) : Setup =
    // object
    for j in 0 .. 0x7FF / 4 do w32 (OBJ + nativeint (4 * j)) 0
    Marshal.WriteInt64(OBJ, int64 VT)
    Marshal.WriteInt64(VT + 0xE8n, int64 (nativeint 0))      // filled per region below
    w32 (OBJ + 0x4Cn) 0
    Marshal.WriteInt64(OBJ + 0x3F8n, int64 BLK)
    w32 BLK ([| 3; 7; 0x103; -1 |].[rnd.Next 4] ||| 3)       // ready: bits 1 and 2 set
    let withOffs = rnd.Next 3 <> 0
    Marshal.WriteInt64(BLK + 8n, (if withOffs then int64 OFFS else 0L))
    for j in 0 .. 3 do wf (OFFS + nativeint (4 * j)) (if realistic then float32 ((rnd.NextDouble() - 0.5) * 100.0) else rf (rnd.Next 4))
    Marshal.WriteInt64(OBJ + 0x430n, int64 NODE)
    Marshal.WriteInt64(NODE, 0L); w32 (NODE + 8n) 2
    for j in 0 .. 5 do wf (VB + nativeint (4 * j)) (if realistic then (if j < 3 then -0.3f else 0.3f) * float32 (1.0 + rnd.NextDouble()) else rf (rnd.Next 4))
    let half = realistic || rnd.Next 5 <> 0
    // dequant: offset x, offset z, scale, flag byte
    let dq = [| (if realistic then 0.0f else rf 1); (if realistic then 0.0f else rf 1); (if realistic then 0.01f else rf (rnd.Next 3)) |]
    for j in 0 .. 2 do wf (DEQ + nativeint (4 * j)) dq.[j]; wf (NODE + nativeint (0x10 + 4 * j)) dq.[j]
    let flag = if half then byte (1 + rnd.Next 255) else 0uy
    Marshal.WriteInt32(DEQ + 12n, int flag ||| (rnd.Next 0x1000000 <<< 8)); Marshal.WriteByte(NODE + 0x1Cn, flag)
    let n = if realistic then 24 else 1 + rnd.Next 40
    if half then
        for p in 0 .. n - 1 do
            let q = PARTS + nativeint (20 * p)
            let hk = if realistic then 0 elif rnd.Next 4 = 0 then 1 else 0
            w16 q (if realistic then toHalf (float32 (rnd.NextDouble() * 200.0)) else rhalf hk)
            w16 (q + 2n) (if realistic then toHalf (float32 (rnd.NextDouble() * 200.0)) else rhalf hk)
            wf (q + 4n) (if realistic then float32 (rnd.NextDouble() * 2.0) else rf (rnd.Next 3))
            match keyPool with
            | Some pool -> Marshal.WriteInt64(q + 8n, int64 pool.[rnd.Next pool.Length])
            | None ->
                if realistic then
                    let ax = Array.init 3 (fun _ -> rnd.NextDouble() * 2.0 - 1.0)
                    let l = sqrt (ax |> Array.sumBy (fun x -> x * x))
                    for c in 0 .. 2 do w16 (q + nativeint (8 + 2 * c)) (toHalf (float32 (ax.[c] / l)))
                    w16 (q + 14n) (toHalf (float32 (rnd.NextDouble() * 1.6 - 0.8)))
                else
                    let k = rnd.Next 3
                    for c in 0 .. 3 do w16 (q + nativeint (8 + 2 * c)) (rhalf k)
            wf (q + 16n) (if realistic then float32 (0.5 + rnd.NextDouble()) else rf (rnd.Next 3))
    else
        for j in 0 .. n * 12 - 1 do wf (PARTS + nativeint (4 * j)) (rf (rnd.Next 3))
    { n = n; p6 = (if rnd.Next 4 = 0 then 0n else DEQ); half = half }
let runOn (f: BoundsFn) (r: nativeint) (out: nativeint) (s: Setup) =
    Marshal.WriteInt64(VT + 0xE8n, int64 (r + nativeint STUB_AT))
    for j in 0 .. 0x60 / 4 - 1 do w32 (out + nativeint (4 * j)) (0x7F80DEAD + j)
    f.Invoke(OBJ, 0x1234n, out, PARTS, uint32 s.n, s.p6)
let same () = Seq.forall (fun j -> Marshal.ReadInt32(OUT0 + nativeint (4 * j)) = Marshal.ReadInt32(OUT1 + nativeint (4 * j))) [ 0 .. 0x60 / 4 - 1 ]
let dumpOut tag (o: nativeint) = printfn "    %s %s" tag (String.Join(" ", [| for j in 0 .. 0x13 -> sprintf "%08X" (Marshal.ReadInt32(o + nativeint (4 * j))) |]))
let mutable shown = 0
let compare1 (s: Setup) (label: string) =
    runOn f0 R0 OUT0 s
    runOn f1 R1 OUT1 s
    let r = same ()
    if not r && shown < 6 then
        shown <- shown + 1
        printfn "  MISMATCH (%s) n=%d half=%b p6=%b" label s.n s.half (s.p6 <> 0n); dumpOut "stock" OUT0; dumpOut "patch" OUT1
    r
let clearTable () = for j in 0 .. (1 <<< TBL_BITS) * 8 - 1 do Marshal.WriteInt32(table + nativeint (4 * j), 0)

printfn "== native equivalence: whole 71C60 stock (region 0) vs patched (region 1), output OBB 0x60 bytes bitwise"
let mutable tests, bad = 0, 0
for en in [ 0; 1; 2; 4; 7 ] do
    setEn en
    clearTable ()
    let t0, b0 = tests, bad
    for _ in 1 .. (if en = 7 then 60000 else 8000) do
        let s = setup None (rnd.Next 4 = 0)
        // twice: the first run fills the table (miss), the second hits
        for _ in 1 .. 2 do
            tests <- tests + 1
            if not (compare1 s (sprintf "en=%d" en)) then bad <- bad + 1
    printfn "  switch %d: %d calls, %d mismatches" en (tests - t0) (bad - b0)
check "bit-identical output for every switch value (0 off, 1 Q, 2 L, 4 V, 7 all; miss and hit runs)" (bad = 0)
// every inserted entry maps the key to exactly what the stock code computes for it: run single-part calls with the
// same key through the stock path (table cleared each time) and check the table entry's key = the part's 8 bytes
printfn "== exhaustive half sweep: each lane (ax, ay, az, angle) = every one of the 65536 half codes"
setEn 7
let mutable sweepBad, keyBad, hitBad = 0, 0, 0
for lane in 0 .. 3 do
    for h in 0 .. 0xFFFF do
        let s = setup None false
        let s = { s with n = 1; half = true; p6 = DEQ }
        let mutable flag = Marshal.ReadByte(DEQ + 12n)
        if flag = 0uy then Marshal.WriteByte(DEQ + 12n, 1uy)
        let k = Array.init 4 (fun c -> if c = lane then h else rnd.Next 0x10000)
        for c in 0 .. 3 do w16 (PARTS + nativeint (8 + 2 * c)) k.[c]
        let key = uint64 k.[0] ||| (uint64 k.[1] <<< 16) ||| (uint64 k.[2] <<< 32) ||| (uint64 k.[3] <<< 48)
        let slot = nativeint (int64 table + int64 ((((key * K) >>> 32) &&& uint64 ((1 <<< TBL_BITS) - 1)) * 32UL))
        Marshal.WriteInt32(slot, 0)
        if not (compare1 s "sweep miss") then sweepBad <- sweepBad + 1
        if Marshal.ReadInt32(slot) <> 2 || uint64 (Marshal.ReadInt64(slot + 8n)) <> key then keyBad <- keyBad + 1
        if not (compare1 s "sweep hit") then hitBad <- hitBad + 1
check (sprintf "4 x 65536 single-part calls: %d miss-run mismatches, %d hit-run mismatches" sweepBad hitBad) (sweepBad = 0 && hitBad = 0)
check (sprintf "the entry inserted by QB carries exactly the part's 8 key bytes (re-pack is the inverse of the unpack): %d wrong" keyBad) (keyBad = 0)
// poisoned table: entries whose key matches but whose seq is odd (writer busy) must not be used
printfn "== seqlock: odd / zero sequence words are ignored"
let mutable poisonBad = 0
for _ in 1 .. 3000 do
    let s = setup None true
    for p in 0 .. s.n - 1 do
        let key = uint64 (Marshal.ReadInt64(PARTS + nativeint (20 * p + 8)))
        let slot = nativeint (int64 table + int64 ((((key * K) >>> 32) &&& uint64 ((1 <<< TBL_BITS) - 1)) * 32UL))
        Marshal.WriteInt32(slot, (if rnd.Next 2 = 0 then 0 else 2 * rnd.Next 1000 + 1)); Marshal.WriteInt32(slot + 4n, 0); Marshal.WriteInt64(slot + 8n, int64 key)
        for j in 4 .. 7 do Marshal.WriteInt32(slot + nativeint (4 * j), 0x7FC0BAD0)
    // QB skips odd entries (no insert) and inserts into zero ones; either way the output must be stock
    if not (compare1 s "poison") then poisonBad <- poisonBad + 1
check (sprintf "poisoned entries (seq odd or 0, key equal, garbage value): %d mismatches in 3000 calls" poisonBad) (poisonBad = 0)

// ------------------------------------------------------------------ MXCSR: entries are tagged with FTZ / RC / DAZ
// The unpacked half denormals are float denormals, so DAZ / FTZ (and the rounding mode) change the stock result. An entry
// made under one MXCSR must never serve a thread running another one.
printfn "== MXCSR: FTZ/DAZ and rounding modes (entries carry MXCSR & E040h)"
let mxThunk =
    let p = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)
    // void setmx(uint32 v): mov [rsp+8],ecx ; ldmxcsr [rsp+8] ; ret
    Marshal.Copy([| 0x89uy; 0x4Cuy; 0x24uy; 0x08uy; 0x0Fuy; 0xAEuy; 0x54uy; 0x24uy; 0x08uy; 0xC3uy |], 0, p, 10)
    p
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type SetMx = delegate of uint32 -> unit
let setMx = Marshal.GetDelegateForFunctionPointer<SetMx>(mxThunk)
setEn 7
let modes = [| 0x1F80u; 0x9FC0u; 0x1FC0u; 0x9F80u; 0x3F80u; 0x5F80u; 0x7F80u |]   // default, FTZ+DAZ, DAZ, FTZ, RC down/up/zero
let mutable mxBad, mxCross = 0, 0
// (a) each mode alone (table cleared): miss + hit runs equal the stock code under the same mode
for m in modes do
    clearTable ()
    setMx.Invoke m
    for _ in 1 .. 3000 do
        let s = setup None (rnd.Next 3 = 0)
        for _ in 1 .. 2 do if not (compare1 s (sprintf "mx %X" m)) then mxBad <- mxBad + 1
    setMx.Invoke 0x1F80u
check (sprintf "every MXCSR mode alone (7 modes x 3000 calls x miss/hit): %d mismatches" mxBad) (mxBad = 0)
// (b) cross-mode: fill the table under mode A with denormal-heavy keys, then run the same calls under mode B
let denKeys = Array.init 400 (fun i -> uint64 (rnd.Next 0x400) ||| (uint64 (rnd.Next 0x400) <<< 16) ||| (uint64 (rnd.Next 0x10000) <<< 32) ||| (uint64 (rnd.Next 0x400) <<< 48))
let mutable crossDiffWithoutTag = 0
for (ma, mb) in [ (0x1F80u, 0x9FC0u); (0x9FC0u, 0x1F80u); (0x1F80u, 0x3F80u); (0x7F80u, 0x1F80u) ] do
    clearTable ()
    let rec halfSetup (k: uint64) = (let s = setup (Some [| k |]) false in if s.half then { s with p6 = DEQ } else halfSetup k)
    let calls = [| for k in denKeys -> (let s = halfSetup k in s, Array.init 0x2000 (fun j -> Marshal.ReadByte(mbase + nativeint j))) |]
    let restore (img: byte[]) = Marshal.Copy(img, 0, mbase, img.Length)
    for (s, im) in calls do
        restore im
        if Marshal.ReadByte(DEQ + 12n) = 0uy then Marshal.WriteByte(DEQ + 12n, 1uy)
        setMx.Invoke ma; runOn f1 R1 OUT1 s; setMx.Invoke 0x1F80u
    for (s, im) in calls do
        restore im
        if Marshal.ReadByte(DEQ + 12n) = 0uy then Marshal.WriteByte(DEQ + 12n, 1uy)
        setMx.Invoke mb
        runOn f0 R0 OUT0 s
        let a = Array.init 0x14 (fun j -> Marshal.ReadInt32(OUT0 + nativeint (4 * j)))
        runOn f1 R1 OUT1 s
        setMx.Invoke 0x1F80u
        if not (same ()) then mxCross <- mxCross + 1
        // what an untagged table would have returned: run stock under mode A and compare with stock under mode B
        setMx.Invoke ma; runOn f0 R0 OUT1 s; setMx.Invoke 0x1F80u
        if Array.exists2 (<>) a (Array.init 0x14 (fun j -> Marshal.ReadInt32(OUT1 + nativeint (4 * j)))) then crossDiffWithoutTag <- crossDiffWithoutTag + 1
setMx.Invoke 0x1F80u
printfn "  (info) calls whose stock result differs between the two modes, i.e. an untagged cache would be wrong: %d of %d" crossDiffWithoutTag (4 * denKeys.Length)
check (sprintf "cross-mode (table filled under mode A, read under mode B): %d mismatches" mxCross) (mxCross = 0)

// ------------------------------------------------------------------ concurrency stress: 4 threads, 4-entry table
printfn "== concurrency: 4 threads, table masked to 4 entries (constant evictions), 64 shared keys"
let pool = Array.init 64 (fun _ -> uint64 (rnd.NextInt64()))
// expected per-key OBB with a one-part call (stock region, single thread)
let expected = Dictionary<uint64, int[]>()
setEn 7
let threadMem = Array.init 4 (fun _ -> let m = Marshal.AllocHGlobal(0x10000) in (m + 0xFFn) &&& ~~~0xFFn)
let s0 = setup None true
for key in pool do
    Marshal.WriteInt64(PARTS + 8n, int64 key)
    runOn f0 R0 OUT0 { s0 with n = 1; p6 = DEQ }
    expected.[key] <- Array.init 0x14 (fun j -> Marshal.ReadInt32(OUT0 + nativeint (4 * j)))
let partImg = Array.init 20 (fun j -> Marshal.ReadByte(PARTS + nativeint j))
wq OFF_MASK 3UL
clearTable ()
Marshal.WriteInt64(VT + 0xE8n, int64 (R1 + nativeint STUB_AT))
let stressBad = ref 0
let stressCalls = ref 0
let worker (t: int) () =
    let m = threadMem.[t]
    // private copy of the object block (pointers inside it point to the shared block: read-only, fine)
    let part = m + 0x2000n
    let out = m + 0x3000n
    Marshal.Copy(partImg, 0, part, 20)
    let r = Random(1000 + t)
    let mutable b = 0
    for i in 1 .. 300000 do
        let key = pool.[r.Next pool.Length]
        Marshal.WriteInt64(part + 8n, int64 key)
        f1.Invoke(OBJ, 0x1234n, out, part, 1u, DEQ)
        let e = expected.[key]
        let mutable okk = true
        for j in 0 .. 0x13 do if Marshal.ReadInt32(out + nativeint (4 * j)) <> e.[j] then okk <- false
        if not okk then b <- b + 1
    Interlocked.Add(stressBad, b) |> ignore
    Interlocked.Add(stressCalls, 300000) |> ignore
let ths = [| for t in 0 .. 3 -> Thread(ThreadStart(worker t)) |]
for t in ths do t.Start()
for t in ths do t.Join()
check (sprintf "4 threads x 300000 calls on a 4-entry table: %d wrong outputs" stressBad.Value) (stressBad.Value = 0)
wq OFF_MASK (uint64 ((1 <<< TBL_BITS) - 1))

// ------------------------------------------------------------------ timing
printfn "== timing: realistic object, 24 half-float parts, ready, offset present, param_6 given (per part, call overhead included)"
let sT0 = setup None true
let sT = { sT0 with p6 = DEQ }
Marshal.WriteInt64(BLK + 8n, int64 OFFS)
// interleaved rounds (stock, then every switch value, repeated), best of 9 per variant: drift / load hits all alike
let variants = [| ("stock region", -1); ("switch 0 (caves, all off)", 0); ("switch 1 Q quaternion memo", 1); ("switch 2 L plain ready loads", 2)
                  ("switch 4 V forwarding copy", 4); ("switch 3 Q+L", 3); ("switch 7 Q+L+V", 7) |]
let best = Array.create variants.Length Double.MaxValue
clearTable ()
for _ in 1 .. 9 do
    for k in 0 .. variants.Length - 1 do
        let en = snd variants.[k]
        let f, r = if en < 0 then f0, R0 else (setEn en; f1, R1)
        Marshal.WriteInt64(VT + 0xE8n, int64 (r + nativeint STUB_AT))
        for _ in 1 .. 5000 do f.Invoke(OBJ, 0x1234n, OUT0, PARTS, uint32 sT.n, sT.p6)
        let sw = Stopwatch.StartNew()
        for _ in 1 .. 40000 do f.Invoke(OBJ, 0x1234n, OUT0, PARTS, uint32 sT.n, sT.p6)
        best.[k] <- min best.[k] (sw.Elapsed.TotalMilliseconds * 1e6 / (40000.0 * float sT.n))
let tStock = best.[0]
for k in 0 .. variants.Length - 1 do
    printfn "  %-30s %6.1f ns/part  (%+5.1f ns, %+4.0f%%)" (fst variants.[k]) best.[k] (best.[k] - tStock) (100.0 * (best.[k] - tStock) / tStock)
printfn "  ALL: %.1f -> %.1f ns/part = %.0f%% less per part (in-process, warm caches, uncontended ready word)" tStock best.[6] (100.0 * (1.0 - best.[6] / tStock))
printfn "== cave disassembly (test address)"
let fmt = NasmFormatter()
let so = StringOutput()
let dec = Decoder.Create(64, ByteArrayCodeReader(cv.bytes, CODE, cv.bytes.Length - CODE))
dec.IP <- caveBase + uint64 CODE
while dec.IP < caveBase + uint64 cv.bytes.Length do
    let i = dec.Decode()
    fmt.Format(&i, so)
    let tag = if i.IP = cv.qa then "QA:" elif i.IP = cv.qb then "QB:" elif i.IP = cv.l1 then "L1:" elif i.IP = cv.l2 then "L2:" elif i.IP = cv.v then "V:" else ""
    printfn "  %-4s %X  %s" tag (i.IP - caveBase) (so.ToStringAndReset())
printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")
