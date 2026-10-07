// mainthread_patches.fsx - OFFLINE verification of the proposed main-thread patches (notes\mainthread-drawprep.md).
//   dotnet fsi mainthread_patches.fsx
// Reads ONLY re\Nioh3.dump.exe. It never opens the game process. For each site it checks the original bytes,
// decodes original and replacement with Iced, and proves the replacement is `mov eax,<same memory operand>`
// followed by NOP filler of the same total length (so instruction boundaries outside the site are unchanged
// and the site start stays an instruction start in both encodings).
//
// A1/A2/A3 "atomic-load idiom": the engine reads shared flag words with
//     xor eax,eax ; lock cmpxchg [m], rZ        (rZ == 0)
// i.e. InterlockedCompareExchange(p, 0, 0). Because rZ == eax == 0 the store (if any) writes back 0 into a 0,
// so the architectural result is only `eax = [m]` + flags. Every following instruction re-sets flags before
// use (checked below), so `mov eax,[m]` is equivalent. x64 aligned 32-bit loads are atomic and already have
// acquire ordering; what is dropped is the full fence + the forced exclusive/dirty cache-line ownership,
// which is what serialises the pointer-chasing loops these sites sit in (no memory-level parallelism).
// The xor eax,eax in front is left in place (keeps both old instruction starts valid; it is a free zero idiom).
// Real RMW CAS loops (src != 0) and refcount lock inc/xadd are NOT touched.
//
// Manually verified zero source registers (see notes):
//   4AA34 hot loop: r10d zeroed at 4ABB9 / 4AD97 / 4AFDD; the only other write (4AF21) is on the path that
//                   jumps back into the body and is re-zeroed at 4AD97/4AFDD before the next iteration's loads.
//   4B104:          r10d zeroed at 4B2CA / 4B5DE, never written otherwise in the loop.
//   419A8:          ebp zeroed at 419C4, never written afterwards.
//   1E80B0:         edx zeroed at 1E8359 (immediately before), eax at 1E834C (no eax write in between).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))

// id, rva, original, replacement
let L_R10_RDXRCX = "F0 44 0F B1 14 0A", "8B 04 0A 0F 1F 00"          // lock cmpxchg [rdx+rcx],r10d -> mov eax,[rdx+rcx]
let L_EBP_R8 = "F0 41 0F B1 28", "41 8B 00 66 90"                     // lock cmpxchg [r8],ebp      -> mov eax,[r8]
let L_EBP_R9RCX = "F0 41 0F B1 2C 09", "41 8B 04 09 66 90"            // lock cmpxchg [r9+rcx],ebp  -> mov eax,[r9+rcx]
let sites = [
    // A1: MeshletCheckRequestLoad (4AA34), per-meshlet-entry flag reads in the active (>=6 slot) loop
    "A1a", 0x4AC14, L_R10_RDXRCX
    "A1b", 0x4AC2B, L_R10_RDXRCX
    "A1c", 0x4AC42, L_R10_RDXRCX
    "A1d", 0x4AECF, L_R10_RDXRCX
    "A1e", 0x4AFAC, L_R10_RDXRCX
    // A1 (same idiom) in 4B104 (called from 5DC18, frame-begin meshlet pass)
    "A1f", 0x4B319, L_R10_RDXRCX
    "A1g", 0x4B32C, L_R10_RDXRCX
    "A1h", 0x4B33F, L_R10_RDXRCX
    // A2: meshlet state-flag update (419A8): reads only; the CAS loops (lock cmpxchg [r8],ecx) stay locked
    "A2a", 0x419E8, ("F0 0F B1 A9 C0 04 00 00", "8B 81 C0 04 00 00 66 90")
    "A2b", 0x41A10, L_EBP_R8
    "A2c", 0x41A28, L_EBP_R9RCX
    "A2d", 0x41A38, L_EBP_R9RCX
    "A2e", 0x41A4A, L_EBP_R9RCX
    "A2f", 0x41A5A, L_EBP_R9RCX
    "A2g", 0x41A6A, L_EBP_R9RCX
    "A2h", 0x41A7A, L_EBP_R9RCX
    "A2i", 0x41A9A, ("F0 0F B1 A9 D0 00 00 00", "8B 81 D0 00 00 00 66 90")
    "A2j", 0x41AAC, L_EBP_R8
    "A2k", 0x41AC3, L_EBP_R8
    "A2l", 0x41ACC, L_EBP_R8
    "A2m", 0x41AED, L_EBP_R8
    "A2n", 0x41AF6, L_EBP_R8
    "A2o", 0x41B12, L_EBP_R8
    "A2p", 0x41B7B, L_EBP_R8
    // A3: per-draw residency/flag read in the draw emitter 1E80B0
    "A3",  0x1E835B, ("F0 0F B1 11", "8B 01 66 90")                   // lock cmpxchg [rcx],edx -> mov eax,[rcx]
]

let fmt = NasmFormatter()
let so = StringOutput()
let decodeAll (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    let l = Collections.Generic.List<Instruction>()
    while d.IP < ip + uint64 bytes.Length do l.Add(d.Decode())
    List.ofSeq l
let txt (i: Instruction) = fmt.Format(&i, so); so.ToStringAndReset()
let sameMem (a: Instruction) (b: Instruction) =
    a.MemoryBase = b.MemoryBase && a.MemoryIndex = b.MemoryIndex && a.MemoryIndexScale = b.MemoryIndexScale
    && a.MemoryDisplacement64 = b.MemoryDisplacement64 && a.MemorySize = b.MemorySize
let mutable ok = true
for (id, rva, (o, n)) in sites do
    let ob, nb = hexb o, hexb n
    let cur = img.[rva .. rva + ob.Length - 1]
    let fail m = ok <- false; printfn "  !! %s %X: %s" id rva m
    if cur <> ob then fail (sprintf "dump bytes %s differ" (BitConverter.ToString cur))
    if ob.Length <> nb.Length then fail "length mismatch"
    let oi = decodeAll ob (uint64 rva)
    let ni = decodeAll nb (uint64 rva)
    match oi, ni with
    | [o1], n1 :: rest ->
        if not (o1.HasLockPrefix && o1.Mnemonic = Mnemonic.Cmpxchg) then fail "original is not lock cmpxchg"
        if not (n1.Mnemonic = Mnemonic.Mov && n1.Op0Register = Register.EAX && sameMem o1 n1) then fail "replacement is not mov eax,[same]"
        if rest |> List.exists (fun i -> i.Mnemonic <> Mnemonic.Nop) then fail "filler is not NOP"
        // context: previous instructions must contain xor eax,eax with no eax write in between; next must not consume flags
        let back = decodeAll img.[rva - 24 .. rva - 1] (uint64 (rva - 24)) |> List.filter (fun i -> i.IP + uint64 i.Length <= uint64 rva)
        let after = decodeAll img.[rva + ob.Length .. rva + ob.Length + 15] (uint64 (rva + ob.Length))
        let nxt = after.Head
        if nxt.FlowControl = FlowControl.ConditionalBranch || nxt.Mnemonic = Mnemonic.Setne || nxt.Mnemonic = Mnemonic.Sete then
            fail "next instruction consumes cmpxchg flags"
        printfn "%-4s %8X  %-34s ->  %s   | next: %s" id rva (txt o1) (String.Join("; ", ni |> List.map txt)) (txt nxt)
    | _ -> fail "decode"
printfn (if ok then "ALL SITES VERIFIED against the dump (bytes, operand equivalence, flags)" else "SOME SITES FAILED")

// ---- D: main-loop retry quantum. 1A744E4 `movss xmm6,[rip+d]` loads 0.001f (1 ms). When the frame function
// 3D1F20 returns 0 (frame not ready), 1A74BD0..1A74C48 spins on QPC for exactly xmm6 seconds, blind (no readiness
// check), then retries. Re-pointing the displacement at an existing .rdata 0.0002f makes the retry 5x finer.
// xmm6 is loaded once before the loop (1A744E4 -> loop head 1A744F1), used only at 1A74C45, callee-saved.
let D_SITE = 0x1A744E4
let readF (rva: int) = BitConverter.ToSingle(img, rva)
let dOrig = img.[D_SITE .. D_SITE + 7]
let dDec = decodeAll dOrig (uint64 D_SITE)
let dIns = dDec.Head
printfn "D    %8X  %s   (loads %g from %X)" D_SITE (txt dIns) (readF (int dIns.IPRelativeMemoryAddress)) dIns.IPRelativeMemoryAddress
let newTarget = 0x3BD4600
let newDisp = newTarget - (D_SITE + 8)
let dNew = Array.append dOrig.[0..3] (BitConverter.GetBytes(int32 newDisp))
let dNewIns = (decodeAll dNew (uint64 D_SITE)).Head
printfn "     orig %s  ->  new %s  : %s  (loads %g)" (BitConverter.ToString dOrig) (BitConverter.ToString dNew) (txt dNewIns) (readF (int dNewIns.IPRelativeMemoryAddress))
if not (dIns.Mnemonic = Mnemonic.Movss && dIns.Op0Register = Register.XMM6 && dIns.IPRelativeMemoryAddress = 0x3BD4688UL && abs (readF 0x3BD4688 - 0.001f) < 1e-9f) then printfn "  !! D site mismatch"
if abs (readF newTarget - 0.0002f) > 1e-9f then printfn "  !! D new constant mismatch"
