// reswait_live.fsx - RWb: the pool-1 ("ResourceLoad") wake storm during and after level loads (notes\stutter.md 3.5).
//
// ResStateWaitSwap 8BBB84 (behind ResSetWithStateWait 367674, 356 callers) spins until a resource's state bits clear.
// Its cold spin at 1A31388 calls JobWakeOneInPool(sched, [sched+3A8h] = 1) on EVERY iteration and Sleep(0) only every
// 256th. Nothing is pushed by the spinner, so each woken ResourceLoad worker scans empty queues and sleeps again: the
// 25-150k/s wake-one 48C6B storm on pool 1 seen on the Ally (v7.1 log, ~4 min around a level load).
// RWb moves the wake next to the Sleep(0): one wake per 256 spins instead of one per spin. Same exit condition, same
// SchedRequestFlush (bit 27) every spin, same Sleep(0) cadence in spin count; only the number of futile wakes changes.
// Any job pushed to pool 1 comes with its pusher's own wake (JobPushToPool + wake-one, SchedCtxKick 8FE3BC).
// The cave variant RWc (wake only when a pool-1 worker has queued work) is the ASI design in the note; this byte-only
// variant needs no cave and is the live A/B tool.
//
//   dotnet fsi reswait_live.fsx offline        NO GAME ACCESS: bytes vs dump, decode of old/new, branch targets inside the
//                                              range (whole-executable sweep), overlap with other patch sets, the
//                                              Sleep(0) call and the loop exit unchanged, rax/flags dead after the call
//   dotnet fsi reswait_live.fsx dry|status     READ-ONLY on the game
//   $env:FT_ALLOW_WRITE=1; dotnet fsi reswait_live.fsx apply|revert
#r "nuget: Iced, 1.21.0"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\patcher\Mem.fsx"
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open System.IO
open System.Text.RegularExpressions
open Iced.Intel
open Mem
open Wj_common

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0].ToLowerInvariant() else "offline"

let SITE = 0x1A3146A
// mov edx,[rbp+3A8h]; mov rcx,rbp; call 48B9C; inc ebx; test bl,bl; jne 1A31486; xor ecx,ecx
let ORIG = hexb "8B 95 A8 03 00 00 48 8B CD E8 24 77 61 FE FF C3 84 DB 75 08 33 C9"
// inc ebx; test bl,bl; jne 1A31486; mov edx,[rbp+3A8h]; mov rcx,rbp; call 48B9C; xor ecx,ecx
let NEW  = hexb "FF C3 84 DB 75 16 8B 95 A8 03 00 00 48 8B CD E8 1E 77 61 FE 33 C9"
let SKIP_TARGET = 0x1A31486UL
let SLEEP_CALL = 0x1A31480UL
let WAKE_FN = 0x48B9CUL

let offline () =
    let mutable ok = true
    let check (name: string) (c: bool) = (printfn "  [%s] %s" (if c then "ok" else "FAIL") name; ok <- ok && c)
    let cur = img.[SITE .. SITE + ORIG.Length - 1]
    printfn "site %X: %s" SITE (hexs cur)
    check "original bytes match the dump" (cur = ORIG)
    printfn "original:"; printDis ORIG (uint64 SITE) 0UL
    printfn "new:"; printDis NEW (uint64 SITE) 0UL
    let o = decodeAll ORIG (uint64 SITE)
    let n = decodeAll NEW (uint64 SITE)
    let callTo (l: Instruction list) = l |> List.tryFind (fun i -> i.Mnemonic = Mnemonic.Call) |> Option.map (fun i -> i.NearBranchTarget)
    let jneTo (l: Instruction list) = l |> List.tryFind (fun i -> i.Mnemonic = Mnemonic.Jne) |> Option.map (fun i -> i.NearBranchTarget)
    check "both versions call JobWakeOneInPool 48B9C" (callTo o = Some WAKE_FN && callTo n = Some WAKE_FN)
    check "both versions branch to 1A31486 when bl != 0" (jneTo o = Some SKIP_TARGET && jneTo n = Some SKIP_TARGET)
    check "same length, ends on an instruction boundary" (NEW.Length = ORIG.Length && (List.last n).NextIP = uint64 SITE + uint64 NEW.Length)
    let nx = decodeAt (uint64 SITE + uint64 ORIG.Length)
    check (sprintf "next instruction is the Sleep(0) call at %X (%s)" SLEEP_CALL (fmtIns nx)) (nx.IP = SLEEP_CALL && nx.Mnemonic = Mnemonic.Call)
    let tgt = decodeAt SKIP_TARGET
    printfn "  skip target %X: %s" SKIP_TARGET (fmtIns tgt)
    // the new code skips the call on bl != 0; volatile registers at 1A31486 were call-clobbered garbage in the original,
    // so nothing after it may read them before writing: walk from the skip target and from the post-call point.
    let f = walk [ SKIP_TARGET ] [ Register.RAX; Register.RCX; Register.RDX; Register.R8; Register.R9; Register.R10; Register.R11 ] true false 0
    check "rax/rcx/rdx/r8-r11 and flags dead at 1A31486 (written before read on every path)" f.IsEmpty
    for x in f do printfn "      %s" x
    let others = (OTHER_SITES @ (let p = @"C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix\patches.json"
                                 if File.Exists p then [ for m in Regex.Matches(File.ReadAllText p, "\"rva\"\\s*:\\s*\"0x([0-9A-Fa-f]+)\"") -> Convert.ToInt32(m.Groups.[1].Value, 16) ] else []))
    let ov = others |> List.filter (fun r -> r < SITE + ORIG.Length && SITE < r + 10)
    check (sprintf "no overlap with other patch sets %A" ov) ov.IsEmpty
    let hits = scanTargets [ "RWb", uint64 SITE, uint64 SITE + uint64 ORIG.Length ]
    check (sprintf "no direct branch/call target strictly inside the range (%s)" (String.Join("; ", hits))) (hits.Count = 0)
    printfn "%s" (if ok then "OFFLINE CHECKS: ALL PASSED" else "OFFLINE CHECKS: FAILURES ABOVE")

let writeGuard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1 to write"
let live () =
    let t = attach "Nioh3"
    let a = t.Base + nativeint SITE
    let state () = let c = t.Read(a, ORIG.Length) in if c = ORIG then "original" elif c = NEW then "patched" else "UNKNOWN"
    match mode with
    | "dry" | "status" -> printfn "RWb %X: %s" SITE (state ())
    | "apply" ->
        writeGuard ()
        match state () with
        | "original" -> printfn "RWb applied (attempt %d)" (safeWrite t [ a, NEW, ORIG ])
        | "patched" -> printfn "already applied"
        | _ -> failwith "unexpected bytes - aborting"
    | "revert" ->
        writeGuard ()
        match state () with
        | "patched" -> printfn "RWb reverted (attempt %d)" (safeWrite t [ a, ORIG, NEW ])
        | "original" -> printfn "already original"
        | _ -> failwith "unexpected bytes - aborting"
    | m -> printfn "unknown mode %s" m

if mode = "offline" then offline () else live ()
