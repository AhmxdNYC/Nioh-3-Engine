// branch_into_sites.fsx - sweep .text (pdata functions) for near branches/calls landing strictly inside the latency_live.fsx patch sites.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
// ranges strictly inside which no branch may land: (start, len)
let ranges = [ 0x5AB6CFUL,5UL; 0x5ACAAAUL,5UL; 0x4A52D4UL,5UL; 0x4A535EUL,5UL; 0x3D6552UL,6UL; 0x5AD2BBUL,7UL ]
let seen = Collections.Generic.HashSet<uint32>()
let mutable hits = 0
for i in 0 .. excSz / 12 - 1 do
    let b, e = u32 (excRva + i * 12), u32 (excRva + i * 12 + 4)
    if e > b && e - b < 0x200000u && seen.Add b then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int b, int (e - b)))
        dec.IP <- uint64 b
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch || ins.FlowControl = FlowControl.Call then
                if ins.Op0Kind = OpKind.NearBranch64 then
                    let t = ins.NearBranchTarget
                    for (s, l) in ranges do
                        if t > s && t < s + l then hits <- hits + 1; printfn "branch into site %X from %X -> %X" s ins.IP t
printfn "done, %d hits" hits
