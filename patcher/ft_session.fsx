// Read-only: online-session state used by Online::IsOnlinePlay (0x85834) and IsPlayerGuest (0x2853D4).
#load "Mem.fsx"
open System
open Mem
let t = attach "Nioh3"
let b = t.Base
let rd32 (a: nativeint) = BitConverter.ToInt32(t.Read(a, 4), 0)
let net = t.ReadI64(b + 0x4709B50n)
printfn "net session obj [0x4709B50] = %X" net
if net <> 0L then printfn "  +0x34 mode = %d   (IsOnlinePlay: 1 or 2 = online; 2 = the fn 0x85948 case)" (rd32 (nativeint net + 0x34n))
let mm = t.ReadI64(b + 0x45C84F8n)
printfn "mission/online obj [0x45C84F8] = %X" mm
if mm <> 0L then
    let r = t.Read(nativeint mm + 0x1DCn, 1).[0]
    let g = t.Read(nativeint mm + 0x2C8n, 1).[0]
    printfn "  +0x1DC=%d  +0x2C8=%d   (IsPlayerGuest = +1DC && !+2C8)" r g
