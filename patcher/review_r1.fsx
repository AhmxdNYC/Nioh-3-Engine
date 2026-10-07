// Read-only: the R1 render-begin wait loop before patching.
#load "Mem.fsx"
open Mem
let t = attach "Nioh3"
disasm t (t.Base + 0x4A5320n) 0x50
