// scan_store.fsx - find code that stores to [base+dispA] and [samebase+dispB] within a small window.
//   dotnet fsi scan_store.fsx <dispAHex> <dispBHex> [windowBytes=64]
// Used to locate the "reset" of the UI glyph atlas (fields +74h count, +84h/+88h cursor).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let pe = BitConverter.ToInt32(img, 0x3C)
let nsec = int (BitConverter.ToUInt16(img, pe + 6))
let optsz = int (BitConverter.ToUInt16(img, pe + 20))
let args = fsi.CommandLineArgs |> Array.skip 1
let dA = Convert.ToInt64(args.[0], 16)
let dB = Convert.ToInt64(args.[1], 16)
let win = if args.Length > 2 then int args.[2] else 64
for i in 0 .. nsec - 1 do
    let sh = pe + 24 + optsz + i * 40
    let vs = BitConverter.ToInt32(img, sh + 8)
    let va = BitConverter.ToInt32(img, sh + 12)
    let ch = BitConverter.ToUInt32(img, sh + 36)
    if ch &&& 0x20000000u <> 0u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, va, vs))
        dec.IP <- uint64 va
        let recent = Collections.Generic.Queue<uint64 * Register * int64>()
        let fin = uint64 (va + vs)
        while dec.IP < fin do
            let ins = dec.Decode()
            if ins.Op0Kind = OpKind.Memory && ins.MemoryIndex = Register.None && ins.Mnemonic = Mnemonic.Mov then
                let d = int64 ins.MemoryDisplacement64
                let b = ins.MemoryBase
                while recent.Count > 0 && (let (ip, _, _) = recent.Peek() in ins.IP - ip > uint64 win) do recent.Dequeue() |> ignore
                for (ip, rb, rd) in recent do
                    if rb = b && ((rd = dA && d = dB) || (rd = dB && d = dA)) then
                        printfn "%X / %X  base=%A" ip ins.IP b
                recent.Enqueue((ins.IP, b, d))
