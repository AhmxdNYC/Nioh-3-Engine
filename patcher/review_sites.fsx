// Read-only: disassemble around the frame-timing patch sites to check flag/register reuse.
#load "Mem.fsx"
open Mem
let t = attach "Nioh3"
for (name, rva, before, len) in [ "P1", 0x5AD677, 0x30, 0x70; "P2", 0x5AD6AA, 0x10, 0x30; "P4", 0x5AD860, 0x10, 0x30 ] do
    printfn "==== %s @ rva %X" name rva
    disasm t (t.Base + nativeint (rva - before)) len
