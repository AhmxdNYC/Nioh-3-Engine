# Stateless compute key hashing: bounded reduction

2026-10-11. Offline same-picture arithmetic candidate, separate from unsafe
cached-PSO serving. No ASI/game/install changes or FPS measurement.

Fresh retained-dump inspection closes a small concrete dependency: three
compute callers pass exactly eight bytes to generic PsoKeyHash1199A4:
58BF3B (dispatch),58C020 (shared getter),2EFE212 (preload). The function's
164-byte SHA256 is
`91feb6ed7555a21750613844e53e01158565fb1d0a9081d7803dd207fad0d24e`.
`tools/bench/audit_compute_hash8.py` verifies native instruction anchors and
straight-line length8 argument/call windows, emitting each window hash.

Native mode is read from context+2490 on every invocation. Zero mode XORs two
little-endian dwords. Nonzero mode seeds EAX with FFFFFFFF and executes two
CRC32 dword iterations, including generic loop/tail branches. Its result is
raw CRC state, with no final complement. The fixed-size equivalent is XOR of
the two halves or one CRC32 qword instruction with the same zero-extended
FFFFFFFF seed. All results remain zero-extended32-bit. Do not substitute a
normal file CRC or apply a complement.

`tools/bench/compute_hash8_test.cpp` compiled MSVC /O2 /W4 /WX /EHsc /std:c++17
and passed131,204 hardware arithmetic cases across both modes: every single
input bit, zero/allones, and65,536 deterministic64-bit samples. Wrong seed,
missing upper half and reversed dword order each fail. The algebraic basis is
the CRC instruction consuming the same ordered eight bytes with one initial
state; XOR associativity handles the other branch. Tests establish finite
arithmetic agreement, not universal native patch or thread safety.

This removes one dependent CRC operation and generic loop/length bookkeeping
on the eight-byte path. It retains no PSO pointer or shader object and thus
does not require cache mutation epochs merely to compute the hash. Native
lookup, full24-byte output, miss/create, trim, locking and PSO ownership must
remain stock. No gain estimate is justified: this is probably small compared
with B-tree traversal/locking/driver creation, and code-cave/helper call overhead
could erase it. The existing4% slow-link compute lookup sample cannot be billed
to these few instructions.

## Native implementation boundary

### Peer-review correction and ordered-load offline helper

The dispatch hash at58BF3B and subsequent key reread58BF4A both precede
EnterCriticalSection58BF55. The later cache lock therefore supplies **no key
stability proof**. The original single-qword arithmetic test above consumes an
already copied value; it was not a safe native load replacement.

The audit now rejects mismatching complete hash-body and all three caller-body
SHA256s, and verifies concrete key/context pointer provenance, rather than
merely printing hashes and checking length8. Six deliberate mode/CRC/argument/
length mutations are rejected by `test_compute_hash8_binding.py`.

New offline `compute_hash8_ordered.asm` preserves the Win64 call interface
(RCX=context,RDX=key,R8D=length). For length8 it reads the mode byte first,
then exactly two ordered DWORDs at key+0/key+4. CRC mode assembles the qword
**from those two loaded registers**, without a qword memory read, and performs
one raw seeded CRC. XOR mode likewise reads both DWORDs in order. It uses only
volatile RAX/R9 and flags, leaves the stack and nonvolatile registers untouched,
and zero-extends the32-bit result. Other lengths tail-call an externally supplied
original pointer with arguments unchanged; this is an offline test stub, not a
live address/patch or installed trampoline.

ML64 plus MSVC /O2 /W4 /WX build PASS. Actual assembled helper passes98,306
full64-bit return checks over8 alignments, zero/one/255 mode bytes and a guarded
page ending exactly after the8-byte key;129 non-eight lengths forward unchanged
without touching null context/key. Dumpbin confirms the52-byte helper has only
those two DWORD memory reads per mode, one byte mode read and register CRC.
No instruction touches a nonvolatile register or RSP. These checks do not prove
caller lifetime, race freedom or identical exception instruction addresses;
they preserve the original load granularity/order instead of assuming a lock.
The later caller key reread and lookup remain unchanged. Native admission and
actual worthwhile CPU cost are still open; no installed edits or FPS claim.

The next implementation unit, if selected by lead, is an exact callsite-bound
length8 helper for58BF3B only, preserving ABI/nonvolatile registers and stock
mode reads. Other lengths must stay generic. The8-byte input must remain stable
over the original operation: replacing two32-bit loads with one64-bit load is
not an equivalence claim under concurrent key writes. A proof of that caller's
context ownership or a same-load-pattern variant is still needed before live
installation; this note authorizes neither. Byte binding/patch thread-safety,
actual emitted instructions and profiler-off short matched CPU tests remain
native gates. Reject if overhead or lack of critical-path cost makes it neutral.

Fresh audit also confirms preload2EFE1CC creates a PSO before taking the cache
lock and checking for a duplicate. An early duplicate return is a separate,
unproved proposal: CreateComputePso58C2B0 invokes device creation and failure
callback18956D0, and concurrent cache removal can change later contents.
Do not merge that behavior change into this stateless hash reduction.

Reproduce the native binding without reading the whole dump into RAM:

    py -I -B tools/bench/audit_compute_hash8.py C:/Users/"Ahmad Hamza"/nioh3-engine/re/Nioh3.dump.exe

Compile the standalone C++ test with the MSVC flags above. It checks SSE4.2
before using hardware CRC; an unsupported CPU returns SKIP, never PASS.
