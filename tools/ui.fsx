// Desktop driver under the signed dotnet host (no Add-Type temp DLLs → Smart App Control safe).
// usage: dotnet fsi ui.fsx <cmd> [args]
//   shot <out.png> [x y w h]     capture screen region (physical pixels)
//   focus <processName> [titleSubstring]
//   click <x> <y> | rclick <x> <y> | move <x> <y>
//   key <vk-hex...>               press chord, e.g. key 11 4C  (Ctrl+L)
//   keyseq <vk-hex|wMS>...        key presses one after another (50 ms down, arrows with the EXTENDED flag so DirectInput sees
//                                 DIK_UP/DOWN); wMS = wait MS ms. Used by testmatrix\run_pc.ps1 -LiveMenu
//   type <text>                   unicode text
//   scroll <x> <y> <notches>      wheel (negative = down)
#r "nuget: System.Drawing.Common, 9.0.0"
open System
open System.Diagnostics
open System.Runtime.InteropServices

[<DllImport("user32.dll")>] extern bool SetProcessDPIAware()
[<DllImport("user32.dll")>] extern bool SetCursorPos(int x, int y)
[<DllImport("user32.dll")>] extern void mouse_event(uint32 f, int dx, int dy, int data, nativeint extra)
[<DllImport("user32.dll")>] extern void keybd_event(byte vk, byte scan, uint32 f, nativeint extra)
[<DllImport("user32.dll")>] extern bool SetForegroundWindow(nativeint h)
[<DllImport("user32.dll")>] extern bool ShowWindow(nativeint h, int cmd)
[<DllImport("user32.dll")>] extern bool IsIconic(nativeint h)
[<DllImport("user32.dll")>] extern nativeint GetForegroundWindow()
[<DllImport("user32.dll")>] extern bool AttachThreadInput(uint32 a, uint32 b, bool attach)
[<DllImport("user32.dll")>] extern bool BringWindowToTop(nativeint h)
[<DllImport("kernel32.dll")>] extern uint32 GetCurrentThreadId()
type EnumWindowsProc = delegate of nativeint * nativeint -> bool
[<DllImport("user32.dll")>] extern bool EnumWindows(EnumWindowsProc cb, nativeint lp)
[<DllImport("user32.dll")>] extern bool IsWindowVisible(nativeint h)
[<DllImport("user32.dll", CharSet = CharSet.Unicode)>] extern int GetClassNameW(nativeint h, Text.StringBuilder s, int n)
[<DllImport("user32.dll", CharSet = CharSet.Unicode)>] extern int GetWindowTextW(nativeint h, Text.StringBuilder s, int n)
[<DllImport("user32.dll")>] extern uint32 GetWindowThreadProcessId(nativeint h, uint32& pid)
[<DllImport("user32.dll", CharSet = CharSet.Unicode)>] extern int16 VkKeyScanW(char c)

SetProcessDPIAware() |> ignore
let sleep (ms: int) = Threading.Thread.Sleep ms

let args = fsi.CommandLineArgs |> Array.skip 1
let i (k: int) = int args.[k]

[<DllImport("user32.dll")>] extern uint32 MapVirtualKeyW(uint32 code, uint32 mapType)
// Include hardware scan codes: games reading raw input / DirectInput ignore VK-only events.
let scan (vk: byte) = byte (MapVirtualKeyW(uint32 vk, 0u))
let keyDown (vk: byte) = keybd_event(vk, scan vk, 0u, 0n)
let keyUp (vk: byte) = keybd_event(vk, scan vk, 2u, 0n)

// SAFETY GUARD (2026-10-08): input commands (move/click/rclick/pan/scroll/key/keyseq/type) are only sent while the
// target game (env UI_TARGET, default Nioh3) is running AND owns the foreground window - checked before every click /
// key / mouse step. Otherwise nothing is sent and the process exits with code 3 ("REFUSED"), so a harness whose game
// died or lost focus can no longer click/type into VS Code or whatever else is in front.
let target = match Environment.GetEnvironmentVariable "UI_TARGET" with | null | "" -> "Nioh3" | s -> s
let guard (what: string) =
    let h = GetForegroundWindow()
    let mutable pid = 0u
    GetWindowThreadProcessId(h, &pid) |> ignore
    let procs = Process.GetProcessesByName target
    if procs.Length = 0 then
        eprintfn "REFUSED %s: %s is not running - no input sent" what target; exit 3
    if h = 0n || not (procs |> Array.exists (fun p -> uint32 p.Id = pid)) then
        eprintfn "REFUSED %s: %s is not the foreground window - no input sent" what target; exit 3

match args.[0] with
| "shot" ->
    let bounds =
        if args.Length >= 6 then Drawing.Rectangle(i 2, i 3, i 4, i 5)
        else Drawing.Rectangle(0, 0, 2560, 1440)
    use bmp = new Drawing.Bitmap(bounds.Width, bounds.Height)
    use g = Drawing.Graphics.FromImage bmp
    g.CopyFromScreen(bounds.X, bounds.Y, 0, 0, bounds.Size)
    bmp.Save(args.[1], Drawing.Imaging.ImageFormat.Png)
    printfn "saved %s %A" args.[1] bounds
| "focus" ->
    let title = if args.Length > 2 then args.[2] else ""
    let p =
        Process.GetProcessesByName args.[1]
        |> Array.filter (fun p -> p.MainWindowHandle <> 0n && p.MainWindowTitle.Contains title)
        |> Array.head
    let h = p.MainWindowHandle
    if IsIconic h then ShowWindow(h, 9) |> ignore
    // Borrow the foreground thread's input state so SetForegroundWindow is honoured.
    let mutable dummy = 0u
    let fgThread = GetWindowThreadProcessId(GetForegroundWindow(), &dummy)
    let me = GetCurrentThreadId()
    AttachThreadInput(me, fgThread, true) |> ignore
    BringWindowToTop h |> ignore
    SetForegroundWindow h |> ignore
    AttachThreadInput(me, fgThread, false) |> ignore
    sleep 150
    printfn "focused %s (foreground ok=%b)" p.MainWindowTitle (GetForegroundWindow() = h)
| "dialog" ->
    // dialog <processName>: print any standard dialog box (#32770) the process owns, e.g. the crash popup.
    let pids = Process.GetProcessesByName args.[1] |> Array.map (fun p -> uint32 p.Id) |> Set.ofArray
    let found = Collections.Generic.List<string>()
    let cb = EnumWindowsProc(fun h _ ->
        let mutable pid = 0u
        GetWindowThreadProcessId(h, &pid) |> ignore
        if pids.Contains pid && IsWindowVisible h then
            let cls = Text.StringBuilder(64)
            GetClassNameW(h, cls, 64) |> ignore
            if cls.ToString() = "#32770" then
                let title = Text.StringBuilder(256)
                GetWindowTextW(h, title, 256) |> ignore
                found.Add(title.ToString())
        true)
    EnumWindows(cb, 0n) |> ignore
    printfn "dialogs: %d %s" found.Count (String.Join(" | ", found))
| "fg" ->
    let h = GetForegroundWindow()
    let mutable pid = 0u
    GetWindowThreadProcessId(h, &pid) |> ignore
    printfn "foreground: pid=%d %s" pid (Process.GetProcessById(int pid).ProcessName)
| "move" -> guard "move"; SetCursorPos(i 1, i 2) |> ignore
| "click" | "rclick" ->
    guard args.[0]
    SetCursorPos(i 1, i 2) |> ignore
    sleep 80
    guard args.[0]
    let down, up = if args.[0] = "click" then 0x2u, 0x4u else 0x8u, 0x10u
    mouse_event(down, 0, 0, 0, 0n); sleep 50; mouse_event(up, 0, 0, 0, 0n)
| "pan" ->
    // pan <dx-per-step> <steps> <ms-per-step>: relative mouse motion (raw input sees it).
    for _ in 1 .. i 2 do
        guard "pan"
        mouse_event(0x1u, i 1, 0, 0, 0n)
        sleep (i 3)
| "scroll" ->
    guard "scroll"
    SetCursorPos(i 1, i 2) |> ignore
    sleep 50
    guard "scroll"
    mouse_event(0x800u, 0, 0, 120 * i 3, 0n)
| "key" ->
    let vks = args.[1..] |> Array.map (fun s -> Convert.ToByte(s, 16))
    guard "key"
    vks |> Array.iter keyDown
    sleep 40
    vks |> Array.rev |> Array.iter keyUp
| "keyseq" ->
    for a in args.[1..] do
        if a.StartsWith "w" then sleep (int (a.Substring 1))
        else
            let vk = Convert.ToByte(a, 16)
            guard "keyseq"
            let ext = if vk >= 0x21uy && vk <= 0x2Euy then 1u else 0u   // PgUp..Del block + arrows
            keybd_event(vk, scan vk, ext, 0n)
            sleep 50
            keybd_event(vk, scan vk, ext ||| 2u, 0n)
| "type" ->
    let text = String.Join(" ", args.[1..])
    for c in text do
        guard "type"
        let r = VkKeyScanW c
        let vk, shift = byte (r &&& 0xFFs), (r >>> 8) &&& 1s = 1s
        if shift then keyDown 0x10uy
        keyDown vk; keyUp vk
        if shift then keyUp 0x10uy
        sleep 10
| c -> failwithf "unknown cmd %s" c
