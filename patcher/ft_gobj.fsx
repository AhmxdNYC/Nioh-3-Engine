// Read-only: sample the frame/time manager object G=[base+0x4759910] and print changing fields.
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let gp = t.ReadI64(t.Base + 0x4759910n)
printfn "G = %X" gp
let k = BitConverter.ToSingle(t.Read(t.Base + 0x3BD4080n, 4), 0)
printfn "const 3BD4080 = %f" k
let lo, n = 0x0, 0x800
let N = if fsi.CommandLineArgs.Length > 1 then int fsi.CommandLineArgs.[1] else 8
let dly = if fsi.CommandLineArgs.Length > 2 then int fsi.CommandLineArgs.[2] else 7
let snaps = [| for i in 0..N-1 -> (let b = t.Read(nativeint gp + nativeint lo, n) in Threading.Thread.Sleep dly; b) |]
for o in 0 .. 4 .. n-4 do
    let iv = snaps |> Array.map (fun s -> BitConverter.ToInt32(s, o))
    let fv = snaps |> Array.map (fun s -> BitConverter.ToSingle(s, o))
    let distinct = iv |> Array.distinct |> Array.length
    let f0 = fv.[0]
    let looksF = Single.IsFinite f0 && abs f0 > 1e-6f && abs f0 < 1e7f
    let show = (o >= 0x580 && o <= 0x720) || distinct > 1
    if show && iv.[0] <> 0 || distinct > 1 then
        let fs = if looksF then String.Join(" ", fv |> Array.map (sprintf "%.5g")) else ""
        printfn "+%03X  i=%s  %s" (lo+o) (String.Join(" ", iv |> Array.map string)) fs
