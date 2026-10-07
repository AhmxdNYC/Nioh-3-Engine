open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
for a in fsi.CommandLineArgs |> Array.skip 1 do
    let p = a.Split(':')
    let r = Convert.ToInt32(p.[0], 16)
    let n = Convert.ToInt32(p.[1], 16)
    printfn "%X: %s" r (BitConverter.ToString(img, r, n).Replace("-", " "))
