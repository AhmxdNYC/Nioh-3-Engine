// coreaff.fsx — give the main game thread ([App]Game) a whole physical core to itself (no SMT sibling contention).
//
//   dotnet fsi coreaff.fsx status            show topology, the main thread and current masks (read-only)
//   dotnet fsi coreaff.fsx apply [core=0]    main thread -> first logical CPU of that physical core; every other game
//                                            thread -> all logical CPUs EXCEPT that core's (both SMT siblings)
//   dotnet fsi coreaff.fsx revert            every game thread -> the process affinity mask (stock)
//
// Live, nothing written to the game's memory or files; gone when the game exits. Threads created after `apply`
// keep the default mask (run apply again to include them).
open System
open System.Diagnostics
open System.Runtime.InteropServices

module N =
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenThread(uint32 access, bool inh, int tid)
    [<DllImport("kernel32.dll")>]
    extern bool CloseHandle(nativeint h)
    [<DllImport("kernel32.dll")>]
    extern int GetThreadDescription(nativeint h, nativeint& s)
    [<DllImport("kernel32.dll")>]
    extern nativeint LocalFree(nativeint p)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern unativeint SetThreadAffinityMask(nativeint h, unativeint mask)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool GetLogicalProcessorInformation(nativeint buf, uint32& len)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool GetProcessAffinityMask(nativeint h, unativeint& proc, unativeint& sys)

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "status"
let coreIdx = if args.Length > 1 then int args.[1] else 0

// physical cores -> logical-CPU masks (RelationProcessorCore = 0); x64 record = 32 bytes
let cores =
    let mutable len = 0u
    N.GetLogicalProcessorInformation(0n, &len) |> ignore
    let buf = Marshal.AllocHGlobal(int len)
    try
        if not (N.GetLogicalProcessorInformation(buf, &len)) then failwith "GetLogicalProcessorInformation failed"
        [ for i in 0 .. int len / 32 - 1 do
            let mask = uint64 (Marshal.ReadInt64(buf, i * 32))
            let rel = Marshal.ReadInt32(buf, i * 32 + 8)
            if rel = 0 then yield mask ]
    finally Marshal.FreeHGlobal buf
let p = Process.GetProcessesByName "Nioh3" |> Array.head
let mutable pmask, smask = 0un, 0un
N.GetProcessAffinityMask(p.Handle, &pmask, &smask) |> ignore
let desc (h: nativeint) =
    let mutable s = 0n
    if N.GetThreadDescription(h, &s) >= 0 && s <> 0n then
        let r = Marshal.PtrToStringUni s
        N.LocalFree s |> ignore
        r
    else ""
let threads =
    [ for t in p.Threads |> Seq.cast<ProcessThread> do
        let h = N.OpenThread(0x0060u ||| 0x0400u ||| 0x0800u ||| 0x1000u, false, t.Id)  // SET_INFORMATION|QUERY_INFORMATION|QUERY_LIMITED|SET_LIMITED
        if h <> 0n then yield t.Id, h, desc h ]
let main = threads |> List.tryFind (fun (_, _, d) -> d = "[App]Game")
printfn "process mask %X; %d physical cores: %s" (uint64 pmask) cores.Length (cores |> List.map (sprintf "%X") |> String.concat " ")
printfn "%d threads; main thread %s" threads.Length (match main with Some (id, _, _) -> string id | None -> "NOT FOUND")
match mode with
| "status" -> ()
| "apply" ->
    let main = match main with Some m -> m | None -> failwith "main thread not found"
    let coreMask = cores.[coreIdx]
    let first = coreMask &&& (~~~coreMask + 1UL)          // lowest set bit
    let others = uint64 pmask &&& ~~~coreMask
    let mutable ok, fail = 0, 0
    for (id, h, _) in threads do
        let m = if id = (let (a, _, _) = main in a) then first else others
        if N.SetThreadAffinityMask(h, unativeint m) <> 0un then ok <- ok + 1 else fail <- fail + 1
    printfn "applied: main -> %X, %d others -> %X (ok %d, failed %d)" first (threads.Length - 1) others ok fail
| "revert" ->
    let mutable ok = 0
    for (_, h, _) in threads do
        if N.SetThreadAffinityMask(h, pmask) <> 0un then ok <- ok + 1
    printfn "reverted %d threads to %X" ok (uint64 pmask)
| m -> printfn "unknown mode %s" m
for (_, h, _) in threads do N.CloseHandle h |> ignore
