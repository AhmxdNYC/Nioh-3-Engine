// d3d12_vsites.fsx -- whole-binary dump of every COM-style virtual call `call/jmp [vt+slot]` with a symbolic "this" key,
// plus IID arguments, out-params and typed direct-call arguments. Input for d3d12_sites.py (classification + census).
//   dotnet fsi d3d12_vsites.fsx > vsites.tsv        (reads re\Nioh3.dump.exe only; ~1 min)
// Keys (backward slice inside the .pdata function, linear, <= 1500 instructions; a call clobbers volatile registers):
//   G:X      qword at global RVA X           K>D   qword at [K + D]        S:D  stack slot [rsp+D] with no earlier store
//   &G:X     address of global X             &K+D  address K + D           arg:rcx / nv:rbx  value live at function entry
//   ret:F    rax returned by direct call F   ret:? rax from an indirect call   ?   unknown
// Rows:  V  func site slot thisKey rcxKey rdxKey r8Key r9Key s20 s28 s30 s38 s40  (virtual call; vtable = [thisKey])
//        D  func site target rcxKey rdxKey r8Key r9Key                  (direct call, all of them; py filters)
//        P  func site target reg key0   (reg = &local struct; key0 = value stored at its +0 before the call)
//        W  func site storeKey srcKey                                   (store of a register into memory, py follows out-params)
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let dd = pe + 24 + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let textEnd = 0x38E0000
let funcs =
    [| for i in 0 .. excSz / 12 - 1 do
         let b, e = int (u32 (excRva + i * 12)), int (u32 (excRva + i * 12 + 4))
         if e > b && e <= textEnd then yield (b, e) |] |> Array.distinct |> Array.sort

let decodeRange s e =
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, s, e - s))
    dec.IP <- uint64 s
    let l = List<Instruction>()
    while dec.IP < uint64 e do l.Add(dec.Decode())
    l
let volatile = set [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ]
let argRegs = set [ Register.RCX; Register.RDX; Register.R8; Register.R9 ]
let rn (r: Register) = r.ToString().ToLower()
let writesReg (i: Instruction) (r: Register) =
    i.Op0Kind = OpKind.Register && i.Op0Register.GetFullRegister() = r
    && i.Mnemonic <> Mnemonic.Cmp && i.Mnemonic <> Mnemonic.Test && i.Mnemonic <> Mnemonic.Push
let isStackStore (i: Instruction) (d: uint64) =
    i.Op0Kind = OpKind.Memory && i.MemoryBase = Register.RSP && i.MemoryIndex = Register.None && i.MemoryDisplacement64 = d
    && (i.Mnemonic = Mnemonic.Mov) && i.Op1Kind = OpKind.Register

let rec slice (l: List<Instruction>) (k: int) (r: Register) (depth: int) : string =
    let mutable j = k - 1
    let mutable res = null
    while isNull res && j >= 0 && k - j <= 1500 do
        let i = l.[j]
        if i.Mnemonic = Mnemonic.Call && volatile.Contains r then
            res <- if r = Register.RAX then (if i.Op0Kind = OpKind.NearBranch64 then sprintf "ret:%X" i.NearBranch64 else "ret:?") else "?"
        elif writesReg i r then
            res <-
                if i.Mnemonic = Mnemonic.Mov && i.Op1Kind = OpKind.Register then
                    (if depth < 4 then slice l j (i.Op1Register.GetFullRegister()) (depth + 1) else "?")
                elif (i.Mnemonic = Mnemonic.Mov || i.Mnemonic = Mnemonic.Lea) && i.Op1Kind = OpKind.Memory && i.MemoryIndex = Register.None then
                    let lea = i.Mnemonic = Mnemonic.Lea
                    let d = i.MemoryDisplacement64
                    if i.MemoryBase = Register.RIP then (if lea then sprintf "&G:%X" d else sprintf "G:%X" d)
                    elif i.MemoryBase = Register.RSP then
                        if lea then sprintf "&S:%X" d
                        else
                            // chase the latest store to that stack slot
                            let mutable m = j - 1
                            let mutable s = null
                            while isNull s && m >= 0 && j - m <= 80 do
                                if isStackStore l.[m] d then s <- (if depth < 4 then slice l m (l.[m].Op1Register.GetFullRegister()) (depth + 1) else "?")
                                m <- m - 1
                            if isNull s then sprintf "S:%X" d else s
                    elif depth < 4 then
                        let b = slice l j (i.MemoryBase.GetFullRegister()) (depth + 1)
                        if lea then sprintf "&%s+%X" b d else sprintf "%s>%X" b d
                    else "?"
                else "?"
        j <- j - 1
    if isNull res then (if j < 0 then (if volatile.Contains r && not (argRegs.Contains r) then "?" elif argRegs.Contains r then "arg:" + rn r else "nv:" + rn r) else "?")
    else res

let stackArg (l: List<Instruction>) (k: int) (d: uint64) =
    let mutable m = k - 1
    let mutable s = null
    while isNull s && m >= 0 && k - m <= 120 do
        let i = l.[m]
        if i.Mnemonic = Mnemonic.Call then s <- "-"
        elif isStackStore i d then s <- slice l m (i.Op1Register.GetFullRegister()) 1
        m <- m - 1
    if isNull s then "-" else s

let out = new StreamWriter(Console.OpenStandardOutput())
out.AutoFlush <- false
for (fb, fe) in funcs do
    let l = try decodeRange fb fe with _ -> List<Instruction>()
    for k in 0 .. l.Count - 1 do
        let i = l.[k]
        if (i.Mnemonic = Mnemonic.Call || i.Mnemonic = Mnemonic.Jmp) && i.Op0Kind = OpKind.Memory && i.MemoryBase <> Register.RIP && i.MemoryBase <> Register.RSP
           && i.MemoryBase <> Register.None && i.MemoryIndex = Register.None && i.MemoryDisplacement64 % 8UL = 0UL && i.MemoryDisplacement64 <= 0x400UL then
            let vt = slice l k (i.MemoryBase.GetFullRegister()) 0
            if vt.EndsWith ">0" then
                let this = vt.Substring(0, vt.Length - 2)
                let a r = slice l k r 0
                let st = [ for d in 0x20UL .. 8UL .. 0x40UL -> stackArg l k d ] |> String.concat "\t"
                out.WriteLine(sprintf "V\t%X\t%X\t%X\t%s\t%s\t%s\t%s\t%s\t%s" fb i.IP i.MemoryDisplacement64 this
                                  (a Register.RCX) (a Register.RDX) (a Register.R8) (a Register.R9) st)
        elif i.Mnemonic = Mnemonic.Call && i.Op0Kind = OpKind.NearBranch64 then
            let a r = slice l k r 0
            let ks = [ Register.RCX; Register.RDX; Register.R8; Register.R9 ] |> List.map a
            out.WriteLine(sprintf "D\t%X\t%X\t%X\t%s" fb i.IP i.NearBranch64 (String.concat "\t" ks))
            // pointer-to-local-struct arguments: what was stored at its +0 before the call (P rows)
            for (rname, key) in List.zip [ "rcx"; "rdx"; "r8"; "r9" ] ks do
                if key.StartsWith "&S:" && key.Length <= 7 && Seq.forall Uri.IsHexDigit (key.Substring 3) then
                    let v = stackArg l k (Convert.ToUInt64(key.Substring 3, 16))
                    if v <> "-" then out.WriteLine(sprintf "P\t%X\t%X\t%X\t%s\t%s" fb i.IP i.NearBranch64 rname v)
        elif i.Mnemonic = Mnemonic.Mov && i.Op0Kind = OpKind.Memory && i.Op1Kind = OpKind.Register && i.MemoryIndex = Register.None
             && i.Op1Register.GetFullRegister() = i.Op1Register && i.Op1Register.IsGPR64() && i.MemoryBase <> Register.RSP then
            let d = i.MemoryDisplacement64
            let dst = if i.MemoryBase = Register.RIP then sprintf "G:%X" d else sprintf "%s>%X" (slice l k (i.MemoryBase.GetFullRegister()) 1) d
            let src = slice l k (i.Op1Register) 0
            if src.StartsWith "ret:" || src.StartsWith "S:" || src.Contains ">" || src.StartsWith "G:" || src.StartsWith "arg:" then
                out.WriteLine(sprintf "W\t%X\t%X\t%s\t%s" fb i.IP dst src)
out.Flush()
