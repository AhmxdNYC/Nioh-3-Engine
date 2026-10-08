// testpopup.fsx - "Nioh 3 test running" popup + tray icon with an opt-in input blocker (dotnet fsi; no Add-Type / temp DLLs,
// Smart App Control safe). Started by vpad.ps1 Start-TestPopup (abba2_pc.ps1, run_tour.ps1, vpad_live.ps1, playsession).
//
//   * small always-on-top window (never takes focus) bottom-right: harness name, current step, ~minutes left, a BIG toggle button
//     "Block my input" / "Release"; the same toggle on the tray icon's left click (right click = menu).
//   * blocking is OFF by default and only ever turned on by the user's click. While on, low-level keyboard + mouse hooks swallow
//     REAL input (events without LLKHF_INJECTED / LLMHF_INJECTED), so the harness's own injected input still works. Mouse input over
//     the popup always passes (the Release button stays clickable). With -Input vpad mouse MOVES pass too (an unfocused game never
//     reads the mouse); clicks, wheel and keys are swallowed. With the SendInput path everything real is swallowed.
//   * release: the button, the tray icon, Ctrl+Alt+Shift+F12 (always), automatically when the harness / owner process ends, after
//     --block-max-min (default 30) of blocking, and when the popup exits. Ctrl+Alt+Del always works (Windows never routes it to
//     hooks). If this process hangs, Windows drops its low-level hooks by itself (LowLevelHooksTimeout).
//   * real controller: while blocking with -Input vpad the mod is told 'realpads off' over its pipe (VirtualPad = 2 merge mode;
//     VirtualPad = 1 already hides real pads for the whole session) and 'realpads on' at release.
//
// usage: dotnet fsi testpopup.fsx --name abba2 --harness-pid N [--owner-pid N] [--status-file F] [--max-min M] [--input vpad|ui]
//                                 [--no-block] [--block-max-min 30] [--pipe \\.\pipe\nioh3vpad]
//        dotnet fsi testpopup.fsx --selftest      (offline: decision table, hotkey state, timers; hooks run in log-only mode)
// status file: line 1 = current step, line 2 = optional ISO time the run should end; "DONE" on line 1 closes the popup.
#load "testpopup_refs.fsx"   // written by vpad.ps1 Start-TestPopup: #r of the installed Microsoft.WindowsDesktop.App (WinForms)
open System
open System.Diagnostics
open System.Drawing
open System.IO
open System.Runtime.InteropServices
open System.Windows.Forms

// ---- args ----
let argv = fsi.CommandLineArgs |> Array.skip 1
let arg (k: string) (d: string) = match Array.tryFindIndex ((=) k) argv with Some i when i + 1 < argv.Length -> argv.[i + 1] | _ -> d
let flag (k: string) = Array.contains k argv
let selftest = flag "--selftest"
let name = arg "--name" "test"
let harnessPid = int (arg "--harness-pid" "0")
let ownerPid = int (arg "--owner-pid" "0")
let statusFile = arg "--status-file" ""
let maxMin = float (arg "--max-min" "180")
let inputMode = arg "--input" "vpad"
let noBlock = flag "--no-block"
let blockMaxMin = float (arg "--block-max-min" "30")
let pipeName = arg "--pipe" @"\\.\pipe\nioh3vpad"
let logFile = arg "--log" ""
let t0 = DateTime.Now
let log (s: string) =
    let l = sprintf "[%s] %s" (DateTime.Now.ToString "HH:mm:ss") s
    if logFile <> "" then (try File.AppendAllText(logFile, l + "\r\n") with _ -> ())
    if selftest then printfn "%s" l

// ---- decisions (pure; selftest checks them) ----
type Act = Pass | Swallow | Release
// keyboard: injected always passes; Ctrl+Alt+Shift+F12 (real, key down) releases; else swallow while blocking.
// gameOnly (vpad): only keys going to the game (game window in the foreground) are swallowed; other programs keep working.
let decideKey (blocking: bool) (injected: bool) (vk: int) (down: bool) (ctrl: bool) (alt: bool) (shift: bool) (gameOnly: bool) (gameFocused: bool) =
    if injected then Pass
    elif down && vk = 0x7B && ctrl && alt && shift then (if blocking then Release else Pass)
    elif blocking && (not gameOnly || gameFocused) then Swallow
    else Pass
// mouse: injected / over the popup always pass; gameOnly (vpad): only input over the game window is swallowed;
// moves pass when allowed (vpad); else swallow while blocking
let decideMouse (blocking: bool) (injected: bool) (overPopup: bool) (isMove: bool) (allowMove: bool) (gameOnly: bool) (overGame: bool) =
    if not blocking || injected || overPopup then Pass
    elif gameOnly && not overGame then Pass
    elif isMove && allowMove then Pass
    else Swallow
// modifier tracking from REAL key events (swallowed keys never reach the async key state)
type Mods() =
    member val Ctrl = false with get, set
    member val Alt = false with get, set
    member val Shift = false with get, set
    member m.Update(vk: int, down: bool) =
        match vk with
        | 0x11 | 0xA2 | 0xA3 -> m.Ctrl <- down
        | 0x12 | 0xA4 | 0xA5 -> m.Alt <- down
        | 0x10 | 0xA0 | 0xA1 -> m.Shift <- down
        | _ -> ()
let procAlive (pid: int) =
    if pid <= 0 then true
    else
        try
            let p = Process.GetProcessById pid
            not p.HasExited
        with _ -> false
let minutesLeft (now: DateTime) (endIso: string) =
    match DateTime.TryParse endIso with
    | true, e -> Some(max 0.0 (e - now).TotalMinutes)
    | _ -> None

// ---- Win32 ----
type LowLevelProc = delegate of int * nativeint * nativeint -> nativeint
[<Struct; StructLayout(LayoutKind.Sequential)>]
type KBDLLHOOKSTRUCT =
    val mutable vkCode: uint32
    val mutable scanCode: uint32
    val mutable flags: uint32
    val mutable time: uint32
    val mutable extra: nativeint
[<Struct; StructLayout(LayoutKind.Sequential)>]
type POINT =
    val mutable x: int
    val mutable y: int
[<Struct; StructLayout(LayoutKind.Sequential)>]
type MSLLHOOKSTRUCT =
    val mutable pt: POINT
    val mutable mouseData: uint32
    val mutable flags: uint32
    val mutable time: uint32
    val mutable extra: nativeint
[<DllImport("user32.dll", SetLastError = true)>] extern nativeint SetWindowsHookExW(int id, LowLevelProc fn, nativeint hmod, uint32 tid)
[<DllImport("user32.dll")>] extern bool UnhookWindowsHookEx(nativeint h)
[<DllImport("user32.dll")>] extern nativeint CallNextHookEx(nativeint h, int code, nativeint w, nativeint l)
[<DllImport("kernel32.dll", CharSet = CharSet.Unicode)>] extern nativeint GetModuleHandleW(string n)
[<DllImport("user32.dll")>] extern nativeint WindowFromPoint(POINT p)
[<DllImport("user32.dll")>] extern nativeint GetAncestor(nativeint h, uint32 flags)
[<DllImport("user32.dll")>] extern nativeint GetForegroundWindow()
[<DllImport("user32.dll")>] extern uint32 GetWindowThreadProcessId(nativeint h, uint32& pid)

// ---- the popup ----
type Popup() =
    inherit Form()
    override _.ShowWithoutActivation = true
    override _.CreateParams =
        let cp = base.CreateParams
        cp.ExStyle <- cp.ExStyle ||| 0x08000000 ||| 0x00000080 ||| 0x00000008  // NOACTIVATE | TOOLWINDOW | TOPMOST
        cp

let mutable blocking = false
let mutable blockSince = DateTime.MinValue
let mutable hkKb = 0n
let mutable hkMs = 0n
let mutable popupHandle = 0n
let mutable exiting = false
let mods = Mods()
let allowMove = inputMode = "vpad"
let gameOnly = inputMode = "vpad"   // the SendInput path ('ui') needs the game in the foreground, so it keeps the global block
let mutable releaseRequested = false
let mutable swallowed = 0
let mutable passedInjected = 0
// Nioh3 pids, refreshed by the UI timer (never enumerated inside a hook callback)
let mutable gamePids : int[] = [||]
let refreshGamePids () = gamePids <- (try Process.GetProcessesByName "Nioh3" |> Array.map (fun p -> p.Id) with _ -> [||])
let isGameWindow (h: nativeint) =
    if h = 0n || gamePids.Length = 0 then false
    else
        let mutable pid = 0u
        GetWindowThreadProcessId(h, &pid) |> ignore
        Array.contains (int pid) gamePids

let kbProc =
    LowLevelProc(fun code w l ->
        if code >= 0 then
            let k = Marshal.PtrToStructure<KBDLLHOOKSTRUCT> l
            let msg = int w
            let down = msg = 0x100 || msg = 0x104
            let up = msg = 0x101 || msg = 0x105
            let injected = (k.flags &&& 0x10u) <> 0u
            if not injected && (down || up) then mods.Update(int k.vkCode, down)
            let gameFocused = gameOnly && isGameWindow (GetForegroundWindow())
            match decideKey blocking injected (int k.vkCode) down mods.Ctrl mods.Alt mods.Shift gameOnly gameFocused with
            | Release -> releaseRequested <- true; (if selftest then CallNextHookEx(0n, code, w, l) else 1n)
            | Swallow -> swallowed <- swallowed + 1; (if selftest then CallNextHookEx(0n, code, w, l) else 1n)
            | Pass -> (if injected then passedInjected <- passedInjected + 1); CallNextHookEx(0n, code, w, l)
        else CallNextHookEx(0n, code, w, l))
let msProc =
    LowLevelProc(fun code w l ->
        if code >= 0 then
            let m = Marshal.PtrToStructure<MSLLHOOKSTRUCT> l
            let injected = (m.flags &&& 0x1u) <> 0u
            let root = GetAncestor(WindowFromPoint m.pt, 2u)  // GA_ROOT
            let over = popupHandle <> 0n && root = popupHandle
            let overGame = gameOnly && isGameWindow root
            match decideMouse blocking injected over (int w = 0x200) allowMove gameOnly overGame with
            | Swallow -> swallowed <- swallowed + 1; (if selftest then CallNextHookEx(0n, code, w, l) else 1n)
            | _ -> CallNextHookEx(0n, code, w, l)
        else CallNextHookEx(0n, code, w, l))

let sendPipe (cmd: string) =
    if inputMode = "vpad" && not selftest then
        Threading.Tasks.Task.Run(fun () ->
            try
                use c = new Pipes.NamedPipeClientStream(".", pipeName.Replace(@"\\.\pipe\", ""), Pipes.PipeDirection.InOut)
                c.Connect 1000
                let b = Text.Encoding.UTF8.GetBytes(cmd + "\n")
                c.Write(b, 0, b.Length)
                let r = Array.zeroCreate<byte> 512
                let n = c.Read(r, 0, r.Length)
                log (sprintf "pipe %s -> %s" cmd (Text.Encoding.UTF8.GetString(r, 0, n).Trim()))
            with e -> log (sprintf "pipe %s: %s" cmd e.Message)) |> ignore

let form = new Popup()
form.Text <- "Nioh 3 test"
form.FormBorderStyle <- FormBorderStyle.FixedToolWindow
form.TopMost <- true
form.ShowInTaskbar <- false
form.StartPosition <- FormStartPosition.Manual
form.ClientSize <- Size(440, 190)
form.BackColor <- Color.FromArgb(32, 32, 36)
form.ForeColor <- Color.White
let wa = Screen.PrimaryScreen.WorkingArea
form.Location <- Point(wa.Right - form.Width - 16, wa.Bottom - form.Height - 16)
let title = new Label(AutoSize = false, Location = Point(12, 8), Size = Size(416, 24), Font = new Font("Segoe UI", 11.0f, FontStyle.Bold))
let stepL = new Label(AutoSize = false, Location = Point(12, 34), Size = Size(416, 40), Font = new Font("Segoe UI", 9.0f))
let btn = new Button(Location = Point(12, 80), Size = Size(416, 58), Font = new Font("Segoe UI Emoji", 14.0f, FontStyle.Bold), FlatStyle = FlatStyle.Flat)
let help = new Label(AutoSize = false, Location = Point(12, 142), Size = Size(416, 44), Font = new Font("Segoe UI", 8.0f), ForeColor = Color.Silver)
form.Controls.AddRange [| title :> Control; stepL; btn; help |]
let tray = new NotifyIcon(Icon = SystemIcons.Shield, Visible = true)
let menu = new ContextMenuStrip()
let miToggle = menu.Items.Add "Block my input"
let miShow = menu.Items.Add "Show / hide the popup"
menu.Items.Add "Release key: Ctrl+Alt+Shift+F12 (Ctrl+Alt+Del always works)" |> ignore
tray.ContextMenuStrip <- menu

let refresh () =
    if noBlock then
        btn.Visible <- false
        help.Text <- "Capture session: play normally. This window closes when the session stops."
    else
        btn.Text <- (if blocking then "🔓  Release my mouse / keyboard" else "🔒  Block my input")
        btn.BackColor <- (if blocking then Color.FromArgb(170, 40, 40) else Color.FromArgb(40, 110, 60))
        miToggle.Text <- (if blocking then "Release my input" else "Block my input")
        help.Text <-
            (if blocking && gameOnly then "BLOCKED for the game only: your controller, and clicks / keys aimed at the game, are ignored. Other programs work normally. Release: this button, the tray icon, or Ctrl+Alt+Shift+F12."
             elif blocking then "BLOCKED: your mouse clicks / keys do not reach any program (this test needs the game in front). Release: this button, the tray icon, or Ctrl+Alt+Shift+F12. Ctrl+Alt+Del always works."
             elif gameOnly then "Off by default. Block = your controller and clicks / keys aimed at the game are ignored so you cannot disturb the test. Everything else keeps working."
             else "Off by default. Block = your mouse clicks and keys are ignored so you cannot disturb the test (the test's own input still works). Ctrl+Alt+Del always works.")
    tray.Text <- (sprintf "Nioh 3 test: %s%s" name (if blocking then " (input BLOCKED)" else "")).Substring(0, min 63 (sprintf "Nioh 3 test: %s%s" name (if blocking then " (input BLOCKED)" else "")).Length)

let setBlock (on: bool) (why: string) =
    if on && noBlock then ()
    elif on && not blocking then
        refreshGamePids ()
        let hm = GetModuleHandleW null
        hkKb <- SetWindowsHookExW(13, kbProc, hm, 0u)
        hkMs <- SetWindowsHookExW(14, msProc, hm, 0u)
        blocking <- hkKb <> 0n && hkMs <> 0n
        if not blocking then
            (if hkKb <> 0n then UnhookWindowsHookEx hkKb |> ignore)
            (if hkMs <> 0n then UnhookWindowsHookEx hkMs |> ignore)
            hkKb <- 0n; hkMs <- 0n
            log "could not install the input hooks - not blocking"
        else
            blockSince <- DateTime.Now
            sendPipe "realpads off"
            log (sprintf "BLOCK on (%s)" why)
    elif not on && blocking then
        blocking <- false
        (if hkKb <> 0n then UnhookWindowsHookEx hkKb |> ignore)
        (if hkMs <> 0n then UnhookWindowsHookEx hkMs |> ignore)
        hkKb <- 0n; hkMs <- 0n
        sendPipe "realpads on"
        log (sprintf "BLOCK off (%s)" why)
    refresh ()

btn.Click.Add(fun _ -> setBlock (not blocking) "button")
miToggle.Click.Add(fun _ -> setBlock (not blocking) "tray menu")
miShow.Click.Add(fun _ -> form.Visible <- not form.Visible)
tray.MouseClick.Add(fun e -> if e.Button = MouseButtons.Left then (if noBlock then form.Visible <- true else setBlock (not blocking) "tray click"))
form.FormClosing.Add(fun e -> if not exiting && e.CloseReason = CloseReason.UserClosing then (e.Cancel <- true; form.Hide()))

let quit (why: string) =
    log (sprintf "exit: %s" why)
    setBlock false why
    exiting <- true
    tray.Visible <- false
    form.Close()

let timer = new Timer(Interval = 300)
let mutable ticks = 0
timer.Tick.Add(fun _ ->
    ticks <- ticks + 1
    if blocking && gameOnly && ticks % 3 = 0 then refreshGamePids ()   // ~1 s: a relaunched game gets a new pid
    if releaseRequested then (releaseRequested <- false; setBlock false "Ctrl+Alt+Shift+F12")
    if blocking && (DateTime.Now - blockSince).TotalMinutes >= blockMaxMin then setBlock false (sprintf "blocked for %.0f min (max)" blockMaxMin)
    if not (procAlive harnessPid) then quit "the harness ended"
    elif not (procAlive ownerPid) then quit "the owner process ended"
    elif (DateTime.Now - t0).TotalMinutes >= maxMin then quit "max time"
    else
        let lines = if statusFile <> "" && File.Exists statusFile then (try File.ReadAllLines statusFile with _ -> [||]) else [||]
        let step = if lines.Length > 0 then lines.[0] else "running"
        if step = "DONE" then quit "status DONE"
        else
            let left =
                match (if lines.Length > 1 then minutesLeft DateTime.Now lines.[1] else None) with
                | Some m -> m
                | None -> max 0.0 (maxMin - (DateTime.Now - t0).TotalMinutes)
            title.Text <- sprintf "Nioh 3 test running - %s" name
            stepL.Text <- sprintf "%s\n~%.0f min left%s" step left (if inputMode = "vpad" then " - the game does not need your screen" else " - the game needs the foreground")
)

// ---- selftest ----
if selftest then
    let fails = Collections.Generic.List<string>()
    let ck c m = if not c then fails.Add m
    ck (decideKey false false 0x41 true false false false false false = Pass) "not blocking: keys pass"
    ck (decideKey true false 0x41 true false false false false false = Swallow) "blocking: real key swallowed"
    ck (decideKey true true 0x41 true false false false false false = Pass) "blocking: injected key passes"
    ck (decideKey true false 0x7B true true true true false false = Release) "Ctrl+Alt+Shift+F12 releases"
    ck (decideKey true false 0x7B true true true false false false = Swallow) "F12 without Shift does not release"
    ck (decideKey false false 0x7B true true true true false false = Pass) "hotkey passes when not blocking"
    ck (decideKey true false 0x7B false true true true false false = Swallow) "hotkey key-up swallowed"
    ck (decideMouse true false false false false false false = Swallow) "blocking: real click swallowed"
    ck (decideMouse true false true false false false false = Pass) "click over the popup passes"
    ck (decideMouse true true false false false false false = Pass) "injected click passes"
    ck (decideMouse true false false true true false false = Pass) "vpad: real move passes"
    ck (decideMouse true false false true false false false = Swallow) "ui: real move swallowed"
    ck (decideMouse false false false false false false false = Pass) "not blocking: mouse passes"
    ck (decideKey true false 0x41 true false false false true false = Pass) "game-only: key to another program passes"
    ck (decideKey true false 0x41 true false false false true true = Swallow) "game-only: key to the focused game swallowed"
    ck (decideKey true false 0x7B true true true true true false = Release) "game-only: hotkey still releases"
    ck (decideMouse true false false false true true false = Pass) "game-only: click on another program passes"
    ck (decideMouse true false false false true true true = Swallow) "game-only: click on the game swallowed"
    ck (decideMouse true false false true true true true = Pass) "game-only: move over the game passes (vpad)"
    let m = Mods()
    m.Update(0xA2, true); m.Update(0xA4, true); m.Update(0xA0, true)
    ck (m.Ctrl && m.Alt && m.Shift) "left mods tracked"
    m.Update(0xA0, false)
    ck (not m.Shift) "shift up tracked"
    ck (procAlive 0 && not (procAlive 999999)) "procAlive"
    ck (minutesLeft (DateTime(2026, 1, 1, 10, 0, 0)) "2026-01-01T10:30:00" = Some 30.0) "minutes left from ISO"
    // live loop: a short-lived fake harness; block on (hooks in log-only mode), then the harness exits -> release + exit
    let fake = Process.Start(ProcessStartInfo("cmd.exe", "/c ping -n 4 127.0.0.1 >nul", CreateNoWindow = true, UseShellExecute = false))
    let mutable sawBlock = false
    let mutable hooks = false
    let tt = new Timer(Interval = 200)
    let mutable n = 0
    tt.Tick.Add(fun _ ->
        n <- n + 1
        if n = 2 then
            setBlock true "selftest"
            sawBlock <- blocking; hooks <- hkKb <> 0n && hkMs <> 0n
        if not (procAlive fake.Id) && not blocking then (tt.Stop()))
    let hp = fake.Id
    form.Load.Add(fun _ -> popupHandle <- form.Handle; tt.Start(); refresh ())
    // reuse the real timer logic with the fake harness pid
    let watch = new Timer(Interval = 300)
    watch.Tick.Add(fun _ -> if not (procAlive hp) then quit "the harness ended")
    form.Shown.Add(fun _ -> watch.Start())
    let tStart = DateTime.Now
    Application.Run(form)
    ck sawBlock "blocking turned on"
    ck hooks "both low-level hooks installed"
    ck (not blocking && hkKb = 0n && hkMs = 0n) "hooks removed at exit"
    ck ((DateTime.Now - tStart).TotalSeconds < 15.0) "exit soon after the harness ended"
    tray.Dispose()
    if fails.Count = 0 then printfn "TESTPOPUP SELFTEST: ALL PASSED (decisions, hotkey, mods, timers, hooks on/off, exit with the harness)"
    else (printfn "FAILED:"; for f in fails do printfn "  - %s" f; exit 1)
else
    form.Load.Add(fun _ -> popupHandle <- form.Handle; refresh (); timer.Start())
    log (sprintf "popup start: name=%s harness=%d owner=%d input=%s noBlock=%b max=%.0f min" name harnessPid ownerPid inputMode noBlock maxMin)
    Application.Run(form)
    tray.Dispose()
