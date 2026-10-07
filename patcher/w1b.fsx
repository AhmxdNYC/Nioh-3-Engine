// w1b.fsx — W1b: replace the W1 fix's `rep stosd` (count is ~1 per glyph row here, so its startup cost dominates)
// with a plain store loop over the same index range ([r14]+r15 .. [r14]+2*r15-1, at least one store like the
// original do-while). edx/ecx are dead after 61BC2 (edx rewritten at 61BCC, ecx at 61B9C / 61BE6). From
// notes\ui-mainthread.md.
//   dotnet fsi w1b.fsx status | apply | revert      (apply: W1 or stock -> W1b; revert: W1b -> W1)
#load "Mem.fsx"
open System
open Mem
let hexb (s: string) = s.Split(' ', StringSplitOptions.RemoveEmptyEntries) |> Array.map (fun x -> Convert.ToByte(x, 16))
let STOCK = hexb "33 C9 41 0F B6 06 41 03 C7 03 C1 FF C1 83 24 83 00 41 3B CF 72 EC"
let W1    = hexb "41 0F B6 06 41 03 C7 48 8D 3C 83 44 89 F9 31 C0 F3 AB 0F 1F 40 00"
let W1B   = hexb "41 0F B6 06 44 01 F8 42 8D 0C 38 31 D2 89 14 83 FF C0 39 C8 72 F7"
let mode = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else "status"
let t = attach "Nioh3"
let a = t.Base + 0x61BACn
let cur = t.Read(a, 22)
let name = if cur = STOCK then "stock" elif cur = W1 then "W1" elif cur = W1B then "W1b" else "UNKNOWN"
let guard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1"
match mode with
| "status" -> printfn "61BAC: %s" name
| "apply" ->
    guard ()
    if name = "W1b" then printfn "already W1b"
    elif name = "UNKNOWN" then failwith "unexpected bytes"
    else printfn "W1b applied (from %s) on attempt %d" name (safeWrite t [ a, W1B, cur ])
| "revert" ->
    guard ()
    if name = "W1b" then printfn "back to W1 on attempt %d" (safeWrite t [ a, W1, W1B ]) else printfn "not W1b (%s)" name
| m -> printfn "unknown mode %s" m
