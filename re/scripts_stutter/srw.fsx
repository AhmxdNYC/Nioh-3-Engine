// srw.fsx - every call to an imported function (by name) with the instruction that loaded rcx (up to 12 back).
//   dotnet fsi srw.fsx <ImportName> [rcxRegex]
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.Collections.Generic
open System.Text.RegularExpressions
open Iced.Intel
open Wj_common
let a = fsi.CommandLineArgs |> Array.skip 1
let want = a.[0]
let rx = Regex(if a.Length > 1 then a.[1] else ".")
// IAT slots by name
let dd = optOff + 112
let cstr o = let e = Array.IndexOf(img, 0uy, o) in Text.Encoding.ASCII.GetString(img, o, e - o)
let slots = HashSet<uint64>()
let mutable d = int (u32 (dd + 8))
while u32 (d + 12) <> 0u do
    let oft, ft = u32 d, u32 (d + 16)
    let th = if oft <> 0u then oft else ft
    let mutable i = 0
    while BitConverter.ToUInt64(img, int th + i * 8) <> 0UL do
        let ent = BitConverter.ToUInt64(img, int th + i * 8)
        if ent &&& (1UL <<< 63) = 0UL && ent < uint64 img.Length && cstr (int ent + 2) = want then slots.Add(uint64 ft + uint64 (i * 8)) |> ignore
        i <- i + 1
    d <- d + 20
for i in 0 .. pBegin.Length - 1 do
    let s, e = pBegin.[i], pEnd.[i]
    if e > s && e - s < 0x200000u then
        let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
        dec.IP <- uint64 s
        let win = Queue<Instruction>()
        while dec.IP < uint64 e do
            let ins = dec.Decode()
            if ins.Mnemonic = Mnemonic.Call && ins.IsIPRelativeMemoryOperand && slots.Contains ins.IPRelativeMemoryAddress then
                let r = win.ToArray() |> Array.rev |> Array.tryFind (fun x -> x.Op0Kind = OpKind.Register && x.Op0Register = Register.RCX)
                let t = match r with Some x -> fmtIns x | None -> "?"
                if rx.IsMatch t then
                    let p = match primaryEntry (uint32 ins.IP) with Some x -> x | None -> s
                    printfn "%8X fn %8X  rcx: %s" ins.IP p t
            win.Enqueue ins
            if win.Count > 12 then win.Dequeue() |> ignore
