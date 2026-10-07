// gallop_live.fsx — patch B from notes\mainthread-drawprep.md: the draw-list walker's group-length binary search
// (Nioh3+3BC0C, type-3 path at 3BC17) becomes a galloping search (probe s, s+1, s+3, s+7 ... then binary search in
// the last bracket). Same result on sorted lists (verified offline: re\scripts_cpu\gallop_3BC0C.fsx, 24k random
// cases, 0 mismatches); O(log group) probes instead of O(log n).
//   dotnet fsi gallop_live.fsx status | apply | revert        (apply/revert need FT_ALLOW_WRITE=1)
#load "Mem.fsx"
open System
open System.IO
open Iced.Intel
open Mem
open type Iced.Intel.AssemblerRegisters

let mode = if fsi.CommandLineArgs.Length > 1 then fsi.CommandLineArgs.[1] else "status"
let SITE = 0x3BC17
let ORIG = [| 0x44uy; 0x8Buy; 0x54uy; 0x24uy; 0x28uy |]      // mov r10d,[rsp+28h]
let stateFile = Path.Combine(__SOURCE_DIRECTORY__, "gallop_state.txt")

let buildCave (caveAddr: uint64) =
    let a = Assembler(64)
    let mutable gal = a.CreateLabel()
    let mutable bs = a.CreateLabel()
    let mutable bsN = a.CreateLabel()
    let mutable found = a.CreateLabel()
    let mutable eIsS = a.CreateLabel()
    let mutable tail = a.CreateLabel()
    a.mov(r10d, __dword_ptr.[rsp + 0x28L]); a.cmp(r9d, r10d); a.jae(tail)
    a.mov(r11, __qword_ptr.[r8 + 0x18L]); a.mov(rbx, __qword_ptr.[rdx])
    a.mov(eax, r9d); a.mov(rcx, __qword_ptr.[rbx + rax * 8]); a.mov(rcx, __qword_ptr.[rcx])
    a.cmp(r11, __qword_ptr.[rcx + 0x18L]); a.jne(eIsS)
    a.mov(r8d, r9d); a.mov(edx, 1)
    a.Label(&gal)
    a.lea(eax, __dword_ptr.[r8 + rdx]); a.cmp(eax, r10d); a.jae(bsN)
    a.mov(rcx, __qword_ptr.[rbx + rax * 8]); a.mov(rcx, __qword_ptr.[rcx])
    a.cmp(r11, __qword_ptr.[rcx + 0x18L]); a.jne(bs)
    a.mov(r8d, eax); a.add(edx, edx); a.jmp(gal)
    a.Label(&bsN)
    a.mov(eax, r10d)
    a.Label(&bs)
    a.mov(ecx, eax); a.sub(ecx, r8d); a.cmp(ecx, 1); a.jbe(found)
    a.shr(ecx, 1uy); a.add(ecx, r8d)
    a.mov(rdx, __qword_ptr.[rbx + rcx * 8]); a.mov(rdx, __qword_ptr.[rdx])
    a.cmp(r11, __qword_ptr.[rdx + 0x18L]); a.cmove(r8d, ecx); a.cmovne(eax, ecx); a.jmp(bs)
    a.Label(&found)
    a.mov(r10d, eax); a.jmp(tail)
    a.Label(&eIsS)
    a.mov(r10d, r9d)
    a.Label(&tail)
    a.sub(r10d, r9d); a.inc(r10d); a.mov(rax, __qword_ptr.[rsp + 0x30L]); a.mov(__dword_ptr.[rax], r10d)
    a.mov(rbx, __qword_ptr.[rsp + 8L]); a.ret()
    use ms = new MemoryStream()
    a.Assemble(StreamCodeWriter(ms), caveAddr) |> ignore
    ms.ToArray()

let t = attach "Nioh3"
let siteAddr = t.Base + nativeint SITE
let cur () = t.Read(siteAddr, 5)
let jmpTo (cave: nativeint) = Array.append [| 0xE9uy |] (BitConverter.GetBytes(int32 (int64 cave - (int64 siteAddr + 5L))))
let guard () = if Environment.GetEnvironmentVariable "FT_ALLOW_WRITE" <> "1" then failwith "set FT_ALLOW_WRITE=1"
match mode with
| "status" -> printfn "3BC17: %s (%s)" (BitConverter.ToString(cur ())) (if cur () = ORIG then "original" elif (cur ()).[0] = 0xE9uy then "patched" else "UNKNOWN")
| "apply" ->
    guard ()
    if cur () <> ORIG then failwith "site not original"
    let cave = t.AllocNear(siteAddr, 0x1000)
    t.Write(cave, buildCave (uint64 cave))
    File.WriteAllText(stateFile, sprintf "%X %d" (int64 cave) t.Process.Id)
    let n = safeWrite t [ siteAddr, jmpTo cave, ORIG ]
    printfn "B gallop applied on attempt %d (cave %X)" n (int64 cave)
| "revert" ->
    guard ()
    let v = File.ReadAllText(stateFile).Split(' ')
    if int v.[1] <> t.Process.Id then failwith "state belongs to another process"
    let cave = nativeint (Convert.ToInt64(v.[0], 16))
    if cur () = jmpTo cave then
        safeWrite t [ siteAddr, ORIG, jmpTo cave ] |> ignore
        printfn "B gallop reverted (cave left allocated)"
    else printfn "not patched"
| m -> printfn "unknown mode %s" m
