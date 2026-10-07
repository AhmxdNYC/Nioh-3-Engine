// pool_patch.fsx — P12: lift the "12 or fewer logical CPUs" worker-pool penalty (Nioh 3 v2.02).
//
// fn 1FE2FD4 sizes the engine's 74 worker pools once at boot. For CPUs with < 13 logical processors it then
// takes 3 threads away from pool 3 (KIDS_RESOURCE) and pool 5 (StageUpdate):
//   1FE30D5  41 83 FB 0C   cmp r11d, 0Ch    ; r11d = logical CPU count (clamped 6..32)
//   1FE30D9  7F 2B         jg  skip-penalty
// Patch: imm 0Ch -> 00h, so the penalty never applies (7600X: KIDS_RESOURCE 7 -> 10, StageUpdate 4 -> 7).
//
// The code only runs during startup, so this must be written before it runs. Start this script FIRST, then
// launch the game: it waits for Nioh3.exe, waits until SteamStub has decrypted .text (the original bytes show
// up), and writes the byte immediately. The exe on disk is never touched.
//   dotnet fsi pool_patch.fsx [timeoutSeconds]     (needs FT_ALLOW_WRITE=1)
#load "Mem.fsx"
open System
open System.Diagnostics
open Mem

if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let timeout = match fsi.CommandLineArgs |> Array.tryItem 1 with Some s -> float s | None -> 180.0
let SITE = 0x1FE30D5n
let ORIG = [| 0x41uy; 0x83uy; 0xFBuy; 0x0Cuy |]
let NEW  = [| 0x41uy; 0x83uy; 0xFBuy; 0x00uy |]
let sw = Stopwatch.StartNew()
let fail msg = printfn "P12 FAILED: %s (%.2f s)" msg sw.Elapsed.TotalSeconds; exit 1

printfn "P12 waiting for Nioh3.exe ..."
let mutable proc : Process = null
while isNull proc do
    if sw.Elapsed.TotalSeconds > timeout then fail "game did not start"
    proc <- Process.GetProcessesByName "Nioh3" |> Array.tryHead |> Option.toObj
    if isNull proc then Threading.Thread.Sleep 2
let tStart = sw.Elapsed.TotalSeconds
let mutable t : Target option = None
while t.IsNone do
    if sw.Elapsed.TotalSeconds - tStart > 30.0 then fail "could not open the game process"
    try
        let x = new Target(proc)
        x.Base |> ignore
        t <- Some x
    with _ -> Threading.Thread.Sleep 1
let tg = t.Value
let addr = tg.Base + SITE
let mutable cur = [||]
let mutable polls = 0
while cur <> ORIG do
    if sw.Elapsed.TotalSeconds - tStart > 60.0 then fail (sprintf "original bytes never appeared (last %s)" (BitConverter.ToString cur))
    if proc.HasExited then fail "game exited"
    cur <- (try tg.Read(addr, 4) with _ -> [||])
    if cur = NEW then fail "already patched in this process"
    polls <- polls + 1
tg.Write(addr, NEW)
printfn "P12 written %.0f ms after the process appeared (%d polls); bytes now %s" ((sw.Elapsed.TotalSeconds - tStart) * 1000.0) polls (BitConverter.ToString(tg.Read(addr, 4)))
