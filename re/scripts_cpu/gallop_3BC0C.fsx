// gallop_3BC0C.fsx - design + offline test of a galloping replacement for the group-length binary search
// Nioh3+3BC0C (called from the draw-list walker 39E20, 86-94% from the "skip group" path @3A51A).
//
//   dotnet fsi gallop_3BC0C.fsx            assemble the cave, run original vs patched on synthetic sorted lists
//
// NOTHING here touches the game. The original 150 bytes are copied from re\Nioh3.dump.exe into this
// process's own executable memory and called through a delegate, next to the patched version, on random
// inputs; the outputs must match exactly.
//
// Function semantics (from decompile + asm):
//   void GroupLen(void* unused, List* L /*rdx*/, Obj* cur /*r8*/, uint s /*r9d*/, uint n /*[rsp+28h]*/, int* out /*[rsp+30h]*/)
//   key(i) = *(u64*)(*(u64*)L->arr[i] + 0x18); K = cur->+0x18
//   type==3 (sorted list): branchless binary search for the first i>=s with key(i)!=K  -> ~log2(n) dependent
//                          pointer-chasing probes (each probe = 2 likely cache misses)
//   else: linear scan.            *out = e - s + 1
// Patch: at 3BC17 (first instruction of the type-3 path, `mov r10d,[rsp+28h]`, 5 bytes) -> `jmp cave`.
// Cave: probe s, then exponential (galloping) probes s+1, s+3, s+7 ..., then binary search inside the last
// bracket. Same result whenever the list is sorted by key (which the original binary search already assumes),
// O(log g) probes for a group of g instead of O(log n). The linear (type != 3) path is untouched.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Runtime.InteropServices
open Iced.Intel
open type Iced.Intel.AssemblerRegisters

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let FN = 0x3BC0C
let FN_LEN = 150
let SITE = 0x3BC17          // `44 8B 54 24 28  mov r10d,[rsp+28h]`
let TAIL = 0x3BC5B          // `sub r10d,r9d; inc r10d; mov rax,[rsp+30h]; mov [rax],r10d; mov rbx,[rsp+8]; ret`
let orig = img.[FN .. FN + FN_LEN - 1]
if img.[SITE .. SITE + 4] <> [| 0x44uy; 0x8Buy; 0x54uy; 0x24uy; 0x28uy |] then failwith "unexpected bytes at 3BC17"

/// Cave body. Entered by `jmp` from 3BC17: rbx already saved at [rsp+8], rdx=list, r8=cur obj, r9d=s.
let buildCave (caveAddr: uint64) =
    let a = Assembler(64)
    let mutable gal = a.CreateLabel()
    let mutable bs = a.CreateLabel()
    let mutable bsN = a.CreateLabel()
    let mutable found = a.CreateLabel()
    let mutable eIsS = a.CreateLabel()
    let mutable tail = a.CreateLabel()
    a.mov(r10d, __dword_ptr.[rsp + 0x28L])     // n
    a.cmp(r9d, r10d)
    a.jae(tail)                               // s >= n : e = n (same as original)
    a.mov(r11, __qword_ptr.[r8 + 0x18L])       // K
    a.mov(rbx, __qword_ptr.[rdx])             // arr
    a.mov(eax, r9d)
    a.mov(rcx, __qword_ptr.[rbx + rax * 8])
    a.mov(rcx, __qword_ptr.[rcx])
    a.cmp(r11, __qword_ptr.[rcx + 0x18L])
    a.jne(eIsS)                               // group ends right after cur (most common case)
    a.mov(r8d, r9d)                           // lo = last index known == K
    a.mov(edx, 1)                             // step
    a.Label(&gal)
    a.lea(eax, __dword_ptr.[r8 + rdx])        // probe = lo + step
    a.cmp(eax, r10d)
    a.jae(bsN)
    a.mov(rcx, __qword_ptr.[rbx + rax * 8])
    a.mov(rcx, __qword_ptr.[rcx])
    a.cmp(r11, __qword_ptr.[rcx + 0x18L])
    a.jne(bs)
    a.mov(r8d, eax)
    a.add(edx, edx)
    a.jmp(gal)
    a.Label(&bsN)
    a.mov(eax, r10d)                          // hi = n
    a.Label(&bs)                              // key(lo)==K ; hi==n or key(hi)!=K ; find first != in (lo,hi]
    a.mov(ecx, eax)
    a.sub(ecx, r8d)
    a.cmp(ecx, 1)
    a.jbe(found)
    a.shr(ecx, 1uy)
    a.add(ecx, r8d)                           // mid
    a.mov(rdx, __qword_ptr.[rbx + rcx * 8])
    a.mov(rdx, __qword_ptr.[rdx])
    a.cmp(r11, __qword_ptr.[rdx + 0x18L])
    a.cmove(r8d, ecx)
    a.cmovne(eax, ecx)
    a.jmp(bs)
    a.Label(&found)
    a.mov(r10d, eax)
    a.jmp(tail)
    a.Label(&eIsS)
    a.mov(r10d, r9d)
    a.Label(&tail)                            // identical to original tail at 3BC5B
    a.sub(r10d, r9d)
    a.inc(r10d)
    a.mov(rax, __qword_ptr.[rsp + 0x30L])
    a.mov(__dword_ptr.[rax], r10d)
    a.mov(rbx, __qword_ptr.[rsp + 8L])
    a.ret()
    use ms = new MemoryStream()
    a.Assemble(StreamCodeWriter(ms), caveAddr) |> ignore
    ms.ToArray()

// ---- native harness (own process only) --------------------------------------------------------------
[<DllImport("kernel32.dll")>] extern nativeint VirtualAlloc(nativeint addr, unativeint size, uint32 typ, uint32 prot)
[<UnmanagedFunctionPointer(CallingConvention.Winapi)>]
type GroupLenFn = delegate of int64 * nativeint * nativeint * uint32 * uint32 * nativeint -> unit

let exec = VirtualAlloc(0n, 0x1000un, 0x3000u, 0x40u)   // RWX, this process
let origAddr = exec
let patAddr = exec + 0x200n
let caveAddr = exec + 0x400n
Marshal.Copy(orig, 0, origAddr, FN_LEN)
Marshal.Copy(orig, 0, patAddr, FN_LEN)
let cave = buildCave (uint64 caveAddr)
Marshal.Copy(cave, 0, caveAddr, cave.Length)
// site: jmp rel32 at patAddr + (SITE-FN)
let siteAddr = patAddr + nativeint (SITE - FN)
let rel = int32 (int64 caveAddr - (int64 siteAddr + 5L))
Marshal.WriteByte(siteAddr, 0xE9uy)
Marshal.WriteInt32(siteAddr + 1n, rel)
let fOrig = Marshal.GetDelegateForFunctionPointer<GroupLenFn>(origAddr)
let fPat = Marshal.GetDelegateForFunctionPointer<GroupLenFn>(patAddr)

printfn "cave: %d bytes" cave.Length
let fmt = NasmFormatter()
let so = StringOutput()
let dec = Decoder.Create(64, ByteArrayCodeReader(cave))
dec.IP <- uint64 caveAddr
while dec.IP < uint64 caveAddr + uint64 cave.Length do
    let ins = dec.Decode()
    fmt.Format(&ins, so)
    printfn "  +%03X  %s" (ins.IP - uint64 caveAddr) (so.ToStringAndReset())

let rnd = Random(12345)
let mutable tests = 0
let mutable bad = 0
for round in 1 .. 400 do
    let n = rnd.Next(1, 3000)
    let maxRun = [| 1; 2; 3; 8; 40; 400 |].[rnd.Next 6]
    // keys: sorted runs
    let keys = Array.zeroCreate<int64> n
    let mutable i = 0
    let mutable k = 1000L
    while i < n do
        let r = rnd.Next(1, maxRun + 1)
        for j in i .. min (n - 1) (i + r - 1) do keys.[j] <- k
        i <- i + r
        k <- k + int64 (rnd.Next(1, 5))
    // objects: [obj+18h] = key ; items: [item] = obj ; arr[i] = item
    let objs = Marshal.AllocHGlobal(n * 0x20)
    let items = Marshal.AllocHGlobal(n * 8)
    let arr = Marshal.AllocHGlobal(n * 8)
    for j in 0 .. n - 1 do
        Marshal.WriteInt64(objs + nativeint (j * 0x20 + 0x18), keys.[j])
        Marshal.WriteIntPtr(items + nativeint (j * 8), objs + nativeint (j * 0x20))
        Marshal.WriteIntPtr(arr + nativeint (j * 8), items + nativeint (j * 8))
    let list = Marshal.AllocHGlobal(0x20)
    Marshal.WriteIntPtr(list, arr)
    Marshal.WriteInt32(list + 0x10n, 3)
    let out = Marshal.AllocHGlobal(8)
    for _ in 1 .. 60 do
        let cur = rnd.Next(0, n)
        let s = uint32 (cur + 1)
        let nn = if rnd.Next(10) = 0 then uint32 (rnd.Next(cur + 1, n + 1)) else uint32 n   // sometimes a shorter range
        let curObj = objs + nativeint (cur * 0x20)
        Marshal.WriteInt32(out, -7)
        fOrig.Invoke(0L, list, curObj, s, nn, out)
        let a = Marshal.ReadInt32 out
        Marshal.WriteInt32(out, -9)
        fPat.Invoke(0L, list, curObj, s, nn, out)
        let b = Marshal.ReadInt32 out
        tests <- tests + 1
        if a <> b then
            bad <- bad + 1
            if bad < 10 then printfn "MISMATCH n=%d cur=%d nn=%d orig=%d new=%d" n cur nn a b
    Marshal.FreeHGlobal objs; Marshal.FreeHGlobal items; Marshal.FreeHGlobal arr; Marshal.FreeHGlobal list; Marshal.FreeHGlobal out
printfn "%d tests, %d mismatches" tests bad
// probe-count model (log2 n vs 1 + 2*log2 g) for the write-up
for (n, g) in [ (2000, 1); (2000, 2); (2000, 4); (2000, 16); (2000, 200); (20000, 1) ] do
    let binary = int (ceil (Math.Log2(float n)))
    let gallop = if g <= 1 then 1 else 1 + 2 * int (ceil (Math.Log2(float g + 1.0)))
    printfn "n=%5d group=%3d : binary ~%2d probes, gallop ~%2d probes" n g binary gallop
