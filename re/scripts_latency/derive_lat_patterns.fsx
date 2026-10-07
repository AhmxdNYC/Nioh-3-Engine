// derive_lat_patterns.fsx - copy of mod/Nioh3EngineFix/tools/derive_patterns.fsx (core unchanged) for the input-latency
// sites; PRINTS patches.json rows instead of writing the file (mod/ is owned by another agent).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Text
open Iced.Intel

let dumpPath = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let outPath = Path.Combine(__SOURCE_DIRECTORY__, "..", "patches.json")
let img = File.ReadAllBytes dumpPath
let u16 o = int (BitConverter.ToUInt16(img, o))
let u32 o = BitConverter.ToUInt32(img, o)
let pe = int (u32 0x3C)
let nsec = u16 (pe + 6)
let timeStamp = u32 (pe + 8)
let opt = pe + 24
let sizeOfImage = u32 (opt + 56)
let secTab = opt + u16 (pe + 20)
let execSecs =
    [ for i in 0 .. nsec - 1 do
        let s = secTab + 40 * i
        let va, vs, ch = int (u32 (s + 12)), int (u32 (s + 8)), u32 (s + 36)
        if ch &&& 0x20000000u <> 0u then yield va, vs ]
let dd = opt + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let pBegin = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12))
let pEnd = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12 + 4))
let funcRange (rva: int) =
    let i = Array.BinarySearch(pBegin, uint32 rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && uint32 rva < pEnd.[i] then Some (int pBegin.[i], int pEnd.[i]) else None

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

// ---- spec: id, group, rva, original bytes, kind, extra json fields -----------------------------------------
// kinds: bytes (fixed replacement `new`), call (E8 rel32 -> near trampoline -> `target`, at `call_at`, nop padded),
//        ripload (`opcode` + rel32 -> near data slot `data`, e.g. mov eax,[ticks120])
type Site = { Id: string; Group: string; Rva: int; Orig: string; Kind: string; Extra: (string * string) list }
let S id g rva orig kind extra = { Id = id; Group = g; Rva = rva; Orig = orig; Kind = kind; Extra = extra }
let str (s: string) = "\"" + s + "\""
let spec = [
    S "IK"  "IK" 0x5AB6CF "E8 DC 00 00 00" "call" [ "target", str "hookInputKickEnd"; "call_at", "0"; "reads", str "callee_rel32@1" ]
    S "IKc" "IK" 0x5ACAAA "E8 69 F5 2B 00" "call" [ "target", str "hookPadStageWait"; "call_at", "0"; "reads", str "callee_rel32@1" ]
    S "TSl" "TS" 0x5AD2BB "49 89 17 0F 57 C9" "call" [ "target", str "caveTsLimiterExit"; "call_at", "0" ]
    S "TSw" "TS" 0x4A52D4 "E8 57 02 D6 FF" "call" [ "target", str "caveTsRenderBegin"; "call_at", "0"; "reads", str "callee_rel32@1" ]
    S "TSb" "TS" 0x4A535E "E8 D1 39 21 00" "call" [ "target", str "caveTsAfterWaitable"; "call_at", "0"; "reads", str "callee_rel32@1" ]
    S "TSp" "TS" 0x3D6552 "FF 15 F0 BD 50 03" "call" [ "target", str "caveTsPresentHandoff"; "call_at", "0"; "reads", str "iat_disp32@2" ]
]
// ---- instruction decoding with wildcard masks ----------------------------------------------------------------
type Ins = { Ip: int; Len: int; Wild: bool[] }
let decodeFn (rva: int) =
    let s, e = match funcRange rva with Some r -> r | None -> failwithf "no pdata function for %X" rva
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, s, e - s))
    dec.IP <- uint64 s
    [| while dec.IP < uint64 e do
         let ins = dec.Decode()
         let co = dec.GetConstantOffsets(&ins)
         let w = Array.zeroCreate ins.Length
         if ins.IsIPRelativeMemoryOperand && co.HasDisplacement then
             for k in 0 .. int co.DisplacementSize - 1 do w.[int co.DisplacementOffset + k] <- true
         let isRel32Branch = (ins.Op0Kind = OpKind.NearBranch64) && co.HasImmediate && co.ImmediateSize = 4uy
         if isRel32Branch then for k in 0 .. 3 do w.[int co.ImmediateOffset + k] <- true
         yield { Ip = int ins.IP; Len = ins.Length; Wild = w } |]

let countMatches (pat: byte[]) (wild: bool[]) (limit: int) =
    // anchor on the longest exact run, vectorised IndexOf, then verify the rest
    let mutable best, bestLen, cur, curStart = 0, 0, 0, 0
    for i in 0 .. pat.Length - 1 do
        if wild.[i] then cur <- 0
        else
            if cur = 0 then curStart <- i
            cur <- cur + 1
            if cur > bestLen then best <- curStart; bestLen <- cur
    let anchor = ReadOnlySpan(pat, best, bestLen).ToArray()
    let mutable n = 0
    let hits = Collections.Generic.List<int>()
    for (va, vs) in execSecs do
        let mutable pos = va
        let fin = va + vs
        while pos < fin && n < limit do
            let idx = MemoryExtensions.IndexOf(ReadOnlySpan(img, pos, fin - pos), ReadOnlySpan(anchor))
            if idx < 0 then pos <- fin
            else
                let start = pos + idx - best
                if start >= va && start + pat.Length <= fin then
                    let mutable ok = true
                    let mutable k = 0
                    while ok && k < pat.Length do
                        if not wild.[k] && img.[start + k] <> pat.[k] then ok <- false
                        k <- k + 1
                    if ok then n <- n + 1; hits.Add start
                pos <- pos + idx + 1
    n, List.ofSeq hits

let derive (s: Site) =
    let orig = hexb s.Orig
    let cur = img.[s.Rva .. s.Rva + orig.Length - 1]
    if cur <> orig then failwithf "%s: dump bytes at %X are %s, expected %s" s.Id s.Rva (hexs cur) s.Orig
    let ins = decodeFn s.Rva
    let first = ins |> Array.findIndex (fun i -> i.Ip = s.Rva)
    let mutable last = first
    while ins.[last].Ip + ins.[last].Len < s.Rva + orig.Length do last <- last + 1
    if ins.[last].Ip + ins.[last].Len <> s.Rva + orig.Length then failwithf "%s: site does not end on an instruction boundary" s.Id
    let build a b =
        let st = ins.[a].Ip
        let en = ins.[b].Ip + ins.[b].Len
        let wild = Array.concat [ for i in a .. b -> ins.[i].Wild ]
        img.[st .. en - 1], wild, st
    let mutable a, b = first, last
    let mutable result = None
    let mutable step = 0
    while result.IsNone do
        let pat, wild, st = build a b
        let exact = wild |> Array.filter not |> Array.length
        if exact >= 12 then
            let n, hits = countMatches pat wild 2
            if n = 1 then result <- Some (pat, wild, st)
            elif n = 0 then failwithf "%s: pattern matches nothing (bug)" s.Id
        if result.IsNone then
            if step % 2 = 0 && b + 1 < ins.Length then b <- b + 1
            elif a > 0 then a <- a - 1
            else b <- b + 1
            step <- step + 1
            if (ins.[b].Ip + ins.[b].Len) - ins.[a].Ip > 96 then failwithf "%s: no unique pattern within 96 bytes" s.Id
    let pat, wild, st = result.Value
    let pstr = String.Join(" ", [ for i in 0 .. pat.Length - 1 -> if wild.[i] then "??" else pat.[i].ToString("X2") ])
    pstr, s.Rva - st

for s in spec do
    let pstr, off = derive s
    let extra = String.Join("", [ for (k, v) in s.Extra -> sprintf ", \"%s\": %s" k v ])
    printfn "    { \"id\": \"%s\", \"group\": \"%s\", \"rva\": \"0x%X\", \"len\": %d, \"orig\": \"%s\", \"kind\": \"%s\"%s,\n      \"pattern\": \"%s\", \"offset\": %d }," s.Id s.Group s.Rva (hexb s.Orig).Length s.Orig s.Kind extra pstr off
