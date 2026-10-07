// callgraph.fsx - direct call graph of Nioh3.dump.exe from .pdata functions (offline, dump only).
//   dotnet fsi callgraph.fsx build                 -> D:\nioh3-re\logic\callgraph.bin (edges caller->callee, primary starts)
//   dotnet fsi callgraph.fsx reach <root> [maxDepth] <rva> <rva> ...   -> is each rva reachable from root (direct calls + tail jmps)?
//   dotnet fsi callgraph.fsx path <root> <rva>      -> one shortest direct-call path root -> rva
// Only direct `call rel32` / `jmp rel32` to another function start are edges; virtual calls are invisible.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let dd = opt + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let n = excSz / 12
let pB = Array.init n (fun i -> u32 (excRva + i * 12))
let pE = Array.init n (fun i -> u32 (excRva + i * 12 + 4))
let pU = Array.init n (fun i -> u32 (excRva + i * 12 + 8))
let findIdx (rva: uint32) =
    let i = Array.BinarySearch(pB, rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && rva < pE.[i] then i else -1
/// primary function start of a pdata fragment (follow chained unwind info)
let primary (i: int) =
    let mutable uw = int (pU.[i] &&& ~~~1u)
    let mutable start = pB.[i]
    let mutable g = 0
    let mutable go = true
    while go && g < 16 do
        g <- g + 1
        let flags = int img.[uw] >>> 3
        let cnt = int img.[uw + 2]
        if flags &&& 4 <> 0 then
            let o = uw + 4 + 2 * ((cnt + 1) &&& ~~~1)
            start <- u32 o
            uw <- int (u32 (o + 8))
        else go <- false
    start
let primOf = Array.init n primary
let funcOf (rva: uint32) = let i = findIdx rva in if i < 0 then 0u else primOf.[i]

let binPath = @"D:\nioh3-re\logic\callgraph.bin"
let build () =
    let edges = HashSet<uint64>()
    let sw = Diagnostics.Stopwatch.StartNew()
    for i in 0 .. n - 1 do
        let s, e = pB.[i], pE.[i]
        if e > s && int e <= img.Length then
            let me = primOf.[i]
            let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
            dec.IP <- uint64 s
            let mutable ins = Instruction()
            while dec.IP < uint64 e do
                dec.Decode(&ins)
                if not ins.IsInvalid then
                    let isCall = ins.FlowControl = FlowControl.Call
                    let isJmp = ins.FlowControl = FlowControl.UnconditionalBranch && ins.Op0Kind = OpKind.NearBranch64
                    if (isCall || isJmp) && ins.Op0Kind = OpKind.NearBranch64 then
                        let t = uint32 ins.NearBranchTarget
                        let f = funcOf t
                        // call: any function; jmp: only if it leaves the current function (tail call or cold fragment of another fn)
                        if f <> 0u && (isCall || f <> me) then
                            // jmp to a fragment of a different primary = tail call; count edge to primary of target
                            edges.Add((uint64 me <<< 32) ||| uint64 f) |> ignore
    use w = new BinaryWriter(File.Create binPath)
    w.Write(edges.Count)
    for e in edges do w.Write(e)
    printfn "edges %d in %.1fs -> %s" edges.Count sw.Elapsed.TotalSeconds binPath

let load () =
    use r = new BinaryReader(File.OpenRead binPath)
    let c = r.ReadInt32()
    let fwd = Dictionary<uint32, ResizeArray<uint32>>()
    for _ in 1 .. c do
        let e = r.ReadUInt64()
        let a, b = uint32 (e >>> 32), uint32 e
        match fwd.TryGetValue a with
        | true, l -> l.Add b
        | _ -> let l = ResizeArray() in l.Add b; fwd.[a] <- l
    fwd

let bfs (fwd: Dictionary<uint32, ResizeArray<uint32>>) (root: uint32) (maxD: int) =
    let prev = Dictionary<uint32, uint32>()
    let depth = Dictionary<uint32, int>()
    let q = Queue<uint32>()
    prev.[root] <- root; depth.[root] <- 0; q.Enqueue root
    while q.Count > 0 do
        let x = q.Dequeue()
        let d = depth.[x]
        if d < maxD then
            match fwd.TryGetValue x with
            | true, l ->
                for y in l do
                    if not (prev.ContainsKey y) then prev.[y] <- x; depth.[y] <- d + 1; q.Enqueue y
            | _ -> ()
    prev, depth

let hx (s: string) = Convert.ToUInt32(s.Replace("0x", "").Replace("Nioh3+", ""), 16)
let argv = fsi.CommandLineArgs |> Array.skip 1
match argv with
| [| "build" |] -> build ()
| a when a.Length >= 3 && a.[0] = "reach" ->
    let fwd = load ()
    let root = funcOf (hx a.[1])
    let maxD, rest = (match Int32.TryParse a.[2] with | true, v when v < 1000 -> v, a.[3..] | _ -> 64, a.[2..])
    let prev, depth = bfs fwd root maxD
    printfn "root %X: %d functions reachable by direct calls (depth<=%d)" root prev.Count maxD
    for s in rest do
        let f = funcOf (hx s)
        printfn "  %-10s fn %8X : %s" s f (if prev.ContainsKey f then sprintf "REACHABLE depth %d" depth.[f] else "-")
| [| "path"; r; t |] ->
    let fwd = load ()
    let root, tgt = funcOf (hx r), funcOf (hx t)
    let prev, _ = bfs fwd root 64
    if not (prev.ContainsKey tgt) then printfn "no direct path %X -> %X" root tgt
    else
        let p = ResizeArray<uint32>()
        let mutable x = tgt
        while x <> root do p.Add x; x <- prev.[x]
        p.Add root
        p.Reverse()
        printfn "%s" (String.Join(" -> ", p |> Seq.map (sprintf "%X")))
| [| "callers"; t |] ->
    let fwd = load ()
    let tgt = funcOf (hx t)
    for kv in fwd do if kv.Value.Contains tgt then printfn "%X" kv.Key
| _ -> printfn "usage: build | reach <root> [maxDepth] <rva>... | path <root> <rva> | callers <rva>"
