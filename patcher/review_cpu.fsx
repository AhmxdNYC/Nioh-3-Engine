// Read-only: disassemble the W1/W2 hot functions to verify the proposed in-place rewrites.
#load "Mem.fsx"
open Mem
let t = attach "Nioh3"
printfn "==== W1 fn 61B1C"
disasm t (t.Base + 0x61B1Cn) 0xE0
printfn "==== W2 fn 1371D0"
disasm t (t.Base + 0x1371D0n) 0x150
