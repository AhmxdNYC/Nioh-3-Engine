// N3Query - ad-hoc queries against the analyzed Nioh3 dump (run read-only).
//
// Args: <outFile> <query> [<query> ...]   (all addresses are RVAs, hex with or without 0x)
//   0x5af50a            decompile the function containing this RVA (+ header, callers, callees)
//   d:0x5af50a          same as above
//   asm:0x5af50a[:N]    disassemble N (default 24) instructions before/after the RVA
//   x:0x44b5000         list references TO this RVA (from-address, containing function, ref type)
//   c:0x5af50a          callers of the function containing the RVA (1 level) with call sites
//   s:REGEX             strings matching REGEX (Java regex, case-insensitive) + referencing functions
//
// Function containment falls back to the PE .pdata table (chained unwind entries resolved to the
// primary entry), and a function is created in memory if Ghidra has none there. -readOnly means
// nothing is ever saved back to the project.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.StringDataInstance;
import ghidra.app.cmd.function.CreateFunctionCmd;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class N3Query extends GhidraScript {
    long base;
    PrintWriter out;
    DecompInterface di;
    byte[] pdata;
    long pdataRva;

    String rva(Address a) { return "0x" + Long.toHexString(a.getOffset() - base); }
    Address addr(long r) { return currentProgram.getImageBase().add(r); }
    long parse(String s) { s = s.trim().toLowerCase(); if (s.startsWith("0x")) s = s.substring(2); return Long.parseUnsignedLong(s, 16); }

    int u32(byte[] b, int o) { return (b[o] & 0xff) | (b[o+1] & 0xff) << 8 | (b[o+2] & 0xff) << 16 | (b[o+3] & 0xff) << 24; }

    void loadPdata() throws Exception {
        Memory mem = currentProgram.getMemory();
        Address hdr = currentProgram.getImageBase();
        byte[] h = new byte[0x400];
        mem.getBytes(hdr, h);
        int pe = u32(h, 0x3c);
        int opt = pe + 24;
        int dir3 = opt + 112 + 8 * 3;
        pdataRva = u32(h, dir3) & 0xffffffffL;
        int size = u32(h, dir3 + 4);
        pdata = new byte[size];
        mem.getBytes(addr(pdataRva), pdata);
    }

    // returns primary function start RVA from .pdata, or -1
    long pdataStart(long r) throws Exception {
        if (pdata == null) loadPdata();
        int n = pdata.length / 12;
        int lo = 0, hi = n - 1, hit = -1;
        while (lo <= hi) {
            int mid = (lo + hi) >>> 1;
            long b = u32(pdata, mid * 12) & 0xffffffffL, e = u32(pdata, mid * 12 + 4) & 0xffffffffL;
            if (r < b) hi = mid - 1; else if (r >= e) lo = mid + 1; else { hit = mid; break; }
        }
        if (hit < 0) return -1;
        long begin = u32(pdata, hit * 12) & 0xffffffffL;
        long unw = u32(pdata, hit * 12 + 8) & 0xffffffffL;
        Memory mem = currentProgram.getMemory();
        for (int guard = 0; guard < 32; guard++) {
            if ((unw & 1) != 0) { // pointer to another RUNTIME_FUNCTION
                long rf = unw & ~1L;
                begin = mem.getInt(addr(rf)) & 0xffffffffL;
                unw = mem.getInt(addr(rf + 8)) & 0xffffffffL;
                continue;
            }
            int b0 = mem.getByte(addr(unw)) & 0xff;
            int flags = b0 >> 3;
            if ((flags & 4) == 0) break;
            int cnt = mem.getByte(addr(unw + 2)) & 0xff;
            long chained = unw + 4 + (((cnt + 1) & ~1) * 2L);
            begin = mem.getInt(addr(chained)) & 0xffffffffL;
            unw = mem.getInt(addr(chained + 8)) & 0xffffffffL;
        }
        return begin;
    }

    Function funcFor(long r) throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        long ps = pdataStart(r);
        if (ps >= 0) {
            Function f = fm.getFunctionAt(addr(ps));
            if (f != null) return f;
        }
        Function f = fm.getFunctionContaining(addr(r));
        if (f != null) return f;
        if (ps >= 0) {
            CreateFunctionCmd cmd = new CreateFunctionCmd(addr(ps));
            cmd.applyTo(currentProgram, monitor);
            f = fm.getFunctionAt(addr(ps));
            if (f != null) out.println("// (function created on the fly from .pdata)");
            return f;
        }
        return null;
    }

    void decompile(long r) throws Exception {
        Function f = funcFor(r);
        out.println("==== decompile RVA 0x" + Long.toHexString(r) + " ====");
        if (f == null) { out.println("// no function contains this address"); return; }
        long ps = pdataStart(r);
        out.println("// function " + f.getName() + " @ " + rva(f.getEntryPoint()) + "  body=" + f.getBody().getNumAddresses()
                + " bytes  pdataStart=" + (ps >= 0 ? "0x" + Long.toHexString(ps) : "none"));
        StringBuilder cs = new StringBuilder();
        int k = 0;
        for (Function c : f.getCallingFunctions(monitor)) { if (k++ < 40) cs.append(c.getName()).append("@").append(rva(c.getEntryPoint())).append(' '); }
        out.println("// callers(" + k + "): " + cs);
        StringBuilder ce = new StringBuilder();
        k = 0;
        for (Function c : f.getCalledFunctions(monitor)) { if (k++ < 60) ce.append(c.getName()).append("@").append(c.isExternal() ? "EXT" : rva(c.getEntryPoint())).append(' '); }
        out.println("// callees(" + k + "): " + ce);
        if (di == null) {
            di = new DecompInterface();
            DecompileOptions o = new DecompileOptions();
            di.setOptions(o);
            di.toggleCCode(true);
            di.toggleSyntaxTree(false);
            di.setSimplificationStyle("decompile");
            di.openProgram(currentProgram);
        }
        DecompileResults res = di.decompileFunction(f, 180, monitor);
        if (res == null || !res.decompileCompleted()) {
            out.println("// DECOMPILE FAILED: " + (res == null ? "null" : res.getErrorMessage()));
            return;
        }
        out.println(res.getDecompiledFunction().getC());
    }

    void asm(long r, int n) throws Exception {
        out.println("==== asm around RVA 0x" + Long.toHexString(r) + " ====");
        Listing l = currentProgram.getListing();
        Address a = addr(r);
        Instruction ins = l.getInstructionContaining(a);
        if (ins == null) {
            // disassemble in memory starting at the containing function / address
            Function f = funcFor(r);
            if (f == null) { ghidra.app.cmd.disassemble.DisassembleCommand dc = new ghidra.app.cmd.disassemble.DisassembleCommand(a, null, true); dc.applyTo(currentProgram, monitor); }
            ins = l.getInstructionContaining(a);
        }
        if (ins == null) { out.println("// no instruction at address"); return; }
        Instruction start = ins;
        for (int i = 0; i < n; i++) { Instruction p = start.getPrevious(); if (p == null) break; start = p; }
        Instruction cur = start;
        for (int i = 0; i < 2 * n + 1 && cur != null; i++) {
            StringBuilder bytes = new StringBuilder();
            for (byte b : cur.getBytes()) bytes.append(String.format("%02x ", b));
            String mark = cur.getAddress().equals(ins.getAddress()) ? "=>" : "  ";
            StringBuilder refs = new StringBuilder();
            for (Reference rf : cur.getReferencesFrom()) {
                Address to = rf.getToAddress();
                if (!to.isMemoryAddress()) continue;
                refs.append(" ; ").append(rf.getReferenceType()).append(" ").append(rva(to));
                Symbol s = currentProgram.getSymbolTable().getPrimarySymbol(to);
                if (s != null) refs.append(" ").append(s.getName());
                Data d = l.getDataAt(to);
                if (d != null && d.hasStringValue()) refs.append(" \"").append(String.valueOf(d.getValue())).append("\"");
            }
            out.println(String.format("%s %-10s %-30s %s%s", mark, rva(cur.getAddress()), bytes.toString().trim(), cur.toString(), refs));
            cur = cur.getNext();
        }
    }

    void xrefs(long r) throws Exception {
        out.println("==== references to RVA 0x" + Long.toHexString(r) + " ====");
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(addr(r));
        int n = 0;
        while (it.hasNext() && n < 500) {
            Reference x = it.next();
            Function f = fm.getFunctionContaining(x.getFromAddress());
            out.println(rva(x.getFromAddress()) + "\t" + x.getReferenceType() + "\t" + (f == null ? "-" : f.getName() + "@" + rva(f.getEntryPoint())));
            n++;
        }
        if (n == 0) out.println("(none)");
    }

    void callers(long r) throws Exception {
        Function f = funcFor(r);
        out.println("==== callers of function containing 0x" + Long.toHexString(r) + " ====");
        if (f == null) { out.println("(no function)"); return; }
        out.println("// " + f.getName() + " @ " + rva(f.getEntryPoint()));
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(f.getEntryPoint());
        while (it.hasNext()) {
            Reference x = it.next();
            Function c = fm.getFunctionContaining(x.getFromAddress());
            out.println(rva(x.getFromAddress()) + "\t" + x.getReferenceType() + "\t" + (c == null ? "-" : c.getName() + "@" + rva(c.getEntryPoint())));
        }
    }

    void strings(String re) throws Exception {
        out.println("==== strings matching /" + re + "/ ====");
        Pattern p = Pattern.compile(re, Pattern.CASE_INSENSITIVE);
        FunctionManager fm = currentProgram.getFunctionManager();
        DataIterator it = currentProgram.getListing().getDefinedData(true);
        int n = 0;
        while (it.hasNext() && n < 2000) {
            Data d = it.next();
            if (!d.hasStringValue()) continue;
            String s = StringDataInstance.getStringDataInstance(d).getStringValue();
            if (s == null || !p.matcher(s).find()) continue;
            LinkedHashSet<String> fs = new LinkedHashSet<>();
            ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(d.getAddress());
            while (ri.hasNext()) {
                Reference x = ri.next();
                Function f = fm.getFunctionContaining(x.getFromAddress());
                fs.add(f == null ? "D" + rva(x.getFromAddress()) : rva(f.getEntryPoint()));
            }
            out.println(rva(d.getAddress()) + "\t" + String.join(",", fs) + "\t" + s.replace("\n", "\\n"));
            n++;
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        base = currentProgram.getImageBase().getOffset();
        out = new PrintWriter(new OutputStreamWriter(new FileOutputStream(args[0]), "UTF-8"));
        int tx = currentProgram.startTransaction("query");
        try {
            for (int i = 1; i < args.length; i++) {
                String q = args[i];
                try {
                    if (q.startsWith("asm:")) {
                        String[] p = q.substring(4).split(":");
                        asm(parse(p[0]), p.length > 1 ? Integer.parseInt(p[1]) : 24);
                    } else if (q.startsWith("x:")) xrefs(parse(q.substring(2)));
                    else if (q.startsWith("c:")) callers(parse(q.substring(2)));
                    else if (q.startsWith("s:")) strings(q.substring(2));
                    else if (q.startsWith("d:")) decompile(parse(q.substring(2)));
                    else decompile(parse(q));
                } catch (Exception e) {
                    out.println("// ERROR on query " + q + ": " + e);
                }
                out.println();
                out.flush();
            }
        } finally {
            currentProgram.endTransaction(tx, false);
            if (di != null) di.dispose();
            out.close();
        }
    }
}
