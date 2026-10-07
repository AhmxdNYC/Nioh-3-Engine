// Read-only: hexdump qwords at an address expression: ft_peek.fsx ADDR [count]
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let a = Convert.ToInt64(fsi.CommandLineArgs.[1], 16)
let n = if fsi.CommandLineArgs.Length > 2 then int fsi.CommandLineArgs.[2] else 32
let b = t.Read(nativeint a, n*8)
let bs = int64 t.Base
for i in 0..n-1 do
    let v = BitConverter.ToInt64(b, i*8)
    let tag = if v >= bs && v < bs + int64 t.ModuleSize then sprintf "  (RVA %X)" (v-bs) else ""
    printfn "+%03X  %016X  f=%g,%g%s" (i*8) v (BitConverter.ToSingle(b,i*8)) (BitConverter.ToSingle(b,i*8+4)) tag
