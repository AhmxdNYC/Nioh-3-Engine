// N3Export - dump functions / strings / imports tables from the analyzed Nioh3 dump.
// Usage (headless, -process Nioh3.dump.exe -readOnly -noanalysis):
//   -postScript N3Export.java <outDir>
// All addresses are written as RVAs (address - image base).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.StringDataInstance;

import java.io.*;
import java.util.*;

public class N3Export extends GhidraScript {
    long base;

    String rva(Address a) { return "0x" + Long.toHexString(a.getOffset() - base); }

    String clean(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '\t') b.append("\\t");
            else if (c == '\n') b.append("\\n");
            else if (c == '\r') b.append("\\r");
            else if (c < 0x20) b.append(String.format("\\x%02x", (int) c));
            else b.append(c);
        }
        return b.toString();
    }

    // Functions referencing address a (code refs -> containing function; data refs -> one hop through the pointer).
    String refFuncs(Address a, int max) {
        LinkedHashSet<String> out = new LinkedHashSet<>();
        ReferenceManager rm = currentProgram.getReferenceManager();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceIterator it = rm.getReferencesTo(a);
        int n = 0;
        while (it.hasNext() && out.size() < max) {
            Reference r = it.next();
            Address from = r.getFromAddress();
            Function f = fm.getFunctionContaining(from);
            if (f != null) out.add(rva(f.getEntryPoint()));
            else if (currentProgram.getListing().getInstructionAt(from) != null) out.add("?" + rva(from));
            else {
                // data pointer to the string: follow refs to the pointer
                ReferenceIterator it2 = rm.getReferencesTo(from);
                boolean any = false;
                while (it2.hasNext() && out.size() < max) {
                    Reference r2 = it2.next();
                    Function f2 = fm.getFunctionContaining(r2.getFromAddress());
                    if (f2 != null) { out.add("p" + rva(f2.getEntryPoint())); any = true; }
                }
                if (!any) out.add("D" + rva(from));
            }
            if (++n > 5000) break;
        }
        return String.join(",", out);
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args.length > 0 ? args[0] : "out");
        outDir.mkdirs();
        base = currentProgram.getImageBase().getOffset();
        FunctionManager fm = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();

        // ---------- functions ----------
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(new File(outDir, "functions.tsv")), "UTF-8")))) {
            w.println("rva\tsize\tname\tcallers");
            int n = 0;
            for (Function f : fm.getFunctions(true)) {
                if (monitor.isCancelled()) break;
                long size = f.getBody().getNumAddresses();
                int callers = 0;
                ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(f.getEntryPoint());
                while (ri.hasNext()) { Reference r = ri.next(); if (r.getReferenceType().isCall()) callers++; if (callers > 9999) break; }
                w.println(rva(f.getEntryPoint()) + "\t" + size + "\t" + f.getName() + "\t" + callers);
                n++;
            }
            println("functions: " + n);
        }

        // ---------- strings ----------
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(new File(outDir, "strings.tsv")), "UTF-8")))) {
            w.println("rva\ttype\tnrefs\treferencing_functions\tstring");
            int n = 0, withRefs = 0;
            DataIterator di = listing.getDefinedData(true);
            while (di.hasNext()) {
                if (monitor.isCancelled()) break;
                Data d = di.next();
                if (!d.hasStringValue()) continue;
                StringDataInstance sdi = StringDataInstance.getStringDataInstance(d);
                String s = sdi.getStringValue();
                if (s == null) continue;
                Address a = d.getAddress();
                int nrefs = currentProgram.getReferenceManager().getReferenceCountTo(a);
                String refs = nrefs > 0 ? refFuncs(a, 64) : "";
                if (nrefs > 0) withRefs++;
                String t = d.getDataType().getName();
                w.println(rva(a) + "\t" + t + "\t" + nrefs + "\t" + refs + "\t" + clean(s));
                n++;
            }
            println("strings: " + n + " (with refs " + withRefs + ")");
        }

        // ---------- imports ----------
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(new File(outDir, "imports.tsv")), "UTF-8")))) {
            w.println("library\tname\tiat_rva\tnrefs\tcalling_functions");
            SymbolTable st = currentProgram.getSymbolTable();
            ExternalManager em = currentProgram.getExternalManager();
            int n = 0;
            SymbolIterator syms = st.getExternalSymbols();
            while (syms.hasNext()) {
                Symbol s = syms.next();
                ExternalLocation el = em.getExternalLocation(s);
                String lib = el != null ? el.getLibraryName() : "?";
                // references to the external symbol come from IAT slots (data) or thunks
                Reference[] refs = s.getReferences();
                if (refs.length == 0) {
                    w.println(lib + "\t" + s.getName() + "\t-\t0\t");
                    n++;
                    continue;
                }
                for (Reference r : refs) {
                    Address iat = r.getFromAddress();
                    LinkedHashSet<String> callers = new LinkedHashSet<>();
                    int cnt = 0;
                    ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(iat);
                    while (it.hasNext()) {
                        Reference r2 = it.next();
                        cnt++;
                        Function f = fm.getFunctionContaining(r2.getFromAddress());
                        if (f != null) {
                            // if f is a thunk (jmp [iat]), list its callers too
                            if (f.isThunk() || f.getBody().getNumAddresses() <= 8) {
                                callers.add("T" + rva(f.getEntryPoint()));
                                for (Function c : f.getCallingFunctions(monitor)) callers.add(rva(c.getEntryPoint()));
                            } else callers.add(rva(f.getEntryPoint()));
                        } else callers.add("?" + rva(r2.getFromAddress()));
                    }
                    if (fm.getFunctionContaining(iat) != null) { // ref came from a thunk function itself
                        Function f = fm.getFunctionContaining(iat);
                        for (Function c : f.getCallingFunctions(monitor)) callers.add(rva(c.getEntryPoint()));
                    }
                    w.println(lib + "\t" + s.getName() + "\t" + rva(iat) + "\t" + cnt + "\t" + String.join(",", callers));
                    n++;
                }
            }
            println("imports: " + n);
        }
    }
}
