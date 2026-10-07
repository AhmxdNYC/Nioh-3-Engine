// drawprep_elim.fsx — main-thread draw-prep WORK ELIMINATION (not parallelisation). See notes\parallel-drawprep.md §5.
//
//   dotnet fsi drawprep_elim.fsx offline              NO GAME ACCESS: every site/byte checked against re\Nioh3.dump.exe,
//                                                     caves built at a dummy address and disassembled, then both caves
//                                                     run natively in this process against the ORIGINAL 3C768C/3C774C
//                                                     bytes (FPC) and a model (PP).
//   dotnet fsi drawprep_elim.fsx dry   [PP,FPC]       READ-ONLY on the live game: bytes, vtable slot, g, epoch rate, main TID.
//   dotnet fsi drawprep_elim.fsx apply [PP,FPC]       WRITES (FT_ALLOW_WRITE=1): one cave per game process; hooks are
//                                                     installed with their switches OFF (= stock behaviour).
//   dotnet fsi drawprep_elim.fsx on|off [PP,FPC]      WRITES one dword per id: runtime switch, no code touched.
//   dotnet fsi drawprep_elim.fsx ppn <2|4|8|16>       WRITES: PP decimation period (default 4).
//   dotnet fsi drawprep_elim.fsx status               READ-ONLY: switches + per-second counters (10 x 100 ms).
//   dotnet fsi drawprep_elim.fsx revert [PP,FPC]      WRITES: original call bytes / vtable pointer back (cave stays allocated).
//
// Patches (both default OFF after apply; turn on with `on`):
//   PP   Page-pool usage scan decimation. PagePoolFrameUpdate 252510 calls PagePoolScanAll 2522C4 every frame; on the
//        hot path (pool+B28h != 0) that is PagePoolCountUsed 2523FC = a sweep over EVERY 64-byte entry of the streaming
//        page pool (2.2-3.0 % of [App]Game self in r5/r7/r9). The result is only a statistic: pool+B2Ch (used entries this
//        frame) / +B34h (full buckets), shifted to +B30h / +B38h (= g+6CD0h / g+6CD8h) and read by two heuristics
//        (PagePoolOccupancyCheck 126C64: >85 % -> mip-bias step with a 0.3 s cooldown; PagePoolStatsReader 28B1B1C: a
//        streaming-settled state machine) and two trivial getters. Call site 252677 `call 2522C4` -> `call cave`:
//        run the scan only when (pool+D10h & (N-1)) == 0; on skipped frames publish the last measurement as "previous"
//        (B30h=B2Ch, B38h=B34h) so readers see a value at most N-1 frames old. pool+D10h (= g+6EB0h, the frame stamp)
//        is still incremented by 252510 itself (25267C), so nothing else changes. Not taken when pool+B28h == 0
//        (the other, unanalysed branch).
//   FPC  Material FindParam per-frame cache. MaterialFindParam 3C768C (material vtbl+F8h, slot 38FB9D8) hashes the name
//        and walks the material's parameter list with a virtual compare per node (3C774C/3C7750): 3 dependent loads +
//        an indirect call per node. It is called per draw ("Meshlet" in DrawSubmit 1D2F60; "WritePositionBuffer",
//        "SoftBody", "ControlPointMeshDeformation" in DrawEmit 1E80B0) and per group per pass (39E20: "SSSTransmit",
//        "Refraction*", "IndexedRim", "LightMap"), so the same (material,name) is looked up many times per frame
//        (every pass/cascade/sub-pass that draws it). The vtable slot is re-pointed at a cave: on the main render
//        thread only, for names that are string literals in .rdata, a 2048-entry direct-mapped cache keyed on
//        (material, name pointer, material+1338h first-link, epoch) returns the previous result; anything else calls
//        the original. epoch = own 64-bit counter bumped whenever g+3C60h (stream generation, incremented at every
//        frame end 418D3D and on stream wrap 1910CAF) changes, so entries never outlive a frame.
//
// Safety model = patcher\cpu_caves.fsx / re\scripts_meshlet\meshlet_caves.fsx: original-byte checks, Mem.fsx safeWrite
// (process frozen + thread-RIP check) for the call site, a frozen 8-byte write for the vtable slot, revert leaves the
// cave allocated (threads inside it return normally).
#load "Mem.fsx"
open System
open System.IO
open System.Diagnostics
open System.Runtime.InteropServices
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "offline"
let ALL = "PP,FPC"
let selArg = if args.Length > 1 && mode <> "ppn" then args.[1] else ALL
let selected = selArg.Split(',', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun s -> s.Trim().ToUpper()) |> Set.ofArray
let ids () = [ "PP"; "FPC" ] |> List.filter selected.Contains
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "drawprep_elim_state.txt")
let DUMP = Path.Combine(__SOURCE_DIRECTORY__, @"..\re\Nioh3.dump.exe")
let img = File.ReadAllBytes DUMP
let DUMP_BASE = 0x7FF74F410000UL

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- sites (Steam v2.02) -----------------------------------------------------------------------------------
let PP_CALL, PP_CALL_ORIG = 0x252677, "E8 48 FC FF FF"                 // call PagePoolScanAll 2522C4
let PP_TARGET = 0x2522C4
// instructions inside 2522C4/252510 whose offsets the PP cave relies on (B28h branch byte, D10h stamp, B2C..B38 shift)
let PP_CHECKS =
    [ 0x2522C4, "48 89 5C 24 10 57"            // prologue
      0x252303, "80 BB 38 02 00 00 00"         // cmp byte [rbx+238h],0   (rbx = pool+8F0h -> pool+B28h)
      0x252310, "48 8B 97 10 0D 00 00"         // mov rdx,[rdi+0D10h]     (frame stamp passed to 2523FC)
      0x25232D, "8B 8B 3C 02 00 00"            // mov ecx,[rbx+23Ch]      old B2Ch
      0x252338, "89 83 3C 02 00 00"            // mov [rbx+23Ch],eax      B2Ch = new count
      0x252341, "8B 83 44 02 00 00"            // mov eax,[rbx+244h]      old B34h
      0x25234C, "89 83 48 02 00 00"            // mov [rbx+248h],eax      B38h = old B34h
      0x252356, "89 8B 40 02 00 00"            // mov [rbx+240h],ecx      B30h = old B2Ch
      0x252363, "89 83 44 02 00 00"            // mov [rbx+244h],eax      B34h = new full buckets
      0x25267C, "48 FF 87 10 0D 00 00" ]       // inc qword [rdi+0D10h]   (after the call, untouched)
let FPC_SLOT = 0x38FB9D8                                               // material vtable (38FB8E0) + F8h
let FPC_FN = 0x3C768C
let FPC_FN_BYTES =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F2 33 FF EB 0B 6B FF 1B 0F BE C0 03 F8 48 FF C2 8A 02 84 C0 75 EF " +
    "48 8D 99 30 13 00 00 4C 8B 0B 44 8B C7 48 8B D6 48 8B CB 41 FF 51 60 84 C0 75 09 48 8B 5B 08 48 85 DB 75 E3 48 8B 74 24 38 " +
    "48 8B C3 48 8B 5C 24 30 48 83 C4 20 5F C3"
let NODECMP = 0x3C774C                                                 // node vtbl+60h: mov r9,[rcx+10h]; 3C7750 ...
let NODECMP_BYTES =
    "4C 8B 49 10 45 39 01 74 03 32 C0 C3 41 8B 41 08 4D 8D 51 0C 4D 8D 14 C2 4C 2B D2 0F B6 0A 46 0F B6 04 12 41 2B C8 75 08 " +
    "48 FF C2 45 85 C0 75 EB 85 C9 75 D5 B0 01 C3"
let G_GLOBAL = 0x45BA588                                               // GfxDevice/KtglContext* g
let EPOCH_OFF = 0x3C60L                                                // g+3C60h stream generation (inc at 418D3D, 1910CAF)
let EPOCH_INCS = [ 0x418D3D, "FF 83 60 3C 00 00"; 0x1910CAF, "FF 87 60 3C 00 00" ]
let RDATA_LO, RDATA_SIZE = 0x38E2000, 0xBD1B8C
let FPC_ENTRIES = 2048                                                 // x 64 B = 128 KB
let TABLE_OFF = 0x1000
let ALLOC_SIZE = TABLE_OFF + FPC_ENTRIES * 64
// other patch sets' sites (notes A/B/D, W1/W2, cpu_caves, ft_vrr, meshlet, workers, jobs, logic) - must not overlap
let OTHER_SITES =
    [ 0x4AC14; 0x4AC2B; 0x4AC42; 0x4AECF; 0x4AFAC; 0x4B319; 0x4B32C; 0x4B33F; 0x419E8; 0x41A10; 0x41AAC; 0x41AC3
      0x41ACC; 0x41AED; 0x41AF6; 0x41B12; 0x41B7B; 0x41A28; 0x41A38; 0x41A4A; 0x41A5A; 0x41A6A; 0x41A7A; 0x41A9A
      0x1E835B; 0x1A744E4; 0x3BC17; 0x61BAC; 0x13721F; 0x137269; 0x1372B4; 0x1372FE; 0x1A6BF2D; 0x19DF57E; 0x4A5346
      0x4A5357; 0x5AD677; 0x5AD6AA; 0x5AD860; 0x43593A; 0x437EE5; 0x438473; 0x55DC56; 0x5AF6C9; 0x29770C6; 0x29771B4
      0x419A8; 0x4ABD2; 0x1E72C6; 0x1E71CD; 0x49131; 0x48FB9; 0x49046; 0x49261; 0x4933D; 0x493DC; 0x4122D; 0x41265
      0x41397; 0x414B6; 0x40899; 0x40481; 0x3E7F5; 0x3E821; 0x40A4C; 0x6EA50; 0x1FE300B; 0x1FE3011; 0x1FE30D5; 0x418F68 ]

// ---- cave ----------------------------------------------------------------------------------------------------
// Fixed data layout (read/written by the live modes):
//  +00 en PP (d)  +04 en FPC (d)  +08 PP mask (d)  +0C main TID (d)
//  +10 rdataLo  +18 rdataHi  +20 &g (address of the g global)  +28 original FindParam  +30 PP target  +38 table (all q)
//  +40 lastSeen epoch source (d)  +48 own epoch (q)           [main-thread line]
//  +80 PP ran (d)  +84 PP skipped (d)
//  +C0 FPC hits (q)  +C8 FPC misses (q)
//  +100 FPC main-thread bypasses (q: name not in .rdata, or g == 0)
//  +140 code;  table at +1000h: 2048 x 64 B {material, name, first link, epoch, result}
type Cave = { Bytes: byte[]; PP: uint64; FPC: uint64 }
let OFF_EN_PP, OFF_EN_FPC, OFF_MASK, OFF_TID = 0x00, 0x04, 0x08, 0x0C
let OFF_RLO, OFF_RHI, OFF_GADDR, OFF_ORIG, OFF_PPT, OFF_TAB = 0x10, 0x18, 0x20, 0x28, 0x30, 0x38
let OFF_LAST, OFF_EPOCH = 0x40, 0x48
let OFF_PPRAN, OFF_PPSKIP, OFF_HIT, OFF_MISS, OFF_BYP = 0x80, 0x84, 0xC0, 0xC8, 0x100

type CaveParams = { rdataLo: uint64; rdataHi: uint64; gAddr: uint64; origFn: uint64; ppTarget: uint64; mainTid: uint32 }

let build (at: uint64) (p: CaveParams) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let lbl = Collections.Generic.Dictionary<int, Label>()
    let mutable pos = 0
    let field (off: int) (emit: unit -> unit) (size: int) =
        if off > pos then (a.db(Array.zeroCreate<byte> (off - pos)); pos <- off)
        let mutable l = L()
        a.Label(&l); lbl.[off] <- l
        emit (); pos <- pos + size
    field OFF_EN_PP (fun () -> a.dd(0u)) 4
    field OFF_EN_FPC (fun () -> a.dd(0u)) 4
    field OFF_MASK (fun () -> a.dd(3u)) 4
    field OFF_TID (fun () -> a.dd(p.mainTid)) 4
    field OFF_RLO (fun () -> a.dq(p.rdataLo)) 8
    field OFF_RHI (fun () -> a.dq(p.rdataHi)) 8
    field OFF_GADDR (fun () -> a.dq(p.gAddr)) 8
    field OFF_ORIG (fun () -> a.dq(p.origFn)) 8
    field OFF_PPT (fun () -> a.dq(p.ppTarget)) 8
    field OFF_TAB (fun () -> a.dq(at + uint64 TABLE_OFF)) 8
    field OFF_LAST (fun () -> a.dd(0u)) 4
    field OFF_EPOCH (fun () -> a.dq(1UL)) 8
    field OFF_PPRAN (fun () -> a.dd(0u)) 4
    field OFF_PPSKIP (fun () -> a.dd(0u)) 4
    field OFF_HIT (fun () -> a.dq(0UL)) 8
    field OFF_MISS (fun () -> a.dq(0UL)) 8
    field OFF_BYP (fun () -> a.dq(0UL)) 8
    if 0x140 > pos then (a.db(Array.zeroCreate<byte> (0x140 - pos)); pos <- 0x140)
    let d32 off = __dword_ptr.[lbl.[off]]
    let d64 off = __qword_ptr.[lbl.[off]]

    // ===== PP: entered by `call` from 252677 with rcx = page pool (g+61A0h). Scratch rax + flags (dead after the call:
    //       25267C inc [rdi+0D10h]; 252683 lea rcx; 25268A mov rdx,rsi; 25268D call -> rax is never read).
    let mutable lPP, ppRun = L(), L()
    a.Label(&lPP)
    a.cmp(d32 OFF_EN_PP, 0); a.je(ppRun)
    a.cmp(__byte_ptr.[rcx + 0xB28L], 0); a.je(ppRun)          // only the analysed branch (2523FC path)
    a.mov(eax, __dword_ptr.[rcx + 0xD10L]); a.test(d32 OFF_MASK, eax); a.je(ppRun)
    a.mov(eax, __dword_ptr.[rcx + 0xB2CL]); a.mov(__dword_ptr.[rcx + 0xB30L], eax)   // previous := latest measured
    a.mov(eax, __dword_ptr.[rcx + 0xB34L]); a.mov(__dword_ptr.[rcx + 0xB38L], eax)
    a.inc(d32 OFF_PPSKIP)
    a.ret()
    a.Label(&ppRun)
    a.inc(d32 OFF_PPRAN)
    a.jmp(d64 OFF_PPT)

    // ===== FPC: replaces material vtbl+F8h. rcx = material, rdx = name. Standard x64 ABI (virtual call sites only).
    let mutable lFPC, fBypass, fBypassMain, fEpochOk, fMiss = L(), L(), L(), L(), L()
    a.int3(); a.int3(); a.int3(); a.int3()
    a.Label(&lFPC)
    a.cmp(d32 OFF_EN_FPC, 0); a.je(fBypass)
    a.db([| 0x65uy; 0x8Buy; 0x04uy; 0x25uy; 0x48uy; 0x00uy; 0x00uy; 0x00uy |])     // mov eax,gs:[48h] (TEB ClientId.UniqueThread)
    a.cmp(eax, d32 OFF_TID); a.jne(fBypass)
    a.cmp(rdx, d64 OFF_RLO); a.jb(fBypassMain)
    a.cmp(rdx, d64 OFF_RHI); a.jae(fBypassMain)
    a.mov(r8, d64 OFF_GADDR); a.mov(r8, __qword_ptr.[r8]); a.test(r8, r8); a.je(fBypassMain)
    a.mov(r8d, __dword_ptr.[r8 + EPOCH_OFF])
    a.cmp(r8d, d32 OFF_LAST); a.je(fEpochOk)
    a.mov(d32 OFF_LAST, r8d); a.inc(d64 OFF_EPOCH)
    a.Label(&fEpochOk)
    a.mov(r8, d64 OFF_EPOCH)
    a.mov(r9, __qword_ptr.[rcx + 0x1338L])                                  // first link of the param list
    a.mov(rax, rcx); a.shr(rax, 6uy); a.mov(r10, rcx); a.shr(r10, 17uy); a.xor(rax, r10)
    a.mov(r10, rdx); a.shr(r10, 3uy); a.xor(rax, r10)
    a.``and``(eax, FPC_ENTRIES - 1); a.shl(rax, 6uy)
    a.add(rax, d64 OFF_TAB); a.mov(r10, rax)
    a.cmp(__qword_ptr.[r10], rcx); a.jne(fMiss)
    a.cmp(__qword_ptr.[r10 + 8L], rdx); a.jne(fMiss)
    a.cmp(__qword_ptr.[r10 + 0x10L], r9); a.jne(fMiss)
    a.cmp(__qword_ptr.[r10 + 0x18L], r8); a.jne(fMiss)
    a.mov(rax, __qword_ptr.[r10 + 0x20L])
    a.inc(d64 OFF_HIT)
    a.ret()
    a.Label(&fMiss)
    a.push(rbx); a.push(rsi); a.push(rdi); a.push(r12); a.push(r13)          // rsp = 16n here (entry was 16n+8)
    a.sub(rsp, 0x20)
    a.mov(rbx, r10); a.mov(rsi, rcx); a.mov(rdi, rdx); a.mov(r12, r9); a.mov(r13, r8)
    a.call(d64 OFF_ORIG)                                                    // rcx/rdx unchanged = original args
    a.mov(__qword_ptr.[rbx], 0)                                             // single writer (main thread); invalidate first
    a.mov(__qword_ptr.[rbx + 8L], rdi); a.mov(__qword_ptr.[rbx + 0x10L], r12)
    a.mov(__qword_ptr.[rbx + 0x18L], r13); a.mov(__qword_ptr.[rbx + 0x20L], rax)
    a.mov(__qword_ptr.[rbx], rsi)
    a.inc(d64 OFF_MISS)
    a.add(rsp, 0x20)
    a.pop(r13); a.pop(r12); a.pop(rdi); a.pop(rsi); a.pop(rbx)
    a.ret()                                                                 // rax = original result
    a.Label(&fBypassMain)
    a.inc(d64 OFF_BYP)
    a.Label(&fBypass)
    a.jmp(d64 OFF_ORIG)

    use ms = new MemoryStream()
    let r = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    { Bytes = ms.ToArray(); PP = r.GetLabelRIP(&lPP); FPC = r.GetLabelRIP(&lFPC) }

let printDis (bytes: byte[]) (ip: uint64) (n: int) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    let f = NasmFormatter()
    let o = StringOutput()
    let fin = ip + uint64 (min n bytes.Length)
    while d.IP < fin do
        let ins = d.Decode()
        f.Format(&ins, o)
        printfn "    %X  %s" ins.IP (o.ToStringAndReset())

let callBytes (site: uint64) (target: uint64) =
    Array.append [| 0xE8uy |] (BitConverter.GetBytes(int32 (int64 target - int64 (site + 5UL))))

// ---- native tests (this process only) ------------------------------------------------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<DllImport("kernel32.dll")>] extern uint32 GetCurrentThreadId()
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type FindFn = delegate of nativeint * nativeint -> nativeint
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type PPFn = delegate of nativeint -> unit

let testPP () =
    let exec = VirtualAlloc(0n, unativeint ALLOC_SIZE + 0x1000un, 0x3000u, 0x40u)
    let stub = exec + nativeint ALLOC_SIZE                                  // stand-in for 2522C4: inc dword [rcx+F00h]; ret
    Marshal.Copy([| 0xFFuy; 0x81uy; 0x00uy; 0x0Fuy; 0x00uy; 0x00uy; 0xC3uy |], 0, stub, 7)
    let c = build (uint64 exec) { rdataLo = 0UL; rdataHi = 0UL; gAddr = 0UL; origFn = 0UL; ppTarget = uint64 stub; mainTid = 0u }
    Marshal.Copy(c.Bytes, 0, exec, c.Bytes.Length)
    let f = Marshal.GetDelegateForFunctionPointer<PPFn>(nativeint c.PP)
    let pool = Marshal.AllocHGlobal 0x1000
    let rnd = Random(77)
    let mutable bad, runs, skips = 0, 0, 0
    for i in 1 .. 20000 do
        let b = Array.zeroCreate<byte> 0x1000
        rnd.NextBytes b
        Array.fill b 0xF00 4 0uy
        b.[0xB28] <- (if rnd.Next 4 = 0 then 0uy else byte (rnd.Next(1, 256)))
        let en = rnd.Next 5 <> 0
        let n = [| 2; 4; 8; 16 |].[rnd.Next 4]
        Marshal.WriteInt32(exec + nativeint OFF_EN_PP, if en then 1 else 0)
        Marshal.WriteInt32(exec + nativeint OFF_MASK, n - 1)
        Marshal.Copy(b, 0, pool, 0x1000)
        f.Invoke pool
        let got = Array.zeroCreate<byte> 0x1000
        Marshal.Copy(pool, got, 0, 0x1000)
        // model
        let exp = Array.copy b
        let stamp = BitConverter.ToUInt32(b, 0xD10)
        let skip = en && b.[0xB28] <> 0uy && (stamp &&& uint32 (n - 1)) <> 0u
        if skip then
            Array.blit b 0xB2C exp 0xB30 4
            Array.blit b 0xB34 exp 0xB38 4
            skips <- skips + 1
        else
            Array.blit (BitConverter.GetBytes 1) 0 exp 0xF00 4
            runs <- runs + 1
        if got <> exp then
            bad <- bad + 1
            if bad <= 3 then printfn "  PP MISMATCH case %d en=%b n=%d b28=%d stamp=%X" i en n b.[0xB28] stamp
    let ran, sk = Marshal.ReadInt32(exec + nativeint OFF_PPRAN), Marshal.ReadInt32(exec + nativeint OFF_PPSKIP)
    printfn "  PP: 20000 cases, %d mismatches (model: %d run / %d skipped; cave counters %d / %d)" bad runs skips ran sk
    bad = 0 && ran = runs && sk = skips

let testFPC () =
    let exec = VirtualAlloc(0n, unativeint ALLOC_SIZE + 0x1000un, 0x3000u, 0x40u)
    let fnAddr, cmpAddr = exec + nativeint ALLOC_SIZE, exec + nativeint ALLOC_SIZE + 0x100n
    // the ORIGINAL bytes from the dump (both are position independent: no rel32 / RIP-relative operands)
    Marshal.Copy(img, FPC_FN, fnAddr, 0x5E)
    Marshal.Copy(img, NODECMP, cmpAddr, 0x37)
    // fake .rdata of name literals + heap names (outside the range -> bypass)
    let names = [| "Meshlet"; "SSSTransmit"; "RefractionPrePass"; "Refraction"; "IndexedRim"; "LightMap"; "SoftBody"
                   "WritePositionBuffer"; "ControlPointMeshDeformation"; "Missing"; "A"; "" |]
    let rdata = Marshal.AllocHGlobal 0x1000
    let namePtr = Array.zeroCreate<nativeint> names.Length
    let mutable o = 0
    for i in 0 .. names.Length - 1 do
        let b = Text.Encoding.ASCII.GetBytes(names.[i] + "\000")
        Marshal.Copy(b, 0, rdata + nativeint o, b.Length); namePtr.[i] <- rdata + nativeint o; o <- o + b.Length + 3
    let heapNames = names |> Array.map (fun s -> let b = Text.Encoding.ASCII.GetBytes(s + "\000") in let p = Marshal.AllocHGlobal(b.Length + 8) in Marshal.Copy(b, 0, p, b.Length); p)
    let hash (s: string) = s |> Seq.fold (fun (h: int) ch -> h * 27 + int (sbyte (byte ch))) 0
    // g with the epoch at +3C60h, and the global that points at g
    let g = Marshal.AllocHGlobal 0x4000
    let gGlobal = Marshal.AllocHGlobal 8
    Marshal.WriteIntPtr(gGlobal, g)
    Marshal.WriteInt32(g + nativeint EPOCH_OFF, 1000)
    // node vtable: +60h = original 3C774C copy
    let vt = Marshal.AllocHGlobal 0x80
    Marshal.WriteIntPtr(vt + 0x60n, cmpAddr)
    let tid = GetCurrentThreadId()
    let c = build (uint64 exec) { rdataLo = uint64 rdata; rdataHi = uint64 rdata + 0x1000UL; gAddr = uint64 gGlobal
                                  origFn = uint64 fnAddr; ppTarget = 0UL; mainTid = tid }
    Marshal.Copy(c.Bytes, 0, exec, c.Bytes.Length)
    Marshal.WriteInt32(exec + nativeint OFF_EN_FPC, 1)
    let fOrig = Marshal.GetDelegateForFunctionPointer<FindFn>(fnAddr)
    let fCave = Marshal.GetDelegateForFunctionPointer<FindFn>(nativeint c.FPC)
    let rnd = Random(9090)
    // records: {hash@0, count@8, name@+0Ch+count*8}; nodes {vtbl, next, record}
    let mkRecord (nameIdx: int) (corruptHash: bool) =
        let s = names.[nameIdx]
        let cnt = rnd.Next 3
        let p = Marshal.AllocHGlobal(0x10 + cnt * 8 + s.Length + 1 + 8)
        Marshal.WriteInt32(p, if corruptHash then hash s + 1 else hash s)
        Marshal.WriteInt32(p + 8n, cnt)
        let b = Text.Encoding.ASCII.GetBytes(s + "\000")
        Marshal.Copy(b, 0, p + nativeint (0xC + cnt * 8), b.Length)
        p
    let nodePool = Collections.Generic.List<nativeint>()
    let NMAT = 48
    let mats = Array.init NMAT (fun _ -> Marshal.AllocHGlobal 0x1400)
    // (re)build a material's list in place: embedded node at +1330h, then k extra nodes
    let buildList (m: nativeint) =
        let k = rnd.Next 9
        let pick () = if rnd.Next 6 = 0 then 10 else rnd.Next names.Length   // "A" and others; index 9 ("Missing") never stored
        let pickStored () = let i = pick () in if i = 9 then 0 else i
        Marshal.WriteIntPtr(m + 0x1330n, vt)
        Marshal.WriteIntPtr(m + 0x1340n, mkRecord (pickStored ()) (rnd.Next 8 = 0))
        let mutable prev = m + 0x1330n
        for _ in 1 .. k do
            let nd = Marshal.AllocHGlobal 0x20
            nodePool.Add nd
            Marshal.WriteIntPtr(nd, vt)
            Marshal.WriteIntPtr(nd + 0x10n, mkRecord (pickStored ()) (rnd.Next 8 = 0))
            Marshal.WriteIntPtr(prev + 8n, nd)
            prev <- nd
        Marshal.WriteIntPtr(prev + 8n, 0n)
    for m in mats do buildList m
    let mutable tests, bad, frames = 0, 0, 0
    for frame in 1 .. 4000 do
        frames <- frames + 1
        // frame boundary: the engine's generation counter moves; some materials are destroyed and a new one is built
        // at the SAME address (ABA across frames), some lists change; sometimes the counter moves twice (stream wrap)
        Marshal.WriteInt32(g + nativeint EPOCH_OFF, Marshal.ReadInt32(g + nativeint EPOCH_OFF) + (if rnd.Next 10 = 0 then 2 else 1))
        for _ in 1 .. rnd.Next 4 do buildList mats.[rnd.Next NMAT]
        let enabled = rnd.Next 20 <> 0
        Marshal.WriteInt32(exec + nativeint OFF_EN_FPC, if enabled then 1 else 0)
        let hotM = Array.init 6 (fun _ -> mats.[rnd.Next NMAT])           // per-frame hot set (repeated lookups, like
        let hotN = Array.init 4 (fun _ -> rnd.Next names.Length)           // the same material drawn in many passes)
        for _ in 1 .. 60 do
            let hot = rnd.Next 4 <> 0
            let m = if hot then hotM.[rnd.Next hotM.Length] else mats.[rnd.Next NMAT]
            let ni = if hot then hotN.[rnd.Next hotN.Length] else rnd.Next names.Length
            let np = if rnd.Next 8 = 0 then heapNames.[ni] else namePtr.[ni]
            let a = fOrig.Invoke(m, np)
            let b = fCave.Invoke(m, np)
            tests <- tests + 1
            if a <> b then
                bad <- bad + 1
                if bad <= 5 then printfn "  FPC MISMATCH frame %d mat %X name %s orig %X cave %X" frame m names.[ni] a b
    let hits = Marshal.ReadInt64(exec + nativeint OFF_HIT)
    let miss = Marshal.ReadInt64(exec + nativeint OFF_MISS)
    let byp = Marshal.ReadInt64(exec + nativeint OFF_BYP)
    printfn "  FPC: %d lookups over %d frames, %d mismatches; cave hits %d, misses %d, .rdata bypasses %d" tests frames bad hits miss byp
    // documented limitation: a list edited INSIDE a frame behind the first link is not seen until the next frame
    let m = mats.[0]
    Marshal.WriteInt32(exec + nativeint OFF_EN_FPC, 1)
    Marshal.WriteInt32(g + nativeint EPOCH_OFF, Marshal.ReadInt32(g + nativeint EPOCH_OFF) + 1)
    let before = fCave.Invoke(m, namePtr.[9])                                // "Missing": cached negative
    let tail = Marshal.AllocHGlobal 0x20
    Marshal.WriteIntPtr(tail, vt); Marshal.WriteIntPtr(tail + 0x10n, mkRecord 9 false); Marshal.WriteIntPtr(tail + 8n, 0n)
    let mutable last = m + 0x1330n
    while Marshal.ReadIntPtr(last + 8n) <> 0n do last <- Marshal.ReadIntPtr(last + 8n)
    Marshal.WriteIntPtr(last + 8n, tail)
    let sameFrame = fCave.Invoke(m, namePtr.[9])
    Marshal.WriteInt32(g + nativeint EPOCH_OFF, Marshal.ReadInt32(g + nativeint EPOCH_OFF) + 1)
    let nextFrame = fCave.Invoke(m, namePtr.[9])
    printfn "  FPC limitation check (param appended mid-frame): before=%X same frame=%X (stale, expected) next frame=%X orig=%X"
        before sameFrame nextFrame (fOrig.Invoke(m, namePtr.[9]))
    // rough cost: hit path vs original walk on a long list (delegate overhead included in both)
    let longM = Marshal.AllocHGlobal 0x1400
    Marshal.WriteIntPtr(longM + 0x1330n, vt); Marshal.WriteIntPtr(longM + 0x1340n, mkRecord 1 false)
    let mutable prev = longM + 0x1330n
    for _ in 1 .. 30 do
        let nd = Marshal.AllocHGlobal 0x20
        Marshal.WriteIntPtr(nd, vt); Marshal.WriteIntPtr(nd + 0x10n, mkRecord 2 false); Marshal.WriteIntPtr(prev + 8n, nd); prev <- nd
    Marshal.WriteIntPtr(prev + 8n, 0n)
    let time (f: FindFn) =
        let sw = Stopwatch.StartNew()
        for _ in 1 .. 200000 do f.Invoke(longM, namePtr.[0]) |> ignore
        float sw.Elapsed.TotalMilliseconds * 1e6 / 200000.0
    time fOrig |> ignore; time fCave |> ignore
    printfn "  FPC timing (31-node list, name absent, warm caches, incl. delegate overhead): orig %.0f ns, cave hit %.0f ns" (time fOrig) (time fCave)
    bad = 0 && nextFrame = fOrig.Invoke(m, namePtr.[9])

// ---- offline -------------------------------------------------------------------------------------------------
let offline () =
    printfn "dump: %s" DUMP
    let mutable ok = true
    let chk (rva: int) (exp: string) (what: string) =
        let e = hexb exp
        let cur = img.[rva .. rva + e.Length - 1]
        let m = cur = e
        ok <- ok && m
        printfn "  %-34s %8X  %s" what rva (if m then "MATCH" else "DIFFERENT: " + hexs cur)
    printfn "site bytes vs dump:"
    chk PP_CALL PP_CALL_ORIG "PP call site (call 2522C4)"
    let tgt = int64 PP_CALL + 5L + int64 (BitConverter.ToInt32(img, PP_CALL + 1))
    printfn "  PP call target decodes to %X (%s)" tgt (if tgt = int64 PP_TARGET then "ok" else "WRONG")
    ok <- ok && tgt = int64 PP_TARGET
    for (rva, b) in PP_CHECKS do chk rva b "PP dependency"
    let slot = BitConverter.ToUInt64(img, FPC_SLOT)
    printfn "  FPC vtable slot %X = %X (= base+%X, %s)" FPC_SLOT slot (slot - DUMP_BASE) (if slot = DUMP_BASE + uint64 FPC_FN then "ok" else "WRONG")
    ok <- ok && slot = DUMP_BASE + uint64 FPC_FN
    chk FPC_FN FPC_FN_BYTES "FPC MaterialFindParam 3C768C"
    chk NODECMP NODECMP_BYTES "FPC node compare 3C774C"
    for (rva, b) in EPOCH_INCS do chk rva b "epoch writer inc [g+3C60h]"
    let meshlet = 0x38EA848
    printfn "  \"Meshlet\" literal at %X: in .rdata [%X,%X) = %b" meshlet RDATA_LO (RDATA_LO + RDATA_SIZE) (meshlet >= RDATA_LO && meshlet < RDATA_LO + RDATA_SIZE)
    // other pointers to 3C768C in the image (another vtable would keep the uncached path - harmless, but report)
    let pat = BitConverter.GetBytes(DUMP_BASE + uint64 FPC_FN)
    let mutable n, i = 0, 0
    while i <= img.Length - 8 do
        if img.[i] = pat.[0] && img.[i + 1] = pat.[1] && img.[i .. i + 7] = pat then n <- n + 1
        i <- i + 8
    printfn "  qword pointers to 3C768C in the image: %d (expected 1: the slot)" n
    ok <- ok && n = 1
    for s in OTHER_SITES do
        if s < PP_CALL + 5 && PP_CALL < s + 8 then (printfn "  !! PP overlaps another patch site %X" s; ok <- false)
    printfn "  no overlap with %d known patch sites" OTHER_SITES.Length
    // PP call-site boundary: new bytes = one 5-byte call at the same address (boundaries identical)
    let at = DUMP_BASE + 0x5F10000UL
    let c = build at { rdataLo = DUMP_BASE + uint64 RDATA_LO; rdataHi = DUMP_BASE + uint64 (RDATA_LO + RDATA_SIZE)
                       gAddr = DUMP_BASE + uint64 G_GLOBAL; origFn = DUMP_BASE + uint64 FPC_FN
                       ppTarget = DUMP_BASE + uint64 PP_TARGET; mainTid = 0u }
    printfn "cave @dummy %X: %d bytes (PP=+%X FPC=+%X), table +%X (%d x 64 B)" at c.Bytes.Length (c.PP - at) (c.FPC - at) TABLE_OFF FPC_ENTRIES
    printDis c.Bytes.[int (c.PP - at) ..] c.PP 0x200
    printfn "patched call site: %s -> %s" PP_CALL_ORIG (hexs (callBytes (DUMP_BASE + uint64 PP_CALL) c.PP))
    printfn "native tests:"
    ok <- testPP () && ok
    ok <- testFPC () && ok
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

// ---- live ----------------------------------------------------------------------------------------------------
module TN =
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenThread(uint32 access, bool inheritHandle, int tid)
    [<DllImport("kernel32.dll")>]
    extern int GetThreadDescription(nativeint h, nativeint& s)
    [<DllImport("kernel32.dll")>]
    extern nativeint LocalFree(nativeint p)
    [<DllImport("kernel32.dll")>]
    extern bool CloseHandle(nativeint h)
let mainTid (p: Process) =
    p.Refresh()
    [ for th in p.Threads |> Seq.cast<ProcessThread> do
        let h = TN.OpenThread(0x0400u ||| 0x1000u, false, th.Id)
        if h <> 0n then
            let mutable s = 0n
            let d = if TN.GetThreadDescription(h, &s) >= 0 && s <> 0n then (let r = Marshal.PtrToStringUni s in TN.LocalFree s |> ignore; r) else ""
            TN.CloseHandle h |> ignore
            if d = "[App]Game" then yield th.Id ]
    |> List.tryHead

let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let B = uint64 t.Base
    let at (rva: int) = nativeint (B + uint64 rva)
    let rdU64 (a: nativeint) = BitConverter.ToUInt64(t.Read(a, 8), 0)
    let rdU32 (a: nativeint) = BitConverter.ToUInt32(t.Read(a, 4), 0)
    let paramsFor tid = { rdataLo = B + uint64 RDATA_LO; rdataHi = B + uint64 (RDATA_LO + RDATA_SIZE); gAddr = B + uint64 G_GLOBAL
                          origFn = B + uint64 FPC_FN; ppTarget = B + uint64 PP_TARGET; mainTid = tid }
    let readState () =
        if not (File.Exists stateFile) then failwith "no drawprep_elim_state.txt - apply first"
        let v = File.ReadAllText(stateFile).Trim().Split(' ') |> Array.map (fun x -> Convert.ToUInt64(x, 16))
        if v.Length < 3 || int v.[1] <> t.Process.Id then failwith "state file belongs to another game process"
        v   // cave pid tid
    let ppState (caveAddr: uint64 option) =
        let cur = t.Read(at PP_CALL, 5)
        if cur = hexb PP_CALL_ORIG then "original"
        else match caveAddr with
             | Some ca when cur = callBytes (B + uint64 PP_CALL) (build ca (paramsFor 0u)).PP -> "patched"
             | _ -> "UNKNOWN " + hexs cur
    let fpcState (caveAddr: uint64 option) =
        let cur = rdU64 (at FPC_SLOT)
        if cur = B + uint64 FPC_FN then "original"
        else match caveAddr with
             | Some ca when cur = (build ca (paramsFor 0u)).FPC -> "patched"
             | _ -> sprintf "UNKNOWN %X" cur
    let epochRate () =
        let g = rdU64 (at G_GLOBAL)
        if g = 0UL then 0.0 else
            let e0 = rdU32 (nativeint (g + uint64 EPOCH_OFF))
            Threading.Thread.Sleep 500
            float (rdU32 (nativeint (g + uint64 EPOCH_OFF)) - e0) * 2.0
    let depsOk () =
        let mutable ok = true
        for (rva, b) in PP_CHECKS do if t.Read(at rva, (hexb b).Length) <> hexb b then (printfn "  !! PP dependency %X differs" rva; ok <- false)
        if t.Read(at FPC_FN, 0x5E) <> hexb FPC_FN_BYTES then (printfn "  !! 3C768C body differs"; ok <- false)
        if t.Read(at NODECMP, 0x37) <> hexb NODECMP_BYTES then (printfn "  !! 3C774C body differs"; ok <- false)
        for (rva, b) in EPOCH_INCS do if t.Read(at rva, 6) <> hexb b then (printfn "  !! epoch writer %X differs" rva; ok <- false)
        ok
    match mode with
    | "dry" ->
        let ca = if File.Exists stateFile then (try Some (readState ()).[0] with _ -> None) else None
        printfn "base=%X  PP site %X: %s   FPC slot %X: %s" B PP_CALL (ppState ca) FPC_SLOT (fpcState ca)
        printfn "dependencies: %s" (if depsOk () then "ALL MATCH" else "MISMATCH (see above)")
        let g = rdU64 (at G_GLOBAL)
        printfn "g=[%X]=%X  stream ptr g+3C40=%X  epoch g+3C60 advances %.0f /s (expect ~fps; must be > 0 for FPC)" (B + uint64 G_GLOBAL) g
            (if g = 0UL then 0UL else rdU64 (nativeint (g + 0x3C40UL))) (epochRate ())
        if g <> 0UL then
            let pool = g + 0x61A0UL
            printfn "page pool g+61A0: B28h=%d  D10h(frame stamp)=%d  B2Ch used=%d  B30h prev=%d  B34h full=%d" (t.Read(nativeint (pool + 0xB28UL), 1).[0])
                (rdU64 (nativeint (pool + 0xD10UL))) (rdU32 (nativeint (pool + 0xB2CUL))) (rdU32 (nativeint (pool + 0xB30UL))) (rdU32 (nativeint (pool + 0xB34UL)))
        printfn "[App]Game TID: %A" (mainTid t.Process)
    | "apply" ->
        writeGuard ()
        if not (depsOk ()) then failwith "dependency bytes differ - wrong game version?"
        let tid = match mainTid t.Process with Some x -> uint32 x | None -> failwith "[App]Game thread not found"
        if ids () |> List.contains "PP" && ppState None <> "original" then failwith "PP site not original"
        if ids () |> List.contains "FPC" && fpcState None <> "original" then failwith "FPC slot not original"
        let caveMem = t.AllocNear(t.Base + 0x200000n, ALLOC_SIZE)
        let c = build (uint64 caveMem) (paramsFor tid)
        t.Write(caveMem, c.Bytes)                               // switches are 0 in the image
        File.WriteAllText(stateFile, sprintf "%X %X %X" (uint64 caveMem) t.Process.Id tid)
        if ids () |> List.contains "PP" then
            let n = safeWrite t [ at PP_CALL, callBytes (B + uint64 PP_CALL) c.PP, hexb PP_CALL_ORIG ]
            printfn "PP hook written on attempt %d" n
        if ids () |> List.contains "FPC" then
            t.Frozen(fun () -> t.Write(at FPC_SLOT, BitConverter.GetBytes c.FPC))
            printfn "FPC vtable slot re-pointed"
        printfn "applied %A, cave %X, main TID %d. Switches are OFF: run `on %s`." (ids ()) (uint64 caveMem) tid (String.Join(",", ids ()))
    | "on" | "off" ->
        writeGuard ()
        let v = readState ()
        if mode = "on" && ids () |> List.contains "FPC" then
            let r = epochRate ()
            if r <= 0.0 then failwith "epoch g+3C60 is not advancing (menu/paused?) - refusing to enable FPC"
            match mainTid t.Process with
            | Some x when uint64 x = v.[2] -> ()
            | x -> failwithf "[App]Game TID changed (%A vs %d) - re-apply in a fresh process" x v.[2]
        for id in ids () do
            let off = if id = "PP" then OFF_EN_PP else OFF_EN_FPC
            t.Write(nativeint (v.[0] + uint64 off), BitConverter.GetBytes(if mode = "on" then 1u else 0u))
        printfn "%s %A" mode (ids ())
    | "ppn" ->
        writeGuard ()
        let v = readState ()
        let n = if args.Length > 1 then int args.[1] else 4
        if not (List.contains n [ 2; 4; 8; 16 ]) then failwith "ppn must be 2, 4, 8 or 16"
        t.Write(nativeint (v.[0] + uint64 OFF_MASK), BitConverter.GetBytes(uint32 (n - 1)))
        printfn "PP period = %d frames" n
    | "status" ->
        let v = readState ()
        printfn "PP site: %s   FPC slot: %s" (ppState (Some v.[0])) (fpcState (Some v.[0]))
        let r32 off = rdU32 (nativeint (v.[0] + uint64 off))
        let r64 off = rdU64 (nativeint (v.[0] + uint64 off))
        printfn "switches PP=%d FPC=%d  PP period=%d  main TID=%d  epoch=%d" (r32 OFF_EN_PP) (r32 OFF_EN_FPC) (r32 OFF_MASK + 1u) (r32 OFF_TID) (r64 OFF_EPOCH)
        let snap () = r32 OFF_PPRAN, r32 OFF_PPSKIP, r64 OFF_HIT, r64 OFF_MISS, r64 OFF_BYP
        let mutable p = snap ()
        for _ in 1 .. 10 do
            Threading.Thread.Sleep 100
            let q = snap ()
            let (a1, b1, c1, d1, e1), (a2, b2, c2, d2, e2) = p, q
            let h, m = c2 - c1, d2 - d1
            printfn "  per s: PP scans %4d skipped %4d | FPC hits %7d misses %6d (hit rate %3.0f %%) bypass %5d"
                ((a2 - a1) * 10u) ((b2 - b1) * 10u) (h * 10UL) (m * 10UL) (if h + m > 0UL then 100.0 * float h / float (h + m) else 0.0) ((e2 - e1) * 10UL)
            p <- q
    | "revert" ->
        writeGuard ()
        let v = readState ()
        if ids () |> List.contains "PP" then
            match ppState (Some v.[0]) with
            | "patched" ->
                let c = build v.[0] (paramsFor 0u)
                safeWrite t [ at PP_CALL, hexb PP_CALL_ORIG, callBytes (B + uint64 PP_CALL) c.PP ] |> ignore
                printfn "PP reverted"
            | s -> printfn "PP: %s (nothing to do)" s
        if ids () |> List.contains "FPC" then
            match fpcState (Some v.[0]) with
            | "patched" ->
                t.Frozen(fun () -> t.Write(at FPC_SLOT, BitConverter.GetBytes(B + uint64 FPC_FN)))
                printfn "FPC reverted"
            | s -> printfn "FPC: %s (nothing to do)" s
        printfn "(cave left allocated)"
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
