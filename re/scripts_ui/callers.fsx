// callers.fsx - find E8/E9 rel32 call/jmp sites to target RVAs (raw scan, may include false positives)
open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let ts = fsi.CommandLineArgs.[1].Split(',') |> Array.map (fun s -> Convert.ToInt32(s, 16)) |> Set.ofArray
for i in 0x1000 .. 0x3800000 do
    if img.[i] = 0xE8uy || img.[i] = 0xE9uy then
        let t = i + 5 + BitConverter.ToInt32(img, i + 1)
        if ts.Contains t then printfn "%X %s -> %X" i (if img.[i] = 0xE8uy then "call" else "jmp ") t
