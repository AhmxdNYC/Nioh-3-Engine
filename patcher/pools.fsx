// pools.fsx — read the live worker-pool table (static object Nioh3+0x4B722D0; entry i at +0xC + i*0x18: u8 count, i32 weight at +4). READ-ONLY.
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let o = t.Base + 0x4B722D0n
let b = t.Read(o, 0xC + 74 * 0x18)
printfn "workers base (+0x740) = %d" (t.Read(o + 0x740n, 1)).[0]
for i in [3; 5; 6; 9; 10; 11] do
    let e = 0xC + i * 0x18
    printfn "  pool %2d  threads=%d weight=%d" i b.[e] (BitConverter.ToInt32(b, e + 4))
