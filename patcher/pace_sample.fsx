// Sample the live frame-pacing globals for N seconds: G+0x614 fps (int), +0x618/+0x620 tier, DYNAMIC_FPS cfg.
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let secs = match fsi.CommandLineArgs |> Array.tryItem 1 with Some s -> int s | None -> 8
let g = nativeint (BitConverter.ToInt64(t.Read(t.Base + 0x4759910n, 8), 0))
let i32 (a: nativeint) = BitConverter.ToInt32(t.Read(a, 4), 0)
let counts = Collections.Generic.Dictionary<string,int>()
for _ in 1 .. secs * 20 do
    let k = sprintf "fps614=%d t618=%d t620=%d" (i32 (g + 0x614n)) (i32 (g + 0x618n)) (i32 (g + 0x620n))
    counts.[k] <- (match counts.TryGetValue k with | true, v -> v + 1 | _ -> 1)
    Threading.Thread.Sleep 50
printfn "DYNAMIC_FPS cfg=%d REN_FPS idx=%d" (i32 (t.Base + 0x45BAF58n)) (i32 (t.Base + 0x45BAF54n))
for kv in counts |> Seq.sortByDescending (fun kv -> kv.Value) |> Seq.truncate 8 do printfn "  %4d x %s" kv.Value kv.Key
