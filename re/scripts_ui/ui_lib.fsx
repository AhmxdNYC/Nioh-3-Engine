// ui_lib.fsx - shared definitions for the UI main-thread fixes (notes\ui-mainthread.md).
// Loaded by ui_offline.fsx (dump + in-process tests, never touches the game) and ui_live.fsx (guarded live patcher).
//
//   W1b  61BAC   glyph padding clear: W1's `rep stosd` (count = pad = 1) -> plain store loop      (in place, 22 B)
//   W2e  1371BC  UI vertex helper `and dword [rax],0` on the mapped VB -> plain store loop           (in place, 20 B)
//   E    1CD690  UI vertex build runs in a cached scratch buffer; one streaming copy into the mapped
//                upload buffer right before Unmap (removes the uncached alpha RMW reads)            (cave, 4 sites)
//   C    4A5420  glyph-atlas per-frame wipe only when needed (atlas persistent mode is already on);
//        4A2EBA  upload only the rows that can have changed;  196051A  remember an atlas overflow     (cave, 3 sites)
//   R    7D640E  skip the render-path 1CE818 walk when it is called with rasterize=0 (pure no-op)    (cave, 1 site)
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
open type Iced.Intel.AssemblerRegisters

let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---------------------------------------------------------------------------------------------------------
// in-place patches: name, rva, accepted current bytes (stock first), new bytes
// ---------------------------------------------------------------------------------------------------------
let W1_STOCK = "33 C9 41 0F B6 06 41 03 C7 03 C1 FF C1 83 24 83 00 41 3B CF 72 EC"
let W1_NEW   = "41 0F B6 06 41 03 C7 48 8D 3C 83 44 89 F9 31 C0 F3 AB 0F 1F 40 00"      // cpu_patches.fsx W1
// W1b: movzx eax,[r14]; add eax,r15d; lea ecx,[rax+r15]; xor edx,edx;
//      L: mov [rbx+rax*4],edx; inc eax; cmp eax,ecx; jb L          (r15d >= 1 is guaranteed by 61B96)
let W1B_NEW  = "41 0F B6 06 44 01 F8 42 8D 0C 38 31 D2 89 14 83 FF C0 39 C8 72 F7"
// W2e (1371BC..1371CF, after `test edx,edx; je 1371CF`):
//      lea rax,[rcx+28h]; xor ecx,ecx; L: mov [rax],ecx; add rax,2Ch; dec edx; jne L; nop3; (ret at 1371CF kept)
let W2E_STOCK = "48 8D 41 28 8B CA 83 20 00 48 8D 40 2C 48 83 E9 01 75 F3"
let W2E_NEW   = "48 8D 41 28 31 C9 89 08 48 83 C0 2C FF CA 75 F6 0F 1F 00"
let inplace =
    [ "W1b", 0x61BAC, [ W1_STOCK; W1_NEW ], W1B_NEW
      "W2e", 0x1371BC, [ W2E_STOCK ], W2E_NEW ]

// ---------------------------------------------------------------------------------------------------------
// cave sites
// ---------------------------------------------------------------------------------------------------------
let E_S1, E_S1_ORIG = 0x1CD74F, "48 8B 2E 48 85 ED 0F 84 29 02 00 00"     // mov rbp,[rsi]; test rbp,rbp; je 1CD984
let E_X1, E_X2, E_X3 = 0x1CD942, 0x19602C8, 0x19602DC                     // mov rcx,[rdi+378h] before each Unmap(VB)
let E_X_ORIG = "48 8B 8F 78 03 00 00"
let C1_SITE, C1_ORIG = 0x4A5420, "E8 9B 4E 4C 00"                          // call 96A2C0 (atlas wipe)
let C2_SITE, C2_ORIG = 0x4A2EBA, "83 64 24 34 00"                          // and dword [rsp+34h],0 (rect.top = 0)
let C3_SITE, C3_ORIG = 0x196051A, "45 32 F6 E9 26 E3 86 FE"                // xor r14b,r14b; jmp 1CE848 (text overflow)
let R_SITE, R_ORIG = 0x7D640E, "E8 05 84 9F FF"                            // call 1CE818 (render-path walk)
let RVA_1CD75B, RVA_1CD984, RVA_96A2C0, RVA_1CE848, RVA_1CE818 = 0x1CD75BUL, 0x1CD984UL, 0x96A2C0UL, 0x1CE848UL, 0x1CE818UL

// ---------------------------------------------------------------------------------------------------------
// cave data block (fixed offsets; ui_live.fsx status reads them)
// ---------------------------------------------------------------------------------------------------------
module D =
    let enE, enC, enR, busy = 0x00, 0x04, 0x08, 0x0C
    let cntRedir, cntFallCap, cntFallBusy, maxBytes = 0x10, 0x14, 0x18, 0x1C
    let cntSkip, cntReset, cntFail, dirtyTop = 0x20, 0x24, 0x28, 0x2C
    let thrCount, thrHeight16, failFlag, cntRSkip = 0x30, 0x34, 0x38, 0x3C
    let scratch, scratchCap, savedObj, savedReal = 0x40, 0x48, 0x50, 0x58
    let trackedG, lastTex, lastFont, lastFontData = 0x60, 0x68, 0x70, 0x78
    let lastR, gFlags, gCount, gMaxY, gW, gH, cntInvalid = 0x80, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C
    let SIZE = 0x100
let THR_COUNT = 512u          // keep glyphs while fewer than half of the 1024 slots are used ...
let THR_HEIGHT16 = 8u         // ... and the packed height is below 8/16 of the atlas height

type Cave = { Bytes: byte[]; Data: uint64; E_S1: uint64; E_X1: uint64; E_X2: uint64; E_X3: uint64
              C1: uint64; C2: uint64; C3: uint64; R: uint64 }

/// Build the cave at address `at`. `B` = module base (the fake one in tests), scratch/cap = E's cached buffer.
let build (at: uint64) (B: uint64) (scratch: uint64) (scratchCap: uint64) =
    let a = Assembler(64)
    let L () = a.CreateLabel()
    let blk = Array.zeroCreate<byte> D.SIZE
    let put32 (o: int) (v: uint32) = Array.blit (BitConverter.GetBytes v) 0 blk o 4
    let put64 (o: int) (v: uint64) = Array.blit (BitConverter.GetBytes v) 0 blk o 8
    put32 D.enE 1u; put32 D.enC 1u; put32 D.enR 1u
    put32 D.thrCount THR_COUNT; put32 D.thrHeight16 THR_HEIGHT16
    put64 D.scratch scratch; put64 D.scratchCap scratchCap
    // data block first (at `at`), one label per dword slot so every field is a plain RIP-relative operand
    let slots = Array.init (D.SIZE / 4) (fun _ -> a.CreateLabel())
    for k in 0 .. slots.Length - 1 do
        a.Label(&slots.[k]); a.dd(BitConverter.ToUInt32(blk, k * 4))
    let mutable dat = slots.[0]
    let d32 (o: int) = __dword_ptr.[slots.[o / 4]]
    let d64 (o: int) = __qword_ptr.[slots.[o / 4]]
    let jmpB (rva: uint64) = a.jmp(B + rva)

    // ======================= E: UI vertex build in cached scratch =======================
    // S1 (replaces 1CD74F..1CD75A). rdi=obj, rsi=&obj+358h (= mapped VB ptr), rbx=3 (live!), rcx/rax/rdx dead.
    let mutable eS1, eFail, eBack, eCap, eBusy = L(), L(), L(), L(), L()
    a.Label(&eS1)
    a.mov(rbp, __qword_ptr.[rsi]); a.test(rbp, rbp); a.je(eFail)
    a.cmp(d32 D.enE, 0); a.je(eBack)
    a.mov(eax, __dword_ptr.[rdi + 0x394]); a.cmp(rax, d64 D.scratchCap); a.ja(eCap)   // producers never exceed [obj+394h]
    a.xor(eax, eax); a.mov(ecx, 1); a.``lock``.cmpxchg(d32 D.busy, ecx); a.jne(eBusy)
    a.mov(d64 D.savedReal, rbp); a.mov(d64 D.savedObj, rdi)
    a.mov(rbp, d64 D.scratch); a.mov(__qword_ptr.[rsi], rbp)
    a.inc(d32 D.cntRedir)
    a.Label(&eBack); jmpB RVA_1CD75B
    a.Label(&eCap); a.``lock``.inc(d32 D.cntFallCap); a.jmp(eBack)
    a.Label(&eBusy); a.``lock``.inc(d32 D.cntFallBusy); a.jmp(eBack)
    a.Label(&eFail); jmpB RVA_1CD984
    // release routine (called from the three exits). rdi=obj, rbp=walk pointer (base if no 2nd pass).
    // Free: rax rcx rdx r8-r11 xmm0-3 (an Unmap call follows immediately).
    let mutable eRel, eRelRet, eLenOk, eRbpOk, eL64, eTail, eMaxOk = L(), L(), L(), L(), L(), L(), L()
    a.Label(&eRel)
    a.cmp(d64 D.savedObj, rdi); a.jne(eRelRet)
    a.mov(ecx, __dword_ptr.[rdi + 0x39C])                       // bytes the producers committed
    a.mov(rdx, rbp); a.sub(rdx, d64 D.scratch); a.jb(eRbpOk)    // bytes the 2nd pass walked (rbp-scratch), if in range
    a.cmp(rdx, d64 D.scratchCap); a.ja(eRbpOk)
    a.cmp(rcx, rdx); a.cmovb(rcx, rdx)
    a.Label(&eRbpOk)
    a.mov(rax, __qword_ptr.[rdi + 0x378]); a.mov(rax, __qword_ptr.[rax + 0x78])   // mapped size (set by 4FD84)
    a.cmp(rcx, rax); a.cmova(rcx, rax)
    a.cmp(rcx, d64 D.scratchCap); a.cmova(rcx, d64 D.scratchCap)
    a.cmp(ecx, d32 D.maxBytes); a.jbe(eMaxOk); a.mov(d32 D.maxBytes, ecx); a.Label(&eMaxOk)
    // keep the cursor consistent with the real mapping (it is zeroed right after Unmap anyway)
    a.mov(rax, __qword_ptr.[rdi + 0x358]); a.sub(rax, d64 D.scratch); a.add(rax, d64 D.savedReal); a.mov(__qword_ptr.[rdi + 0x358], rax)
    a.push(rsi); a.push(rdi)
    a.mov(rsi, d64 D.scratch); a.mov(rdi, d64 D.savedReal)
    a.cmp(rcx, 64); a.jb(eTail)
    a.Label(&eL64)
    a.movdqu(xmm0, __xmmword_ptr.[rsi]); a.movdqu(xmm1, __xmmword_ptr.[rsi + 16])
    a.movdqu(xmm2, __xmmword_ptr.[rsi + 32]); a.movdqu(xmm3, __xmmword_ptr.[rsi + 48])
    a.movdqu(__xmmword_ptr.[rdi], xmm0); a.movdqu(__xmmword_ptr.[rdi + 16], xmm1)
    a.movdqu(__xmmword_ptr.[rdi + 32], xmm2); a.movdqu(__xmmword_ptr.[rdi + 48], xmm3)
    a.add(rsi, 64); a.add(rdi, 64); a.sub(rcx, 64); a.cmp(rcx, 64); a.jae(eL64)
    a.Label(&eTail)
    a.rep.movsb()
    a.sfence()
    a.pop(rdi); a.pop(rsi)
    a.mov(d64 D.savedObj, 0); a.mov(d32 D.busy, 0)
    a.Label(&eRelRet); a.ret()
    let exitStub (siteRva: int) =
        let mutable l = L()
        a.Label(&l)
        a.call(eRel); a.mov(rcx, __qword_ptr.[rdi + 0x378]); jmpB (uint64 siteRva + 7UL)
        l
    let mutable eX1 = exitStub E_X1
    let mutable eX2 = exitStub E_X2
    let mutable eX3 = exitStub E_X3

    // ======================= C: atlas wipe only when needed =======================
    // C1 replaces `call 96A2C0` at 4A5420 (entered by call). rcx = text renderer R (must survive), G = [R+28h].
    // Uses rax rdx r8 r9 only (96A2C0 itself clobbers rdx/eax).
    let mutable c1, cNoTex, cRet, cInvalid, cReset, cChkFail, cTopOk, cNoFont = L(), L(), L(), L(), L(), L(), L(), L()
    a.Label(&c1)
    a.mov(d64 D.lastR, rcx)
    a.mov(rdx, __qword_ptr.[rcx + 0x28]); a.test(rdx, rdx); a.je(cRet)       // stock: G==0 -> nothing
    a.mov(d64 D.trackedG, rdx)
    a.mov(eax, __dword_ptr.[rdx + 0x74]); a.mov(d32 D.gCount, eax)
    a.mov(eax, __dword_ptr.[rdx + 0x8C]); a.mov(d32 D.gMaxY, eax)
    a.mov(eax, __dword_ptr.[rdx + 0x90]); a.mov(d32 D.gW, eax)
    a.mov(eax, __dword_ptr.[rdx + 0x94]); a.mov(d32 D.gH, eax)
    a.movzx(eax, __byte_ptr.[rdx + 0xA8])
    a.mov(r8, __qword_ptr.[rdx + 0x40]); a.test(r8, r8); a.je(cNoTex)
    a.movzx(r9d, __byte_ptr.[r8 + 0xA8]); a.shl(r9d, 8); a.``or``(eax, r9d)
    a.Label(&cNoTex); a.mov(d32 D.gFlags, eax)
    a.cmp(d32 D.enC, 0); a.je(cReset)
    a.cmp(__byte_ptr.[rdx + 0xA8], 0); a.je(cInvalid)                       // atlas not in persistent mode
    a.test(r8, r8); a.je(cInvalid)
    a.cmp(__byte_ptr.[r8 + 0xA8], 0); a.je(cInvalid)                        // texture staging not persistent
    a.cmp(r8, d64 D.lastTex); a.jne(cReset)                                  // generation: texture
    a.mov(r9, __qword_ptr.[rdx + 0x10]); a.cmp(r9, d64 D.lastFont); a.jne(cReset)   // font object
    a.test(r9, r9); a.je(cChkFail)
    a.mov(r9, __qword_ptr.[r9 + 0x18]); a.cmp(r9, d64 D.lastFontData); a.jne(cReset) // font data
    a.Label(&cChkFail)
    a.cmp(d32 D.failFlag, 0); a.jne(cReset)                                  // text was dropped last frame
    a.mov(eax, __dword_ptr.[rdx + 0x74]); a.cmp(eax, d32 D.thrCount); a.jae(cReset)
    a.mov(eax, __dword_ptr.[rdx + 0x8C]); a.shl(eax, 4)
    a.mov(r9d, __dword_ptr.[rdx + 0x94]); a.imul(r9d, d32 D.thrHeight16); a.cmp(eax, r9d); a.jae(cReset)
    // keep the glyphs: rows that can change from now on start at (shelf top - pad)
    a.mov(eax, __dword_ptr.[rdx + 0x88]); a.sub(eax, __dword_ptr.[rdx + 0x7C]); a.jae(cTopOk); a.xor(eax, eax)
    a.Label(&cTopOk); a.mov(d32 D.dirtyTop, eax)
    a.inc(d32 D.cntSkip)
    a.Label(&cRet); a.ret()
    a.Label(&cInvalid); a.inc(d32 D.cntInvalid)
    a.Label(&cReset)
    a.mov(r8, __qword_ptr.[rdx + 0x40]); a.mov(d64 D.lastTex, r8)
    a.mov(r9, __qword_ptr.[rdx + 0x10]); a.mov(d64 D.lastFont, r9)
    a.xor(eax, eax); a.test(r9, r9); a.je(cNoFont); a.mov(rax, __qword_ptr.[r9 + 0x18])
    a.Label(&cNoFont); a.mov(d64 D.lastFontData, rax)
    a.mov(d32 D.failFlag, 0); a.mov(d32 D.dirtyTop, 0)
    a.inc(d32 D.cntReset)
    jmpB RVA_96A2C0                                                          // stock wipe, returns to 4A5425
    // C2 replaces `and dword [rsp+34h],0` at 4A2EBA (entered by call): rect.top for the atlas upload.
    // rbx = rcx = G; eax (= W), rdx, r8, r9 are live here -> only r10/r11 used.
    let mutable c2, c2Put = L(), L()
    a.Label(&c2)
    a.xor(r10d, r10d)
    a.cmp(d32 D.enC, 0); a.je(c2Put)
    a.cmp(rbx, d64 D.trackedG); a.jne(c2Put)
    a.mov(r10d, d32 D.dirtyTop); a.mov(r11d, __dword_ptr.[rbx + 0x8C])
    a.cmp(r10d, r11d); a.jbe(c2Put); a.mov(r10d, r11d)
    a.Label(&c2Put); a.mov(__dword_ptr.[rsp + 0x3C], r10d)                  // caller's [rsp+34h]
    a.ret()
    // C3 replaces `xor r14b,r14b; jmp 1CE848` at 196051A (text rasterization failed -> rest of frame dropped)
    let mutable c3 = L()
    a.Label(&c3)
    a.mov(d32 D.failFlag, 1); a.inc(d32 D.cntFail)
    a.xor(r14b, r14b); jmpB RVA_1CE848

    // ======================= R: skip the no-op render-path walk =======================
    // replaces `call 1CE818` at 7D640E (entered by call; tail-jumps to 1CE818 when it has work to do)
    let mutable r, rGo = L(), L()
    a.Label(&r)
    a.test(r9b, r9b); a.jne(rGo)
    a.cmp(d32 D.enR, 0); a.je(rGo)
    a.inc(d32 D.cntRSkip); a.ret()
    a.Label(&rGo); jmpB RVA_1CE818
    a.int3(); a.int3(); a.int3(); a.int3()

    use ms = new MemoryStream()
    let res = a.Assemble(StreamCodeWriter(ms), at, BlockEncoderOptions.ReturnNewInstructionOffsets)
    let addr (l: Label) = (let mutable x = l in res.GetLabelRIP(&x))
    { Bytes = ms.ToArray(); Data = addr dat; E_S1 = addr eS1; E_X1 = addr eX1; E_X2 = addr eX2; E_X3 = addr eX3
      C1 = addr c1; C2 = addr c2; C3 = addr c3; R = addr r }

let rel32 (from: uint64) (target: uint64) =
    let d = int64 target - int64 from
    if d > int64 Int32.MaxValue || d < int64 Int32.MinValue then failwith "rel32 out of range"
    BitConverter.GetBytes(int d)

/// (feature, name, rva, original, new) for the selected cave features
let caveEdits (B: uint64) (c: Cave) (features: Set<string>) =
    let jmp5 (rva: int) (t: uint64) (pad: byte[]) = Array.concat [ [| 0xE9uy |]; rel32 (B + uint64 rva + 5UL) t; pad ]
    let call5 (rva: int) (t: uint64) = Array.append [| 0xE8uy |] (rel32 (B + uint64 rva + 5UL) t)
    [ if features.Contains "E" then
        yield "E", "E_S1", E_S1, hexb E_S1_ORIG, jmp5 E_S1 c.E_S1 (hexb "0F 1F 80 00 00 00 00")
        yield "E", "E_X1", E_X1, hexb E_X_ORIG, jmp5 E_X1 c.E_X1 (hexb "66 90")
        yield "E", "E_X2", E_X2, hexb E_X_ORIG, jmp5 E_X2 c.E_X2 (hexb "66 90")
        yield "E", "E_X3", E_X3, hexb E_X_ORIG, jmp5 E_X3 c.E_X3 (hexb "66 90")
      if features.Contains "C" then
        yield "C", "C1", C1_SITE, hexb C1_ORIG, call5 C1_SITE c.C1
        yield "C", "C2", C2_SITE, hexb C2_ORIG, call5 C2_SITE c.C2
        yield "C", "C3", C3_SITE, hexb C3_ORIG, jmp5 C3_SITE c.C3 (hexb "0F 1F 00")
      if features.Contains "R" then
        yield "R", "R", R_SITE, hexb R_ORIG, call5 R_SITE c.R ]

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
            if ins.IsJmpNear || ins.IsCallNear || ins.IsJccNear || ins.IsJmpShort || ins.IsJccShort then
                let t = ins.NearBranchTarget
                if t >= B && t < B + 0x6000000UL && (t < at || t >= at + uint64 bytes.Length) then sprintf "   ; Nioh3+%X" (t - B) else ""
            else ""
        printfn "  %016X  %-48s%s" ins.IP txt note
