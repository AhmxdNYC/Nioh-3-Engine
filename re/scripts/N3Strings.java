// N3Strings - full string table with cross references, independent of which strings Ghidra defined.
//   -postScript N3Strings.java <outFile>
// Scans .rdata/.data/_RDATA for NUL-terminated ASCII (>=4 chars) and UTF-16LE (>=4 chars) strings.
// For each string reports:
//   funcs : functions whose code references the string directly (lea/mov rip-relative)
//   via   : pointer slots (absolute qwords in .data/.rdata - this is a runtime dump, so statically
//           initialised pointer tables hold real addresses) as  slotRVA[>tableBaseRVA]=funcs
//           where tableBase is the nearest lower address in the same block that code references
//           (string tables are indexed from their base, so that is where the xref lands).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.util.*;

public class N3Strings extends GhidraScript {
    long base, imgEnd;
    FunctionManager fm;
    ReferenceManager rm;

    String hx(long r) { return "0x" + Long.toHexString(r); }
    Address addr(long r) { return currentProgram.getImageBase().add(r); }

    String funcsRefTo(long r, int max) {
        LinkedHashSet<String> s = new LinkedHashSet<>();
        ReferenceIterator it = rm.getReferencesTo(addr(r));
        int n = 0;
        while (it.hasNext() && s.size() < max && n++ < 2000) {
            Reference x = it.next();
            Function f = fm.getFunctionContaining(x.getFromAddress());
            if (f != null) s.add(hx(f.getEntryPoint().getOffset() - base));
            else s.add("D" + hx(x.getFromAddress().getOffset() - base));
        }
        return String.join(",", s);
    }

    static String clean(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '\t') b.append("\\t"); else if (c == '\n') b.append("\\n"); else if (c == '\r') b.append("\\r");
            else if (c < 0x20) b.append(String.format("\\x%02x", (int) c)); else b.append(c);
        }
        return b.toString();
    }

    static boolean pr(int c) { return (c >= 0x20 && c < 0x7f) || c == 9 || c == 10 || c == 13; }

    @Override
    public void run() throws Exception {
        String outFile = getScriptArgs()[0];
        base = currentProgram.getImageBase().getOffset();
        fm = currentProgram.getFunctionManager();
        rm = currentProgram.getReferenceManager();
        Memory mem = currentProgram.getMemory();
        imgEnd = currentProgram.getMaxAddress().getOffset();

        String[] blocks = {".rdata", ".data", "_RDATA"};
        Map<String, byte[]> data = new LinkedHashMap<>();
        Map<String, Long> start = new HashMap<>();
        for (String bn : blocks) {
            MemoryBlock b = mem.getBlock(bn);
            if (b == null) { println("no block " + bn); continue; }
            byte[] buf = new byte[(int) b.getSize()];
            b.getBytes(b.getStart(), buf);
            data.put(bn, buf);
            start.put(bn, b.getStart().getOffset() - base);
        }

        // 1. pointer slots: target RVA -> list of slot RVAs
        HashMap<Long, List<Long>> ptrs = new HashMap<>();
        for (String bn : data.keySet()) {
            byte[] buf = data.get(bn);
            long s0 = start.get(bn);
            for (int o = 0; o + 8 <= buf.length; o += 8) {
                long v = 0;
                for (int k = 7; k >= 0; k--) v = (v << 8) | (buf[o + k] & 0xff);
                if (v >= base + 0x38e2000L && v <= imgEnd) {
                    ptrs.computeIfAbsent(v - base, z -> new ArrayList<>(2)).add(s0 + o);
                }
            }
        }
        println("pointer targets in data: " + ptrs.size());

        // 2. sorted list of referenced addresses inside the data blocks (for table-base lookup)
        TreeSet<Long> referenced = new TreeSet<>();
        AddressSet dataSet = new AddressSet();
        for (String bn : data.keySet()) { MemoryBlock b = mem.getBlock(bn); dataSet.add(b.getStart(), b.getEnd()); }
        AddressIterator ai = rm.getReferenceDestinationIterator(dataSet, true);
        while (ai.hasNext()) {
            Address a = ai.next();
            // only count references coming from code
            ReferenceIterator ri = rm.getReferencesTo(a);
            boolean code = false;
            while (ri.hasNext()) { if (fm.getFunctionContaining(ri.next().getFromAddress()) != null) { code = true; break; } }
            if (code) referenced.add(a.getOffset() - base);
        }
        println("code-referenced data addresses: " + referenced.size());

        int total = 0, withX = 0;
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outFile), "UTF-8")))) {
            w.println("rva\tenc\tfuncs\tvia\tstring");
            for (String bn : data.keySet()) {
                byte[] buf = data.get(bn);
                long s0 = start.get(bn);
                int i = 0;
                while (i < buf.length) {
                    // UTF-16LE candidate (aligned 2)
                    int j = i, n = 0;
                    if ((i & 1) == 0) {
                        while (j + 1 < buf.length && pr(buf[j] & 0xff) && buf[j + 1] == 0) { j += 2; n++; }
                    }
                    if (n >= 4 && j + 1 < buf.length && buf[j] == 0 && buf[j + 1] == 0) {
                        StringBuilder sb = new StringBuilder();
                        for (int k = i; k < j; k += 2) sb.append((char) (buf[k] & 0xff));
                        if (emit(w, s0 + i, "u16", sb.toString(), ptrs, referenced)) withX++;
                        total++;
                        i = j + 2;
                        continue;
                    }
                    j = i; n = 0;
                    while (j < buf.length && pr(buf[j] & 0xff)) { j++; n++; }
                    if (n >= 4 && j < buf.length && buf[j] == 0) {
                        String s = new String(buf, i, n, "ISO-8859-1");
                        if (emit(w, s0 + i, "ascii", s, ptrs, referenced)) withX++;
                        total++;
                        i = j + 1;
                        continue;
                    }
                    i = (n > 0) ? j : i + 1;
                }
            }
        }
        println("strings: " + total + " with xrefs: " + withX);
    }

    boolean emit(PrintWriter w, long r, String enc, String s, HashMap<Long, List<Long>> ptrs, TreeSet<Long> referenced) {
        String direct = funcsRefTo(r, 48);
        StringBuilder via = new StringBuilder();
        List<Long> slots = ptrs.get(r);
        if (slots != null) {
            int k = 0;
            for (long slot : slots) {
                if (k++ >= 6) break;
                String f = funcsRefTo(slot, 16);
                if (via.length() > 0) via.append(' ');
                via.append(hx(slot));
                if (f.isEmpty()) {
                    Long tb = referenced.floor(slot);
                    if (tb != null && slot - tb <= 0x4000) {
                        via.append(">").append(hx(tb)).append("=").append(funcsRefTo(tb, 16));
                    }
                } else via.append("=").append(f);
            }
        }
        boolean has = !direct.isEmpty() || via.indexOf("=") >= 0;
        w.println(hx(r) + "\t" + enc + "\t" + direct + "\t" + via + "\t" + clean(s));
        return has;
    }
}
