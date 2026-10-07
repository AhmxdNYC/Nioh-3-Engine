// scan_disp.fsx - list instructions using memory displacement(s) in executable sections.
//   dotnet fsi scan_disp.fsx <dispHex>[,<dispHex>...] [mustAlsoHaveDispHexWithin256Bytes]
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let pe = BitConverter.ToInt32(img, 0x3C)
let nsec = int (BitConverter.ToUInt16(img, pe + 6))
let optsz = int (BitConverter.ToUInt16(img, pe + 20))
let args = fsi.CommandLineArgs |> Array.skip 1
let wanted = args.[0].Split(',') |> Array.map (fun s -> Convert.ToUInt64(s, 16)) |> Set.ofArray
let also = if args.Length > 1 then Some (Convert.ToUInt64(args.[1], 16)) else None
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
        let hits = Collections.Generic.List<uint64 * string>()
        let alsoIps = Collections.Generic.List<uint64>()
        while dec.IP < fin do
            let ins = dec.Decode()
            let mutable isMem = false
            for k in 0 .. ins.OpCount - 1 do
                if ins.GetOpKind(k) = OpKind.Memory then isMem <- true
            if isMem && ins.MemoryBase <> Register.RIP && ins.MemoryBase <> Register.RSP && ins.MemoryBase <> Register.RBP then
                let d = ins.MemoryDisplacement64
                if wanted.Contains d then
                    fmt.Format(&ins, out)
                    hits.Add((ins.IP, out.ToStringAndReset()))
                match also with
                | Some a when d = a -> alsoIps.Add ins.IP
                | _ -> ()
        for (ip, txt) in hits do
            let ok =
                match also with
                | None -> true
                | Some _ -> alsoIps |> Seq.exists (fun a -> (if a > ip then a - ip else ip - a) < 256UL)
            if ok then printfn "%X  %s" ip txt
