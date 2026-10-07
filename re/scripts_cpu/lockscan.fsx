// lockscan.fsx - list LOCK-prefixed instructions inside RVA ranges, with context, and classify the
// "atomic load" idiom  `xor eax,eax ; lock cmpxchg [m], r32`  (a CAS 0->r used as a read when r==0).
//   dotnet fsi lockscan.fsx <startHex>:<lenHex> [...]
// For each lock cmpxchg it prints 4 instructions before and 2 after, the instruction bytes, and
// whether the following instruction consumes flags from the cmpxchg (needs a `test eax,eax` in a patch).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let fmt = NasmFormatter()
let out = StringOutput()
let show (ins: Instruction) =
    fmt.Format(&ins, out)
    let b = img.[int ins.IP .. int ins.IP + ins.Length - 1] |> Array.map (sprintf "%02X") |> String.concat " "
    sprintf "%8X  %-40s %s" ins.IP (out.ToStringAndReset()) b
let mutable total = 0
for a in fsi.CommandLineArgs |> Array.skip 1 do
    let p = a.Split(':')
    let s = Convert.ToInt32(p.[0], 16)
    let n = Convert.ToInt32(p.[1], 16)
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, s, n))
    dec.IP <- uint64 s
    let list = Collections.Generic.List<Instruction>()
    while dec.IP < uint64 (s + n) do list.Add(dec.Decode())
    let mutable cnt = 0
    let mutable loads = 0
    for i in 0 .. list.Count - 1 do
        let ins = list.[i]
        if ins.HasLockPrefix then
            cnt <- cnt + 1
            let isCx = ins.Mnemonic = Mnemonic.Cmpxchg
            let prevXor = i > 0 && list.[i-1].Mnemonic = Mnemonic.Xor && list.[i-1].Op0Register = Register.EAX && list.[i-1].Op1Register = Register.EAX
            let nxt = if i + 1 < list.Count then list.[i+1] else ins
            let flagsUsed =
                nxt.FlowControl = FlowControl.ConditionalBranch || nxt.Mnemonic = Mnemonic.Sete || nxt.Mnemonic = Mnemonic.Setne
            let kind =
                if isCx && prevXor then (loads <- loads + 1; sprintf "ATOMIC-LOAD? (src %A must be 0)%s" ins.Op1Register (if flagsUsed then " FLAGS-USED" else ""))
                elif isCx then "CAS"
                else "RMW"
            printfn "--- %s" kind
            for j in max 0 (i - 4) .. min (list.Count - 1) (i + 2) do
                printfn "%s%s" (if j = i then "> " else "  ") (show list.[j])
    printfn "=== %X..+%X : %d lock ops, %d atomic-load candidates" s n cnt loads
    total <- total + cnt
