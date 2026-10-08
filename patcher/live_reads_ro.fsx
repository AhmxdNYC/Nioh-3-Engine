// READ-ONLY live probe helper (VM_READ|QUERY only). Usage: dotnet fsi live_reads_ro.fsx <mode>
open System
open System.Diagnostics
open System.Runtime.InteropServices
module N =
    [<DllImport("kernel32.dll", SetLastError=true)>] extern nativeint OpenProcess(uint32 a, bool i, int pid)
    [<DllImport("kernel32.dll", SetLastError=true)>] extern bool ReadProcessMemory(nativeint h, nativeint a, byte[] b, nativeint n, nativeint& r)
let p = Process.GetProcessesByName("Nioh3").[0]
let h = N.OpenProcess(0x0410u, false, p.Id)   // VM_READ | QUERY_INFORMATION
let B = p.MainModule.BaseAddress
let rd (a:nativeint) n =
    let b = Array.zeroCreate<byte> n
    let mutable r = 0n
    if N.ReadProcessMemory(h,a,b,nativeint n,&r) then Some b else None
let u64 a = match rd a 8 with Some b -> BitConverter.ToUInt64(b,0) | None -> 0UL
let u32 a = match rd a 4 with Some b -> BitConverter.ToUInt32(b,0) | None -> 0u
let u8 a = match rd a 1 with Some b -> int b.[0] | None -> -1
let f32 a = match rd a 4 with Some b -> BitConverter.ToSingle(b,0) | None -> nanf
let ptr (v:uint64) = nativeint (int64 v)
let rva (o:int) = B + nativeint o
let ts () = DateTime.Now.ToString("HH:mm:ss.fff")
let mode = fsi.CommandLineArgs.[1]
let gm () = ptr (u64 (rva 0x4759910))
let stop () = let g = gm () in if g = 0n then "?" else String.Join("", [for o in 0x588..0x58D -> string (u8 (g + nativeint o))])
match mode with
| "a4" ->
    let g = ptr (u64 (rva 0x45B9E30))
    printfn "base=%X g=%X flagB08=%d stop=%s" (int64 B) (int64 g) (u8 (g+0xB08n)) (stop())
    let T = ptr (u64 (g+0xA08n))
    let Bq = ptr (u64 T)
    let cnt = u32 (Bq+4n)
    printfn "T=%X [T]=%X count=%d" (int64 T) (int64 Bq) cnt
    for i in 0 .. int (min cnt 40u) - 1 do
        printfn "entry %d: %s" i (String.Join(",", [for k in 0..6 -> sprintf "%g" (f32 (Bq + 8n + nativeint (i*0x1C+k*4)))]))
    // key map: vector [M+8,M+0x10) of {u8 key, u32 idx @+4}
    let M = T + 0x20n
    let vb, ve = ptr (u64 (M+8n)), ptr (u64 (M+0x10n))
    printfn "map vec %X..%X" (int64 vb) (int64 ve)
    if ve > vb && (ve - vb) < 4096n then
        for q in 0 .. int (ve - vb) / 8 - 1 do printfn "map key=%d idx=%d" (u8 (vb+nativeint(q*8))) (u32 (vb+nativeint(q*8+4)))
    let sd = ptr (u64 (rva 0x45B9E58))
    printfn "sceneCfg=%X +8BC=%g +8C0=%g" (int64 sd) (f32 (sd+0x8BCn)) (f32 (sd+0x8C0n))
    printfn "MESHLET_QUALITY(0x45BAFB0)=%d" (u32 (rva 0x45BAFB0))
| "gl1" ->
    for s in 1..20 do
        let P = ptr (u64 (ptr (u64 (rva 0x45B9E40))))
        let V = ptr (u64 (P + 0x48n))
        let c o = u64 (V + nativeint o)
        let pr o = (let b = c o in b)
        let mset = rva 0x4759950
        printfn "%s,stop=%s,P=%X,V=%X,v0cnt=%d,vBcnt=%d,vEcnt=%d,set0=%X,set8=%d,set10=%d" (ts()) (stop()) (int64 P) (int64 V) (c 8) (c 0x60) (c 0x78) (u64 mset) (u64 (mset+8n)) (u64 (mset+16n))
        Threading.Thread.Sleep 1000
| _ -> ()
match mode with
| "pp" ->
    // device candidates: pointers found at known globals; validate by pool D20 enabled byte and D10 frame ticking
    let cands = [ "RG+70", ptr (u64 (ptr (u64 (rva 0x45BA588)) + 0x70n)); "RG", ptr (u64 (rva 0x45BA588)) ]
    for (n,d) in cands do
        let pool = d + 0x61A0n
        printfn "%s dev=%X D20=%d D21=%d D10=%d hits=%d miss=%d qf=%d" n (int64 d) (u8 (pool+0xD20n)) (u8 (pool+0xD21n)) (u32 (pool+0xD10n)) (u32 (pool+0x918n)) (u32 (pool+0x91Cn)) (u32 (pool+0x90Cn))
| "pps" ->
    let d = ptr (u64 (rva 0x45BA588))
    let pool = d + 0x61A0n
    printfn "t,frame,cur_hit,cur_miss,cur_qfull,scan_hit,scan_miss,scan_qfull,stop"
    for s in 1..int fsi.CommandLineArgs.[2] do
        printfn "%s,%d,%d,%d,%d,%d,%d,%d,%s" (ts()) (u32 (pool+0xD10n)) (u32 (pool+0x918n)) (u32 (pool+0x91Cn)) (u32 (pool+0x90Cn)) (u32 (pool+0xA30n)) (u32 (pool+0xA34n)) (u32 (pool+0xA24n)) (stop())
        Threading.Thread.Sleep 1000
| _ -> ()
