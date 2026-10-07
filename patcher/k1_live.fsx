// k1_live.fsx - K1: the KTGL main-thread kick threshold KtglContext::CmdFlushLimit (+0x3C70)  (notes\gpu-scheduling.md §B)
//
//   dotnet fsi k1_live.fsx offline              NO GAME ACCESS. Checks every code fact this script relies on in re\Nioh3.dump.exe.
//   dotnet fsi k1_live.fsx dry|status [sec]     READ-ONLY on the game: limit, ring, and the engine's own per-frame stream
//                                               stats (draws, kicks, end-of-frame drain wait, ring-full wait) for [sec] s.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi k1_live.fsx apply <N>    write CmdFlushLimit = N (32..8192); original saved.
//   $env:FT_ALLOW_WRITE=1; dotnet fsi k1_live.fsx revert       write the saved original back (same game process only).
// Default: nothing is written unless `apply` is run. Memory only; restart = stock.
//
// What K1 is (all v2.02 RVAs, verified by `offline`):
//   * Every draw-recording path bumps CmdCountSinceFlush (+0x3C74) and compares it with CmdFlushLimit (+0x3C70), e.g.
//     DrawSubmit 1D3106: inc [rsi+3C78h] (draws this frame); inc [rsi+3C74h]; mov eax,[rsi+3C74h]; cmp eax,[rsi+3C70h];
//     jnc -> 1D3186: xor edx,edx; call KtglCmdStreamFlush 254110 (submit = 0).
//   * KtglCmdStreamFlush(ctx, submit) pushes the token segment to the dispatcher ring (ctx+3BF8h, capacity +3C00h,
//     write +3C04h, read +3C08h, in flight +3C10h), ReleaseSemaphore(+3BF0h), ++kicks (+3C7Ch). If the ring is full
//     the MAIN thread spins on Sleep(0) (timed into the stats block +3C88h).
//   * submit = 0 at every K1 site (29 sites); only 4 explicit sites (253EE0, 3D2CDF, 6D7FD9, 1A1786F), fence points
//     (KTGL token 1, handler 8F0B44) and the frame-end kick (418C70) make the dispatcher close + ExecuteCommandLists.
//     So K1 changes WHEN the dispatcher records, NOT how many D3D12 lists are submitted.
//   * The value is copied once at init (5D7023: mov eax,[rdi+804h]; mov [rdi+3C70h],eax) and only read afterwards, so
//     a 4-byte aligned data write takes effect on the next draw (the compare is >=; a lower value just kicks at once).
//   * CmdStreamEndFrame 418B94 copies the per-frame stats +3C78h..+3CF8h to +3D00h..+3D80h every frame:
//     +3D00 draws (u32), +3D04 kicks (u32), +3D10 ring-full wait {u64 total, u64 max, u32 n} (QPC ticks),
//     +3D28 end-of-frame drain spin 418F40 {total, max, n}. `status` reads exactly these.
// Expected effect (notes §B): lower N -> the dispatcher starts earlier, the frame-end drain spin shrinks, explicit
// submits leave earlier by the dispatcher's backlog; cost = more kicks (ReleaseSemaphore ~1-2 us on main + one
// dispatcher wake each, i.e. CPU watts on the Ally). Higher N -> fewer wakes, longer drain. A/B both directions.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
open System
open System.IO
open System.Diagnostics
open Iced.Intel
open Mem

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "k1_state.txt")
let DUMP = @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"

let G_DEVICE = 0x45BA588n            // g_RenderGlobals (aka g_pGfxDevice): GfxDevice == KtglContext*
let OFF_LIMIT, OFF_COUNT = 0x3C70, 0x3C74
let OFF_RING_CAP, OFF_RING_W, OFF_RING_R, OFF_RING_INFL = 0x3C00, 0x3C04, 0x3C08, 0x3C10
let OFF_GEN = 0x3C60                 // += 1 per frame end (CmdStreamFinalKick_IncGeneration 418D3D)
let SNAP = 0x3D00                    // previous frame's stream stats
let MIN_N, MAX_N = 32, 8192

let hexs (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let SITE_K1 = 0x1D3106, hexs "FF 86 78 3C 00 00 FF 86 74 3C 00 00 8B 86 74 3C 00 00 3B 86 70 3C 00 00 73 66"
let SITE_KICK = 0x1D3186, hexs "33 D2 48 8B CE E8 80 0F 08 00"
let SITE_INIT = 0x5D701D, hexs "8B 87 04 08 00 00 89 87 70 3C 00 00"
let SITE_FLUSH_TEST = 0x254158, hexs "84 D2 0F 85 EA 00 00 00"

// ---------------------------------------------------------------------------------------------------------------
let offline () =
    let img = File.ReadAllBytes DUMP
    let mutable bad = 0
    let check name (rva: int, b: byte[]) =
        let ok = img.[rva .. rva + b.Length - 1] = b
        if not ok then bad <- bad + 1
        printfn "  %-38s %X  %s" name rva (if ok then "OK" else "MISMATCH " + BitConverter.ToString(img.[rva .. rva + b.Length - 1]))
    printfn "K1 offline checks against %s" DUMP
    check "DrawSubmit K1 compare" SITE_K1
    check "DrawSubmit K1 kick (submit=0)" SITE_KICK
    check "init copy +804h -> +3C70h" SITE_INIT
    check "KtglCmdStreamFlush test dl,dl" SITE_FLUSH_TEST
    // every `cmp r32,[r64+3C70h]` in .text: the K1 sites sharing the limit
    let mutable n = 0
    for i in 0x1000 .. 0x38E0000 - 6 do
        if img.[i] = 0x3Buy && (img.[i + 1] &&& 0xC0uy) = 0x80uy && (img.[i + 1] &&& 7uy) <> 4uy
           && img.[i + 2] = 0x70uy && img.[i + 3] = 0x3Cuy && img.[i + 4] = 0uy && img.[i + 5] = 0uy then n <- n + 1
    printfn "  cmp r32,[reg+3C70h] sites in .text: %d (every one reads the same limit)" n
    // the stats snapshot in CmdStreamEndFrame 418B94: rbx = dev+3C78h (418BA9), rcx = rbx+88h = dev+3D00h (418BBE), 0x88 B copied
    check "EndFrame: rbx = dev+3C78h" (0x418BA9, hexs "48 81 C3 78 3C 00 00")
    check "EndFrame: rcx = rbx+88h (= dev+3D00h)" (0x418BBE, hexs "48 8D 8B 88 00 00 00")
    // 4-byte alignment of the field relative to a 16-aligned object (the device is a heap object)
    printfn "  +3C70h is 16-byte aligned within the object: %b (plain 4-byte store is atomic)" (OFF_LIMIT % 4 = 0)
    if bad = 0 then printfn "K1 OFFLINE: ALL PASSED" else printfn "K1 OFFLINE: %d FAILED" bad
    bad = 0

// ---------------------------------------------------------------------------------------------------------------
let u32 (t: Target) (a: nativeint) = BitConverter.ToUInt32(t.Read(a, 4), 0)
let u64 (t: Target) (a: nativeint) = BitConverter.ToUInt64(t.Read(a, 8), 0)
let device (t: Target) =
    let d = nativeint (t.ReadI64(t.Base + G_DEVICE))
    if d = 0n then failwith "g_RenderGlobals is NULL (renderer not up yet)"
    d
let liveSites (t: Target) =
    [ SITE_K1; SITE_KICK; SITE_INIT; SITE_FLUSH_TEST ]
    |> List.forall (fun (rva, b) -> t.Read(t.Base + nativeint rva, b.Length) = b)

let status (t: Target) (secs: float) =
    if not (liveSites t) then failwith "code at the K1 sites differs from v2.02 - wrong game version, nothing read"
    let d = device t
    let ticksUs (x: uint64) = float x * 1e6 / float Stopwatch.Frequency
    printfn "GfxDevice %X  CmdFlushLimit %d  CmdCountSinceFlush %d  ring cap %d (w %d r %d inflight %d)"
        d (u32 t (d + nativeint OFF_LIMIT)) (u32 t (d + nativeint OFF_COUNT)) (u32 t (d + nativeint OFF_RING_CAP))
        (u32 t (d + nativeint OFF_RING_W)) (u32 t (d + nativeint OFF_RING_R)) (u32 t (d + nativeint OFF_RING_INFL))
    let sw = Stopwatch.StartNew()
    let mutable lastGen = u32 t (d + nativeint OFF_GEN)
    let rows = ResizeArray<uint32 * uint32 * float * float>()
    while sw.Elapsed.TotalSeconds < secs do
        let g = u32 t (d + nativeint OFF_GEN)
        if g <> lastGen then
            lastGen <- g
            let s = t.Read(d + nativeint SNAP, 0x40)
            let draws, kicks = BitConverter.ToUInt32(s, 0), BitConverter.ToUInt32(s, 4)
            let ringWait = BitConverter.ToUInt64(s, 0x10)
            let drain = BitConverter.ToUInt64(s, 0x28)
            rows.Add((draws, kicks, ticksUs ringWait, ticksUs drain))
        Threading.Thread.Sleep 1
    if rows.Count = 0 then printfn "no frame ended in %.1f s (menu / loading?)" secs
    else
        let med (f: _ -> float) = rows |> Seq.map f |> Seq.sort |> Seq.item (rows.Count / 2)
        printfn "frames sampled %d (%.0f fps):" rows.Count (float rows.Count / secs)
        printfn "  draws/frame   median %.0f" (med (fun (a, _, _, _) -> float a))
        printfn "  kicks/frame   median %.0f   (dispatcher hand-offs; draws/kick = %.0f)" (med (fun (_, b, _, _) -> float b))
            (med (fun (a, b, _, _) -> if b = 0u then 0.0 else float a / float b))
        printfn "  ring-full wait median %.1f us/frame (main thread Sleep(0) because the dispatcher ring was full)" (med (fun (_, _, c, _) -> c))
        printfn "  frame-end drain median %.1f us/frame (418F40 busy spin until the dispatcher is idle)" (med (fun (_, _, _, e) -> e))
        printfn "  (timer = QPC if the engine's [g+12h] flag is set, else clock() ms; QPC assumed above)"

match mode with
| "offline" -> offline () |> ignore
| "dry" | "status" ->
    let secs = if args.Length > 1 then float args.[1] else 3.0
    use t = attach "Nioh3"
    status t secs
| "apply" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    let n = if args.Length > 1 then int args.[1] else failwith "usage: apply <N>"
    if n < MIN_N || n > MAX_N then failwithf "N must be %d..%d" MIN_N MAX_N
    use t = attach "Nioh3"
    if not (liveSites t) then failwith "code at the K1 sites differs from v2.02 - nothing written"
    let d = device t
    let a = d + nativeint OFF_LIMIT
    let orig = u32 t a
    let saved =
        if File.Exists stateFile then
            let p = File.ReadAllText(stateFile).Trim().Split(' ')
            if p.Length = 3 && int p.[0] = t.Process.Id then Some (uint32 p.[2]) else None
        else None
    let original = defaultArg saved orig
    File.WriteAllText(stateFile, sprintf "%d %X %d" t.Process.Id d original)
    t.Write(a, BitConverter.GetBytes(uint32 n))
    printfn "K1 CmdFlushLimit %d -> %d (original %d saved to %s); now %d" orig n original stateFile (u32 t a)
| "revert" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
    if not (File.Exists stateFile) then failwith "no k1_state.txt - nothing to revert"
    let p = File.ReadAllText(stateFile).Trim().Split(' ')
    use t = attach "Nioh3"
    if int p.[0] <> t.Process.Id then
        printfn "state file is from another game process - the game restarted, so it is already stock"; File.Delete stateFile
    else
        let d = device t
        if d <> nativeint (Convert.ToInt64(p.[1], 16)) then failwith "device pointer changed (renderer re-created) - restart the game instead"
        t.Write(d + nativeint OFF_LIMIT, BitConverter.GetBytes(uint32 p.[2]))
        File.Delete stateFile
        printfn "K1 reverted: CmdFlushLimit = %d" (u32 t (d + nativeint OFF_LIMIT))
| m -> printfn "unknown mode %s (offline | dry | status [sec] | apply <N> | revert)" m
