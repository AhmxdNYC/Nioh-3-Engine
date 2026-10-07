#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let a = t.Base + 0x45BAF58n
let v = int fsi.CommandLineArgs.[1]
printfn "DYNAMIC_FPS before=%d" (BitConverter.ToInt32(t.Read(a, 4), 0))
t.Write(a, BitConverter.GetBytes v)
printfn "DYNAMIC_FPS after=%d" (BitConverter.ToInt32(t.Read(a, 4), 0))
