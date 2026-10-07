// In-place CPU fixes for Nioh 3 (v2.02), see notes\cpu-profile.md.
//   dotnet fsi cpu_patches.fsx dry|apply|revert
// Every site is checked against its original (or patched) bytes first; writes happen with the
// process frozen so the game thread can never execute a half-written instruction.
#load "Mem.fsx"
open System
open Mem

let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))

// name, rva, original, replacement (same length)
let patches = [
    // W1: zero-fill loop used `and dword [rbx+rax*4],0` on write-combined upload memory (RMW = uncached
    //     read per element, 11-22% of the main thread). Same stores via rep stosd; rdi/rcx/rax dead after.
    "W1", 0x61BAC,
        "33 C9 41 0F B6 06 41 03 C7 03 C1 FF C1 83 24 83 00 41 3B CF 72 EC",
        "41 0F B6 06 41 03 C7 48 8D 3C 83 44 89 F9 31 C0 F3 AB 0F 1F 40 00"
    // W2: quad writer did `and dword [z],0` on WC memory before `movss [y]`; y comes from a movss load
    //     (lanes 1-3 zero), so `movq [y],xmm` writes y and z=0 in one store with no read.
    "W2a", 0x13721F, "83 63 08 00 80 7C 24 58 00 F3 0F 11 43 04", "80 7C 24 58 00 66 0F D6 43 04 0F 1F 40 00"
    "W2b", 0x137269, "83 63 34 00 80 7C 24 58 00 F3 0F 11 4B 30", "80 7C 24 58 00 66 0F D6 4B 30 0F 1F 40 00"
    "W2c", 0x1372B4, "83 63 60 00 80 7C 24 58 00 F3 0F 11 4B 5C", "80 7C 24 58 00 66 0F D6 4B 5C 0F 1F 40 00"
    "W2d", 0x1372FE,
        "83 A3 8C 00 00 00 00 48 8B 6C 24 38 48 8B 74 24 40 F3 0F 11 8B 88 00 00 00",
        "48 8B 6C 24 38 48 8B 74 24 40 66 0F D6 8B 88 00 00 00 0F 1F 80 00 00 00 00"
]

let mode = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else "dry"
let t = attach "Nioh3"
let state (rva: int) (o: byte[]) (n: byte[]) =
    let cur = t.Read(t.Base + nativeint rva, o.Length)
    if cur = o then "original" elif cur = n then "patched" else "UNKNOWN"
let rows = patches |> List.map (fun (name, rva, o, n) -> name, rva, hexb o, hexb n)
for (name, rva, o, n) in rows do printfn "%-4s %8X  %s" name rva (state rva o n)
if rows |> List.exists (fun (_, rva, o, n) -> state rva o n = "UNKNOWN") then failwith "unexpected bytes - aborting"
match mode with
| "apply" | "revert" ->
    let writes = rows |> List.map (fun (_, rva, o, n) ->
        let addr = t.Base + nativeint rva
        if mode = "apply" then addr, n, o else addr, o, n)
    let attempts = safeWrite t writes
    printfn "written on attempt %d (no thread inside a patched range)" attempts
    for (name, rva, o, n) in rows do printfn "%-4s -> %s" name (state rva o n)
| _ -> ()
