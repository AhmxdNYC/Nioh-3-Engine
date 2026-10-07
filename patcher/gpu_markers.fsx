// gpu_markers.fsx - switch the engine's OWN debug/PIX markers on, so an RGP / PIX capture shows named passes
// ("Cascade 0", "VirtualShadowmap", "Clipmap 2", "Tile 3 5", "Batch Tiles 8", "MeshletInstanceCulling",
//  "MeshletPreculling", "GenerateShadowMapHiZ", render-task-graph node names ...). See notes\shadows-culling.md Â§4.
//
//   dotnet fsi gpu_markers.fsx offline      NO GAME ACCESS: every site/byte this relies on is checked in re\Nioh3.dump.exe
//   dotnet fsi gpu_markers.fsx dry          READ-ONLY on the live game: g, both flags, marker-interface slot, cmd list
//   dotnet fsi gpu_markers.fsx on           WRITES (FT_ALLOW_WRITE=1): replay flag, then record flag = 1
//   dotnet fsi gpu_markers.fsx off          WRITES: record flag, then (100 ms later) replay flag = 0
//   dotnet fsi gpu_markers.fsx status       READ-ONLY: both flags sampled 10 x 100 ms (shows whether the engine resets them)
//   dotnet fsi gpu_markers.fsx revert       WRITES: the two original bytes saved by `on` (state file, same process only)
//
// Default OFF: nothing is written unless `on` is run. It is a diagnostic, not a performance patch: the look cannot change
// (markers carry no rendering state), and it costs some CPU (one vsnprintf + one token per marker on [App]Game, one
// MultiByteToWideChar + ID3D12GraphicsCommandList::BeginEvent per marker on SynergisticDispatcherTask). Turn it off after
// the capture.
//
// How the engine's markers work (Steam v2.02, RVAs):
//   g = [0x45BA588] = KtglContext == GfxDevice (one object).  SynCtx (dispatcher context) = g + 0x10.
//   Record side: GfxBeginMarker 0x1D3AD8(ctx, fmt, ...) returns at once unless byte [ctx+0x2A62] != 0; then it formats
//     the name, derives a colour and writes token 0xA5 (+string) into the stream at [ctx+0x3C40]. GfxEndMarker 0x113CA4
//     and the inline end sites (535188 / 1239860 ...) write token 0xA6 under the same byte. 25 code sites test
//     [reg+0x2A62]; none writes it (it is a boot-time debug switch).
//   Replay side (SynergisticDispatcherTask, token table 0x3C02220): tok 0xA5 = SynTokBeginEvent 0x114024 (-> SynBeginEvent
//     0x113E64) and tok 0xA6 =
//     SynTokEndEvent 0x113FC8 return at once unless byte [SynCtx+0x542] != 0; then: if the marker interface
//     [0x4BD09C0] (set by tok 0x8E, 0xA96D90) is non-null they call it, else ID3D12GraphicsCommandList::BeginEvent
//     (vtbl+0x1C8, UTF-16 name) / EndEvent (vtbl+0x1D0) on [SynCtx+0x580]. tok 0xA0 (0xA96D70) is the engine's own
//     setter of [SynCtx+0x542] (from the stream); nothing in the exported code emits it.
//   So both bytes are needed: record g+0x2A62 and replay g+0x10+0x542 = g+0x552.
//
// Safety:
//   - Two aligned single-byte data writes, no code. `on` writes replay first, so a token can never be recorded while
//     the dispatcher would skip it in a way that unbalances begin/end; `off` clears record first and replay 100 ms later.
//   - The only conceivable side effect of toggling mid-frame is ONE frame with an unbalanced Begin/End pair on one
//     command list (no validation in the release D3D12 runtime). Toggle, then wait a second before capturing.
//   - RGP: whether the AMD driver turns D3D12 BeginEvent into RGP user markers depends on the driver/RGP version; PIX
//     and RenderDoc always show them. Note: the engine passes its colour (0xFFrrggbb) as the BeginEvent Metadata; tools
//     that expect Metadata 0 (UTF-16) may print the colour or skip the string - check the first capture.
#load "Mem.fsx"
open System
open System.IO
open Mem

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "offline"
let DUMP = Path.Combine(__SOURCE_DIRECTORY__, @"..\re\Nioh3.dump.exe")
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "gpu_markers_state.txt")

let RVA_G = 0x45BA588          // g_RenderGlobals / KtglContext pointer slot
let OFF_REC = 0x2A62           // record flag (KtglContext)
let OFF_SYN = 0x10             // SynCtx embedded in the device
let OFF_REP = OFF_SYN + 0x542  // replay flag (SynCtx+0x542)
let OFF_CL = OFF_SYN + 0x580   // SynCtx+0x580 = the open ID3D12GraphicsCommandList
let OFF_STREAM = 0x3C40        // token stream write pointer
let RVA_IFACE = 0x4BD09C0      // optional marker interface (tok 0x8E)
let RVA_TOKTAB = 0x3C02220     // KtglTokenHandlerTable (185 x 8-byte pointers)

// (rva, expected bytes, what) - every code fact the tool depends on
let SITES = [
    0x1D3B0E, "38 99 62 2A 00 00",          "GfxBeginMarker: cmp [rcx+2A62h],bl (record flag test)"
    0x113CA4, "80 B9 62 2A 00 00 00",       "GfxEndMarker entry: cmp byte [rcx+2A62h],0"
    0x113E8F, "44 38 B1 42 05 00 00",       "SynBeginEvent: cmp [rcx+542h],r14b (replay flag test)"
    0x11409A, "E8 C5 FD FF FF",             "SynTokBeginEvent 114024: call SynBeginEvent 113E64"
    0x113F4B, "FF 90 C8 01 00 00",          "SynBeginEvent: call [rax+1C8h] = ID3D12GraphicsCommandList::BeginEvent"
    0x113FD5, "80 B9 42 05 00 00 00",       "SynTokEndEvent: cmp byte [rcx+542h],0"
    0x113F76, "80 B9 42 05 00 00 00",       "tok handler 113F70 (second EndEvent path, vtbl+1D0): cmp byte [rcx+542h],0"
    0xA96D81, "44 88 81 42 05 00 00",       "tok 0xA0 handler: mov [rcx+542h],r8b (engine's own replay-flag setter)" ]

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = b |> Array.map (sprintf "%02X") |> String.concat " "

let offline () =
    let img = File.ReadAllBytes DUMP
    let pe = BitConverter.ToInt32(img, 0x3C)
    let imageBase = BitConverter.ToUInt64(img, pe + 24 + 24)
    let mutable ok = true
    let check (c: bool) (msg: string) = printfn "%s %s" (if c then "OK  " else "FAIL") msg; if not c then ok <- false
    for (rva, exp, what) in SITES do
        let e = hexb exp
        let got = img.[rva .. rva + e.Length - 1]
        check (got = e) (sprintf "%X %-22s %s (have %s)" rva exp what (hexs got))
    // token table entries
    let tok i = BitConverter.ToUInt64(img, RVA_TOKTAB + i * 8)
    for (i, rva, nm) in [ 0xA5, 0x114024, "SynTokBeginEvent (unpacks the name, calls SynBeginEvent 113E64)"; 0xA6, 0x113FC8, "SynTokEndEvent"; 0xA0, 0xA96D70, "SynTokSetMarkerReplay"; 0x8E, 0xA96D90, "SynTokSetMarkerIface" ] do
        let v = tok i
        check (v = imageBase + uint64 rva) (sprintf "token table[%02X] = %X (expect base+%X %s)" i v rva nm)
    // every reference to +2A62 must be a read (cmp): scan .text for disp32 0x2A62 used as [reg+disp] by a store
    let pe2 = pe
    let nsec = int (BitConverter.ToUInt16(img, pe2 + 6))
    let optsz = int (BitConverter.ToUInt16(img, pe2 + 20))
    let mutable reads, writes = 0, 0
    for i in 0 .. nsec - 1 do
        let sh = pe2 + 24 + optsz + i * 40
        let vs = BitConverter.ToInt32(img, sh + 8)
        let va = BitConverter.ToInt32(img, sh + 12)
        let ch = BitConverter.ToUInt32(img, sh + 36)
        if ch &&& 0x20000000u <> 0u then
            let dec = Iced.Intel.Decoder.Create(64, Iced.Intel.ByteArrayCodeReader(img, va, vs))
            dec.IP <- uint64 va
            while dec.IP < uint64 (va + vs) do
                let ins = dec.Decode()
                if ins.OpCount > 0 && ins.MemoryBase <> Iced.Intel.Register.RIP && ins.MemoryDisplacement64 = 0x2A62UL then
                    if ins.Op0Kind = Iced.Intel.OpKind.Memory && ins.Mnemonic <> Iced.Intel.Mnemonic.Cmp then writes <- writes + 1
                    else reads <- reads + 1
    check (reads >= 20 && writes = 0) (sprintf "+2A62 references: %d reads, %d writes (expect only reads: boot-time switch)" reads writes)
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILED")
    if not ok then exit 1

let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"

let live () =
    use t = attach "Nioh3"
    let b = t.Base
    let g = nativeint (t.ReadI64(b + nativeint RVA_G))
    if g = 0n then failwith "g ([Nioh3+45BA588]) is null - not in a level yet?"
    let rd off = (t.Read(g + nativeint off, 1)).[0]
    let show () =
        let cl = t.ReadI64(g + nativeint OFF_CL)
        let stream = t.ReadI64(g + nativeint OFF_STREAM)
        let iface = t.ReadI64(b + nativeint RVA_IFACE)
        printfn "g=%X  record[g+2A62]=%d  replay[g+552]=%d  cmdlist[g+590]=%X  stream[g+3C40]=%X  iface[4BD09C0]=%X"
            g (rd OFF_REC) (rd OFF_REP) cl stream iface
        cl, stream
    match mode with
    | "dry" ->
        let cl, stream = show ()
        // plausibility: the command list is a COM object whose vtable is outside Nioh3's image (d3d12/driver)
        if cl <> 0L then
            let vt = t.ReadI64(nativeint cl)
            let inImg = vt >= int64 b && vt < int64 b + int64 t.ModuleSize
            printfn "cmdlist vtable %X (%s)" vt (if inImg then "INSIDE Nioh3 image - unexpected, do not use `on`" else "outside the game image: OK")
        else printfn "cmdlist pointer is 0 right now (between lists) - run dry again"
        if stream = 0L then printfn "stream pointer is 0 - wrong object? do not use `on`"
        for (rva, exp, what) in SITES do
            let e = hexb exp
            let got = t.Read(b + nativeint rva, e.Length)
            printfn "%s %X %s" (if got = e then "OK  " else "DIFF") rva what
    | "status" ->
        for _ in 1 .. 10 do show () |> ignore; Threading.Thread.Sleep 100
    | "on" ->
        writeGuard ()
        if not (File.Exists stateFile) then
            File.WriteAllText(stateFile, sprintf "%d %X %d %d" t.Process.Id g (rd OFF_REC) (rd OFF_REP))
        t.Write(g + nativeint OFF_REP, [| 1uy |])
        Threading.Thread.Sleep 50
        t.Write(g + nativeint OFF_REC, [| 1uy |])
        show () |> ignore
        printfn "markers ON - wait ~1 s, then capture (RGP/PIX). Turn off with `off`."
    | "off" ->
        writeGuard ()
        t.Write(g + nativeint OFF_REC, [| 0uy |])
        Threading.Thread.Sleep 100
        t.Write(g + nativeint OFF_REP, [| 0uy |])
        show () |> ignore
    | "revert" ->
        writeGuard ()
        if not (File.Exists stateFile) then failwith "no gpu_markers_state.txt - nothing to revert"
        let v = File.ReadAllText(stateFile).Trim().Split(' ')
        if int v.[0] <> t.Process.Id then failwith "state file belongs to another game process (flags reset at boot anyway)"
        if Convert.ToInt64(v.[1], 16) <> int64 g then failwith "g moved since `on` - refusing"
        t.Write(g + nativeint OFF_REC, [| byte (int v.[2]) |])
        Threading.Thread.Sleep 100
        t.Write(g + nativeint OFF_REP, [| byte (int v.[3]) |])
        File.Delete stateFile
        show () |> ignore
    | m -> failwithf "unknown mode %s (offline|dry|status|on|off|revert)" m

if mode = "offline" then offline () else live ()
