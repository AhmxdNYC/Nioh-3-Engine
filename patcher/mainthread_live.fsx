// mainthread_live.fsx — live main-thread patches from notes\mainthread-drawprep.md (sites verified offline by
// re\scripts_cpu\mainthread_patches.fsx against the dump).
//   dotnet fsi mainthread_live.fsx dry|status [A,D]     READ-ONLY: show site states
//   dotnet fsi mainthread_live.fsx apply [A,D]          WRITES (FT_ALLOW_WRITE=1), via Mem.safeWrite
//   dotnet fsi mainthread_live.fsx revert [A,D]
// A = 24 `xor eax,eax; lock cmpxchg [m],0` atomic-load idioms (meshlet streaming bookkeeping 4AA34 / 4B104 / 419A8 and
//     once per draw in 1E80B0) -> `mov eax,[m]` + NOP (same length). Drops a full fence per read so the pointer-chasing
//     loops can overlap their cache misses. Real CAS loops / refcounts are not touched.
// D = main-loop retry quantum: 1A744E4 `movss xmm6,[rip+d]` (0.001 s blind QPC spin when the frame isn't ready) ->
//     points at an existing 0.0002f constant (5x finer retry).
#load "Mem.fsx"
open System
open Mem

let args = fsi.CommandLineArgs |> Array.skip 1
let mode = if args.Length > 0 then args.[0] else "dry"
let groups = (if args.Length > 1 then args.[1] else "A,D").Split(',') |> Set.ofArray
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let hexs (b: byte[]) = BitConverter.ToString(b).Replace("-", " ")

let L_R10_RDXRCX = "F0 44 0F B1 14 0A", "8B 04 0A 0F 1F 00"
let L_EBP_R8 = "F0 41 0F B1 28", "41 8B 00 66 90"
let L_EBP_R9RCX = "F0 41 0F B1 2C 09", "41 8B 04 09 66 90"
let siteA = [
    "A1a", 0x4AC14, L_R10_RDXRCX; "A1b", 0x4AC2B, L_R10_RDXRCX; "A1c", 0x4AC42, L_R10_RDXRCX
    "A1d", 0x4AECF, L_R10_RDXRCX; "A1e", 0x4AFAC, L_R10_RDXRCX
    "A1f", 0x4B319, L_R10_RDXRCX; "A1g", 0x4B32C, L_R10_RDXRCX; "A1h", 0x4B33F, L_R10_RDXRCX
    "A2a", 0x419E8, ("F0 0F B1 A9 C0 04 00 00", "8B 81 C0 04 00 00 66 90")
    "A2b", 0x41A10, L_EBP_R8; "A2c", 0x41A28, L_EBP_R9RCX; "A2d", 0x41A38, L_EBP_R9RCX; "A2e", 0x41A4A, L_EBP_R9RCX
    "A2f", 0x41A5A, L_EBP_R9RCX; "A2g", 0x41A6A, L_EBP_R9RCX; "A2h", 0x41A7A, L_EBP_R9RCX
    "A2i", 0x41A9A, ("F0 0F B1 A9 D0 00 00 00", "8B 81 D0 00 00 00 66 90")
    "A2j", 0x41AAC, L_EBP_R8; "A2k", 0x41AC3, L_EBP_R8; "A2l", 0x41ACC, L_EBP_R8; "A2m", 0x41AED, L_EBP_R8
    "A2n", 0x41AF6, L_EBP_R8; "A2o", 0x41B12, L_EBP_R8; "A2p", 0x41B7B, L_EBP_R8
    "A3", 0x1E835B, ("F0 0F B1 11", "8B 01 66 90") ]
// D: F3 0F 10 35 <disp32> at 1A744E4 (8 bytes); new disp -> 0x3BD4600 (0.0002f)
let D_SITE = 0x1A744E4
let dOrig = "F3 0F 10 35 " + hexs (BitConverter.GetBytes(int32 (0x3BD4688 - (D_SITE + 8))))
let dNew = "F3 0F 10 35 " + hexs (BitConverter.GetBytes(int32 (0x3BD4600 - (D_SITE + 8))))
let sites =
    [ if groups.Contains "A" then yield! siteA |> List.map (fun (id, r, (o, n)) -> id, r, hexb o, hexb n)
      if groups.Contains "D" then yield "D", D_SITE, hexb dOrig, hexb dNew ]

let t = attach "Nioh3"
let B = t.Base
let cur (rva: int) n = t.Read(B + nativeint rva, n)
let state (_, rva, o: byte[], n: byte[]) = let c = cur rva o.Length in if c = o then "original" elif c = n then "patched" else "UNKNOWN " + hexs c
// sanity: the constants D relies on
if groups.Contains "D" then
    let f a = BitConverter.ToSingle(t.Read(B + nativeint a, 4), 0)
    if abs (f 0x3BD4688 - 0.001f) > 1e-9f || abs (f 0x3BD4600 - 0.0002f) > 1e-9f then failwith "D constants not as expected"
match mode with
| "dry" | "status" ->
    for (id, rva, o, n) as s in sites do printfn "  %-4s %8X  %s" id rva (state s)
| "apply" | "revert" ->
    if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1"
    let want = if mode = "apply" then "original" else "patched"
    let todo = sites |> List.filter (fun s -> state s = want)
    let bad = sites |> List.filter (fun s -> (state s).StartsWith "UNKNOWN")
    if not bad.IsEmpty then failwithf "unexpected bytes at %A - nothing written" (bad |> List.map (fun (id, _, _, _) -> id))
    let plan = todo |> List.map (fun (_, rva, o, n) -> if mode = "apply" then B + nativeint rva, n, o else B + nativeint rva, o, n)
    let attempts = if plan.IsEmpty then 0 else safeWrite t plan
    printfn "%s %d site(s) (%s) on attempt %d" (if mode = "apply" then "patched" else "reverted") todo.Length (String.Join(",", groups)) attempts
| m -> printfn "unknown mode %s" m
