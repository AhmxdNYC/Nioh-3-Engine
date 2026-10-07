// scan_store.fsx - list memory WRITES with a given displacement (optionally only in rva range)
//   dotnet fsi scan_store.fsx <dispHex> [loHex hiHex]
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let pe = BitConverter.ToInt32(img, 0x3C)
let nsec = int (BitConverter.ToUInt16(img, pe + 6))
let optsz = int (BitConverter.ToUInt16(img, pe + 20))
let a = fsi.CommandLineArgs
let disp = Convert.ToUInt64(a.[1], 16)
let lo, hi = if a.Length > 3 then Convert.ToUInt64(a.[2],16), Convert.ToUInt64(a.[3],16) else 0UL, UInt64.MaxValue
let fmt = NasmFormatter()
let out = StringOutput()
let info = InstructionInfoFactory()
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
            if ins.IP >= lo && ins.IP < hi && ins.Op0Kind = OpKind.Memory && ins.MemoryBase <> Register.RIP && ins.MemoryBase <> Register.RSP && ins.MemoryDisplacement64 = disp then
                let ii = info.GetInfo(&ins)
                let mutable w = false
                for m in ii.GetUsedMemory() do
                    if m.Access = OpAccess.Write || m.Access = OpAccess.ReadWrite || m.Access = OpAccess.CondWrite then w <- true
                if w then
                    fmt.Format(&ins, out)
                    printfn "%X  %s" ins.IP (out.ToStringAndReset())
