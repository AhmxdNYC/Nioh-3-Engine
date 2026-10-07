// scan_rip.fsx - list instructions whose RIP-relative memory operand targets one of the given RVAs
//   dotnet fsi scan_rip.fsx <rvaHex>[,<rvaHex>...]
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let pe = BitConverter.ToInt32(img, 0x3C)
let nsec = int (BitConverter.ToUInt16(img, pe + 6))
let optsz = int (BitConverter.ToUInt16(img, pe + 20))
let wanted = fsi.CommandLineArgs.[1].Split(',') |> Array.map (fun s -> Convert.ToUInt64(s, 16)) |> Set.ofArray
let fmt = NasmFormatter()
let out = StringOutput()
for i in 0 .. nsec - 1 do
    let sh = pe + 24 + optsz + i * 40
    let vs = BitConverter.ToInt32(img, sh + 8)
    let va = BitConverter.ToInt32(img, sh + 12)
    let ch = BitConverter.ToUInt32(img, sh + 36)
    if ch &&& 0x20000000u <> 0u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, va, vs))
        dec.IP <- uint64 va
        let fin = uint64 (va + vs)
        while dec.IP < fin do
            let ins = dec.Decode()
            if ins.IsIPRelativeMemoryOperand && wanted.Contains ins.IPRelativeMemoryAddress then
                fmt.Format(&ins, out)
                printfn "%X  %s" ins.IP (out.ToStringAndReset())
            elif (ins.FlowControl = FlowControl.Call || ins.FlowControl = FlowControl.UnconditionalBranch) && ins.Op0Kind = OpKind.NearBranch64 && wanted.Contains ins.NearBranch64 then
                fmt.Format(&ins, out)
                printfn "%X  %s" ins.IP (out.ToStringAndReset())
