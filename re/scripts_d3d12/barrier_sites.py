r"""barrier_sites.py -- build re\scripts_d3d12\barrier_sites.tsv: one row per SynTransitionResource (115E24) call site with the
return RVA that gpuopt's trace stores as caller2_rva (ASI >= 1.7.9), so vkd3d_cost.py rows can be joined to the caller classes
of notes\vkd3d-audit.md (Addendum B).  Input: the text output of barrier_callers.fsx.
usage: dotnet fsi barrier_callers.fsx > sites.txt ; py -I barrier_sites.py sites.txt > barrier_sites.tsv"""
import re, sys

# func -> (token(s), role, resource kind, stamp source); see notes\vkd3d-audit.md Addendum B (static, dump v2.02)
F = {
 0x113A60: ("5F", "UAV bind (cache +1DB0)", "buf|tex (stream decides)", "stream"),
 0x1140C4: ("5E", "UAV bind (cache +1D30)", "buf|tex", "arg"),
 0x11422C: ("5F/5E", "UAV bind helper", "buf|tex", "arg"),
 0x113BA4: ("4E", "SRV bind (cache +1118), X=(cur&A83)|NPSR", "buf", "stream"),
 0x114E24: ("4F-53", "SRV bind per stage (cache +1118), X=(cur&AC3)|PSR/NPSR", "buf", "arg"),
 0x114F68: ("?", "SRV bind variant (cache +1118)", "buf", "arg"),
 0x115664: ("?", "SRV bind variant (cache +1118), X=arg", "buf", "arg"),
 0x115084: ("54", "SRV bind (cache +1718)", "buf", "stream"),
 0x815DB0: ("55", "SRV bind (cache +1B98)", "buf", "stream"),
 0x116260: ("3C", "VB bind slot, X=(cur&AC3)|vb+48", "buf", "obj+28"),
 0x1163AC: ("71", "VB bind variant", "buf", "obj+28"),
 0x2EFD430: ("79", "draw: VB + IB, X=(cur&AC3)|+50", "buf", "obj+28"),
 0x58BC74: ("81", "EI dispatch args, X=obj+30", "buf", "114C04"),
 0x58BD2C: ("?", "EI dispatch args (twin of 58BC74)", "buf", "114C04"),
 0x6EB908: ("6F", "EI draw args, X=obj+30", "buf", "114C04"),
 0x6EB9CC: ("?", "EI draw args (twin of 6EB908)", "buf", "114C04"),
 0x6ECF48: ("6E", "EI draw args, X=obj+30", "buf", "114C04"),
 0x6ED00C: ("89 via 2EFD8DC", "EI draw args (twin of 6ECF48)", "buf", "114C04"),
 0x2EFD138: ("80", "EI args, X=obj+30", "buf", "114C04"),
 0x2EFD8DC: ("89", "GPU-driven batch: buffer SRV (cur&A83)|NPSR, then X=obj+48", "buf", "obj+28"),
 0x2F00468: ("B8", "predication buffer -> PREDICATION(200), SetPredication", "buf", "inline +18"),
 0x2F0ABC4: ("? via 2EFDC24", "query resolve dst -> COPY_DEST, ResolveQueryData", "buf", "inline (cmove, may be 0)"),
 0x2F0075C: ("68", "CopyBufferRegion src/dst, then STREAM_OUT + SOSetTargets", "buf", "114C04/arg"),
 0x7A5FB8: ("99/8B", "buffer copy dst/src", "buf", "obj+28"),
 0x7F6574: ("95", "COPY_DEST, CopyBufferRegion, restore", "buf", "obj+28"),
 0x7F67EC: ("95", "COPY_DEST, CopyBufferRegion, restore", "buf", "obj+28"),
 0x7B18E4: ("94", "ClearUAVUint -> UAV", "buf|tex", "arg"),
 0x3D62E4: ("main thread", "back buffer -> PRESENT(0)", "tex", "NULL"),
 0x115B9C: ("3E-46,48-4D,86", "texture SRV bind, X=(cur&AC3)|PSR/NPSR (R1 source)", "tex", "NULL"),
 0x11518C: ("57-5B", "texture SRV bind, X=obj+24", "tex", "NULL"),
 0x1152A4: ("?", "texture SRV bind (twin of 11518C)", "tex", "NULL"),
 0x115398: ("56", "texture SRV bind, X=obj+24", "tex", "NULL"),
 0x8507B0: ("5C", "texture SRV bind (cache +1718)", "tex", "NULL"),
 0x823534: ("5D", "texture SRV bind (cache +1B98)", "tex", "NULL"),
 0x698A44: ("87", "texture bind, X=(cur&AC3)|bit", "tex", "NULL"),
 0x698C54: ("?", "texture bind (twin of 698A44)", "tex", "NULL"),
 0x4591DC: ("04 (02 via 458E34)", "ClearRTV -> RT / ClearDSV -> DEPTH_WRITE", "tex", "NULL"),
 0x2EFBDA4: ("05", "ClearRTV -> RT / ClearDSV -> DEPTH_WRITE", "tex", "NULL"),
 0x45A020: ("03", "CopyTextureRegion src/dst + restore", "tex", "NULL"),
 0x45A190: ("01 / 7A / 7B", "CopyTextureRegion src/dst + restore", "tex", "NULL"),
 0x45C880: ("7A", "generic copy (stack trackers): COPY_DEST/COPY_SOURCE/RT", "tex", "NULL"),
 0x45DDC8: ("7B", "depth copy: COPY_*, DEPTH_READ restore", "tex", "NULL"),
 0x9E2BA4: ("91", "CopyTextureRegion src + restore", "tex", "NULL"),
 0x2EFFF54: ("93", "ResolveSubresourceRegion RESOLVE_DEST/SOURCE + restore", "tex", "NULL"),
 0x2EFE094: ("? via 2EED2E0", "X -> COMMON helper", "?", "NULL"),
}
cur = None
print("site\tret_rva(caller2)\tfunc\ttoken\trole\tkind\tstamp\tnew_state_src\tstamp_arg")
rows = []
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    m = re.match(r"site ([0-9A-F]+) func ([0-9A-F]+)", line)
    if m:
        cur = {"site": int(m.group(1), 16), "func": int(m.group(2), 16)}
        rows.append(cur)
        continue
    m = re.match(r"\s+(r9\(new\)|s20\(stamp\))\s+(.*)$", line)
    if m and cur is not None:
        cur["r9" if m.group(1).startswith("r9") else "s20"] = m.group(2).split("  <=")[0].split(":", 1)[-1].strip()
for r in rows:
    tok, role, kind, st = F.get(r["func"], ("?", "?", "?", "?"))
    s20 = r.get("s20", "?")
    if "and qword" in s20:
        st = "NULL"
    print("%X\t%X\t%X\t%s\t%s\t%s\t%s\t%s\t%s" % (r["site"], r["site"] + 5, r["func"], tok, role, kind, st, r.get("r9", "?"), s20))
