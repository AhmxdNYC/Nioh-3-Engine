// Read-only: where do W2's cold paths jump back to? (must not land inside a rewritten range)
#load "Mem.fsx"
open Mem
let t = attach "Nioh3"
for rva in [ 0x190A2B4; 0x190A2C0; 0x190A2CC; 0x13734F ] do
    printfn "==== cold %X" rva
    disasm t (t.Base + nativeint rva) 0x30
printfn "==== tail after 1372FE"
disasm t (t.Base + 0x137317n) 0x40
