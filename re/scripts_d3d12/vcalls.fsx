// vcalls.fsx -- find COM-style virtual calls reachable from the KTGL token handler table.
//   dotnet fsi vcalls.fsx [depth=3] [tableRva=3C02220] [count=185] [extraRoots hex,...]
// For each handler (and functions reachable by direct calls up to <depth>), lists every
// `call [reg+disp]` together with the instruction that loaded `this` (rcx) and the vtable,
// so ctx+580h (ID3D12GraphicsCommandList) etc. can be recognised. Reads the dump only.
#r "nuget: Iced, 1.21.0"
open System
open System.IO
open System.Collections.Generic
open Iced.Intel

let img = File.ReadAllBytes @"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
let BASE = 0x7FF74F410000UL
let u32 o = BitConverter.ToUInt32(img, o)
let pe = BitConverter.ToInt32(img, 0x3C)
let opt = pe + 24
let dd = opt + 112
let excRva, excSz = int (u32 (dd + 24)), int (u32 (dd + 28))
let pBegin = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12))
let pEnd = Array.init (excSz / 12) (fun i -> u32 (excRva + i * 12 + 4))
let funcRange (rva: uint32) =
    let i = Array.BinarySearch(pBegin, rva)
    let i = if i >= 0 then i else ~~~i - 1
    if i >= 0 && rva < pEnd.[i] then Some (pBegin.[i], pEnd.[i]) else None

let cl = dict [
    0x48,"Close";0x50,"Reset";0x58,"ClearState";0x60,"DrawInstanced";0x68,"DrawIndexedInstanced";0x70,"Dispatch"
    0x78,"CopyBufferRegion";0x80,"CopyTextureRegion";0x88,"CopyResource";0x90,"CopyTiles";0x98,"ResolveSubresource"
    0xA0,"IASetPrimitiveTopology";0xA8,"RSSetViewports";0xB0,"RSSetScissorRects";0xB8,"OMSetBlendFactor";0xC0,"OMSetStencilRef"
    0xC8,"SetPipelineState";0xD0,"ResourceBarrier";0xD8,"ExecuteBundle";0xE0,"SetDescriptorHeaps";0xE8,"SetComputeRootSignature"
    0xF0,"SetGraphicsRootSignature";0xF8,"SetComputeRootDescriptorTable";0x100,"SetGraphicsRootDescriptorTable"
    0x108,"SetComputeRoot32BitConstant";0x110,"SetGraphicsRoot32BitConstant";0x118,"SetComputeRoot32BitConstants";0x120,"SetGraphicsRoot32BitConstants"
    0x128,"SetComputeRootCBV";0x130,"SetGraphicsRootCBV";0x138,"SetComputeRootSRV";0x140,"SetGraphicsRootSRV";0x148,"SetComputeRootUAV";0x150,"SetGraphicsRootUAV"
    0x158,"IASetIndexBuffer";0x160,"IASetVertexBuffers";0x168,"SOSetTargets";0x170,"OMSetRenderTargets";0x178,"ClearDepthStencilView";0x180,"ClearRenderTargetView"
    0x188,"ClearUAVUint";0x190,"ClearUAVFloat";0x198,"DiscardResource";0x1A0,"BeginQuery";0x1A8,"EndQuery";0x1B0,"ResolveQueryData";0x1B8,"SetPredication"
    0x1C0,"SetMarker";0x1C8,"BeginEvent";0x1D0,"EndEvent";0x1D8,"ExecuteIndirect";0x1E0,"AtomicCopyBufferUINT";0x1F0,"OMSetDepthBounds";0x208,"SetViewInstanceMask"
    0x210,"WriteBufferImmediate";0x220,"BeginRenderPass";0x228,"EndRenderPass";0x258,"SetPipelineState1";0x260,"DispatchRays";0x268,"RSSetShadingRate";0x278,"DispatchMesh";0x280,"Barrier" ]

let args = fsi.CommandLineArgs |> Array.skip 1
let depth = if args.Length > 0 then int args.[0] else 3
let tab = if args.Length > 1 then Convert.ToInt32(args.[1], 16) else 0x3C02220
let cnt = if args.Length > 2 then int args.[2] else 185
let extra = if args.Length > 3 then args.[3].Split(',') |> Array.map (fun s -> Convert.ToUInt32(s, 16)) else [||]

let decodeFn (s: uint32) (e: uint32) =
    let dec = Decoder.Create(64, ByteArrayCodeReader(img, int s, int (e - s)))
    dec.IP <- uint64 s
    let l = List<Instruction>()
    while dec.IP < uint64 e do l.Add(dec.Decode())
    l
let fmt = NasmFormatter()
let so = StringOutput()
let txt (i: Instruction) = fmt.Format(&i, so); so.ToStringAndReset()

let visited = HashSet<uint32>()
let results = List<string>()
let rec walk (root: string) (rva: uint32) (d: int) =
    match funcRange rva with
    | None -> ()
    | Some (s, e) when visited.Add s ->
        let ins = decodeFn s e
        for k in 0 .. ins.Count - 1 do
            let i = ins.[k]
            if i.Mnemonic = Mnemonic.Call then
                if i.Op0Kind = OpKind.NearBranch64 then
                    if d < depth then walk root (uint32 i.NearBranch64) (d + 1)
                elif i.Op0Kind = OpKind.Memory && i.MemoryBase <> Register.RIP && i.MemoryBase <> Register.None then
                    let disp = int i.MemoryDisplacement64
                    // find the most recent load into rcx before this call
                    let mutable thisSrc = "?"
                    let mutable j = k - 1
                    while j >= 0 && j >= k - 12 && thisSrc = "?" do
                        let p = ins.[j]
                        if (p.Mnemonic = Mnemonic.Mov || p.Mnemonic = Mnemonic.Lea) && p.Op0Kind = OpKind.Register && p.Op0Register = Register.RCX then
                            thisSrc <- txt p
                        j <- j - 1
                    let m = if thisSrc.Contains("580h]") then (match cl.TryGetValue disp with | true, n -> "CL." + n | _ -> sprintf "CL.+%X" disp) else ""
                    results.Add(sprintf "%s\t%X\t%X\t+%X\t%s\t%s" root s (uint32 i.IP) disp thisSrc m)
    | _ -> ()

for t in 0 .. cnt - 1 do
    let q = BitConverter.ToUInt64(img, tab + 8 * t)
    let r = uint32 (q - BASE)
    visited.Clear()
    walk (sprintf "tok%02X" t) r 0
for r in extra do
    visited.Clear()
    walk (sprintf "fn%X" r) r 0
printfn "root\tfunc\tsite\tdisp\tthis-load\tmethod"
for l in results |> Seq.distinct do printfn "%s" l
