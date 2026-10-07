// Read-only: snapshot Nioh3 .data a few times and report per-frame counters and dt-like floats.
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let dataRva, dataSz = 0x44b4000, 0x973eb8
let snap () = t.Read(t.Base + nativeint dataRva, dataSz)
let sw = Diagnostics.Stopwatch.StartNew()
let snaps = [| for i in 0..4 do
                 let s = snap ()
                 let ts = sw.Elapsed.TotalSeconds
                 Threading.Thread.Sleep 500
                 yield (ts, s) |]
printfn "snap times: %A" (snaps |> Array.map fst)
let n = dataSz / 4
// int32 counters: monotonic increasing, rate per second
printfn "--- int32 counters (rate/s 20..400, monotonic)"
for i in 0 .. n-1 do
    let o = i*4
    let v = snaps |> Array.map (fun (_,s) -> BitConverter.ToInt32(s,o))
    let d = [| for k in 1..4 -> int64 v.[k] - int64 v.[k-1] |]
    if d |> Array.forall (fun x -> x > 0L) then
        let rate = float (int64 v.[4] - int64 v.[0]) / (fst snaps.[4] - fst snaps.[0])
        if rate >= 20.0 && rate <= 400.0 then
            printfn "%08X  %A  rate=%.1f/s" (dataRva+o) v rate
printfn "--- float32 in [0.003,0.07] (dt candidates)"
for i in 0 .. n-1 do
    let o = i*4
    let v = snaps |> Array.map (fun (_,s) -> BitConverter.ToSingle(s,o))
    if v |> Array.forall (fun x -> x >= 0.003f && x <= 0.07f) then
        let varies = v |> Array.distinct |> Array.length
        printfn "%08X  %s  distinct=%d" (dataRva+o) (String.Join(" ", v |> Array.map (sprintf "%.6f"))) varies
printfn "--- double in [0.003,0.07]"
for i in 0 .. dataSz/8 - 1 do
    let o = i*8
    let v = snaps |> Array.map (fun (_,s) -> BitConverter.ToDouble(s,o))
    if v |> Array.forall (fun x -> x >= 0.003 && x <= 0.07) then
        printfn "%08X  %s" (dataRva+o) (String.Join(" ", v |> Array.map (sprintf "%.6f")))
printfn "--- float/double time accumulators (increase ~0.5 per snap)"
for i in 0 .. n-1 do
    let o = i*4
    let v = snaps |> Array.map (fun (_,s) -> BitConverter.ToSingle(s,o))
    let d = [| for k in 1..4 -> v.[k] - v.[k-1] |]
    if d |> Array.forall (fun x -> x > 0.3f && x < 0.8f) then printfn "F %08X %A" (dataRva+o) v
for i in 0 .. dataSz/8-1 do
    let o = i*8
    let v = snaps |> Array.map (fun (_,s) -> BitConverter.ToDouble(s,o))
    let d = [| for k in 1..4 -> v.[k] - v.[k-1] |]
    if d |> Array.forall (fun x -> x > 0.3 && x < 0.8) then printfn "D %08X %A" (dataRva+o) v
