// Read-only: given an object address expression (global RVA holding the pointer), print vtable + RTTI class name.
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let b = int64 t.Base
let rvaPtr = Convert.ToInt64(fsi.CommandLineArgs.[1].Replace("0x",""), 16)
let extraOff = if fsi.CommandLineArgs.Length > 2 then Convert.ToInt64(fsi.CommandLineArgs.[2].Replace("0x",""),16) else 0L
let obj0 = t.ReadI64(nativeint (b + rvaPtr)) + extraOff
let obj = if extraOff <> 0L then t.ReadI64(nativeint obj0) else obj0
let vt = t.ReadI64(nativeint obj)
printfn "obj=%X vtable=%X (rva %X)" obj vt (vt - b)
let col = t.ReadI64(nativeint (vt - 8L))
printfn "COL=%X" (col - b)
if col > b && col < b + int64 t.ModuleSize then
    let tdRva = BitConverter.ToInt32(t.Read(nativeint (col + 12L), 4), 0)
    let name = t.Read(nativeint (b + int64 tdRva + 16L), 200)
    let s = Text.Encoding.ASCII.GetString(name)
    printfn "class: %s" (s.Substring(0, s.IndexOf '\000'))
for i in 0..40 do
    let f = t.ReadI64(nativeint (vt + int64 (i*8)))
    printfn "  vf[%d] (+%X) = %X" i (i*8) (f - b)
