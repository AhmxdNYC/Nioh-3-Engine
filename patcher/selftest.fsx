#load "Mem.fsx"
open Mem
open Iced.Intel

// Read-only smoke test against an always-running process; never writes.
let t = attach "steam"
printfn "steam base=%X size=%X" t.Base t.ModuleSize
let mz = t.Read(t.Base, 2)
printfn "MZ header: %c%c" (char mz.[0]) (char mz.[1])
let hits = t.Scan "48 89 5C 24 ?? 57 48 83 EC"
printfn "pattern hits: %d (first %A)" hits.Length (List.tryHead hits)
match List.tryHead hits with
| Some a -> disasm t a 16
| None -> ()
let code = assemble 0x140000000n (fun a -> a.mov(AssemblerRegisters.rax, 1L); a.ret())
printfn "assembled %d bytes: %s" code.Length (System.BitConverter.ToString code)
