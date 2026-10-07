// memscan.fsx â€” search all committed, readable memory of Nioh3 for a byte pattern (hex, ?? = any). READ-ONLY.
//   dotnet fsi memscan.fsx "98 08 00 00 96 00 00 00 40 1F 00 00 02 00 00 00" [maxHits]
#load "Mem.fsx"
open System
open System.Runtime.InteropServices
open Mem

[<Struct; StructLayout(LayoutKind.Sequential)>]
type MBI =
    val mutable BaseAddress: nativeint
    val mutable AllocationBase: nativeint
    val mutable AllocationProtect: uint32
    val mutable PartitionId: uint16
    val mutable RegionSize: nativeint
    val mutable State: uint32
    val mutable Protect: uint32
    val mutable Type: uint32
[<DllImport("kernel32.dll")>]
extern nativeint VirtualQueryEx(nativeint h, nativeint addr, MBI& mbi, nativeint len)

let pat = fsi.CommandLineArgs.[1].Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun s -> if s.StartsWith "?" then -1 else Convert.ToInt32(s, 16))
let privOnly = Environment.GetEnvironmentVariable "SCAN_PRIVATE" = "1"
let maxHits = match fsi.CommandLineArgs |> Array.tryItem 2 with Some s -> int s | None -> 50
let t = attach "Nioh3"
let B = int64 t.Base
let modEnd = B + int64 t.ModuleSize
let first = pat.[0]
let mutable addr = 0n
let mutable hits = 0
let mutable scanned = 0L
let mutable mbi = MBI()
let sw = Diagnostics.Stopwatch.StartNew()
while hits < maxHits && VirtualQueryEx(t.Handle, addr, &mbi, nativeint (Marshal.SizeOf<MBI>())) <> 0n do
    let readable = mbi.State = 0x1000u && (mbi.Protect &&& 0x100u) = 0u && (mbi.Protect &&& 0xEEu) <> 0u && (privOnly = false || (mbi.Type = 0x20000u && int64 mbi.RegionSize <= (512L <<< 20)))
    if readable then
        let size = int64 mbi.RegionSize
        let chunk = 16L <<< 20
        let mutable off = 0L
        while off < size && hits < maxHits do
            let n = int (min (chunk + int64 pat.Length) (size - off))
            let buf = try t.Read(mbi.BaseAddress + nativeint off, n) with _ -> [||]
            scanned <- scanned + int64 buf.Length
            let mutable i = if first >= 0 then Array.IndexOf(buf, byte first) else 0
            while i >= 0 && i <= buf.Length - pat.Length && hits < maxHits do
                let mutable j = 0
                while j < pat.Length && (pat.[j] = -1 || int buf.[i + j] = pat.[j]) do j <- j + 1
                if j = pat.Length && int64 i < chunk then
                    let a = int64 mbi.BaseAddress + off + int64 i
                    let where = if a >= B && a < modEnd then sprintf "Nioh3+%X" (a - B) else sprintf "heap (alloc %X, type %X)" (int64 mbi.AllocationBase) mbi.Type
                    let ctx = t.Read(nativeint (a - 0x20L), 0x60)
                    printfn "hit %X  %s\n   %s" a where (BitConverter.ToString(ctx).Replace("-", " "))
                    hits <- hits + 1
                i <- if first >= 0 then (if i + 1 < buf.Length then Array.IndexOf(buf, byte first, i + 1) else -1) else i + 1
            off <- off + chunk
    addr <- mbi.BaseAddress + mbi.RegionSize
printfn "scanned %.1f GB in %.1f s, %d hits" (float scanned / 1e9) sw.Elapsed.TotalSeconds hits
