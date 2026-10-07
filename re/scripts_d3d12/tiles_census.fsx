// tiles_census.fsx -- signature check of candidate D3D12 tile/queue call sites (reads re\Nioh3.dump.exe only).
//   dotnet fsi tiles_census.fsx <scan.tsv>   (scan.tsv = output of tiles_scan.fsx; cols: name func site +disp ...)
// For every candidate `call [reg+disp]` prints the operand size of each stack-arg store [rsp+20h..58h] since the
// previous call, plus the width of r8/r9 setup. UpdateTileMappings(this,res,u32 nReg,ptr,ptr,heap,u32 nRanges,ptr,ptr,ptr,u32 flags)
// => +20 q, +28 q, +30 d, +38 q, +40 q, +48 q, +50 d ; CopyTileMappings => +20 q, +28 q, +30 d.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let fmt = NasmFormatter()
let so = StringOutput()
for line in File.ReadAllLines fsi.CommandLineArgs.[1] do
    let c = line.Split('\t')
    if c.Length > 3 && not (c.[0].StartsWith "ref") then
        let f = Convert.ToUInt32(c.[1], 16)
        let site = Convert.ToUInt32(c.[2], 16)
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int f, int (site - f + 8u)))
        dec.IP <- uint64 f
        let st = Collections.Generic.Dictionary<int,string>()
        let mutable regs = ""
        while dec.IP <= uint64 site do
            let i = dec.Decode()
            if uint32 i.IP = site then ()
            elif i.Mnemonic = Mnemonic.Call then st.Clear(); regs <- ""
            else
                if i.Op0Kind = OpKind.Memory && i.MemoryBase = Register.RSP && i.MemoryIndex = Register.None then
                    let d = int i.MemoryDisplacement64
                    if d >= 0x20 && d <= 0x58 then
                        st.[d] <- (match i.MemorySize with MemorySize.UInt32 | MemorySize.Int32 | MemorySize.DwordOffset -> "d" | MemorySize.UInt64 | MemorySize.Int64 | MemorySize.QwordOffset -> "q" | MemorySize.UInt8 -> "b" | MemorySize.UInt16 -> "w" | _ -> "?")
                if i.Op0Kind = OpKind.Register && (i.Op0Register = Register.R8D || i.Op0Register = Register.R8 || i.Op0Register = Register.R9 || i.Op0Register = Register.R9D) then
                    regs <- regs + (string i.Op0Register).ToLower() + " "
        let sg = [0x20..8..0x58] |> List.map (fun d -> match st.TryGetValue d with | true, s -> s | _ -> ".") |> String.concat ""
        printfn "%s\t%s\t%s\t%s\tsig=%s\tregs=%s" c.[0] c.[1] c.[2] c.[3] sg (regs.Trim())
