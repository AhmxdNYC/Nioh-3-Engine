// Read-only: find the frame-task instance (default vtable RVA 0x40358E8 = class whose vf[1]=0x5ACEE8; arg overrides, owner of the QPC frame limiter in fn 0x5ACF80)
// by scanning private RW heap for its vtable pointer, then sample the limiter/timing block r15=[[inst+60h]+28h].
#load "Mem.fsx"
open System
open System.Runtime.InteropServices
open Mem
[<Struct; StructLayout(LayoutKind.Sequential)>]
type MBI = val BaseAddress: nativeint; val AllocationBase: nativeint; val AllocationProtect: uint32; val PartitionId: uint16
           val RegionSize: nativeint; val State: uint32; val Protect: uint32; val Type: uint32
[<DllImport("kernel32.dll")>]
extern nativeint VirtualQueryEx(nativeint h, nativeint addr, MBI& mbi, nativeint len)
let t = attach "Nioh3"
let vtRva = if fsi.CommandLineArgs.Length > 1 then Convert.ToInt64(fsi.CommandLineArgs.[1].Replace("0x",""),16) else 0x40358E8L
let vt = int64 t.Base + vtRva
let pat = BitConverter.GetBytes vt
let hits = Collections.Generic.List<int64>()
let mutable addr = 0x10000n
let mutable mbi = MBI()
let sw = Diagnostics.Stopwatch.StartNew()
while hits.Count < 8 && VirtualQueryEx(t.Handle, addr, &mbi, nativeint (Marshal.SizeOf<MBI>())) <> 0n && int64 addr < 0x7FF000000000L do
    if mbi.State = 0x1000u && mbi.Type = 0x20000u && (mbi.Protect = 0x04u) then
        let size = int64 mbi.RegionSize
        let mutable off = 0L
        while off < size do
            let n = int (min 0x400000L (size - off))
            let buf = try t.Read(mbi.BaseAddress + nativeint off, n) with _ -> Array.empty
            let mutable i = 0
            while i + 8 <= buf.Length do
                if BitConverter.ToInt64(buf, i) = vt then hits.Add(int64 mbi.BaseAddress + off + int64 i)
                i <- i + 8
            off <- off + int64 n
    addr <- mbi.BaseAddress + mbi.RegionSize
printfn "scan %.1fs, instances: %A" sw.Elapsed.TotalSeconds [for h in hits -> sprintf "%X" h]
for inst in hits do
    let p60 = t.ReadI64(nativeint (inst + 0x60L))
    let r15 = if p60 <> 0L then t.ReadI64(nativeint (p60 + 0x28L)) else 0L
    printfn "inst %X  [+B8]=%d  [+BC]=%d  r15=%X" inst (BitConverter.ToInt32(t.Read(nativeint (inst+0xB8L),4),0)) (t.Read(nativeint (inst+0xBCL),1).[0]) r15
    if r15 <> 0L then
        for k in 0..11 do
            let b = t.Read(nativeint r15, 0x38)
            printfn "  last=%d  +18 dt=%.6f  +24 measured=%.6f  +2C=%.6f  +30 fps=%d" (BitConverter.ToInt64(b,0)) (BitConverter.ToSingle(b,0x18)) (BitConverter.ToSingle(b,0x24)) (BitConverter.ToSingle(b,0x2C)) (BitConverter.ToInt32(b,0x30))
            Threading.Thread.Sleep 9
