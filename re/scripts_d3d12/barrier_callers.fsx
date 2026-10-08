// barrier_callers.fsx -- for every E8 call to a target (default SynTransitionResource 0x115E24), print the instruction that
// last set each argument (rcx ctx, rdx resource, r8 &trackedState, r9d newState, [rsp+20h] &stamp, [rsp+28h] subresource)
// inside the containing .pdata function, walking back from the call. Reads re\Nioh3.dump.exe only (notes/vkd3d-audit.md R1/R2).
//   dotnet fsi barrier_callers.fsx [target_hex] [-v N]     (-v N: also dump the N instructions before each call)
//   dotnet fsi barrier_callers.fsx --writes 6d0            (every instruction that writes [reg+disp], disp given in hex)
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let pB = Array.init (excSz / 12) (fun i -> int (u32 (excRva + i * 12)))
let pE = Array.init (excSz / 12) (fun i -> int (u32 (excRva + i * 12 + 4)))
let textEnd = 0x38E0000
let funcOf rva =
    let mutable best = None
    for i in 0 .. pB.Length - 1 do
        if pB.[i] <= rva && rva < pE.[i] then best <- Some (pB.[i], pE.[i])
    best
let fmt = NasmFormatter()
let so = StringOutput()
let txt (i: Instruction) = fmt.Format(&i, so); so.ToStringAndReset()
let decodeRange s e =
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, s, e - s))
    dec.IP <- uint64 s
    let l = Collections.Generic.List<Instruction>()
    while dec.IP < uint64 e do l.Add(dec.Decode())
    l
let args = fsi.CommandLineArgs |> Array.skip 1
if args.Length >= 2 && args.[0] = "--writes" then
    let disp = Convert.ToUInt64(args.[1], 16)
    for i in 0 .. pB.Length - 1 do
        if pE.[i] <= textEnd && pE.[i] > pB.[i] then
            for ins in decodeRange pB.[i] pE.[i] do
                if ins.Op0Kind = OpKind.Memory && ins.MemoryDisplacement64 = disp && ins.MemoryBase <> Register.RSP
                   && ins.Mnemonic <> Mnemonic.Cmp && ins.Mnemonic <> Mnemonic.Test then
                    printfn "%X\t%X\t%s" ins.IP pB.[i] (txt ins)
else
let target = if args.Length > 0 && not (args.[0].StartsWith "-") then Convert.ToInt32(args.[0], 16) else 0x115E24
let verbose = match Array.tryFindIndex ((=) "-v") args with Some k -> int args.[k + 1] | None -> 0
let sites =
    [ for o in 0x1000 .. textEnd - 5 do
        if img.[o] = 0xE8uy && o + 5 + BitConverter.ToInt32(img, o + 1) = target then yield o ]
let wanted = [ "rcx", Register.RCX; "rdx", Register.RDX; "r8", Register.R8; "r9", Register.R9 ]
for site in sites do
    match funcOf site with
    | None -> ()
    | Some (fb, fe) ->
        let l = decodeRange fb fe
        match Seq.tryFindIndex (fun (i: Instruction) -> int i.IP = site) l with
        | None -> ()   // not an instruction boundary: data or mid-instruction match
        | Some k ->
            let found = Collections.Generic.Dictionary<string, string>()
            let mutable j = k - 1
            while j >= 0 && k - j <= 60 && found.Count < 6 do
                let i = l.[j]
                for (n, r) in wanted do
                    if not (found.ContainsKey n) && i.Op0Kind = OpKind.Register && i.Op0Register.GetFullRegister() = r
                       && i.Mnemonic <> Mnemonic.Cmp && i.Mnemonic <> Mnemonic.Test then
                        found.[n] <- sprintf "%X:%s" i.IP (txt i)
                if i.Op0Kind = OpKind.Memory && i.MemoryBase = Register.RSP then
                    for (n, d) in [ "s20", 0x20UL; "s28", 0x28UL ] do
                        if not (found.ContainsKey n) && i.MemoryDisplacement64 = d then found.[n] <- sprintf "%X:%s" i.IP (txt i)
                j <- j - 1
            // one more level: for "mov X,REG" sources of r9/s20, show the last writer of REG before that instruction
            let srcOf (v: string) =
                let m = Text.RegularExpressions.Regex.Match(v, @"^([0-9A-F]+):\S+\s+[^,]+,([a-z0-9]+)$")
                if not m.Success then "" else
                let ip = Convert.ToUInt64(m.Groups.[1].Value, 16)
                let rn = m.Groups.[2].Value
                let mutable r = ""
                let mutable q = k - 1
                while q >= 0 && int l.[q].IP >= int ip do q <- q - 1
                while q >= 0 && r = "" && k - q <= 200 do
                    let i = l.[q]
                    if i.Op0Kind = OpKind.Register && i.Mnemonic <> Mnemonic.Cmp && i.Mnemonic <> Mnemonic.Test then
                        let fr = i.Op0Register.GetFullRegister()
                        let ok, wr = Enum.TryParse<Register>(rn.ToUpper())
                        let want = if ok && wr <> Register.None then wr.GetFullRegister() else Register.None
                        if want <> Register.None && fr = want then r <- sprintf "  <= %X:%s" i.IP (txt i)
                    q <- q - 1
                r
            for n in [ "r9"; "s20" ] do
                match found.TryGetValue n with
                | true, v -> found.[n] <- v + srcOf v
                | _ -> ()
            let g n = match found.TryGetValue n with | true, v -> v | _ -> "?"
            printfn "site %X func %X\n  rdx(res) %s\n  r8(&st)  %s\n  r9(new)  %s\n  s20(stamp) %s\n  s28(sub) %s" site fb (g "rdx") (g "r8") (g "r9") (g "s20") (g "s28")
            if verbose > 0 then
                for i in max 0 (k - verbose) .. k do printfn "    %X  %s" l.[i].IP (txt l.[i])
