// ma_zero.fsx - reaching-definition proof that the source register of every MA site is zero on every path.
// Reads only re\Nioh3.dump.exe. CFG over the .pdata function range (all chained pieces of the function are included).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel
let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let u32 (o:int) = BitConverter.ToUInt32(img, o)
let pe = int (u32 0x3C)
let opt = pe + 24
let excRva = int (u32 (opt + 112 + 3*8))
let excSz = int (u32 (opt + 112 + 3*8 + 4))
let funcs = [ for i in 0 .. excSz/12 - 1 -> int (u32 (excRva + 12*i)), int (u32 (excRva + 12*i + 4)), int (u32 (excRva + 12*i + 8)) ]
// primary function = the one whose unwind info is not chained, pieces chained to it
let containing a = funcs |> List.find (fun (b, e, _) -> a >= b && a < e)
// root of a pdata piece: follow UNW_FLAG_CHAININFO (flags 4) to the parent RUNTIME_FUNCTION
let rec rootOf (b: int, e: int, u: int) =
    let flags = int img.[u] >>> 3
    if flags &&& 4 = 0 then (b, e, u) else
    let n = int img.[u + 2]
    let off = u + 4 + 2 * ((n + 1) &&& ~~~1)
    let pb, pe2, pu = int (u32 off), int (u32 (off + 4)), int (u32 (off + 8))
    rootOf (pb, pe2, pu)
let piecesOf a =
    let r = rootOf (containing a)
    funcs |> List.filter (fun f -> rootOf f = r) |> List.sortBy (fun (b, _, _) -> b), r
// sites: id, rva, zero-register (full family)
let sites = [
  for (id, r) in ["MA1a",0x4AC14;"MA1b",0x4AC2B;"MA1c",0x4AC42;"MA1d",0x4AECF;"MA1e",0x4AFAC;"MA1f",0x4B319;"MA1g",0x4B32C;"MA1h",0x4B33F] -> id, r, Register.R10
  for (id, r) in ["MA2a",0x419E8;"MA2b",0x41A10;"MA2c",0x41A28;"MA2d",0x41A38;"MA2e",0x41A4A;"MA2f",0x41A5A;"MA2g",0x41A6A;"MA2h",0x41A7A;
                  "MA2i",0x41A9A;"MA2j",0x41AAC;"MA2k",0x41AC3;"MA2l",0x41ACC;"MA2m",0x41AED;"MA2n",0x41AF6;"MA2o",0x41B12;"MA2p",0x41B7B] -> id, r, Register.RBP
  yield "MA3", 0x1E835B, Register.RDX ]
let full (r: Register) = r.GetFullRegister()
let volatileRegs = set [Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11]
let decodeRange (b: int) (e: int) =
    let d = Decoder.Create(64, ByteArrayCodeReader(img, b, e - b))
    d.IP <- uint64 b
    let l = List<Instruction>()
    while int d.IP < e do l.Add(d.Decode())
    l
let mutable allOk = true
let info = InstructionInfoFactory()
for (id, site, reg) in sites do
    let pieces, (fb, fe, _) = piecesOf site
    let ins = List<Instruction>()
    for (b, e, _) in pieces do ins.AddRange(decodeRange b e)
    let rootIdx = ins.FindIndex(fun x -> int x.IP = fb)
    let idx = Dictionary<uint64,int>()
    ins |> Seq.iteri (fun i x -> idx.[x.IP] <- i)
    if not (idx.ContainsKey(uint64 site)) then (printfn "%s: site not an instruction start" id; allOk <- false) else
    // state: 0 = unreached, 1 = zero, 2 = unknown, 3 = both  (bitmask: 1 zero, 2 unknown)
    let st = Array.create ins.Count 0
    let wl = Queue<int>()
    let push i s = if i >= 0 && i < ins.Count && (st.[i] ||| s) <> st.[i] then (st.[i] <- st.[i] ||| s; wl.Enqueue i)
    push rootIdx 2
    let next i = let x = ins.[i] in if idx.ContainsKey x.NextIP then idx.[x.NextIP] else -1
    let mutable indirect = false
    let mutable outside = 0
    while wl.Count > 0 do
        let i = wl.Dequeue()
        let x = ins.[i]
        let sIn = st.[i]
        let writes = info.GetInfo(&x).GetUsedRegisters() |> Seq.exists (fun u -> full u.Register = full reg && (u.Access = OpAccess.Write || u.Access = OpAccess.ReadWrite || u.Access = OpAccess.CondWrite || u.Access = OpAccess.ReadCondWrite))
        let isZero =
            (x.Mnemonic = Mnemonic.Xor && x.OpCount = 2 && x.Op0Kind = OpKind.Register && x.Op1Kind = OpKind.Register && x.Op0Register = x.Op1Register && full x.Op0Register = full reg && (x.Op0Register.GetSize() >= 4))
            || (x.Mnemonic = Mnemonic.Mov && x.Op0Kind = OpKind.Register && full x.Op0Register = full reg && x.Op0Register.GetSize() >= 4 && x.Op1Kind <> OpKind.Register && x.Op1Kind <> OpKind.Memory && x.GetImmediate(1) = 0UL)
        let isCall = x.FlowControl = FlowControl.Call || x.FlowControl = FlowControl.IndirectCall
        let sOut = if isZero then 1 elif writes then 2 elif isCall && volatileRegs.Contains(full reg) then 2 else sIn
        match x.FlowControl with
        | FlowControl.Return | FlowControl.Interrupt | FlowControl.Exception -> ()
        | FlowControl.IndirectBranch -> indirect <- true
        | FlowControl.UnconditionalBranch ->
            let t = x.NearBranchTarget
            if idx.ContainsKey t then push idx.[t] sOut else outside <- outside + 1
        | FlowControl.ConditionalBranch ->
            let t = x.NearBranchTarget
            if idx.ContainsKey t then push idx.[t] sOut else outside <- outside + 1
            push (next i) sOut
        | _ -> push (next i) sOut
    let si = idx.[uint64 site]
    let ok = st.[si] = 1 && not indirect
    if not ok then allOk <- false
    printfn "%-5s %7X  fn %X..%X (%d pieces)  %s at site: %s%s%s" id site fb fe pieces.Length (reg.ToString()) (match st.[si] with 1 -> "ZERO on every path" | 0 -> "UNREACHED" | 2 -> "NOT ZERO" | _ -> "MIXED (zero on some paths only)") (if indirect then "  [indirect jmp in range!]" else "") (if outside > 0 then sprintf "  (%d branches leave the piece)" outside else "")
printfn "%s" (if allOk then "MA ZERO-SOURCE PROOF: ALL SITES OK" else "MA ZERO-SOURCE PROOF: FAILED")
