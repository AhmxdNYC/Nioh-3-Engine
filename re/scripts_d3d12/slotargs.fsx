// slotargs.fsx -- for every E8 call to the given targets, recover the constant passed in EDX (2nd arg) and R8D (3rd arg)
// by decoding the bytes in front of the call (tries every start offset up to 0x60 bytes back that decodes cleanly onto the
// call). Used for notes/gpu-scheduling.md: AsyncSlotGetCtx 0x52BE1C(sched, slot) and AsyncSlotAddFence 0x148514(sched, key, slot, fence).
//   dotnet fsi slotargs.fsx 52be1c,148514
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let textEnd = 0x38E0000
let targets = fsi.CommandLineArgs.[1].Split(',') |> Array.map (fun s -> Convert.ToInt32(s, 16))
let fmt = NasmFormatter()
let so = StringOutput()
for t in targets do
    printfn "== target %X" t
    for i in 0x1000 .. textEnd - 5 do
        if img.[i] = 0xE8uy && i + 5 + BitConverter.ToInt32(img, i + 1) = t then
            // find a start offset that decodes onto i exactly
            let mutable best : Instruction list = []
            for back in 0x60 .. -1 .. 8 do
                if best.IsEmpty then
                    let dec = Decoder.Create(64, ByteArrayCodeReader(img, i - back, back + 5))
                    dec.IP <- uint64 (i - back)
                    let acc = Collections.Generic.List<Instruction>()
                    let mutable ok = true
                    while ok && dec.IP < uint64 i do
                        let x = dec.Decode()
                        if x.IsInvalid then ok <- false else acc.Add x
                    if ok && dec.IP = uint64 i then best <- List.ofSeq acc
            let last (reg: Register) =
                best |> List.rev |> List.tryPick (fun x ->
                    if x.Op0Kind = OpKind.Register && (x.Op0Register = reg || x.Op0Register.GetFullRegister() = reg.GetFullRegister()) then
                        fmt.Format(&x, so); Some(so.ToStringAndReset()) else None)
            let d = defaultArg (last Register.EDX) "?"
            let r8 = defaultArg (last Register.R8D) "?"
            // resolve "lea rdx,[rel X]" to the ASCII string at X (marker names)
            let m = Text.RegularExpressions.Regex.Match(d, @"lea rdx,\[rel ([0-9A-F]+)h\]")
            let str =
                if m.Success then
                    let a = Convert.ToInt32(m.Groups.[1].Value, 16)
                    let e = Array.IndexOf(img, 0uy, a)
                    if e > a && e - a < 120 then "  \"" + Text.Encoding.ASCII.GetString(img, a, e - a) + "\"" else ""
                else ""
            printfn "  call@%X  edx<- %-28s r8<- %s%s" i d r8 str
