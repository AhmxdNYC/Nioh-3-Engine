// find_type2.fsx - find vtables whose slot +8 is a "mov eax,N; ret" stub (element type N), print slot +0x68 etc.
//   dotnet fsi find_type2.fsx <N> [slotHex,...]
open System
open System.IO
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let ib = 0x7FF74F410000UL
let n = int fsi.CommandLineArgs.[1]
let slots = if fsi.CommandLineArgs.Length > 2 then fsi.CommandLineArgs.[2].Split(',') |> Array.map (fun s -> Convert.ToInt32(s, 16)) else [| 0x68 |]
// candidate stubs: B8 nn 00 00 00 C3
let stubs = Collections.Generic.HashSet<uint64>()
for i in 0 .. img.Length - 6 do
    if img.[i] = 0xB8uy && int img.[i+1] = n && img.[i+2] = 0uy && img.[i+3] = 0uy && img.[i+4] = 0uy && img.[i+5] = 0xC3uy then
        stubs.Add(uint64 i) |> ignore
printfn "%d stubs" stubs.Count
let mutable i = 0
while i <= img.Length - 0x200 do
    let v = BitConverter.ToUInt64(img, i + 8)
    if v >= ib && stubs.Contains(v - ib) then
        // require slot 0 also code pointer
        let v0 = BitConverter.ToUInt64(img, i)
        if v0 >= ib && v0 < ib + 0x3900000UL then
            let s = slots |> Array.map (fun o -> sprintf "+%X=%X" o (BitConverter.ToUInt64(img, i + o) - ib)) |> String.concat " "
            printfn "vtbl %X: +8=%X %s" i (v - ib) s
    i <- i + 8
