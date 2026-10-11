# MIC t21 native binding and ownership evidence

2026-10-11. Offline retained-decompile and native-byte review only. No game,
runtime input, installed shader, bridge, executable modification or public
change. Rendering proof agent owns the full-body MIC proof separately.

**Resolved:** the native MIC context field, binding slot/token/cache, acquisition
and release behavior. **Unresolved:** a reachable non-null producer, descriptor
extent, header population, synchronization and runtime dimensions. This is not
a native resource-validity PASS.

## Correct entry and exact binding

`53FD3C` is MeshletInstanceCullingRecord; `5301FC` is MeshletPrecullingRecord.
The distinction matters because other meshlet shaders use t21 for different
resources. Confirmed by `D:\nioh3-re\decomp\0x53fd3c.c` and
`notes/renderer/08-meshlet.md` section3.4, not inferred from the register alone.

The bound shader is digest `e31c389f6d4a38de7b1b784b87c710d2`,
`0x17ad25bb.file` record0. `gpu-shaders/micsat-proof/out/original.ll` declares
typed `Buffer<uint> tValidRectCountCumulativeMap` at SRVt21; handle3 selects it.
The existing root-signature audit establishes the default compute SRV table
t0..47 space0, but a compatible table does not identify the bound resource.

Native bytes in `re/Nioh3.dump.exe`, independently disassembled with Capstone:

| RVA | Instruction / meaning |
|---|---|
| 540540 | `mov r9, [rdi+13A0h]`: MIC context resource pointer |
| 540547 | `mov r8d,15h`: slot21 |
| 54054D / 540550 | stage7 in edx; command context in rcx |
| 540553 | call1D501C, resource-binding token recorder |
| 1D5032 | non-null buffer loads descriptor identity from object+58h |
| 1D5036..1D5041 | compare/update command-context descriptor cache |
| 1D5050..1D5083 | encode slot in token upper16bits, emit64-bit object pointer |
| 1D508C | null input uses command-context+36A0h default descriptor |

Cache address is `ctx+2AA0h+(stage*48+slot)*8`, hence stage7slot21 is
`ctx+35C8h`. Uploader54043C is called by540224, which then flushes shader
constants. The MIC vtable at40B78F0 contains540224 at+0C8h; generic recorder
53FD3C calls that slot before its dispatch. This closes recorder-to-binding
association rather than relying only on adjacent function addresses.

Cleanup53F99C, called from MIC end wrapper53FD1C, compares+35C8h with the
default descriptor, updates the cache, and emits token00150055h with a null
object pointer. See retained `0x53f99c.c`, final block. A null binding is a
supported recorder path; it is not evidence that an actual SAT allocation
exists or that its dimensions are nonzero.

## CPU ownership is explicit; GPU completion is not established

MIC constructor3095154 installs vtable40B78F0 and initializes+13A0h to zero
(native30951BE). Setter3095434 performs:

1. If new pointer is non-null, atomic increment of `[new+8]` at3095449.
2. Load old pointer from `[ctx+13A0h]`; release through532C64 at3095459.
3. Store the new pointer at309545E.

Release532C64 uses atomic decrement (`lock xadd` at532C6D), invoking the
object's virtual+18h destruction route when the result reaches zero.
Teardown30952E0 releases and zeroes+13A0h. These facts prove retained CPU
reference ownership for this setter, including acquire-before-release;
they do not prove GPU fence retirement or absence of concurrent data writes.
The binding-token recorder itself does not acquire another reference in the
inspected1D501C body.

## Producer search result and excluded lookalikes

Retained export `0x3095434.c` reports zero direct callers. No absolute pointer
reference to3095434 was found in the dump. A census of direct+13A0h write
instructions and retained decompiles identified the constructor, setter and
teardown, alongside many unrelated classes using the same offset. This is not
a proof of unreachability: indirect/computed calls, generic copies or alias
writes are not excluded. Do not turn it into an assertion that MIC always
uses a null t21 or that its SAT paths are dead.

The generic53FD3C recorder explicitly sets scene, instance, dispatch, Hi-Z and
cull-code inputs, but does not directly populate+13A0h. Its VSM caller465414
uses a MIC context obtained through110EEC (resource hashDD38B728, fallback
MeshletSys+169DA8h); tracing this setup did not establish a non-null SAT setter.

Existing VSM page-manager dimensions are at+400E4h/+400E8h/+400D8h.
Resize4669E8, retained in `re/scripts_shadow/q3_vsm_state.txt:406`, reallocates
CPU state/stamp/age arrays at+41718h/+41720h/+41728h with `W*H*views*4` bytes.
It also passes those dimensions to3301C2C, which records them at
+40020h/+40024h/+40028h and marks+4002Ch dirty. These are concrete VSM state
facts, **not a demonstrated link to MICt21**. No three-word SAT header or
`3+W*H*views` GPU allocation was proven on this path, so those CPU arrays must
not be substituted for the shader's cumulative map.

## Remaining exact proof obligation

The shader reads t21 words0/1/2 as dimensions/view count and indexes layer data
from `3+width*height*view`. A native resource-bound receipt must connect the
non-null object stored at MICctx+13A0h to its actual SRV FirstElement/NumElements
and typed format, allocation/header producer, selected view range and ordered
producer-to-dispatch lifetime. In the ordinary non-overflowing layout, at least
`3+width*height*viewcount` uint elements are needed for all views. Arithmetic
overflow, zero dimensions and malformed coordinates cannot be dismissed by
this native audit. The full-body shader proof may establish conditional
equivalence without proving every original native input valid, but must state
that domain and its memory-stability assumptions explicitly.

Next narrow source target: resolve how the MIC context acquires a non-null
+13A0h value (setter3095434 or an alias/generic initialization path), then trace
that exact object rather than any similarly named VSM buffer. If the target
runtime never binds one, that affects opportunity frequency, not the shader
equivalence proof. No live observation was made in this task.
