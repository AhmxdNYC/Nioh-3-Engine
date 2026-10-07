// Nioh3Panel.fsx — overlay control panel for the Nioh 3 engine fixes. Start with "Nioh3 Panel.cmd".
//
// * F10 shows/hides the panel on top of the game (borderless/windowed mode).
// * Watches for Nioh3.exe: the boot-time worker-pool fix (P12) is written within the first second, and the
//   chosen live fixes are applied automatically once the game has been up for a while. Nothing on disk changes.
// * Each toggle runs the matching, already-tested patcher script (patcher\*.fsx) in the background.
// * Choices are saved in panel\settings.txt and reused on every launch.
#r "nuget: Iced, 1.21.0"
// WinForms comes from the installed Windows Desktop runtime (Microsoft-signed; update the version if it changes)
#I @"C:\Program Files\dotnet\shared\Microsoft.WindowsDesktop.App\10.0.11"
#r "System.Windows.Forms.dll"
#r "System.Windows.Forms.Primitives.dll"
#r "System.Drawing.Common.dll"
#r "Microsoft.Win32.SystemEvents.dll"
#load "../patcher/Mem.fsx"
open System
open System.IO
open System.Diagnostics
open System.Drawing
open System.Windows.Forms
open System.Runtime.InteropServices
open Mem

let root = Path.GetFullPath(Path.Combine(__SOURCE_DIRECTORY__, ".."))
let patcher = Path.Combine(root, "patcher")
let settingsFile = Path.Combine(__SOURCE_DIRECTORY__, "settings.txt")

// ---- options -------------------------------------------------------------------------------------------
type Opt = { Key: string; Label: string; Help: string; Live: bool; mutable On: bool }
let opts =
    [ { Key = "W12"; Label = "Main-thread memory fix"; Live = true; On = true
        Help = "Fixes a slow GPU-buffer clear on the main game thread (fewer stutters)." }
      { Key = "VRR"; Label = "Real-time frame timing"; Live = true; On = true
        Help = "No slow motion / judder below the fps cap. Turns itself off in online co-op sessions." }
      { Key = "DYN"; Label = "Force Dynamic FPS off"; Live = true; On = true
        Help = "The in-game Dynamic FPS option drops to 60 fps mode; with real-time timing it's not needed." }
      { Key = "CPU"; Label = "CPU spin-wait fixes"; Live = true; On = true
        Help = "Message loop, render wait and frame limiter sleep instead of spinning: less CPU, power, heat." }
      { Key = "FG"; Label = "Frame-gen presenter fix"; Live = true; On = true
        Help = "FSR frame generation's pacing thread sleeps instead of spinning (~0.15 core less, same fps)." }
      { Key = "GHDM"; Label = "Fast shadow shader (applied while game is closed)"; Live = false; On = true
        Help = "Faster shadow depth-reduction shader: ~+3% fps, less GPU time, same image. Swaps one shader file." }
      { Key = "FOL"; Label = "Shorter far foliage/prop distance (applied while game is closed)"; Live = false; On = false
        Help = "Far grass, rocks and props drawn to 60% of the distance: ~+1.5% fps (more in open fields), slight far pop-in." }
      { Key = "PHYS"; Label = "Physics on 4 threads (applied while game is closed, experimental)"; Live = false; On = false
        Help = "Physics solver uses 4 threads instead of 2 (game data edit, backed up, exact revert). Only helps physics-heavy fights." }
      { Key = "P12"; Label = "Worker-pool fix (next launch, experimental)"; Live = false; On = false
        Help = "Keeps 3 more worker threads in two pools on <=12-thread CPUs. Measured: no fps gain on a 7600X." } ]
let opt k = opts |> List.find (fun o -> o.Key = k)
let load () =
    if File.Exists settingsFile then
        for l in File.ReadAllLines settingsFile do
            match l.Split('=') with
            | [| k; v |] -> opts |> List.tryFind (fun o -> o.Key = k.Trim()) |> Option.iter (fun o -> o.On <- v.Trim() = "1")
            | _ -> ()
let save () = File.WriteAllLines(settingsFile, opts |> List.map (fun o -> sprintf "%s=%d" o.Key (if o.On then 1 else 0)))
load ()

// ---- running the patcher scripts -----------------------------------------------------------------------
let log = Collections.Concurrent.ConcurrentQueue<string>()
let say (s: string) = log.Enqueue(sprintf "%s  %s" (DateTime.Now.ToString "HH:mm:ss") s)
let run (script: string) (mode: string) (arg: string) =
    let psi = ProcessStartInfo("dotnet", sprintf "fsi \"%s\" %s %s" (Path.Combine(patcher, script)) mode arg)
    psi.WorkingDirectory <- patcher
    psi.UseShellExecute <- false
    psi.CreateNoWindow <- true
    psi.RedirectStandardOutput <- true
    psi.RedirectStandardError <- true
    psi.Environment.["FT_ALLOW_WRITE"] <- "1"
    use p = Process.Start psi
    let out = p.StandardOutput.ReadToEnd()
    p.StandardError.ReadToEnd() |> ignore
    p.WaitForExit()
    let lines = out.Split('\n') |> Array.map (fun s -> s.Trim()) |> Array.filter (fun s -> s <> "" && not (s.Contains "FS3886"))
    let ok = p.ExitCode = 0
    let last = if lines.Length > 0 then lines.[lines.Length - 1] else ""
    say (sprintf "%s %s%s -> %s" script mode (if arg = "" then "" else " " + arg) (if ok then last else "FAILED: " + last))
    ok

// fast shadow shader: a file swap in the game folder, only possible while the game is closed
let syncShader () =
    if Process.GetProcessesByName "Nioh3" |> Array.isEmpty then
        let script = Path.Combine(root, "gpu-shaders", "apply_ghdm.ps1")
        let args = sprintf "-NoProfile -ExecutionPolicy Bypass -File \"%s\"%s" script (if (opt "GHDM").On then "" else " -Revert")
        let psi = ProcessStartInfo("powershell", args, UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true)
        use p = Process.Start psi
        let out = p.StandardOutput.ReadToEnd().Trim()
        p.WaitForExit()
        say (sprintf "Shadow shader: %s" (if out = "" then "failed" else out.Split('\n') |> Array.last))
        // archive edits (backed up, exact revert); only run when the installed state differs from the setting
        let exec (file: string) (args: string) =
            let psi = ProcessStartInfo(file, args, UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true)
            use p = Process.Start psi
            let o = p.StandardOutput.ReadToEnd().Trim()
            p.WaitForExit()
            if o = "" then "failed" else o.Split('\n') |> Array.last
        // physics threads: data\sysdb_tweaks.ps1 (system object DB)
        let physOn = File.Exists(Path.Combine(root, "data", "physics_backup", "backup.json"))
        if (opt "PHYS").On <> physOn then
            let script = Path.Combine(root, "data", "sysdb_tweaks.ps1")
            say (sprintf "Physics threads: %s" (exec "powershell" (sprintf "-NoProfile -ExecutionPolicy Bypass -File \"%s\"%s" script (if (opt "PHYS").On then " -PhysicsThreads 4" else " -Revert"))))
        // foliage / prop scatter distances x0.6 on all maps: data\tools\n3foliage.py
        let folBackup = Path.Combine(root, "data", "foliage_backup")
        let folOn = File.Exists(Path.Combine(folBackup, "backup.json"))
        if (opt "FOL").On <> folOn then
            let py = Path.Combine(root, "data", "tools", "n3foliage.py")
            let pkg = @"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\package"
            let a = if (opt "FOL").On then sprintf "-I \"%s\" apply \"%s\" \"%s\" 0.6" py pkg folBackup else sprintf "-I \"%s\" revert \"%s\" \"%s\"" py pkg folBackup
            say (sprintf "Foliage distance: %s" (exec "py" a))

// per-process state: which live patches are installed in the current game process
let mutable gamePid = 0
let mutable autoHide = false     // set by the watcher once the fixes are applied: the panel gets out of the way
let installed = Collections.Generic.HashSet<string>()
let busy = obj ()

let setLive (o: Opt) (on: bool) =
    lock busy (fun () ->
        if gamePid = 0 then () else
        match o.Key with
        | "W12" -> if on && not (installed.Contains "W12") then (if run "cpu_patches.fsx" "apply" "" then installed.Add "W12" |> ignore)
                   elif not on && installed.Contains "W12" then (if run "cpu_patches.fsx" "revert" "" then installed.Remove "W12" |> ignore)
        | "VRR" -> if on && not (installed.Contains "VRR") then (if run "ft_vrr_patch.fsx" "apply" "" then installed.Add "VRR" |> ignore)
                   elif installed.Contains "VRR" then run "ft_vrr_patch.fsx" (if on then "on" else "off") "" |> ignore
        | "DYN" ->
            // off = put back the value the game had before the frame-timing fix forced it (saved by ft_vrr_patch)
            let saved = Path.Combine(patcher, "ft_vrr_dynfps.txt")
            let orig =
                try (let d = File.ReadAllText(saved).Trim().Split(' ') in if int d.[0] = gamePid then d.[1] else "1")
                with _ -> "1"
            run "dynfps.fsx" (if on then "0" else orig) "" |> ignore
        | "CPU" -> if on && not (installed.Contains "CPU") then (if run "cpu_caves.fsx" "apply" "W3,R1,L1" then installed.Add "CPU" |> ignore)
                   elif installed.Contains "CPU" then run "cpu_caves.fsx" (if on then "on" else "off") "W3,R1,L1" |> ignore
        | "FG"  -> if on && not (installed.Contains "FG") then (if run "fg_cave.fsx" "apply" "" then installed.Add "FG" |> ignore)
                   elif installed.Contains "FG" then run "fg_cave.fsx" (if on then "on" else "off") "" |> ignore
        | _ -> ())

// ---- P12: boot-time worker-pool fix (must land before the engine sizes its pools) ------------------------
let p12Site = 0x1FE30D5n
let p12Orig = [| 0x41uy; 0x83uy; 0xFBuy; 0x0Cuy |]
let p12New  = [| 0x41uy; 0x83uy; 0xFBuy; 0x00uy |]
let raceP12 (proc: Process) =
    let sw = Stopwatch.StartNew()
    let mutable t : Target option = None
    while t.IsNone && sw.Elapsed.TotalSeconds < 30.0 && not proc.HasExited do
        try (let x = new Target(proc) in x.Base |> ignore; t <- Some x) with _ -> Threading.Thread.Sleep 1
    match t with
    | None -> say "P12 skipped: could not open the game process"
    | Some tg ->
        let a = tg.Base + p12Site
        let mutable cur = [||]
        while cur <> p12Orig && cur <> p12New && sw.Elapsed.TotalSeconds < 60.0 && not proc.HasExited do
            cur <- (try tg.Read(a, 4) with _ -> [||])
        if cur = p12Orig then
            tg.Write(a, p12New)
            say (sprintf "Worker-pool fix written %.1f s after launch" sw.Elapsed.TotalSeconds)
        else say "Worker-pool fix: site not found in time (skipped this launch)"

// ---- game watcher --------------------------------------------------------------------------------------
let watcher () =
    while true do
        let p = Process.GetProcessesByName "Nioh3" |> Array.tryHead
        match p with
        | Some proc when proc.Id <> gamePid ->
            lock busy (fun () -> installed.Clear())
            say (sprintf "Nioh 3 started (pid %d)" proc.Id)
            let age = try (DateTime.Now - proc.StartTime).TotalSeconds with _ -> 0.0
            if (opt "P12").On then
                if age < 5.0 then raceP12 proc
                else say "Worker-pool fix needs the panel running before the game starts (applies next launch)"
            // wait until the game window has been up for a while (title screen is fine)
            let sw = Stopwatch.StartNew()
            let mutable up = 0
            while up < 20 && not proc.HasExited && sw.Elapsed.TotalSeconds < 300.0 do
                Threading.Thread.Sleep 1000
                proc.Refresh()
                if proc.MainWindowHandle <> 0n then up <- up + 1
            if not proc.HasExited then
                gamePid <- proc.Id
                say "Applying your fixes..."
                for o in opts do if o.Live && o.On then setLive o true
                say "Ready. F10 opens this panel."
                autoHide <- true
        | None when gamePid <> 0 ->
            say "Nioh 3 closed"
            gamePid <- 0
            lock busy (fun () -> installed.Clear())
            Threading.Thread.Sleep 3000
            syncShader ()
        | _ -> ()
        Threading.Thread.Sleep 50

// ---- UI ------------------------------------------------------------------------------------------------
[<DllImport("user32.dll")>]
extern bool RegisterHotKey(nativeint hWnd, int id, uint32 mods, uint32 vk)
[<DllImport("user32.dll")>]
extern bool SetForegroundWindow(nativeint hWnd)

let gold = Color.FromArgb(214, 178, 94)
let ink = Color.FromArgb(18, 18, 22)
let panelBack = Color.FromArgb(28, 28, 34)

type Panel() as this =
    inherit Form()
    let title = new Label(Text = "NIOH 3 ENGINE FIXES", ForeColor = gold, Font = new Font("Segoe UI Semibold", 13.0f),
                          AutoSize = true, Location = Point(18, 14))
    let sub = new Label(Text = "F10 show / hide   •   changes apply instantly", ForeColor = Color.FromArgb(150, 150, 160),
                        Font = new Font("Segoe UI", 8.5f), AutoSize = true, Location = Point(20, 40))
    let status = new Label(ForeColor = Color.White, Font = new Font("Segoe UI", 9.0f), AutoSize = false,
                           Location = Point(20, 64), Size = Size(400, 20))
    let logBox = new ListBox(BackColor = ink, ForeColor = Color.FromArgb(170, 170, 180), BorderStyle = BorderStyle.None,
                             Font = new Font("Consolas", 8.0f), Location = Point(20, 0), Size = Size(400, 96))
    let launch = new Button(Text = "Launch Nioh 3", FlatStyle = FlatStyle.Flat, ForeColor = ink, BackColor = gold,
                            Font = new Font("Segoe UI Semibold", 9.0f), Size = Size(130, 30))
    let hide = new Button(Text = "Hide (F10)", FlatStyle = FlatStyle.Flat, ForeColor = Color.White, BackColor = panelBack,
                          Font = new Font("Segoe UI", 9.0f), Size = Size(100, 30))
    let tip = new ToolTip()
    do
        this.Text <- "Nioh 3 Engine Fixes"
        this.FormBorderStyle <- FormBorderStyle.None
        this.TopMost <- true
        this.ShowInTaskbar <- true
        this.BackColor <- panelBack
        this.Opacity <- 0.95
        this.StartPosition <- FormStartPosition.Manual
        this.Location <- Point(40, 40)
        this.Controls.AddRange [| title :> Control; sub; status |]
        let mutable y = 94
        for o in opts do
            let cb = new CheckBox(Text = o.Label, Checked = o.On, ForeColor = Color.White, Font = new Font("Segoe UI", 10.0f),
                                  AutoSize = true, Location = Point(20, y))
            let help = new Label(Text = o.Help, ForeColor = Color.FromArgb(140, 140, 150), Font = new Font("Segoe UI", 8.0f),
                                 AutoSize = false, Location = Point(40, y + 22), Size = Size(380, 30))
            tip.SetToolTip(cb, o.Help)
            cb.CheckedChanged.Add(fun _ ->
                o.On <- cb.Checked
                save ()
                if o.Live then
                    if gamePid = 0 then say (sprintf "%s %s (applies when the game runs)" o.Label (if o.On then "ON" else "OFF"))
                    else Threading.Tasks.Task.Run(fun () -> setLive o o.On) |> ignore
                elif (o.Key = "GHDM" || o.Key = "PHYS" || o.Key = "FOL") && gamePid = 0 then Threading.Tasks.Task.Run(fun () -> syncShader ()) |> ignore
                else say (sprintf "%s %s - takes effect next launch" o.Label (if o.On then "ON" else "OFF")))
            this.Controls.AddRange [| cb :> Control; help |]
            y <- y + 56
        logBox.Location <- Point(20, y + 6)
        launch.Location <- Point(20, y + 112)
        hide.Location <- Point(160, y + 112)
        launch.FlatAppearance.BorderSize <- 0
        hide.FlatAppearance.BorderColor <- gold
        launch.Click.Add(fun _ -> Process.Start(ProcessStartInfo("steam://rungameid/3681010", UseShellExecute = true)) |> ignore)
        hide.Click.Add(fun _ -> this.HideToGame())
        this.Controls.AddRange [| logBox :> Control; launch; hide |]
        this.ClientSize <- Size(440, y + 156)
        // drag anywhere on the background
        let mutable drag = None
        this.MouseDown.Add(fun e -> drag <- Some e.Location)
        this.MouseUp.Add(fun _ -> drag <- None)
        this.MouseMove.Add(fun e -> match drag with
                                     | Some d -> this.Location <- Point(this.Left + e.X - d.X, this.Top + e.Y - d.Y)
                                     | None -> ())
        let timer = new Timer(Interval = 250)
        timer.Tick.Add(fun _ ->
            let mutable s = ""
            while log.TryDequeue(&s) do
                logBox.Items.Add s |> ignore
                logBox.TopIndex <- max 0 (logBox.Items.Count - 1)
            status.Text <- if gamePid = 0 then "Game: not running" else sprintf "Game: running (pid %d)  •  co-op safe" gamePid
            if autoHide then
                autoHide <- false
                if this.Visible then this.HideToGame())
        timer.Start()
    /// hide and give keyboard focus back to the game
    member this.HideToGame() =
        this.Hide()
        match Process.GetProcessesByName "Nioh3" |> Array.tryHead with
        | Some p when p.MainWindowHandle <> 0n -> SetForegroundWindow p.MainWindowHandle |> ignore
        | _ -> ()
    override this.OnHandleCreated e =
        base.OnHandleCreated e
        RegisterHotKey(this.Handle, 1, 0x4000u (* MOD_NOREPEAT *), 0x79u (* F10 *)) |> ignore
    override this.WndProc(m: byref<Message>) =
        if m.Msg = 0x0312 (* WM_HOTKEY *) then
            if this.Visible then this.HideToGame() else (this.Show(); this.Activate())
        base.WndProc(&m)
    override this.OnPaint e =
        base.OnPaint e
        use pen = new Pen(gold, 1.0f)
        e.Graphics.DrawRectangle(pen, 0, 0, this.ClientSize.Width - 1, this.ClientSize.Height - 1)

Application.EnableVisualStyles()
let form = new Panel()
Threading.Tasks.Task.Run(fun () -> syncShader ()) |> ignore
Threading.Thread(watcher, IsBackground = true).Start()
say "Panel started. Settings: panel\\settings.txt"
Application.Run form
