// dis.fsx - read-only: disassemble one function of re\Nioh3.dump.exe (RVA = file offset in the dump) with Iced.
//   dotnet fsi dis.fsx <rva hex> <size hex|dec> [out.txt]
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let args = fsi.CommandLineArgs
let img = File.ReadAllBytes(Path.Combine(__SOURCE_DIRECTORY__, "..", "Nioh3.dump.exe"))
let rva = Convert.ToInt32(args.[1], 16)
let size = if args.[2].StartsWith "0x" then Convert.ToInt32(args.[2].Substring 2, 16) else int args.[2]
let baseVa = 0x7ff74f410000UL
let dec = Decoder.Create(64, ByteArrayCodeReader(img, rva, size))
dec.IP <- baseVa + uint64 rva
let fmt = MasmFormatter()
fmt.Options.FirstOperandCharIndex <- 10
let out = Text.StringBuilder()
let endIp = baseVa + uint64 (rva + size)
while dec.IP < endIp do
    let ins = dec.Decode()
    let o = StringOutput()
    fmt.Format(&ins, o)
    let r = int (ins.IP - baseVa)
    let bytes = img.[r .. r + ins.Length - 1] |> Array.map (sprintf "%02X") |> String.concat ""
    let tgt = if ins.IsCallNear || ins.IsJmpNear || ins.IsJccShortOrNear || ins.IsJmpShort then sprintf "   ; -> %X" (int (ins.NearBranchTarget - baseVa)) else ""
    out.AppendLine(sprintf "%07X  %-24s %s%s" r bytes (o.ToStringAndReset()) tgt) |> ignore
if args.Length > 3 then File.WriteAllText(args.[3], out.ToString()) else printf "%s" (out.ToString())
