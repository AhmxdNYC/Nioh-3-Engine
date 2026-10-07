// Read or set the live REN_FPS tier (config int array entry at RVA 0x45BAF54; {30,60,120}[idx]).
//   dotnet fsi tier.fsx            -> print current index
//   dotnet fsi tier.fsx set <idx>  -> write it (0=30, 1=60, 2=120)
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let addr = t.Base + 0x45BAF54n
let args = fsi.CommandLineArgs |> Array.skip 1
if args.Length = 2 && args.[0] = "set" then
    t.Write(addr, BitConverter.GetBytes(int args.[1]))
printfn "REN_FPS index = %d" (BitConverter.ToInt32(t.Read(addr, 4), 0))
