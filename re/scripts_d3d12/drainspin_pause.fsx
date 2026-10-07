// drainspin_pause.fsx -- offline check of patch S1 (d3d12-submission.md): add PAUSE to the main thread's
// end-of-frame busy-spin that waits for SynergisticDispatcherTask to drain (418F40, loop 418F60..418F77).
// Reads the dump only; prints original/new bytes and both disassemblies. Nothing is written anywhere.
//   dotnet fsi drainspin_pause.fsx
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let site, len = 0x418F68, 17
let orig = img.[site .. site + len - 1]
// expected original: 88 05 rel32 (mov [45CE8A0],al) | 0F B6 05 rel32 (movzx eax,byte [45CE8A0]) | 84 C0 | 75 E7
let ok =
    orig.[0] = 0x88uy && orig.[1] = 0x05uy && orig.[6] = 0x0Fuy && orig.[7] = 0xB6uy && orig.[8] = 0x05uy &&
    orig.[13] = 0x84uy && orig.[14] = 0xC0uy && orig.[15] = 0x75uy && orig.[16] = 0xE7uy
let patched =
    Array.concat [ orig.[0..5]                         // mov [rel 45CE8A0h],al   (unchanged, same rel32)
                   [| 0x84uy; 0xC0uy |]                // test al,al
                   [| 0x74uy; 0x07uy |]                // jz 418F79 (loop exit, same as original fall-through)
                   [| 0xF3uy; 0x90uy |]                // pause
                   [| 0xEBuy; 0xEAuy |]                // jmp 418F60 (loop head, same as original jne target)
                   [| 0x0Fuy; 0x1Fuy; 0x00uy |] ]      // nop3 (pad to 418F79)
let show (title: string) (bytes: byte[]) =
    printfn "%s  %s" title (BitConverter.ToString(bytes).Replace("-", " "))
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- uint64 site
    let f = NasmFormatter()
    let o = StringOutput()
    while dec.IP < uint64 (site + bytes.Length) do
        let i = dec.Decode()
        f.Format(&i, o)
        printfn "   %X  %s" i.IP (o.ToStringAndReset())
printfn "original layout as expected: %b" ok
show "orig" orig
show "new " patched
printfn "same length: %b" (patched.Length = orig.Length)
