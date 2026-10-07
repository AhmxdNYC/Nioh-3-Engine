// aob.fsx - unique AOB patterns (ASI patches.json style) for sites: decode forward from (site - back) instruction
// boundaries, wildcard rel8/rel32 branch targets and RIP-relative displacements, grow until unique in the image.
//   dotnet fsi aob.fsx <rva>[:backRva] ...
#load "wj_common.fsx"
open System
open Iced.Intel
open Wj_common
let textRanges = execSections
let matches (pat: int[]) =
    let mutable n = 0
    for (va, vs) in textRanges do
        let last = va + vs - pat.Length
        let mutable i = va
        while i <= last && n < 2 do
            let mutable j = 0
            while j < pat.Length && (pat.[j] < 0 || int img.[i + j] = pat.[j]) do j <- j + 1
            if j = pat.Length then n <- n + 1
            i <- i + 1
    n
let patFrom (start: int) (nIns: int) =
    let d = Decoder.Create(64, ByteArrayCodeReader(img, start, 200))
    d.IP <- uint64 start
    let pat = ResizeArray<int>()
    for _ in 1 .. nIns do
        let ins = d.Decode()
        let off = int ins.IP
        let bytes = [| for k in 0 .. ins.Length - 1 -> int img.[off + k] |]
        let mutable ii = ins
        let co = d.GetConstantOffsets(&ii)
        if ins.IsIPRelativeMemoryOperand && co.HasDisplacement then
            for k in 0 .. int co.DisplacementSize - 1 do bytes.[int co.DisplacementOffset + k] <- -1
        if ins.FlowControl = FlowControl.Call || ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch then
            if co.HasImmediate then for k in 0 .. int co.ImmediateSize - 1 do bytes.[int co.ImmediateOffset + k] <- -1
        pat.AddRange bytes
    pat.ToArray()
for a in fsi.CommandLineArgs |> Array.skip 1 do
    let p = a.Split(':')
    let site = Convert.ToInt32(p.[0], 16)
    let start = if p.Length > 1 then Convert.ToInt32(p.[1], 16) else site
    let mutable k = 1
    let mutable fin = false
    while not fin && k < 12 do
        let pat = patFrom start k
        if pat.Length >= 8 && matches pat = 1 then
            let s = pat |> Array.map (fun b -> if b < 0 then "??" else sprintf "%02X" b) |> String.concat " "
            printfn "%X  pattern \"%s\"  offset %d" site s (site - start)
            fin <- true
        k <- k + 1
    if not fin then printfn "%X  no unique pattern within 11 instructions" site
