// ws_verify.fsx - offline checks for the pool-1 wake-storm fix (RW = ResourceLoad Wake gate). Dump only.
// 1) original bytes at the sites, 2) branch-target sweep (no jump lands inside the patched ranges),
// 3) rax/eax dead after the replaced calls (cave may return anything), 4) decode the byte-only variant.
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open Iced.Intel
open Wj_common

let mutable ok = true
let check name (rva: int) (hex: string) =
    let want = hexb hex
    let got = img.[rva .. rva + want.Length - 1]
    let pass = got = want
    if not pass then ok <- false
    printfn "[%s] %s @%X  %s%s" (if pass then "ok" else "FAIL") name rva (hexs got) (if pass then "" else "  want " + hex)

check "RWc site 8BBB84 cold: call JobWakeOneInPool" 0x1A31473 "E8 24 77 61 FE"
check "RWb range 1A3146A..1A3147F" 0x1A3146A "8B 95 A8 03 00 00 48 8B CD E8 24 77 61 FE FF C3 84 DB 75 08 33 C9"
check "context 1A31462..1A3146A" 0x1A31462 "48 8B CD E8 8A 4E 05 00"
check "Sleep(0) call stays at 1A31480" 0x1A31480 "FF 15 B2 0E EB 01"
check "RWj site 1343D00: call JobWakeOneInPool" 0x1343D5E "E8 39 4E D0 FE 33 C0"
check "48C6B SetEvent (WS2)" 0x48C6B "FF 15 D7 96 89 03"

// branch-target sweep over executable sections
let ranges = [ (0x1A3146BUL, 0x1A31480UL); (0x1A31474UL, 0x1A31478UL); (0x1343D5FUL, 0x1343D63UL) ]
let mutable hits = 0
for (va, vs) in execSections do
    let d = Decoder.Create(64, ByteArrayCodeReader(img, va, vs))
    d.IP <- uint64 va
    while d.IP < uint64 (va + vs) do
        let ins = d.Decode()
        if ins.FlowControl = FlowControl.ConditionalBranch || ins.FlowControl = FlowControl.UnconditionalBranch || ins.FlowControl = FlowControl.Call then
            let t = ins.NearBranchTarget
            for (a, b) in ranges do
                if t >= a && t < b then
                    hits <- hits + 1
                    printfn "  branch into patched range: %X -> %X (%s)" ins.IP t (fmtIns ins)
printfn "[%s] branch-target sweep: %d hit(s) inside 1A3146B..1A3147F / 1343D5F..1343D62" (if hits = 0 then "ok" else "CHECK") hits

// rax (return value) dead after the replaced calls
for start in [ 0x1A31478UL; 0x1343D63UL ] do
    let f = walk [ start ] [ Register.RAX ] false true 0
    printfn "[%s] rax dead after call (from %X)%s" (if f.IsEmpty then "ok" else "FAIL") start (if f.IsEmpty then "" else "\n    " + String.Join("\n    ", f))
    if not f.IsEmpty then ok <- false

// byte-only variant decode
let newb = hexb "FF C3 84 DB 75 16 8B 95 A8 03 00 00 48 8B CD E8 1E 77 61 FE 33 C9"
printfn "byte-only variant RWb @1A3146A (22 bytes):"
let buf = Array.copy img.[0x1A31462 .. 0x1A3148E]
Array.blit newb 0 buf 8 newb.Length
printDis buf 0x1A31462UL 0UL
let ins = decodeAll buf 0x1A31462UL
let callW = ins |> List.find (fun i -> i.Mnemonic = Mnemonic.Call && i.NearBranchTarget = 0x48B9CUL)
let jne = ins |> List.find (fun i -> i.Mnemonic = Mnemonic.Jne)
printfn "[%s] new call targets 48B9C at %X; jne -> %X (want 1A31486); Sleep call at 1A31480 unchanged"
    (if jne.NearBranchTarget = 0x1A31486UL then "ok" else "FAIL") callW.IP jne.NearBranchTarget
printfn (if ok then "RW OFFLINE CHECKS: ALL PASSED" else "RW OFFLINE CHECKS: FAILURES")
