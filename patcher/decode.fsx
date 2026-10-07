// decode.fsx — disassemble a hex byte string (verify hand-made patch bytes).
//   dotnet fsi decode.fsx <ipHex> "<hex bytes>"
#r "nuget: Iced, 1.21.0"
open Iced.Intel
let a = fsi.CommandLineArgs
let ip = System.Convert.ToUInt64(a.[1], 16)
let b = a.[2].Split([|' '|], System.StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun s -> System.Convert.ToByte(s, 16))
let d = Decoder.Create(64, ByteArrayCodeReader(b))
d.IP <- ip
let f = NasmFormatter()
let o = StringOutput()
while d.IP < ip + uint64 b.Length do
    let i = d.Decode()
    f.Format(&i, o)
    printfn "%8X  %s" i.IP (o.ToStringAndReset())
