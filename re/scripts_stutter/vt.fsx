// vt.fsx - print N qword entries of a vtable (RVA) in the dump as RVAs.  dotnet fsi vt.fsx <rvaHex> [n=12]
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open Wj_common
let a = fsi.CommandLineArgs |> Array.skip 1
let r = Convert.ToInt32(a.[0], 16)
let n = if a.Length > 1 then int a.[1] else 12
let ib = BitConverter.ToUInt64(img, optOff + 24)
for i in 0 .. n - 1 do
    let v = BitConverter.ToUInt64(img, r + i * 8)
    printfn "+%02X  %X" (i * 8) (if v >= ib then v - ib else v)
