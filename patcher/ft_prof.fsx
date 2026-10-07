// Read-only: frame profile block of the frame task (r13 = [[inst+58h]+28h], obj = r13+8)
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let obj = Convert.ToInt64(fsi.CommandLineArgs.[1], 16)
let rd32 (a:int64) = BitConverter.ToInt32(t.Read(nativeint a, 4), 0)
let rdf (a:int64) = BitConverter.ToSingle(t.Read(nativeint a, 4), 0)
printfn "flags [+3BC7C]=%X" (rd32 (obj + 0x3BC7CL))
printfn "fps   [+416E0]=%d" (rd32 (obj + 0x416E0L))
let p = obj + 0x41668L
printfn "profile @+41668: +24=%X +2C=%X +44 dt=%f" (rd32 (p+0x24L)) (rd32 (p+0x2CL)) (rdf (p+0x44L))
let b = t.Read(nativeint p, 0x90)
for i in 0 .. 4 .. 0x8C do printfn "  +%02X  %08X  %g" i (BitConverter.ToUInt32(b,i)) (BitConverter.ToSingle(b,i))
printfn "game clock [r13+3BFBC] = %f" (rdf (obj - 8L + 0x3BFBCL))
