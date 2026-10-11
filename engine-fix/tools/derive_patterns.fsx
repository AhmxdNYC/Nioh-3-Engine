// derive_patterns.fsx - DEV TOOL (offline, reads only the decrypted dump; never touches the game).
// Produces ..\patches.json, the single source of truth for every patch site of Nioh3EngineFix.
//
//   dotnet fsi derive_patterns.fsx [path\to\Nioh3.dump.exe]
//
// For every site in `spec` below it decodes the containing function (from .pdata) with Iced, then builds an AOB
// pattern from whole instructions around the site: rel32 branch/call targets and RIP-relative displacements are
// wildcarded (they move between builds), everything else is exact. The window grows one instruction at a time
// (after, then before) until the pattern matches exactly once in the executable sections. The site's own original
// bytes are recorded exactly in `orig` (the mod checks them before writing).
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Text
open Iced.Intel

let dumpPath = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else @"re\Nioh3.dump.exe"
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
    // W1: zero-fill loop on write-combined memory: `and dword [rbx+rax*4],0` RMW -> rep stosd (notes\cpu-profile.md)
    S "W1" "W1" 0x61BAC "33 C9 41 0F B6 06 41 03 C7 03 C1 FF C1 83 24 83 00 41 3B CF 72 EC" "bytes"
        [ "new", str "41 0F B6 06 41 03 C7 48 8D 3C 83 44 89 F9 31 C0 F3 AB 0F 1F 40 00" ]
    // W2a-d: quad writer `and dword [z],0` + `movss [y]` -> one `movq [y]` store (no read of WC memory)
    S "W2a" "W2" 0x13721F "83 63 08 00 80 7C 24 58 00 F3 0F 11 43 04" "bytes" [ "new", str "80 7C 24 58 00 66 0F D6 43 04 0F 1F 40 00" ]
    S "W2b" "W2" 0x137269 "83 63 34 00 80 7C 24 58 00 F3 0F 11 4B 30" "bytes" [ "new", str "80 7C 24 58 00 66 0F D6 4B 30 0F 1F 40 00" ]
    S "W2c" "W2" 0x1372B4 "83 63 60 00 80 7C 24 58 00 F3 0F 11 4B 5C" "bytes" [ "new", str "80 7C 24 58 00 66 0F D6 4B 5C 0F 1F 40 00" ]
    S "W2d" "W2" 0x1372FE "83 A3 8C 00 00 00 00 48 8B 6C 24 38 48 8B 74 24 40 F3 0F 11 8B 88 00 00 00" "bytes"
        [ "new", str "48 8B 6C 24 38 48 8B 74 24 40 66 0F D6 8B 88 00 00 00 0F 1F 80 00 00 00 00" ]
    // VRR v2 + P9 (notes\frame-timing.md 5-final, 8, 9). All-or-nothing group.
    S "P3a" "VRR" 0x43593A "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P3b" "VRR" 0x437EE5 "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P3c" "VRR" 0x438473 "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P3d" "VRR" 0x55DC56 "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P3e" "VRR" 0x5AF6C9 "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P3f" "VRR" 0x29770C6 "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P3g" "VRR" 0x29771B4 "8D 42 78 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks120" ]
    S "P6" "VRR" 0x1975BAA "8D 42 3C 41 F7 F0" "ripload" [ "opcode", str "8B 05"; "data", str "ticks60" ]
    S "P8a" "VRR" 0x20E77B "E8 14 7E E3 FF" "call" [ "target", str "caveTier"; "call_at", "0" ]
    S "P8b" "VRR" 0x5AF526 "E8 69 70 A9 FF" "call" [ "target", str "caveTier"; "call_at", "0" ]
    S "P8c" "VRR" 0x5AF682 "E8 0D 6F A9 FF" "call" [ "target", str "caveTier"; "call_at", "0" ]
    S "P8d" "VRR" 0x5AF69F "E8 F0 6E A9 FF" "call" [ "target", str "caveTier"; "call_at", "0" ]
    S "P9" "VRR" 0x5AD89B "8B 87 14 06 00 00" "call" [ "target", str "caveTier"; "call_at", "0" ]
    S "P2" "VRR" 0x5AD6AA "F3 0F 11 97 24 06 00 00" "call" [ "target", str "caveScale"; "call_at", "0" ]
    S "P4" "VRR" 0x5AD860 "8B 87 14 06 00 00" "call" [ "target", str "caveLim"; "call_at", "0" ]
    S "P1" "VRR" 0x5AD677 "44 89 87 14 06 00 00" "call" [ "target", str "caveFps"; "call_at", "0" ]
    // P7 (optional, VRR menuQuantize): call GetDelta60 -> caveMenu (callee taken from the original rel32)
    S "P7" "VRRMENU" 0x43312B "E8 54 33 C1 FF" "call" [ "target", str "caveMenu"; "call_at", "0"; "reads", str "callee_rel32@1" ]
    // W3: WinMain pump `call [PeekMessageA]` -> nop; call hookPeek (IAT slot taken from the original disp32)
    S "W3" "W3" 0x1A6BF2D "FF 15 95 6A E7 01" "call" [ "target", str "hookPeek"; "call_at", "1"; "reads", str "iat_disp32@2" ]
    // R1: render-begin `jmp +2; pause; xor edx,edx` -> `mov edx,[r1Timeout]`, loop jne -2
    S "R1" "R1" 0x4A5346 "EB 02 F3 90 33 D2" "ripload" [ "opcode", str "8B 15"; "data", str "r1Timeout" ]
    S "R1j" "R1" 0x4A5357 "75 EF" "bytes" [ "new", str "75 ED" ]
    // L1: limiter slow path `xor edx,edx; mov rcx,rsi; call 916EBC` -> call caveL1; nop5 (callee from rel32)
    S "L1" "L1" 0x19DF57E "33 D2 48 8B CE E8 34 79 F3 FE" "call" [ "target", str "caveL1"; "call_at", "0"; "reads", str "callee_rel32@6" ]
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

let esc (s: string) = s.Replace("\\", "\\\\").Replace("\"", "\\\"")
let sb = StringBuilder()
let ap (x: string) = sb.Append(x) |> ignore
ap "{\n"
ap "  \"_comment\": \"GENERATED by tools/derive_patterns.fsx from the decrypted v2.02 dump. Single source of truth: src/patches_gen.h is generated from this file (tools/gen_header.py) and tests/test_patterns.py verifies it.\",\n"
ap (sprintf "  \"build\": { \"name\": \"Nioh 3 v2.02 (Steam)\", \"timestamp\": \"0x%08X\", \"size_of_image\": \"0x%X\" },\n" timeStamp sizeOfImage)
ap "  \"sites\": [\n"
spec |> List.iteri (fun i s ->
    let pstr, off = derive s
    printfn "%-4s %-8s %8X  off %2d  %s" s.Id s.Group s.Rva off pstr
    let extra = String.Join("", [ for (k, v) in s.Extra -> sprintf ", \"%s\": %s" k v ])
    ap (sprintf "    { \"id\": \"%s\", \"group\": \"%s\", \"rva\": \"0x%X\", \"len\": %d, \"orig\": \"%s\", \"kind\": \"%s\"%s,\n      \"pattern\": \"%s\", \"offset\": %d }%s\n"
            s.Id s.Group s.Rva (hexb s.Orig).Length s.Orig s.Kind extra pstr off (if i < spec.Length - 1 then "," else "")))
ap "  ]\n}\n"
File.WriteAllText(outPath, sb.ToString())
printfn "wrote %s" (Path.GetFullPath outPath)
