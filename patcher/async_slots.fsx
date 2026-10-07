// async_slots.fsx - AS: KTGL async-compute slot experiment (notes\gpu-scheduling.md §A.4). EXPERIMENT, DEFAULT OFF.
//
//   dotnet fsi async_slots.fsx offline            NO GAME ACCESS. Verifies every site in re\Nioh3.dump.exe and the slot table
//                                                 in the installed Nioh3.exe (read-only).
//   dotnet fsi async_slots.fsx dry|status         READ-ONLY on a running game: gate byte, table, and the slot enables the
//                                                 renderer actually used (runtime copy written at init).
//   $env:FT_ALLOW_WRITE=1; dotnet fsi async_slots.fsx arm <slots> [timeoutSec]
//                                                 BOOT RACE (like pool_patch.fsx): start it FIRST, then launch the game. Waits
//                                                 for Nioh3.exe, waits until SteamStub has decrypted .text, checks the renderer
//                                                 has not initialised yet, then enables the given slots, e.g. `arm 0`.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi async_slots.fsx revert
//                                                 original bytes back. Only matters for the NEXT renderer init: restart the game.
//
// What it changes. The renderer init FUN_2269EC4 reads a 13-entry static table at .data 0x44C04D0 (12 B per slot:
// u8 enable, f32 param, u32 slot id) and builds the async-compute slot mask (sched = device-side object +0x1130,
// mask at sched+0x1CC). For every enabled slot FUN_32FA450 -> 2EE256C -> 2EF8354 -> 2EF8448 creates a KTGL async compute
// context with its OWN D3D12 COMPUTE queue, allocators, lists, root signature and descriptor heaps. Each slot-aware
// subsystem then asks AsyncSlotGetCtx 52BE1C(sched, slot): non-NULL -> it records into the async context and publishes a
// fence (AsyncSlotAddFence 148514) the graphics side waits on; NULL -> the same work goes into the graphics stream
// bracketed by tokens 0x7C/0x7D. Same shaders, same data, only the queue differs.
// Shipped state on disk: slots 0, 3, 4, 6 enabled. At init:
//   226A490  cmp dword [rbp+70h],10DEh ; je 226A4AD   <- NOT NVIDIA: every entry whose slot id is 0 gets enable = 0
//   226A571  NVIDIA with driver <= 0x1C92: all 13 enables = 0
// So on AMD (PC RX 9070 and the Ally) slot 0 is switched off by vendor; slots 3/4/6 are the three COMPUTE queues seen in
// RGP (CMP35/36/37: VCLOUD, VFSCAT volumetric fog, ocean PFB/IGW). Slot 0 is the effect path: when it has a context,
// 32FA450 also starts the KIDS job "KIDS_ASync_LLCulling_Effect" (callback 3D16AC).
//   arm 0   : 226A497  74 14 -> EB 14 (skip the non-NVIDIA slot-0 rule) - the engine's own NVIDIA behaviour on AMD.
//   arm N>0 : table enable byte of slot N 00 -> 01 (slots 1,2,5,7..12 are off for every vendor: research only).
// Risks: KT disabled slot 0 on non-NVIDIA on purpose (reason unknown: driver bug, hang, or slower). Possible outcomes:
// faster, slower, one-frame-late effects, corruption, or a GPU hang (TDR). Run it only for an A/B in a test session.
// Watch: RGP must show a 4th COMPUTE queue and the 1x1x1 effect dispatch chains (GFX segs 70-80 in r5) leaving GFX.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
open System
open System.IO
open System.Diagnostics
open Mem

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "async_slots_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let EXE = @"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3.exe"

let hexs (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let GATE = 0x226A490
let GATE_ORIG = hexs "81 7D 70 DE 10 00 00 74 14"
let JCC = 0x226A497
let SLOT0_LOOP = 0x226A499, hexs "48 8B C6 39 78 08 75 03 40 88 38 48 83 C0 0C 48 3B C1 75 EF"
let TABLE_LEA = 0x226A467, hexs "48 8D 35 62 60 25 02"            // lea rsi,[44C04D0h]
let MASK_LOOP = 0x226A4BC, hexs "8B 4A 08 41 3B C8 75 0F 40 38 3A 74 0A B8 01 00 00 00 D3 E0 44 0B C8"
let COPY_LEA = 0x226A4E0, hexs "4C 8D 35 79 5F 35 02"             // lea r14,[45C0460h] (runtime copy, BSS)
let CALL_ALLOC = 0x226A5B9, hexs "E8 92 FE 08 01"                  // call 32FA450 (create the async contexts)
let GETCTX = 0x52BE1C
let TABLE, COPY, NSLOT, STRIDE = 0x44C04D0, 0x45C0460, 13, 12
// AsyncSlotGetCtx(sched, slot) callers with a constant slot (re\scripts_d3d12\slotargs.fsx 52be1c)
let USERS = [ 0, "3D0C67 (fn 3D0AE0), 3D16E1 (3D16AC = KIDS_ASync_LLCulling_Effect job), 3D37FD (3D3350), 544634, 194E70E, 19CFAFC, 32FA505"
              3, "5F328D (fn 5F2A88)"; 4, "14906E (fn 14871C)"; 5, "3658F73 (fn 3658D34)"; 6, "1A25524 (fn 1A2550E)"
              7, "52D65F (fn 52D4C4)"; 8, "19CB677 (lea edx,[r14+8])"; 9, "10D19C, 1A408F4, 32FA4CC"
              10, "52853, 104B54, 1063E6, 193DC34, 1947180, 1A41879, 347AF60, 3675307, 52B727, 32FA4EA"
              11, "52B142 (fn 52AF94)"; 12, "418643 (fn 418284)" ]

let fileOffset (img: byte[]) (rva: int) =
    let pe = BitConverter.ToInt32(img, 0x3C)
    let n = int (BitConverter.ToUInt16(img, pe + 6))
    let so = int (BitConverter.ToUInt16(img, pe + 20))
    let sec = pe + 24 + so
    [ 0 .. n - 1 ] |> List.pick (fun i ->
        let o = sec + 40 * i
        let vs, va, rs, ra = BitConverter.ToInt32(img, o + 8), BitConverter.ToInt32(img, o + 12), BitConverter.ToInt32(img, o + 16), BitConverter.ToInt32(img, o + 20)
        if rva >= va && rva < va + max vs rs then Some (ra + rva - va) else None)

let offline () =
    let img = File.ReadAllBytes DUMP
    let mutable bad = 0
    let check name (rva: int, b: byte[]) =
        let ok = img.[rva .. rva + b.Length - 1] = b
        if not ok then bad <- bad + 1
        printfn "  %-44s %X  %s" name rva (if ok then "OK" else "MISMATCH")
    printfn "AS offline checks (dump %s)" DUMP
    check "vendor gate cmp [rbp+70h],10DEh / je" (GATE, GATE_ORIG)
    check "non-NVIDIA slot-0 disable loop" SLOT0_LOOP
    check "lea rsi, slot table 44C04D0" TABLE_LEA
    check "mask loop (id == index && enable)" MASK_LOOP
    check "lea r14, runtime copy 45C0460" COPY_LEA
    check "call 32FA450 (create async contexts)" CALL_ALLOC
    check "AsyncSlotGetCtx 52BE1C body" (GETCTX, hexs "8B 81 CC 01 00 00")
    // shipped table, from the installed exe (read-only)
    if File.Exists EXE then
        let exe = File.ReadAllBytes EXE
        let fo = fileOffset exe TABLE
        let rows = [ for s in 0 .. NSLOT - 1 -> exe.[fo + s * STRIDE], BitConverter.ToSingle(exe, fo + s * STRIDE + 4), BitConverter.ToInt32(exe, fo + s * STRIDE + 8) ]
        let idsOk = rows |> List.mapi (fun i (_, _, id) -> id = i) |> List.forall id
        let en = rows |> List.mapi (fun i (e, _, _) -> if e <> 0uy then Some i else None) |> List.choose id
        printfn "  installed Nioh3.exe table @file %X: ids 0..12 in order %b, enabled slots %A, slot6 param %.2f" fo idsOk en (let (_, p, _) = rows.[6] in p)
        if not idsOk || en <> [ 0; 3; 4; 6 ] then (bad <- bad + 1; printfn "  TABLE DIFFERS from v2.02 expectation [0;3;4;6]")
        // the gate bytes may be SteamStub-encrypted on disk; only report
        let g = exe.[fileOffset exe GATE .. fileOffset exe GATE + GATE_ORIG.Length - 1]
        printfn "  gate bytes on disk: %s (%s)" (BitConverter.ToString g) (if g = GATE_ORIG then "plain" else "encrypted by SteamStub - normal")
    else printfn "  (installed exe not found; table check skipped)"
    printfn "  slot users (AsyncSlotGetCtx call sites):"
    for (s, u) in USERS do printfn "    slot %2d: %s" s u
    if bad = 0 then printfn "AS OFFLINE: ALL PASSED" else printfn "AS OFFLINE: %d FAILED" bad

let status (t: Target) =
    let b = t.Base
    let g = t.Read(b + nativeint GATE, GATE_ORIG.Length)
    let gateState = if g = GATE_ORIG then "stock (slot 0 off on AMD)" elif g.[7] = 0xEBuy then "PATCHED (slot-0 rule skipped)" else "UNKNOWN " + BitConverter.ToString g
    let tbl = t.Read(b + nativeint TABLE, NSLOT * STRIDE)
    let cp = t.Read(b + nativeint COPY, NSLOT * STRIDE)
    let en (x: byte[]) = [ for s in 0 .. NSLOT - 1 do if x.[s * STRIDE] <> 0uy then yield s ]
    let inited = cp |> Array.exists (fun x -> x <> 0uy)
    printfn "gate %X: %s" (int64 b + int64 JCC) gateState
    printfn "table enables now      : %A" (en tbl)
    printfn "runtime copy (init used): %s" (if inited then sprintf "%A" (en cp) else "not written yet (renderer not initialised)")

let writeEnables (t: Target) (slots: int list) =
    let b = t.Base
    let saved = ResizeArray<string>()
    for s in slots do
        if s = 0 then
            let a = b + nativeint JCC
            let old = t.Read(a, 2)
            if old <> [| 0x74uy; 0x14uy |] then failwithf "gate bytes %s unexpected" (BitConverter.ToString old)
            safeWrite t [ (a, [| 0xEBuy; 0x14uy |], old) ] |> ignore
            saved.Add(sprintf "%X 74" JCC)
        let a = b + nativeint (TABLE + s * STRIDE)
        let old = t.Read(a, 1).[0]
        if old = 0uy then
            t.Write(a, [| 1uy |])
            saved.Add(sprintf "%X 00" (TABLE + s * STRIDE))
    File.WriteAllText(stateFile, sprintf "%d\n%s" t.Process.Id (String.Join("\n", saved)))

match mode with
| "offline" -> offline ()
| "dry" | "status" -> use t = attach "Nioh3" in status t
| "arm" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    let slots = if args.Length > 1 then args.[1].Split(',') |> Array.map int |> List.ofArray else failwith "usage: arm <slot[,slot]> (0 = the vendor-disabled effect slot)"
    if slots |> List.exists (fun s -> s < 0 || s >= NSLOT) then failwith "slots are 0..12"
    if slots |> List.exists (fun s -> s <> 0) then printfn "WARNING: slots other than 0 never ship enabled on PC - research only"
    let timeout = if args.Length > 2 then float args.[2] else 180.0
    let sw = Stopwatch.StartNew()
    printfn "AS waiting for Nioh3.exe (start the game now) ..."
    let mutable proc : Process = null
    while isNull proc do
        if sw.Elapsed.TotalSeconds > timeout then failwith "game did not start"
        proc <- Process.GetProcessesByName "Nioh3" |> Array.tryHead |> Option.toObj
        if isNull proc then Threading.Thread.Sleep 2
    let t0 = sw.Elapsed.TotalSeconds
    let mutable tg : Target option = None
    while tg.IsNone do
        if sw.Elapsed.TotalSeconds - t0 > 30.0 then failwith "could not open the game process"
        try (let x = new Target(proc) in x.Base |> ignore; tg <- Some x) with _ -> Threading.Thread.Sleep 1
    let t = tg.Value
    let mutable g = [||]
    while g <> GATE_ORIG do
        if sw.Elapsed.TotalSeconds - t0 > 60.0 then failwith "gate bytes never appeared (SteamStub / version?)"
        if proc.HasExited then failwith "game exited"
        g <- (try t.Read(t.Base + nativeint GATE, GATE_ORIG.Length) with _ -> [||])
        if g.Length > 7 && g.[7] = 0xEBuy then failwith "already patched in this process"
    if t.Read(t.Base + nativeint COPY, NSLOT * STRIDE) |> Array.exists (fun x -> x <> 0uy) then
        failwith "TOO LATE: the renderer already initialised (runtime copy written) - nothing written; restart and arm earlier"
    writeEnables t slots
    printfn "AS armed slots %A %.0f ms after the process appeared; check later with `status` (runtime copy must list them)" slots ((sw.Elapsed.TotalSeconds - t0) * 1000.0)
| "revert" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    if not (File.Exists stateFile) then failwith "no async_slots_state.txt"
    let lines = File.ReadAllLines stateFile
    use t = attach "Nioh3"
    if int lines.[0] <> t.Process.Id then (printfn "state is from another process: the game restarted, it is stock"; File.Delete stateFile)
    else
        for l in lines |> Array.skip 1 |> Array.filter (fun l -> l.Trim() <> "") do
            let p = l.Split(' ')
            let a = t.Base + nativeint (Convert.ToInt32(p.[0], 16))
            let v = Convert.ToByte(p.[1], 16)
            if Convert.ToInt32(p.[0], 16) = JCC then safeWrite t [ (a, [| v |], t.Read(a, 1)) ] |> ignore else t.Write(a, [| v |])
        File.Delete stateFile
        printfn "AS reverted (takes effect at the next renderer init = next launch)"
| m -> printfn "unknown mode %s (offline | dry | status | arm <slots> [sec] | revert)" m
