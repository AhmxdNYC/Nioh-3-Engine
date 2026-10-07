// ld_pools.fsx - for every pool constructor called by EngineArenaInit 0x294C2E0 (and helpers), find the vtable it
// installs (lea r,[rip+X] ... mov [rcx|rbx|rdi],r) and print the RTTI class name of that vtable (COL at vtable-8).
#load @"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_wjobs\wj_common.fsx"
open System
open Iced.Intel
open Wj_common
let B = DUMP_BASE
let u64 (o:int) = BitConverter.ToUInt64(img, o)
let i32 (o:int) = BitConverter.ToInt32(img, o)
let cstr (o:int) = let mutable e = o in (while img.[e] <> 0uy do e <- e + 1); Text.Encoding.ASCII.GetString(img, o, e - o)
let rttiName (vt:uint64) =
    try
        let col = u64 (int vt - 8)
        let colRva = int (col - B)
        // x64 COL: sig(4) off(4) cdOff(4) pTypeDescriptor(rva 4) pClassDescriptor(rva 4) pSelf(4)
        let td = i32 (colRva + 12)
        cstr (td + 16)
    with _ -> "?"
// walk EngineArenaInit: record (call target, preceding mov edx/rdx immediate, lea rcx disp)
let s = 0x294C2E0u
let e = s + 2650u
let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
dec.IP <- uint64 s
let mutable lastLea = 0L
let mutable lastSize = 0UL
while dec.IP < uint64 e do
    let ins = dec.Decode()
    if ins.Mnemonic = Mnemonic.Lea && ins.Op0Register = Register.RCX then lastLea <- int64 ins.MemoryDisplacement64
    if ins.Mnemonic = Mnemonic.Mov && (ins.Op0Register = Register.EDX || ins.Op0Register = Register.RDX) && ins.Op1Kind <> OpKind.Register && ins.Op1Kind <> OpKind.Memory then lastSize <- ins.GetImmediate 1
    if ins.Mnemonic = Mnemonic.Call && ins.Op0Kind = OpKind.NearBranch64 then
        let t = ins.NearBranchTarget
        // find vtable store in the constructor (first 0x80 bytes, also follow one call/jmp)
        let rec findVt (addr:uint64) depth =
            let d = Decoder.Create(64, ByteArrayCodeReader(img, int addr, 0x100))
            d.IP <- addr
            let mutable res = None
            let mutable lea = 0UL
            let mutable n = 0
            while res.IsNone && n < 40 do
                let x = d.Decode()
                n <- n + 1
                if x.Mnemonic = Mnemonic.Lea && x.IsIPRelativeMemoryOperand then lea <- x.IPRelativeMemoryAddress
                if x.Mnemonic = Mnemonic.Mov && x.Op0Kind = OpKind.Memory && x.MemoryDisplacement64 = 0UL && x.Op1Kind = OpKind.Register && lea <> 0UL then
                    let nm = rttiName lea
                    if nm <> "?" && nm.StartsWith(".?A") then res <- Some (lea, nm)
                if x.Mnemonic = Mnemonic.Ret then n <- 999
                if (x.Mnemonic = Mnemonic.Jmp || x.Mnemonic = Mnemonic.Call) && x.Op0Kind = OpKind.NearBranch64 && depth < 1 && res.IsNone then
                    match findVt x.NearBranchTarget (depth + 1) with
                    | Some r -> res <- Some r
                    | None -> ()
            res
        let vt = findVt t 0
        printfn "%8X call %8X  pool@+%6X  size %5d MB  %s" ins.IP t lastLea (lastSize >>> 20) (match vt with Some (v, n) -> sprintf "vt %X %s" v n | None -> "-")
