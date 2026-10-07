// frames.fsx - static stack-frame size of functions from .pdata unwind codes (dump only, offline).
//   dotnet fsi frames.fsx <rvaHex> [rvaHex ...]
// Prints: function start, size, frame bytes (pushes + allocs + return address), frame register, chained.
open System
open System.IO

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let u16 o = int (BitConverter.ToUInt16(img, o))
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let dd = opt + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let n = excSz / 12
let pB = Array.init n (fun i -> u32 (excRva + i * 12))
let pE = Array.init n (fun i -> u32 (excRva + i * 12 + 4))
let pU = Array.init n (fun i -> u32 (excRva + i * 12 + 8))
let find (rva: uint32) =
    let i = Array.BinarySearch(pB, rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && rva < pE.[i] then i else -1

/// returns (primary start, frame bytes incl. return address, frame reg, chained?)
let frameOf (rva: uint32) =
    let i = find rva
    if i < 0 then None else
    let mutable uw = int (pU.[i] &&& ~~~1u)
    let mutable start = pB.[i]
    let mutable total = 8
    let mutable chained = false
    let mutable fr = 0
    let mutable go = true
    let mutable g = 0
    while go && g < 8 do
        g <- g + 1
        let flags = int img.[uw] >>> 3
        let cnt = int img.[uw + 2]
        if fr = 0 then fr <- int (img.[uw + 3] &&& 0xFuy)
        let mutable k = 0
        while k < cnt do
            let c = u16 (uw + 4 + 2 * k)
            let op = (c >>> 8) &&& 0xF
            let info = (c >>> 12) &&& 0xF
            match op with
            | 0 -> total <- total + 8; k <- k + 1
            | 1 -> (if info = 0 then (total <- total + u16 (uw + 4 + 2 * (k + 1)) * 8; k <- k + 2)
                    else (total <- total + int (u32 (uw + 4 + 2 * (k + 1))); k <- k + 3))
            | 2 -> total <- total + info * 8 + 8; k <- k + 1
            | 3 -> k <- k + 1
            | 4 -> k <- k + 2
            | 5 -> k <- k + 3
            | 8 -> k <- k + 2
            | 9 -> k <- k + 3
            | 10 -> k <- k + 1
            | _ -> k <- k + 1
        if flags &&& 4 <> 0 then
            let o = uw + 4 + 2 * ((cnt + 1) &&& ~~~1)
            start <- u32 o
            uw <- int (u32 (o + 8))
            chained <- true
        else go <- false
    Some (start, total, fr, chained)

let isBig = fsi.CommandLineArgs |> Array.contains "big"
for a in (if isBig then [||] else fsi.CommandLineArgs |> Array.skip 1) do
    let rva = Convert.ToUInt32(a.Replace("0x", ""), 16)
    match frameOf rva with
    | Some (s, t, fr, ch) -> printfn "%8X  start %8X  frame %6d B (0x%X)  fpreg %d %s" rva s t t fr (if ch then "chained" else "")
    | None -> printfn "%8X  (leaf / no pdata)" rva

// mode: big <minBytes>  -> list every primary function with a frame >= minBytes
let argv = fsi.CommandLineArgs |> Array.skip 1
if argv.Length >= 1 && argv.[0] = "big" then
    let lim = if argv.Length > 1 then int argv.[1] else 16384
    let seen = Collections.Generic.HashSet<uint32>()
    for i in 0 .. n - 1 do
        match frameOf pB.[i] with
        | Some (s, t, fr, _) when t >= lim && seen.Add s -> printfn "%8X  frame %7d B  fpreg %d" s t fr
        | _ -> ()
