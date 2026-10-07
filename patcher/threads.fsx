// threads.fsx — count Nioh3 threads by description (KIDSTS[sN:PoolName] -> pool counts). READ-ONLY.
//   dotnet fsi threads.fsx [filter]
open System
open System.Diagnostics
open System.Runtime.InteropServices
open System.Text.RegularExpressions

[<DllImport("kernel32.dll", SetLastError = true)>]
extern nativeint OpenThread(uint32 access, bool inh, int tid)
[<DllImport("kernel32.dll")>]
extern int GetThreadDescription(nativeint h, nativeint& desc)
[<DllImport("kernel32.dll")>]
extern nativeint LocalFree(nativeint p)
[<DllImport("kernel32.dll")>]
extern bool CloseHandle(nativeint h)

let filter = fsi.CommandLineArgs |> Array.tryItem 1 |> Option.defaultValue ""
let p = Process.GetProcessesByName "Nioh3" |> Array.head
let names =
    [ for th in p.Threads |> Seq.cast<ProcessThread> do
        let h = OpenThread(0x1000u, false, th.Id)        // THREAD_QUERY_LIMITED_INFORMATION
        if h <> 0n then
            let mutable d = 0n
            if GetThreadDescription(h, &d) >= 0 && d <> 0n then
                yield Marshal.PtrToStringUni d
                LocalFree d |> ignore
            CloseHandle h |> ignore ]
let pool (n: string) = let m = Regex.Match(n, @"^KIDSTS\[s\d+:(.+)\]$") in if m.Success then m.Groups.[1].Value else n
printfn "threads: %d" names.Length
for (k, c) in names |> List.countBy pool |> List.sortBy fst do
    if filter = "" || k.Contains filter then printfn "  %3d  %s" c k
