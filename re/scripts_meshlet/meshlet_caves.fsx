// meshlet_caves.fsx — main-thread meshlet-streaming caves for Nioh 3 (Steam v2.02). See notes\meshlet-mainthread.md.
//
//   dotnet fsi meshlet_caves.fsx offline              NO GAME ACCESS: verify every site against re\Nioh3.dump.exe,
//                                                     build the cave at a dummy address, print cave + patched sites,
//                                                     prove scratch registers/flags dead (static path walk), scan for
//                                                     branch targets inside the sites, and run the 419A8 rewrite
//                                                     natively against the original bytes (own process only).
//   dotnet fsi meshlet_caves.fsx dry   [C419,P4AA,P1E7,S1E7]   READ-ONLY on the live game: bytes + cave preview.
//   dotnet fsi meshlet_caves.fsx apply [ids]          WRITES (needs FT_ALLOW_WRITE=1): one cave per process.
//   dotnet fsi meshlet_caves.fsx status               READ-ONLY: switches + counters (10 x 100 ms).
//   dotnet fsi meshlet_caves.fsx on|off [ids]         WRITES 4 bytes per id: runtime switch, no code touched.
//   dotnet fsi meshlet_caves.fsx revert [ids]         WRITES: original bytes back (cave stays allocated).
//
// Patches (switch defaults when applied: C419=1 P4AA=1 P1E7=1 S1E7=0):
//   C419  419A8 (per-meshlet-entry state flags, called once per frame from 5D214 via 5B278). Entry `mov [rsp+8],rbx`
//         -> jmp cave. Cave = same function with ONE `lock cmpxchg` per entry, and none when the word doesn't change.
//         Original: up to 9 locked loads + 4 separate CAS loops (bit0 = ready, bit7 = bit6, bit6 = bit5, bit5 = 0).
//         Combined CAS = the 4 RMWs applied as one atomic step (a legal interleaving of the original; workers set
//         bit 9 concurrently at 493E9/49428 and are preserved by the CAS retry). Inlines 4A5768 (pure, 2 loads).
//         Switch off -> original prologue + jmp 419AD.
//   P4AA  4ABD2 (loop head of MeshletCheckRequestLoad 4AA34 entry scan, 11-byte `test dword [r14+1ACh],40000000h`)
//         -> jmp cave: software prefetch for entry i+16 (flag line), i+8 (obj+150h line), i+4 (pass 1 only:
//         load p=[obj+150h], prefetch [p]) then the original `test` and jmp 4ABDD. Scratch rcx/rdx/r8/r9 only.
//   P1E7  1E72C6 (texture loop of 1E7050: per texture 128EF0 + 1290B8, both miss-bound on the streaming object
//         tex+68h) -> jmp cave: prefetch tex[k+2]+68h and the 5 hot lines of (tex[k+1]+68h), then the displaced
//         `mov r14,[r12]; mov r8,[r11+1C8h]`, jmp 1E72D1. Scratch rcx/r10.
//   S1E7  EXPERIMENTAL, default OFF. 1E71CD (`cmp rcx,[rdx+20h]; jne 1E7299` = "texture group not yet updated this
//         frame") -> cave: when ON, half of the groups (pointer-hash parity vs a phase that flips once per frame)
//         take the "already updated" path. Every group is still updated at least every other frame. Halves the
//         128EF0/1290B8 work; texture-streaming requests for a group can be 1 frame later. See notes for hazards.
//
// Safety model = patcher\cpu_caves.fsx: original-byte check, Mem.fsx t.Frozen, thread-RIP boundary check, revert
// leaves the cave allocated (threads inside it return to addresses valid in both encodings).
#load @"..\..\patcher\Mem.fsx"
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "offline"
let ALL = "C419,P4AA,P1E7,S1E7"
let selected = (if args.Length > 1 then args.[1] else ALL).Split(',') |> Set.ofArray
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "meshlet_caves_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let img = File.ReadAllBytes DUMP

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- sites -----------------------------------------------------------------------------------------------
let C419_SITE, C419_ORIG = 0x419A8, "48 89 5C 24 08"                         // mov [rsp+8],rbx
let C419_RESUME = 0x419ADUL
let P4AA_SITE, P4AA_ORIG = 0x4ABD2, "41 F7 86 AC 01 00 00 00 00 00 40"       // test dword [r14+1ACh],40000000h
let P4AA_RESUME = 0x4ABDDUL                                                  // jne 4ADA2
let P1E7_SITE, P1E7_ORIG = 0x1E72C6, "4D 8B 34 24 4D 8B 83 C8 01 00 00"      // mov r14,[r12]; mov r8,[r11+1C8h]
let P1E7_RESUME = 0x1E72D1UL                                                 // test r14,r14
let S1E7_SITE, S1E7_ORIG = 0x1E71CD, "48 3B 4A 20 0F 85 C2 00 00 00"         // cmp rcx,[rdx+20h]; jne 1E7299
let S1E7_SAME, S1E7_UPDATE = 0x1E71D7UL, 0x1E7299UL
let siteLen = dict [ "C419", 5; "P4AA", 11; "P1E7", 11; "S1E7", 10 ]
let siteRva = dict [ "C419", C419_SITE; "P4AA", P4AA_SITE; "P1E7", P1E7_SITE; "S1E7", S1E7_SITE ]
let siteOrig = dict [ "C419", hexb C419_ORIG; "P4AA", hexb P4AA_ORIG; "P1E7", hexb P1E7_ORIG; "S1E7", hexb S1E7_ORIG ]
// sites of other patch sets (notes\mainthread-drawprep.md A/B/D, cpu-profile W1/W2, cpu_caves, ft_vrr) - must not overlap
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4 ]

// ---- cave ------------------------------------------------------------------------------------------------
type Cave = { Bytes: byte[]; C419: uint64; P4AA: uint64; P1E7: uint64; S1E7: uint64
              En: uint64      // 4 dwords: C419 P4AA P1E7 S1E7
              Cnt: uint64 }   // dwords: C419 calls, C419 CAS writes, S1E7 skipped groups, S1E7 phase flips

/// `B` = module base the cave jumps back into. For the native test B is chosen so that B+419AD lands in a copy.
let build (at: uint64) (B: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let mutable lPhase, lLast = L(), L()
    let mutable cC419, cP4AA, cP1E7, cS1E7 = L(), L(), L(), L()
    // data: en[0..3] = switches C419 P4AA P1E7 S1E7, cnt[0..3] = counters (fixed layout, read by `status`)
    let lEn = Array.init 4 (fun _ -> L())
    let lCnt = Array.init 4 (fun _ -> L())
    for k in 0 .. 3 do (a.Label(&lEn.[k]); a.dd(if k = 3 then 0u else 1u))
    for k in 0 .. 3 do (a.Label(&lCnt.[k]); a.dd(0u))
    a.Label(&lPhase); a.dd(0u); a.dd(0u)
    a.Label(&lLast);  a.dq(0UL)
    a.int3(); a.int3(); a.int3(); a.int3()
    let en k = __dword_ptr.[lEn.[k]]
    let cnt k = __dword_ptr.[lCnt.[k]]

    // ===== C419: whole-function replacement of 419A8 (entered by jmp from the function entry; rcx = this)
    let mutable cOrig, cRet, cLoop, cNext, cNotReady, cHave, cCas, cDone = L(), L(), L(), L(), L(), L(), L(), L()
    a.Label(&cC419)
    a.cmp(en 0, 0); a.je(cOrig)
    a.push(rbx); a.push(rsi); a.push(rdi); a.push(rbp)
    a.inc(cnt 0)
    // inlined 4A5768: if (cap == 0 || (flags1AC & 0xA0000000)) return; then 419CE: if (flags1AC & 0x40000000) return
    a.mov(ebx, __dword_ptr.[rcx + 0x11CL]); a.test(ebx, ebx); a.je(cRet)
    a.mov(eax, __dword_ptr.[rcx + 0x1ACL]); a.test(eax, int 0xA0000000u); a.jne(cRet)
    a.test(eax, 0x40000000); a.jne(cRet)
    a.mov(r10, rcx)
    a.mov(esi, __dword_ptr.[r10 + 0x4C0L])          // active count (original: lock cmpxchg load at 419E8)
    a.xor(edi, edi)                                  // active entries seen
    a.xor(r11d, r11d)                                // i
    a.xor(r9d, r9d)                                  // i*0x20
    a.Label(&cLoop)
    a.mov(r8, __qword_ptr.[r10 + 0x568L]); a.add(r8, r9)
    a.mov(eax, __dword_ptr.[r8])                     // f (aligned dword load = atomic)
    a.test(al, 2uy); a.je(cNext)
    a.inc(edi)
    // ready = (f & (2|4|8|0x10|0x100|0x400)) == (2|0x400) && (this+18C == 0 || (obj(+8)->+D0 & 8))
    a.mov(edx, eax); a.``and``(edx, 0x51E); a.cmp(edx, 0x402); a.jne(cNotReady)
    a.cmp(__dword_ptr.[r10 + 0x18CL], 0); a.je(cHave)       // edx==0x402 here -> normalised below
    a.mov(rdx, __qword_ptr.[r8 + 8L]); a.test(__byte_ptr.[rdx + 0xD0L], 8); a.je(cNotReady)
    a.Label(&cHave)
    a.mov(edx, 1); a.jmp(cCas)
    a.Label(&cNotReady)
    a.xor(edx, edx)
    a.Label(&cCas)                                   // new = (f & ~0xE1) | ready | ((f & 0x60) << 1)
    a.mov(ecx, eax); a.``and``(ecx, -226); a.``or``(ecx, edx)
    a.mov(ebp, eax); a.``and``(ebp, 0x60); a.add(ebp, ebp); a.``or``(ecx, ebp)
    a.cmp(ecx, eax); a.je(cDone)                     // no change -> no write (original wrote the same value back)
    a.``lock``.cmpxchg(__dword_ptr.[r8], ecx)
    a.jne(cCas)                                      // eax = current word; `ready` kept, bits 5-7 recomputed (as orig)
    a.inc(cnt 1)
    a.Label(&cDone)
    a.cmp(edi, esi); a.jae(cRet)                     // original 41B27: after processing, stop at the active count
    a.Label(&cNext)
    a.inc(r11d); a.add(r9, 0x20); a.cmp(r11d, ebx); a.jb(cLoop)
    a.Label(&cRet)
    a.pop(rbp); a.pop(rdi); a.pop(rsi); a.pop(rbx)
    a.ret()
    a.Label(&cOrig)
    a.mov(__qword_ptr.[rsp + 8L], rbx); a.jmp(B + C419_RESUME)

    // ===== P4AA: loop head of 4AA34's entry scan. r12d=i, r11d=n, r14=this, [rbp-55h]=pass. Scratch rcx/rdx/r8/r9.
    let mutable p2, p3, pEnd = L(), L(), L()
    // returns: r9 = entry address for index i+d, or jumps to `skip` (bounds / unmapped)
    let entryFor (d: int) (skip: Label) =
        a.lea(r9d, __dword_ptr.[r12 + int64 d]); a.cmp(r9d, r11d); a.jae(skip); a.cmp(r9d, 0x100000); a.jae(skip)
        a.mov(rcx, __qword_ptr.[r14 + 0x4B0L]); a.mov(edx, __dword_ptr.[rcx + r9 * 4]); a.cmp(edx, -1); a.je(skip)
        a.shl(rdx, 5uy); a.add(rdx, __qword_ptr.[r14 + 0x568L])
    a.Label(&cP4AA)
    a.cmp(en 1, 0); a.je(pEnd)
    entryFor 16 p2
    a.prefetcht0(__byte_ptr.[rdx])                               // flag word + obj pointers of entry i+16
    a.Label(&p2)
    entryFor 8 p3
    a.mov(r8d, __dword_ptr.[rdx]); a.``and``(r8d, 0x12); a.cmp(r8d, 2); a.jne(p3)
    a.mov(r8, __qword_ptr.[rdx + 0x10L]); a.prefetcht0(__byte_ptr.[r8 + 0x150L])   // obj line of entry i+8
    a.Label(&p3)
    a.cmp(__dword_ptr.[rbp - 0x55L], 1); a.jb(pEnd)                // stage 3 dereferences: pass 1 only (as original)
    entryFor 4 pEnd
    a.mov(r8d, __dword_ptr.[rdx]); a.``and``(r8d, 0x12); a.cmp(r8d, 2); a.jne(pEnd)
    a.mov(r8, __qword_ptr.[rdx + 0x10L]); a.test(r8, r8); a.je(pEnd)
    a.mov(r8, __qword_ptr.[r8 + 0x150L]); a.prefetcht0(__byte_ptr.[r8])          // the dword read at 4AC73
    a.Label(&pEnd)
    a.test(__dword_ptr.[r14 + 0x1ACL], 0x40000000)                // displaced instruction (sets the flags 4ABDD uses)
    a.jmp(B + P4AA_RESUME)

    // ===== P1E7: texture loop of 1E7050. r12=&tex[k], r15d=k, rdx=group(count at +18h), r11=this. Scratch rcx/r10.
    let mutable t1, tEnd = L(), L()
    a.Label(&cP1E7)
    a.cmp(en 2, 0); a.je(tEnd)
    a.lea(ecx, __dword_ptr.[r15 + 2L]); a.cmp(ecx, __dword_ptr.[rdx + 0x18L]); a.jge(t1)
    a.mov(rcx, __qword_ptr.[r12 + 0x10L]); a.prefetcht0(__byte_ptr.[rcx + 0x68L])   // tex[k+2] object
    a.Label(&t1)
    a.lea(ecx, __dword_ptr.[r15 + 1L]); a.cmp(ecx, __dword_ptr.[rdx + 0x18L]); a.jge(tEnd)
    a.mov(rcx, __qword_ptr.[r12 + 8L]); a.test(rcx, rcx); a.je(tEnd)
    a.mov(r10, __qword_ptr.[rcx + 0x68L]); a.test(r10, r10); a.je(tEnd)            // what 1E72D6 loads next iteration
    a.prefetcht0(__byte_ptr.[r10 + 0x12EL])     // 128EF0 first load (+108h+26h)
    a.prefetcht0(__byte_ptr.[r10 + 0x150L])     // 128EF0 list node +140h..+158h
    a.prefetcht0(__byte_ptr.[r10 + 0x90L])      // 1290B8 first load
    a.prefetcht0(__byte_ptr.[r10 + 0x178L])     // 1290B8 second load
    a.prefetcht0(__byte_ptr.[r10 + 0x1B0L])     // 1290B8 +188h stamp / +1B0h
    a.Label(&tEnd)
    a.mov(r14, __qword_ptr.[r12]); a.mov(r8, __qword_ptr.[r11 + 0x1C8L])           // displaced
    a.jmp(B + P1E7_RESUME)

    // ===== S1E7 (experimental): rcx = frame stamp, rdx = texture group. Scratch rax + flags.
    let mutable sSame, sUpd, sKeep = L(), L(), L()
    a.Label(&cS1E7)
    a.cmp(rcx, __qword_ptr.[rdx + 0x20L]); a.je(sSame)
    a.cmp(en 3, 0); a.je(sUpd)
    a.cmp(rcx, __qword_ptr.[lLast]); a.je(sKeep)
    a.mov(__qword_ptr.[lLast], rcx); a.xor(__dword_ptr.[lPhase], 1); a.inc(cnt 3)
    a.Label(&sKeep)
    a.mov(rax, rdx); a.``and``(eax, 0x3F0); a.popcnt(eax, eax); a.xor(eax, __dword_ptr.[lPhase])
    a.test(al, 1uy); a.jne(sUpd)
    a.inc(cnt 2)
    a.Label(&sSame)
    a.jmp(B + S1E7_SAME)
    a.Label(&sUpd)
    a.jmp(B + S1E7_UPDATE)

    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    { Bytes = ms.ToArray(); C419 = addr cC419; P4AA = addr cP4AA; P1E7 = addr cP1E7; S1E7 = addr cS1E7
      En = addr lEn.[0]; Cnt = addr lCnt.[0] }

// ---- site encodings ----------------------------------------------------------------------------------------
let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)
let nops n = match n with
             | 0 -> [||] | 5 -> hexb "0F 1F 44 00 00" | 6 -> hexb "66 0F 1F 44 00 00"
             | _ -> failwithf "nop %d" n
let siteBytes (B: uint64) (c: Cave) (id: string) =
    let rva, len = siteRva.[id], siteLen.[id]
    let target = match id with "C419" -> c.C419 | "P4AA" -> c.P4AA | "P1E7" -> c.P1E7 | _ -> c.S1E7
    Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 rva + 5UL) target; nops (len - 5) ]
let ids () = [ "C419"; "P4AA"; "P1E7"; "S1E7" ] |> List.filter selected.Contains

let printDis (bytes: byte[]) (at: uint64) (B: uint64) =
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- at
    let fmt = NasmFormatter()
    fmt.Options.RipRelativeAddresses <- true
    let out = StringOutput()
    while dec.IP < at + uint64 bytes.Length do
        let ins = dec.Decode()
        fmt.Format(&ins, out)
        let txt = out.ToStringAndReset()
        let note =
            if ins.IsJmpNear || ins.IsJccNear || ins.IsJccShort || ins.IsJmpShort then
                let tgt = ins.NearBranchTarget
                if tgt >= B && tgt < B + 0x6000000UL && (tgt < at || tgt >= at + uint64 bytes.Length) then sprintf "   ; -> Nioh3+%X" (tgt - B) else ""
            else ""
        printfn "  %016X  %-48s%s" ins.IP txt note

// ---- static proof: scratch registers / flags are dead at the resume points (MS x64 ABI for calls) ----------
let infoFactory = InstructionInfoFactory()
let decodeAt (rva: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(img, int rva, 16))
    d.IP <- rva
    d.Decode()
let volatileRegs = [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ]
let argRegs = [ Register.RCX; Register.RDX; Register.R8; Register.R9 ]
let allStatus = RflagsBits.OF ||| RflagsBits.SF ||| RflagsBits.ZF ||| RflagsBits.AF ||| RflagsBits.CF ||| RflagsBits.PF
/// Virtual calls met on the walked paths, with their argument count (from the Ghidra decompiles of 4AA34/1E7050:
/// param_2 vtable methods +78h/+98h/+B8h/+C8h take only `this`; +A8h takes (this, pass); +D8h takes 4 pointers).
let indirectArity =
    dict [ 0x4ADC0UL, 1; 0x4AE1DUL, 1; 0x4AE7FUL, 1; 0x4AE8BUL, 1; 0x4ABA4UL, 2; 0x4AFE0UL, 1 ]
/// Walk every path from `starts` until each tracked register is fully overwritten (32/64-bit write, or clobbered by
/// a call) and, if `flags`, until all status flags are written. Fails on any read of a still-tracked register/flag.
/// A direct call with a tracked argument register is followed into the callee (depth <= 4); a virtual call needs an
/// entry in `indirectArity`. `retReadsRax=false` when the walked function returns void (checked at its call site).
let rec walk (starts: uint64 list) (regs: Register list) (flags: bool) (retReadsRax: bool) (depth: int) : string list =
    let regIdx = regs |> List.mapi (fun i r -> r, i) |> dict
    let flagBit = 1 <<< regs.Length
    let full0 = (1 <<< regs.Length) - 1 ||| (if flags then flagBit else 0)
    let seen = HashSet<struct (uint64 * int)>()
    let fails = ResizeArray<string>()
    let stack = Stack<uint64 * int>()
    for s in starts do stack.Push((s, full0))
    let fmt = NasmFormatter()
    let so = StringOutput()
    let txt (ins: Instruction) = (fmt.Format(&ins, so); so.ToStringAndReset())
    while stack.Count > 0 && fails.Count < 8 do
        let ip, m0 = stack.Pop()
        if m0 <> 0 && seen.Add(struct (ip, m0)) then
            let ins = decodeAt ip
            let m = ref m0
            let fail why = fails.Add(sprintf "%s%X  %s  : %s" (String(' ', 2 * depth)) ip (txt ins) why)
            let tracked r = match regIdx.TryGetValue r with | true, i -> (m.Value &&& (1 <<< i)) <> 0 | _ -> false
            let isZeroIdiom =
                (ins.Mnemonic = Mnemonic.Xor || ins.Mnemonic = Mnemonic.Sub) && ins.OpCount = 2 &&
                ins.Op0Kind = OpKind.Register && ins.Op1Kind = OpKind.Register && ins.Op0Register = ins.Op1Register
            let info = infoFactory.GetInfo(&ins)
            // reads
            if flags && (m.Value &&& flagBit) <> 0 && ins.RflagsRead <> RflagsBits.None then fail "reads flags"
            if not isZeroIdiom then
                for u in info.GetUsedRegisters() do
                    if tracked (u.Register.GetFullRegister()) then
                        match u.Access with
                        | OpAccess.Read | OpAccess.CondRead | OpAccess.ReadWrite | OpAccess.ReadCondWrite ->
                            fail (sprintf "reads %O" u.Register)
                        | _ -> ()
            match ins.FlowControl with
            | FlowControl.Call ->
                let live = argRegs |> List.filter tracked
                if not live.IsEmpty then
                    if depth >= 4 then fail (sprintf "call depth limit with %A tracked" live)
                    else
                        for f in walk [ ins.NearBranchTarget ] live false false (depth + 1) do
                            fail (sprintf "callee %X reads a tracked argument register:
%s" ins.NearBranchTarget f)
            | FlowControl.IndirectCall ->
                match indirectArity.TryGetValue ip with
                | true, n ->
                    for r in argRegs |> List.take n do if tracked r then fail (sprintf "virtual call takes %O" r)
                | _ ->
                    let live = argRegs |> List.filter tracked
                    if not live.IsEmpty then fail (sprintf "virtual call of unknown arity with %A tracked" live)
            | FlowControl.Return -> if retReadsRax && tracked Register.RAX then fail "ret with rax tracked"
            | FlowControl.IndirectBranch -> fail "indirect branch (cannot follow)"
            | _ -> ()
            // writes
            for u in info.GetUsedRegisters() do
                if u.Access = OpAccess.Write && u.Register.GetSize() >= 4 then
                    match regIdx.TryGetValue (u.Register.GetFullRegister()) with
                    | true, i -> m.Value <- m.Value &&& ~~~(1 <<< i)
                    | _ -> ()
            if isZeroIdiom && ins.Op0Register.GetSize() >= 4 then
                match regIdx.TryGetValue (ins.Op0Register.GetFullRegister()) with
                | true, i -> m.Value <- m.Value &&& ~~~(1 <<< i)
                | _ -> ()
            if flags && (ins.RflagsModified &&& allStatus) = allStatus then m.Value <- m.Value &&& ~~~flagBit
            match ins.FlowControl with
            | FlowControl.Call | FlowControl.IndirectCall ->
                for r in volatileRegs do
                    match regIdx.TryGetValue r with
                    | true, i -> m.Value <- m.Value &&& ~~~(1 <<< i)
                    | _ -> ()
                m.Value <- m.Value &&& ~~~flagBit
                stack.Push((ins.NextIP, m.Value))
            | FlowControl.Next | FlowControl.XbeginXabortXend | FlowControl.Exception -> stack.Push((ins.NextIP, m.Value))
            | FlowControl.UnconditionalBranch -> stack.Push((ins.NearBranchTarget, m.Value))
            | FlowControl.ConditionalBranch -> stack.Push((ins.NearBranchTarget, m.Value)); stack.Push((ins.NextIP, m.Value))
            | _ -> ()   // ret / int3 / indirect: path ends
    List.ofSeq fails
let proveDead (name: string) (starts: uint64 list) (regs: Register list) (flags: bool) (retReadsRax: bool) =
    let fails = walk starts regs flags retReadsRax 0
    if fails.IsEmpty then printfn "  [ok] %-5s %A%s dead after the cave" name regs (if flags then " + flags" else "")
    else
        printfn "  [FAIL] %s:" name
        for f in fails do printfn "      %s" f
    fails.IsEmpty

/// direct branch/call targets anywhere in .text that land strictly inside a patched range (would hit a NOP/jmp tail)
let scanTargets () =
    let pe = BitConverter.ToInt32(img, 0x3C)
    let nsec = int (BitConverter.ToUInt16(img, pe + 6))
    let optsz = int (BitConverter.ToUInt16(img, pe + 20))
    let ranges = [ for KeyValue(id, rva) in siteRva -> id, uint64 rva + 1UL, uint64 rva + uint64 siteLen.[id] ]
    let hits = ResizeArray<string>()
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
                        if t >= lo && t < hi then hits.Add(sprintf "%X -> %X (inside %s)" ins.IP t id)
                | _ -> ()
    hits

// ---- native equivalence test: original 419A8 bytes vs the C419 cave (this process only) -------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type FlagsFn = delegate of nativeint -> unit

let testC419 () =
    let FN, FN_LEN = 0x419A8, 0x1F0
    let H, H_LEN = 0x4A5768, 0x19
    let exec = VirtualAlloc(0n, 0x2000un, 0x3000u, 0x40u)
    let hAddr, oAddr, cAddr = exec, exec + 0x100n, exec + 0x400n
    Marshal.Copy(img, H, hAddr, H_LEN)
    Marshal.Copy(img, FN, oAddr, FN_LEN)
    // re-point the copied `call 4A5768` (419BF) at the copied helper
    let callAt = oAddr + nativeint (0x419BF - FN)
    if Marshal.ReadByte(callAt) <> 0xE8uy then failwith "no call at 419BF"
    Marshal.WriteInt32(callAt + 1n, int (int64 hAddr - (int64 callAt + 5L)))
    let fakeB = uint64 oAddr - uint64 FN                     // so B+419AD = the copy's 419AD (switch-off path)
    let cave = build (uint64 cAddr) fakeB
    Marshal.Copy(cave.Bytes, 0, cAddr, cave.Bytes.Length)
    let enAddr = nativeint cave.En
    let fOrig = Marshal.GetDelegateForFunctionPointer<FlagsFn>(oAddr)
    let fCave = Marshal.GetDelegateForFunctionPointer<FlagsFn>(nativeint cave.C419)
    let rnd = Random(4242)
    let bits = [| 1u; 2u; 4u; 8u; 0x10u; 0x20u; 0x40u; 0x80u; 0x100u; 0x200u; 0x400u; 0x800u; 0x10000u; 0x80000000u |]
    let mutable tests, bad, writesSaved = 0, 0, 0
    let THIS = 0x600
    for round in 1 .. 6000 do
        let n = [| 0; 1; 2; 5; 17; 64; 300 |].[rnd.Next 7]
        let alloc k = Marshal.AllocHGlobal(max k 64)
        let mk () =
            let this = alloc THIS
            let ents = alloc ((n + 1) * 0x20)
            let objs = alloc ((n + 1) * 0x100)
            this, ents, objs
        let (t1, e1, o1), (t2, e2, o2) = mk (), mk ()
        let thisBytes = Array.zeroCreate<byte> THIS
        rnd.NextBytes thisBytes
        let wr32 (b: byte[]) off (v: uint32) = Array.blit (BitConverter.GetBytes v) 0 b off 4
        wr32 thisBytes 0x11C (uint32 n)
        wr32 thisBytes 0x18C (if rnd.Next 2 = 0 then 0u else uint32 (rnd.Next(1, 5)))
        wr32 thisBytes 0x1AC (match rnd.Next 12 with 0 -> 0x40000000u | 1 -> 0x80000000u | 2 -> 0x20000000u | 3 -> uint32 (rnd.Next()) | _ -> uint32 (rnd.Next 0x10000))
        let nActive = ref 0
        let entBytes = Array.zeroCreate<byte> ((n + 1) * 0x20)
        rnd.NextBytes entBytes
        let objBytes = Array.zeroCreate<byte> ((n + 1) * 0x100)
        rnd.NextBytes objBytes
        for j in 0 .. n - 1 do
            let mutable f = 0u
            let dens = rnd.Next(1, 8)
            for b in bits do if rnd.Next(dens + 2) = 0 then f <- f ||| b
            if rnd.Next 3 = 0 then f <- (f ||| 0x402u) &&& ~~~0x11Cu      // make "ready" common
            if rnd.Next 2 = 0 then f <- f ||| 2u
            if f &&& 2u <> 0u then nActive.Value <- nActive.Value + 1
            wr32 entBytes (j * 0x20) f
            objBytes.[j * 0x100 + 0xD0] <- byte (rnd.Next 256)
        wr32 thisBytes 0x4C0 (match rnd.Next 4 with 0 -> uint32 nActive.Value | 1 -> uint32 (rnd.Next(0, nActive.Value + 1)) | 2 -> 0u | _ -> uint32 (nActive.Value + rnd.Next 3))
        for (t, e, o) in [ (t1, e1, o1); (t2, e2, o2) ] do
            Marshal.Copy(thisBytes, 0, t, THIS)
            Marshal.WriteIntPtr(t + 0x568n, e)
            for j in 0 .. n do Array.blit (BitConverter.GetBytes(int64 (o + nativeint (j * 0x100)))) 0 entBytes (j * 0x20 + 8) 8
            Marshal.Copy(entBytes, 0, e, entBytes.Length)
            Marshal.Copy(objBytes, 0, o, objBytes.Length)
        let enabled = rnd.Next 10 <> 0
        Marshal.WriteInt32(enAddr, if enabled then 1 else 0)
        fOrig.Invoke t1
        fCave.Invoke t2
        let rd (p: nativeint) k = (let b = Array.zeroCreate<byte> k in Marshal.Copy(p, b, 0, k); b)
        // compare: this struct (except the +568 pointer), entries (except the obj pointers), objects
        let tA, tB = rd t1 THIS, rd t2 THIS
        Array.fill tA 0x568 8 0uy; Array.fill tB 0x568 8 0uy
        let eA, eB = rd e1 entBytes.Length, rd e2 entBytes.Length
        for j in 0 .. n do Array.fill eA (j * 0x20 + 8) 8 0uy; Array.fill eB (j * 0x20 + 8) 8 0uy
        let ok = tA = tB && eA = eB && rd o1 objBytes.Length = rd o2 objBytes.Length
        tests <- tests + 1
        if not ok then
            bad <- bad + 1
            if bad <= 5 then
                printfn "  MISMATCH n=%d en=%b 18C=%d 1AC=%X 4C0=%d" n enabled (BitConverter.ToInt32(thisBytes, 0x18C))
                    (BitConverter.ToUInt32(thisBytes, 0x1AC)) (BitConverter.ToInt32(thisBytes, 0x4C0))
                for j in 0 .. n - 1 do
                    let a, b = BitConverter.ToUInt32(eA, j * 0x20), BitConverter.ToUInt32(eB, j * 0x20)
                    if a <> b then printfn "    entry %d in %08X orig %08X cave %08X" j (BitConverter.ToUInt32(entBytes, j * 0x20)) a b
        for p in [ t1; e1; o1; t2; e2; o2 ] do Marshal.FreeHGlobal p
    let calls = Marshal.ReadInt32(nativeint cave.Cnt)
    let cas = Marshal.ReadInt32(nativeint cave.Cnt + 4n)
    printfn "  C419 native test: %d cases (10%% with the switch off), %d mismatches; cave calls %d, CAS writes %d" tests bad calls cas
    bad = 0

// ---- offline mode (dump only, never touches the game) -----------------------------------------------------
let offline () =
    printfn "dump: %s (raw offset == RVA)" DUMP
    let mutable allOk = true
    printfn "site bytes vs dump:"
    for id in [ "C419"; "P4AA"; "P1E7"; "S1E7" ] do
        let rva, o = siteRva.[id], siteOrig.[id]
        let cur = img.[rva .. rva + o.Length - 1]
        let ok = cur = o
        allOk <- allOk && ok
        printfn "  %-5s %8X  %s  %s" id rva (if ok then "MATCH" else "DIFFERENT") (hexs cur)
        for s in OTHER_SITES do
            if s < rva + o.Length && rva < s + 8 then (printfn "  !! %s overlaps another patch site %X" id s; allOk <- false)
    let B = 0x7FF74F410000UL
    let at = B + 0x5F00000UL
    let c = build at B
    printfn "cave @dummy %X: %d bytes; C419=+%X P4AA=+%X P1E7=+%X S1E7=+%X en=+%X cnt=+%X" at c.Bytes.Length
        (c.C419 - at) (c.P4AA - at) (c.P1E7 - at) (c.S1E7 - at) (c.En - at) (c.Cnt - at)
    printDis c.Bytes.[int (c.C419 - at) ..] c.C419 B
    printfn "patched sites (decoded):"
    for id in [ "C419"; "P4AA"; "P1E7"; "S1E7" ] do
        let rva = siteRva.[id]
        let nb = siteBytes B c id
        printfn " -- %s @%X  orig %s  ->  new %s" id rva (hexs siteOrig.[id]) (hexs nb)
        printDis nb (B + uint64 rva) B
        let bn, bo = boundaries nb (B + uint64 rva), boundaries siteOrig.[id] (B + uint64 rva)
        let both = Set.intersect (Set.ofSeq bn) (Set.ofSeq bo) |> Set.map (fun x -> sprintf "%X" (x - B))
        printfn "    instruction starts valid in both encodings: %A" (Set.toList both)
    printfn "dead-register proofs (paths walked in the dump from each resume point):"
    allOk <- proveDead "P4AA" [ P4AA_RESUME ] [ Register.RCX; Register.RDX; Register.R8; Register.R9 ] false true && allOk
    allOk <- proveDead "P1E7" [ P1E7_RESUME ] [ Register.RCX; Register.R10 ] true false && allOk
    // 1E7050 returns void: its only caller 4BBEC overwrites rax right after the call (4CAE3 mov rax,[r12+2B0h])
    allOk <- proveDead "S1E7" [ S1E7_SAME; S1E7_UPDATE ] [ Register.RAX ] true false && allOk
    printfn "  (C419 replaces the whole function: clobbers rax rcx rdx r8-r11 + flags, a subset of what 419A8 clobbers;"
    printfn "   the only caller path 5D394 -> 5B278 (tail jmp) -> 419A8 resumes at 5D399 'test rsi,rsi'.)"
    printfn "direct branch/call targets strictly inside the patched ranges (whole .text linear sweep):"
    let hits = scanTargets ()
    if hits.Count = 0 then printfn "  none" else (for h in hits do printfn "  !! %s" h); allOk <- allOk && hits.Count = 0
    printfn "native equivalence test (C419 vs original 419A8 bytes copied from the dump):"
    allOk <- testC419 () && allOk
    printfn "%s" (if allOk then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live modes (same pattern as patcher\cpu_caves.fsx) -----------------------------------------------------
let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let readAt (rva: int) n = t.Read(nativeint (B + uint64 rva), n)
    let classify (c: Cave) id =
        let rva, o = siteRva.[id], siteOrig.[id]
        let cur = readAt rva o.Length
        let st = if cur = o then "original" elif cur = siteBytes B c id then "patched" else "UNKNOWN"
        printfn "  %-5s %8X  %-8s  %s" id rva st (hexs cur)
        st
    let readState () =
        if not (File.Exists stateFile) then failwith "no meshlet_caves_state.txt - apply first"
        let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
        if v.Length < 4 || int v.[3] <> t.Process.Id then failwith "state file belongs to another game process"
        v   // cave en cnt pid
    let enIdx = dict [ "C419", 0; "P4AA", 1; "P1E7", 2; "S1E7", 3 ]
    match mode with
    | "dry" ->
        let c = build (B + 0x5F00000UL) B
        printfn "base=%X selected=%A" B (ids ())
        let st = ids () |> List.map (classify c)
        // the in-function A1/A2 rows may be applied or not; both are fine for these caves
        printfn "originals: %s" (if st |> List.forall ((=) "original") then "ALL MATCH" else sprintf "%A" st)
    | "apply" ->
        writeGuard ()
        let probe = build (B + 0x5F00000UL) B
        if ids () |> List.exists (fun id -> classify probe id <> "original") then failwith "sites not original - aborting"
        let caveMem = t.AllocNear(nativeint (B + 0x200000UL), 0x1000)
        let c = build (uint64 caveMem) B
        t.Write(caveMem, c.Bytes)
        File.WriteAllText(stateFile, String.Join(" ", [ uint64 caveMem; c.En; c.Cnt; uint64 t.Process.Id ] |> List.map (sprintf "%X")))
        let n = safeWrite t [ for id in ids () -> nativeint (B + uint64 siteRva.[id]), siteBytes B c id, siteOrig.[id] ]
        printfn "applied %A on attempt %d; cave %X (S1E7 switch is OFF until 'on S1E7')" (ids ()) n (uint64 caveMem)
    | "revert" ->
        writeGuard ()
        let v = readState ()
        let c = build v.[0] B
        let st = ids () |> List.map (fun id -> id, classify c id)
        if st |> List.exists (fun (_, s) -> s = "UNKNOWN") then failwith "unexpected bytes - aborting"
        let todo = st |> List.filter (fun (_, s) -> s = "patched") |> List.map fst
        if not todo.IsEmpty then
            safeWrite t [ for id in todo -> nativeint (B + uint64 siteRva.[id]), siteOrig.[id], siteBytes B c id ] |> ignore
        printfn "reverted %A (cave left allocated)" todo
    | "on" | "off" ->
        writeGuard ()
        let v = readState ()
        for id in ids () do t.Write(nativeint (v.[1] + uint64 (4 * enIdx.[id])), BitConverter.GetBytes(if mode = "on" then 1u else 0u))
        printfn "%s %A" mode (ids ())
    | "status" ->
        let v = readState ()
        let c = build v.[0] B
        for id in [ "C419"; "P4AA"; "P1E7"; "S1E7" ] do classify c id |> ignore
        let rd (a: uint64) = BitConverter.ToUInt32(t.Read(nativeint a, 4), 0)
        printfn "switches C419=%d P4AA=%d P1E7=%d S1E7=%d" (rd v.[1]) (rd (v.[1] + 4UL)) (rd (v.[1] + 8UL)) (rd (v.[1] + 12UL))
        let mutable p = [| for k in 0 .. 3 -> rd (v.[2] + uint64 (4 * k)) |]
        for _ in 1 .. 10 do
            Threading.Thread.Sleep 100
            let q = [| for k in 0 .. 3 -> rd (v.[2] + uint64 (4 * k)) |]
            printfn "  per s: C419 calls %5d  CAS writes %7d   S1E7 frames %4d  groups deferred %6d"
                ((q.[0] - p.[0]) * 10u) ((q.[1] - p.[1]) * 10u) ((q.[3] - p.[3]) * 10u) ((q.[2] - p.[2]) * 10u)
            p <- q
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
