// Read-only live frame-timing probe. usage: dotnet fsi ft_live.fsx [seconds=4]
// Samples G=[base+0x4759910] fields every ~1 ms and reports real frame rate, the fps used for dt,
// frame-start interval distribution and the implied game-speed ratio (game time / wall time).
#load "Mem.fsx"
open System
open Mem
let secs = if fsi.CommandLineArgs.Length > 1 then float fsi.CommandLineArgs.[1] else 4.0
let t = attach "Nioh3"
let g = nativeint (t.ReadI64(t.Base + 0x4759910n))
printfn "base=%X G=%X" t.Base g
let sw = Diagnostics.Stopwatch.StartNew()
let frames = Collections.Generic.List<int * int * int * int * int * float32 * float32 * int64 * int * float32 * double>()
let mutable lastCtr = -1
while sw.Elapsed.TotalSeconds < secs do
    let b = t.Read(g + 0x610n, 0x20)          // 610..62F
    let m = t.Read(g + 0x7D8n, 0x80)          // fps monitor
    let ctr = BitConverter.ToInt32(b, 0)
    if ctr <> lastCtr then
        lastCtr <- ctr
        frames.Add((ctr, BitConverter.ToInt32(b,4), BitConverter.ToInt32(b,8), BitConverter.ToInt32(b,0xC), BitConverter.ToInt32(b,0x10),
                    BitConverter.ToSingle(b,0x14), BitConverter.ToSingle(b,0x1C), BitConverter.ToInt64(m,0), BitConverter.ToInt32(m,0x70),
                    BitConverter.ToSingle(m,0x74), sw.Elapsed.TotalMilliseconds))
    Threading.Thread.SpinWait 2000
let f = frames.ToArray()
let n = f.Length
let c0,_,_,_,_,_,_,_,_,_,t0 = f.[0]
let c1,_,_,_,_,_,_,_,_,_,t1 = f.[n-1]
printfn "observed %d frame-counter changes, counter advanced %d in %.1f ms -> %.2f real frames/s" n (c1-c0) (t1-t0) (float (c1-c0) * 1000.0 / (t1-t0))
let hist sel = f |> Array.countBy sel |> Array.sortBy fst
printfn "fps used for dt [614]: %A" (hist (fun (_,a,_,_,_,_,_,_,_,_,_) -> a))
printfn "target [618]: %A   prev [61C]: %A   raw [620]: %A" (hist (fun (_,_,a,_,_,_,_,_,_,_,_) -> a)) (hist (fun (_,_,_,a,_,_,_,_,_,_,_) -> a)) (hist (fun (_,_,_,_,a,_,_,_,_,_,_) -> a))
printfn "scale [624]: %A   dyn target (mon+70): %A" (hist (fun (_,_,_,_,_,s,_,_,_,_,_) -> s)) (hist (fun (_,_,_,_,_,_,_,_,d,_,_) -> d))
// frame-start deltas (ns) between consecutive observed frames that are exactly +1 apart
let deltas = [| for i in 1..n-1 do
                  let (ca,_,_,_,_,_,_,ta,_,_,_) = f.[i-1]
                  let (cb,_,_,_,_,_,_,tb,_,_,_) = f.[i]
                  if cb = ca + 1 && tb > ta then yield float (tb - ta) / 1e6 |]
if deltas.Length > 0 then
    let s = Array.sort deltas
    let pct p = s.[min (s.Length-1) (int (float s.Length * p))]
    printfn "frame-start interval ms: n=%d mean=%.3f p1=%.3f p5=%.3f p50=%.3f p95=%.3f p99=%.3f" s.Length (Array.average s) (pct 0.01) (pct 0.05) (pct 0.5) (pct 0.95) (pct 0.99)
    let buckets = s |> Array.countBy (fun d -> Math.Round(d * 2.0) / 2.0) |> Array.sortBy fst
    printfn "interval histogram (0.5 ms bins): %A" buckets
// game speed: sum over frames of scale/fps vs wall
let gameSec = f |> Array.pairwise |> Array.sumBy (fun ((ca,_,_,_,_,_,_,_,_,_,_),(cb,fps,_,_,_,sc,_,_,_,_,_)) -> float (cb-ca) * float sc / float (max fps 1))
printfn "implied game-time/wall-time = %.3f" (gameSec * 1000.0 / (t1 - t0))
printfn "avg CPU frame ms (mon+74): %.2f" (f |> Array.averageBy (fun (_,_,_,_,_,_,_,_,_,a,_) -> float a))
