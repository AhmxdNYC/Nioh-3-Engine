// ripprobe.fsx — sample one thread until RIP is inside [lo,hi) (RVAs), then dump registers and the memory
// type/protection of every register that points into committed memory (detects write-combined/uncached targets).
//   dotnet fsi ripprobe.fsx <tid> <loRva> <hiRva> [maxSeconds=10] [hits=5]
open System
open System.Diagnostics
open System.Runtime.InteropServices

module N =
    [<DllImport("kernel32.dll", SetLastError = true)>] extern nativeint OpenProcess(uint32 a, bool i, int pid)
    [<DllImport("kernel32.dll", SetLastError = true)>] extern nativeint OpenThread(uint32 a, bool i, int tid)
    [<DllImport("kernel32.dll", SetLastError = true)>] extern uint32 SuspendThread(nativeint h)
    [<DllImport("kernel32.dll", SetLastError = true)>] extern uint32 ResumeThread(nativeint h)
    [<DllImport("kernel32.dll", SetLastError = true)>] extern bool GetThreadContext(nativeint h, nativeint ctx)
    [<DllImport("kernel32.dll", SetLastError = true)>] extern nativeint VirtualQueryEx(nativeint h, nativeint a, byte[] mbi, nativeint len)

let a = fsi.CommandLineArgs |> Array.skip 1
let tid = int a.[0]
let lo, hi = Convert.ToUInt64(a.[1], 16), Convert.ToUInt64(a.[2], 16)
let maxS = if a.Length > 3 then float a.[3] else 10.0
let want = if a.Length > 4 then int a.[4] else 5
let p = Process.GetProcessesByName("Nioh3").[0]
let b = uint64 (p.MainModule.BaseAddress.ToInt64())
let hp = N.OpenProcess(0x0410u, false, p.Id)
let ht = N.OpenThread(0x004Au, false, tid)
if ht = 0n then failwith "OpenThread failed"
let mem = Marshal.AllocHGlobal(0x4D0 + 64)
let ctx = nativeint ((int64 mem + 15L) &&& ~~~15L)
let names = [| "rax"; "rcx"; "rdx"; "rbx"; "rsp"; "rbp"; "rsi"; "rdi"; "r8"; "r9"; "r10"; "r11"; "r12"; "r13"; "r14"; "r15" |]
let protStr (pr: uint32) =
    let basep = match pr &&& 0xFFu with 1u -> "NOACCESS" | 2u -> "R" | 4u -> "RW" | 8u -> "WC(copy)" | 0x10u -> "X" | 0x20u -> "RX" | 0x40u -> "RWX" | x -> sprintf "%X" x
    basep + (if pr &&& 0x100u <> 0u then "+GUARD" else "") + (if pr &&& 0x200u <> 0u then "+NOCACHE" else "") + (if pr &&& 0x400u <> 0u then "+WRITECOMBINE" else "")
let query (addr: uint64) =
    let mbi = Array.zeroCreate<byte> 48
    if N.VirtualQueryEx(hp, nativeint addr, mbi, 48n) = 0n then None else
    let state = BitConverter.ToUInt32(mbi, 32)
    let prot = BitConverter.ToUInt32(mbi, 36)
    let typ = BitConverter.ToUInt32(mbi, 40)
    if state <> 0x1000u then None
    else Some (sprintf "%s %s base=%X size=%X" (protStr prot) (match typ with 0x20000u -> "PRIVATE" | 0x40000u -> "MAPPED" | 0x1000000u -> "IMAGE" | _ -> "?")
                    (BitConverter.ToUInt64(mbi, 0)) (BitConverter.ToUInt64(mbi, 24)))
let sw = Stopwatch.StartNew()
let mutable hits, tries = 0, 0
let regs = Array.zeroCreate<uint64> 16
while hits < want && sw.Elapsed.TotalSeconds < maxS do
    tries <- tries + 1
    let mutable rip = 0UL
    if N.SuspendThread ht <> 0xFFFFFFFFu then
        try
            Marshal.WriteInt32(ctx, 0x30, 0x100003)
            if N.GetThreadContext(ht, ctx) then
                rip <- uint64 (Marshal.ReadInt64(ctx, 0xF8))
                for i in 0 .. 15 do regs.[i] <- uint64 (Marshal.ReadInt64(ctx, 0x78 + 8 * i))
        finally N.ResumeThread ht |> ignore
    if rip >= b + lo && rip < b + hi then
        hits <- hits + 1
        printfn "hit %d after %d tries: rip=Nioh3+%X" hits tries (rip - b)
        for i in 0 .. 15 do
            if i <> 4 then
                match query regs.[i] with
                | Some s -> printfn "   %-3s %016X  %s" names.[i] regs.[i] s
                | None -> ()
    else Threading.Thread.Sleep 1
printfn "done: %d hits / %d tries" hits tries
