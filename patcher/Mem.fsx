// External process memory toolkit for Nioh 3 engine patching.
// Runs under `dotnet fsi` (Microsoft-signed host) so nothing unsigned is loaded —
// Smart App Control blocks freshly built native DLLs on this machine.
#r "nuget: Iced, 1.21.0"

open System
open System.Diagnostics
open System.Runtime.InteropServices
open Iced.Intel

module Native =
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenProcess(uint32 access, bool inheritHandle, int pid)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool ReadProcessMemory(nativeint h, nativeint addr, byte[] buf, nativeint size, nativeint& read)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool WriteProcessMemory(nativeint h, nativeint addr, byte[] buf, nativeint size, nativeint& written)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool VirtualProtectEx(nativeint h, nativeint addr, nativeint size, uint32 prot, uint32& oldProt)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint VirtualAllocEx(nativeint h, nativeint addr, nativeint size, uint32 typ, uint32 prot)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool FlushInstructionCache(nativeint h, nativeint addr, nativeint size)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool CloseHandle(nativeint h)
    [<DllImport("ntdll.dll")>]
    extern int NtSuspendProcess(nativeint h)
    [<DllImport("ntdll.dll")>]
    extern int NtResumeProcess(nativeint h)

let PROCESS_ALL = 0x1F0FFFu
let PAGE_EXECUTE_READWRITE = 0x40u
let MEM_COMMIT_RESERVE = 0x3000u

type Target(proc: Process) =
    let h = Native.OpenProcess(PROCESS_ALL, false, proc.Id)
    do if h = 0n then failwithf "OpenProcess failed: %d" (Marshal.GetLastWin32Error())
    member _.Process = proc
    member _.Handle = h
    member _.Base = proc.MainModule.BaseAddress
    member _.ModuleSize = proc.MainModule.ModuleMemorySize
    member _.Read(addr: nativeint, n: int) =
        let buf = Array.zeroCreate<byte> n
        let mutable r = 0n
        if not (Native.ReadProcessMemory(h, addr, buf, nativeint n, &r)) then
            failwithf "RPM %X failed: %d" addr (Marshal.GetLastWin32Error())
        buf
    member t.ReadF32(addr) = BitConverter.ToSingle(t.Read(addr, 4), 0)
    member t.ReadI64(addr) = BitConverter.ToInt64(t.Read(addr, 8), 0)
    member _.Write(addr: nativeint, bytes: byte[]) =
        let mutable old = 0u
        Native.VirtualProtectEx(h, addr, nativeint bytes.Length, PAGE_EXECUTE_READWRITE, &old) |> ignore
        let mutable w = 0n
        let ok = Native.WriteProcessMemory(h, addr, bytes, nativeint bytes.Length, &w)
        let mutable tmp = 0u
        Native.VirtualProtectEx(h, addr, nativeint bytes.Length, old, &tmp) |> ignore
        Native.FlushInstructionCache(h, addr, nativeint bytes.Length) |> ignore
        if not ok then failwithf "WPM %X failed: %d" addr (Marshal.GetLastWin32Error())
    /// Run `f` with every thread of the target frozen, so multi-byte code patches can't be
    /// executed half-written. Always resumes, even if `f` throws.
    member _.Frozen(f: unit -> 'a) =
        Native.NtSuspendProcess(h) |> ignore
        try f () finally Native.NtResumeProcess(h) |> ignore
    /// Allocate RWX memory within +-2GB of `near` so rel32 jumps reach it.
    member _.AllocNear(near: nativeint, size: int) =
        let mutable result = 0n
        let mutable probe = (int64 near &&& ~~~0xFFFFL) - 0x10000000L
        while result = 0n && probe > int64 near - 0x7FF00000L do
            result <- Native.VirtualAllocEx(h, nativeint probe, nativeint size, MEM_COMMIT_RESERVE, PAGE_EXECUTE_READWRITE)
            probe <- probe - 0x10000L
        if result = 0n then failwith "AllocNear failed"
        result
    /// Scan the main module for an IDA-style pattern ("48 8B ?? ?? 89").
    member t.Scan(pattern: string) =
        let toks = pattern.Split(' ', StringSplitOptions.RemoveEmptyEntries)
        let pat = toks |> Array.map (fun s -> if s.StartsWith "?" then -1 else Convert.ToInt32(s, 16))
        let results = ResizeArray<nativeint>()
        let chunk = 0x100000
        let mutable off = 0
        while off < t.ModuleSize do
            let n = min (chunk + pat.Length) (t.ModuleSize - off)
            let buf = try t.Read(t.Base + nativeint off, n) with _ -> Array.empty
            for i in 0 .. buf.Length - pat.Length do
                let mutable j = 0
                while j < pat.Length && (pat.[j] = -1 || int buf.[i + j] = pat.[j]) do j <- j + 1
                if j = pat.Length && i < chunk then results.Add(t.Base + nativeint (off + i))
            off <- off + chunk
        results |> Seq.toList
    interface IDisposable with
        member _.Dispose() = Native.CloseHandle(h) |> ignore

module private ThreadNative =
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern nativeint OpenThread(uint32 access, bool inheritHandle, int tid)
    [<DllImport("kernel32.dll", SetLastError = true)>]
    extern bool GetThreadContext(nativeint h, nativeint ctx)
    [<DllImport("kernel32.dll")>]
    extern bool CloseHandle(nativeint h)

/// Instruction start addresses when `bytes` is decoded at `ip`.
let boundaries (bytes: byte[]) (ip: uint64) =
    let d = Decoder.Create(64, ByteArrayCodeReader(bytes))
    d.IP <- ip
    let s = Collections.Generic.HashSet<uint64>()
    while d.IP < ip + uint64 bytes.Length do s.Add d.IP |> ignore; d.Decode() |> ignore
    s

/// RIP of every thread in the target (call while the process is frozen).
let threadRips (t: Target) =
    let mem = Marshal.AllocHGlobal(0x4D0 + 64)
    let ctx = nativeint ((int64 mem + 15L) &&& ~~~15L)
    t.Process.Refresh()
    let rips =
        [ for th in t.Process.Threads |> Seq.cast<ProcessThread> do
            let h = ThreadNative.OpenThread(0x0008u, false, th.Id)   // THREAD_GET_CONTEXT
            if h <> 0n then
                Marshal.WriteInt32(ctx, 0x30, 0x100001)              // CONTEXT_CONTROL
                if ThreadNative.GetThreadContext(h, ctx) then yield th.Id, uint64 (Marshal.ReadInt64(ctx, 0xF8))
                ThreadNative.CloseHandle h |> ignore ]
    Marshal.FreeHGlobal mem
    rips

/// Write code patches (address, newBytes, oldBytes) with the process frozen, but only at a moment when no
/// thread is paused mid-way through a patched range (unless that spot is an instruction start in BOTH the old
/// and new code). Without this, a thread resumed inside a rewritten loop executes half an instruction and
/// crashes (seen live: 0xC0000005 at W1+0x11 with frame generation on). Retries up to 200 times.
let safeWrite (t: Target) (patches: (nativeint * byte[] * byte[]) list) =
    let ok () =
        let rips = threadRips t
        patches |> List.forall (fun (addr, nw, old) ->
            let s = uint64 addr
            let bn, bo = boundaries nw s, boundaries old s
            rips |> List.forall (fun (_, rip) ->
                not (rip > s && rip < s + uint64 nw.Length) || (bn.Contains rip && bo.Contains rip)))
    let mutable attempt = 0
    let mutable fin = false
    while not fin do
        attempt <- attempt + 1
        if attempt > 200 then failwith "a thread kept sitting inside a patch site - nothing written"
        fin <- t.Frozen(fun () ->
            if ok () then
                for (addr, nw, _) in patches do t.Write(addr, nw)
                true
            else false)
        if not fin then Threading.Thread.Sleep 2
    attempt

let attach (name: string) =
    match Process.GetProcessesByName(name) |> Array.tryHead with
    | Some p -> new Target(p)
    | None -> failwithf "process %s not running" name

/// Disassemble `n` bytes at `addr` for inspection.
let disasm (t: Target) (addr: nativeint) (n: int) =
    let bytes = t.Read(addr, n)
    let dec = Decoder.Create(64, ByteArrayCodeReader(bytes))
    dec.IP <- uint64 addr
    let fmt = NasmFormatter()
    let out = StringOutput()
    let endIp = uint64 addr + uint64 n
    while dec.IP < endIp do
        let ins = dec.Decode()
        fmt.Format(&ins, out)
        printfn "%016X  %s" ins.IP (out.ToStringAndReset())

/// Assemble with Iced at a fixed address (so rel32 branches resolve correctly).
let assemble (at: nativeint) (build: Assembler -> unit) =
    let a = Assembler(64)
    build a
    use ms = new IO.MemoryStream()
    a.Assemble(StreamCodeWriter(ms), uint64 at) |> ignore
    ms.ToArray()

/// rel32 JMP from `src` to `dst`, padded with NOPs to `len` bytes (len >= 5).
let jmpPatch (src: nativeint) (dst: nativeint) (len: int) =
    let rel = int (int64 dst - (int64 src + 5L))
    Array.concat [ [| 0xE9uy |]; BitConverter.GetBytes rel; Array.create (len - 5) 0x90uy ]
