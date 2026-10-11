// N3ApplySymbols - apply symbols\symbols.tsv to a Ghidra program of Nioh3.exe v2.02 (names, labels, comments, /N3 structs).
//
// Args: <symbols.tsv> [<report.txt>] [dryrun]
//   GUI: Script Manager -> run it with no arguments and pick symbols.tsv in the file dialog, then save the program.
//   Headless: analyzeHeadless <proj> <name> -process Nioh3.exe -postScript N3ApplySymbols.java <symbols.tsv> (no -readOnly).
//   "dryrun" rolls the transaction back, so nothing is saved; the report still lists what would change.
//
// TSV columns: rva kind name signature description confidence source   (header line + '#' comment lines skipped)
//   rva is an RVA (hex). For kind=struct_field it is the FIELD OFFSET and name is Struct::Field.
//
// What it does per kind:
//   function      rename the function at the RVA (USER_DEFINED). If none exists and the address is clearly code
//                 (a primary .pdata entry, or an existing instruction no function covers), the function is created.
//                 If the RVA is inside another function, a label is placed instead and a warning is reported.
//                 Plate comment = N3sym block (name, kind, confidence, signature, description, source).
//                 A previous non-default name that was not ours is kept as a secondary label (nothing is lost).
//   label/global/data/vtable/string_table
//                 create (or rename our previous) label at the RVA, EOL comment = N3sym block. global/data rows whose
//                 signature is a simple scalar/pointer type get that data type, but only on undefined bytes.
//   struct_field  build/extend structure /N3/<Struct> in the program's data types; field typed from signature.
//
// Idempotent: each comment carries a "[N3sym] <name>" marker block; re-running replaces that block (keeping any other
// comment text) and renames the label it named before instead of adding a second one. Never deletes symbols, functions,
// data or comments that are not ours.
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.label.SetLabelPrimaryCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public class N3ApplySymbols extends GhidraScript {
    static final String BEGIN = "[N3sym] ";
    static final String END = "[/N3sym]";
    static final Pattern BLOCK = Pattern.compile("\\[N3sym\\] (\\S+)[\\s\\S]*?\\[/N3sym\\]");
    static final CategoryPath CAT = new CategoryPath("/N3");

    long base;
    PrintWriter rep;
    byte[] pdata;
    long pdataRva;
    Map<String, Integer> cnt = new TreeMap<>();
    List<String> warn = new ArrayList<>();
    List<String> fail = new ArrayList<>();

    void inc(String k) { cnt.merge(k, 1, Integer::sum); }
    Address addr(long r) { return currentProgram.getImageBase().add(r); }
    String hx(long r) { return "0x" + Long.toHexString(r); }
    String rva(Address a) { return hx(a.getOffset() - base); }
    static long parse(String s) { s = s.trim().toLowerCase(); if (s.startsWith("+")) s = s.substring(1); if (s.startsWith("0x")) s = s.substring(2); return Long.parseUnsignedLong(s, 16); }
    int u32(byte[] b, int o) { return (b[o] & 0xff) | (b[o+1] & 0xff) << 8 | (b[o+2] & 0xff) << 16 | (b[o+3] & 0xff) << 24; }

    // ---------------- .pdata helpers (same logic as N3Query) ----------------
    void loadPdata() throws Exception {
        Memory mem = currentProgram.getMemory();
        byte[] h = new byte[0x400];
        mem.getBytes(currentProgram.getImageBase(), h);
        int pe = u32(h, 0x3c);
        int dir3 = pe + 24 + 112 + 8 * 3;
        pdataRva = u32(h, dir3) & 0xffffffffL;
        int size = u32(h, dir3 + 4);
        pdata = new byte[size];
        mem.getBytes(addr(pdataRva), pdata);
    }
    int pdataIndex(long r) throws Exception {
        if (pdata == null) loadPdata();
        int lo = 0, hi = pdata.length / 12 - 1;
        while (lo <= hi) {
            int mid = (lo + hi) >>> 1;
            long b = u32(pdata, mid * 12) & 0xffffffffL, e = u32(pdata, mid * 12 + 4) & 0xffffffffL;
            if (r < b) hi = mid - 1; else if (r >= e) lo = mid + 1; else return mid;
        }
        return -1;
    }
    // true when r is the begin of a .pdata entry that is NOT a chained fragment (i.e. a real function entry)
    boolean isPrimaryPdataEntry(long r) throws Exception {
        int i = pdataIndex(r);
        if (i < 0 || (u32(pdata, i * 12) & 0xffffffffL) != r) return false;
        long unw = u32(pdata, i * 12 + 8) & 0xffffffffL;
        if ((unw & 1) != 0) return false;
        Memory mem = currentProgram.getMemory();
        int flags = (mem.getByte(addr(unw)) & 0xff) >> 3;
        return (flags & 4) == 0; // UNW_FLAG_CHAININFO -> fragment of a parent function
    }

    // ---------------- comment block handling ----------------
    String block(String[] c, String kind) {
        StringBuilder sb = new StringBuilder();
        sb.append(BEGIN).append(c[2]).append("  (").append(kind).append(", ").append(c[5].isEmpty() ? "?" : c[5]).append(" confidence)\n");
        if (!c[3].isEmpty()) sb.append("sig: ").append(c[3]).append('\n');
        if (!c[4].isEmpty()) sb.append(c[4]).append('\n');
        if (!c[6].isEmpty()) sb.append("src: ").append(c[6]).append('\n');
        sb.append(END);
        return sb.toString();
    }
    // returns the name recorded in our previous block at this address/comment type, or null
    String oldName(Address a, CommentType t) {
        String s = currentProgram.getListing().getComment(t, a);
        if (s == null) return null;
        Matcher m = BLOCK.matcher(s);
        return m.find() ? m.group(1) : null;
    }
    void putComment(Address a, CommentType t, String blk) {
        Listing l = currentProgram.getListing();
        String s = l.getComment(t, a);
        String n;
        if (s == null || s.isEmpty()) n = blk;
        else {
            Matcher m = BLOCK.matcher(s);
            if (m.find()) n = s.substring(0, m.start()) + blk + s.substring(m.end());
            else n = s + "\n" + blk;
        }
        if (n.equals(s)) { inc("comment.unchanged"); return; }
        l.setComment(a, t, n);
        inc("comment.set");
    }

    // ---------------- types ----------------
    DataType simpleType(String sig) {
        if (sig == null) return null;
        String s = sig.trim().toLowerCase();
        if (s.startsWith("const ")) s = s.substring(6).trim();
        if (s.isEmpty()) return null;
        int arr = 0;
        Matcher am = Pattern.compile("^([a-z0-9_ ]+\\**)\\s*\\[(\\d+|0x[0-9a-f]+)\\](?!\\s*\\[)").matcher(s);
        if (am.find()) { arr = am.group(2).startsWith("0x") ? Integer.parseInt(am.group(2).substring(2), 16) : Integer.parseInt(am.group(2)); s = am.group(1).trim(); }
        else {
            // first token only ("u32 (fps)", "float /* scale */")
            Matcher tm = Pattern.compile("^([a-z_][a-z0-9_ ]*?\\**)(\\s|$|[(/,;])").matcher(s);
            if (!tm.find()) return null;
            s = tm.group(1).trim();
        }
        DataType dt;
        if (s.endsWith("*") || s.equals("ptr") || s.equals("pointer")) dt = PointerDataType.dataType;
        else switch (s) {
            case "u8": case "uint8": case "uint8_t": case "byte": case "uchar": case "unsigned char": dt = ByteDataType.dataType; break;
            case "bool": dt = BooleanDataType.dataType; break;
            case "i8": case "int8": case "int8_t": case "char": case "sbyte": dt = CharDataType.dataType; break;
            case "u16": case "uint16": case "uint16_t": case "ushort": case "word": dt = WordDataType.dataType; break;
            case "i16": case "int16": case "int16_t": case "short": dt = ShortDataType.dataType; break;
            case "u32": case "uint32": case "uint32_t": case "uint": case "unsigned": case "unsigned int": case "dword": dt = UnsignedIntegerDataType.dataType; break;
            case "i32": case "int32": case "int32_t": case "int": dt = IntegerDataType.dataType; break;
            case "u64": case "uint64": case "uint64_t": case "qword": case "size_t": case "uintptr_t": dt = QWordDataType.dataType; break;
            case "i64": case "int64": case "int64_t": case "longlong": case "long long": dt = LongLongDataType.dataType; break;
            case "f32": case "float": dt = FloatDataType.dataType; break;
            case "f64": case "double": dt = DoubleDataType.dataType; break;
            case "wchar_t": dt = WideChar16DataType.dataType; break;
            case "handle": case "hwnd": case "hinstance": case "hmodule": case "hcursor": case "lpcstr": case "lpcwstr": case "fnptr": dt = PointerDataType.dataType; break;
            default: return null;
        }
        if (arr > 0 && arr <= 0x10000) dt = new ArrayDataType(dt, arr, dt.getLength());
        return dt;
    }

    // ---------------- labels ----------------
    // ensure a USER_DEFINED label `name` exists at a; reuse/rename the label our previous run created (prev)
    void ensureLabel(Address a, String name, String prev) throws Exception {
        SymbolTable st = currentProgram.getSymbolTable();
        for (Symbol s : st.getSymbols(a)) {
            if (s.getName().equals(name)) {
                if (!s.isPrimary() && s.getSymbolType() == SymbolType.LABEL && primaryIsDefaultOrOurs(a, prev)) new SetLabelPrimaryCmd(a, name, s.getParentNamespace()).applyTo(currentProgram);
                inc("label.unchanged");
                return;
            }
        }
        if (prev != null && !prev.equals(name)) {
            for (Symbol s : st.getSymbols(a)) {
                if (s.getName().equals(prev) && s.getSymbolType() == SymbolType.LABEL) {
                    s.setName(name, SourceType.USER_DEFINED);
                    inc("label.renamed");
                    return;
                }
            }
        }
        Symbol ns = st.createLabel(a, name, SourceType.USER_DEFINED);
        inc("label.created");
        if (!ns.isPrimary() && primaryIsDefaultOrOurs(a, prev)) new SetLabelPrimaryCmd(a, name, ns.getParentNamespace()).applyTo(currentProgram);
    }
    boolean primaryIsDefaultOrOurs(Address a, String prev) {
        Symbol p = currentProgram.getSymbolTable().getPrimarySymbol(a);
        if (p == null) return true;
        if (p.getSymbolType() == SymbolType.FUNCTION) return false; // never demote a function name
        return p.getSource() == SourceType.DEFAULT || p.getSource() == SourceType.ANALYSIS || (prev != null && p.getName().equals(prev));
    }

    // ---------------- per-kind ----------------
    void doFunction(String[] c, long r) throws Exception {
        Address a = addr(r);
        FunctionManager fm = currentProgram.getFunctionManager();
        Listing l = currentProgram.getListing();
        Function f = fm.getFunctionAt(a);
        if (f == null) {
            Function cont = fm.getFunctionContaining(a);
            if (cont == null) {
                MemoryBlock mb = currentProgram.getMemory().getBlock(a);
                boolean exec = mb != null && mb.isExecute();
                boolean pdataEntry = isPrimaryPdataEntry(r);
                boolean insn = l.getInstructionAt(a) != null;
                boolean called = false; // a CALL/JMP reference to the address also proves it is code
                for (Reference x : currentProgram.getReferenceManager().getReferencesTo(a)) {
                    if (x.getReferenceType().isCall() || x.getReferenceType().isJump()) { called = true; break; }
                }
                if (exec && (pdataEntry || insn || called)) {
                    if (!insn) new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
                    new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
                    f = fm.getFunctionAt(a);
                    if (f != null) inc("function.created");
                }
                if (f == null) {
                    warn.add(c[2] + " @" + hx(r) + ": function row but no function could be created (exec=" + exec + " pdataEntry=" + pdataEntry + " insn=" + insn + " called=" + called + "); placed a label");
                }
            } else {
                warn.add(c[2] + " @" + hx(r) + ": function row is inside " + cont.getName() + "@" + rva(cont.getEntryPoint()) + "; placed a label");
            }
            if (f == null) { inc("function.asLabel"); doLabel(c, r, "function?"); return; }
        }
        String prev = oldName(a, CommentType.PLATE);
        String cur = f.getName();
        if (!cur.equals(c[2])) {
            boolean keepOld = f.getSymbol().getSource() != SourceType.DEFAULT && !cur.equals(prev) && !cur.startsWith("FUN_");
            f.setName(c[2], SourceType.USER_DEFINED);
            inc("function.renamed");
            if (keepOld) {
                try { currentProgram.getSymbolTable().createLabel(a, cur, SourceType.IMPORTED); inc("function.oldNameKeptAsLabel"); }
                catch (Exception e) { warn.add(c[2] + ": could not keep previous name " + cur + ": " + e.getMessage()); }
            }
        } else inc("function.unchanged");
        putComment(a, CommentType.PLATE, block(c, "function"));
    }

    void doLabel(String[] c, long r, String kind) throws Exception {
        Address a = addr(r);
        if (!currentProgram.getMemory().contains(a)) { fail.add(c[2] + " @" + hx(r) + ": address not in program memory"); return; }
        String prev = oldName(a, CommentType.EOL);
        ensureLabel(a, c[2], prev);
        if ((kind.equals("global") || kind.equals("data"))) applyGlobalType(c, a);
        putComment(a, CommentType.EOL, block(c, kind));
    }

    void applyGlobalType(String[] c, Address a) {
        DataType dt = simpleType(c[3]);
        if (dt == null) return;
        Listing l = currentProgram.getListing();
        Data d = l.getDataAt(a);
        if (d != null && d.isDefined()) { if (!d.getDataType().isEquivalent(dt)) inc("global.typeKept(existing)"); return; }
        if (l.getInstructionContaining(a) != null) return;
        Address endA = a.add(dt.getLength() - 1);
        Data dc = l.getDefinedDataContaining(a);
        if (dc != null) return;
        // only over undefined bytes: no defined data or instructions anywhere in the range
        if (l.getInstructions(new AddressSet(a, endA), true).hasNext()) return;
        if (l.getDefinedData(new AddressSet(a, endA), true).hasNext()) return;
        try { l.createData(a, dt); inc("global.typed"); }
        catch (Exception e) { warn.add(c[2] + ": could not type as " + dt.getName() + ": " + e.getMessage()); }
    }

    Map<String, Structure> structs = new HashMap<>();
    void doStructField(String[] c) throws Exception {
        String nm = c[2];
        int k = nm.indexOf("::");
        if (k <= 0) { fail.add(nm + ": struct_field name must be Struct::Field"); return; }
        String sn = nm.substring(0, k).replaceAll("[^A-Za-z0-9_]", "_");
        String fn = nm.substring(k + 2).replaceAll("[^A-Za-z0-9_]", "_");
        int off = (int) parse(c[0]);
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        Structure s = structs.get(sn);
        if (s == null) {
            DataType ex = dtm.getDataType(CAT, sn);
            if (ex instanceof Structure) s = (Structure) ex;
            else {
                if (ex != null) { fail.add(nm + ": /N3/" + sn + " exists and is not a structure"); return; }
                s = (Structure) dtm.addDataType(new StructureDataType(CAT, sn, 0, dtm), DataTypeConflictHandler.KEEP_HANDLER);
                inc("struct.created");
            }
            structs.put(sn, s);
        }
        DataType dt = simpleType(c[3]);
        if (dt == null) dt = Undefined1DataType.dataType;
        int len = dt.getLength();
        int curLen = s.isZeroLength() ? 0 : s.getLength(); // an empty struct reports length 1
        if (curLen < off + len) s.growStructure(off + len - curLen);
        String cm = (c[4] + " [" + c[5] + "; " + c[6] + "]").trim();
        DataTypeComponent at = s.getComponentContaining(off);
        boolean defined = at != null && !(at.getDataType() instanceof DefaultDataType) && at.getDataType() != Undefined1DataType.dataType;
        if (at != null && at.getOffset() == off && fn.equals(at.getFieldName()) && (at.getDataType().isEquivalent(dt) || dt == Undefined1DataType.dataType)) {
            if (!cm.equals(at.getComment())) { at.setComment(cm); inc("struct.field.comment"); } else inc("struct.field.unchanged");
            return;
        }
        if (defined && at.getOffset() == off && dt == Undefined1DataType.dataType) {
            at.setFieldName(fn); at.setComment(cm); inc("struct.field.renamed"); return;
        }
        if (defined && at.getOffset() != off) { warn.add(nm + " @+" + hx(off) + ": overlaps existing field " + at.getFieldName() + " @+" + hx(at.getOffset()) + "; skipped"); return; }
        try { s.replaceAtOffset(off, dt, len, fn, cm); inc("struct.field.set"); }
        catch (Exception e) { warn.add(nm + " @+" + hx(off) + ": " + e.getMessage()); }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            if (isRunningHeadless()) { println("usage: N3ApplySymbols <symbols.tsv> [report.txt] [dryrun]"); return; }
            args = new String[] { askFile("Select symbols.tsv", "Apply").getAbsolutePath() };
        }
        boolean dry = false;
        String repPath = null;
        for (int i = 1; i < args.length; i++) { if (args[i].equalsIgnoreCase("dryrun")) dry = true; else repPath = args[i]; }
        base = currentProgram.getImageBase().getOffset();
        rep = repPath == null ? new PrintWriter(new OutputStreamWriter(System.out, StandardCharsets.UTF_8))
                              : new PrintWriter(new OutputStreamWriter(new FileOutputStream(repPath), StandardCharsets.UTF_8));
        List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
        int tx = currentProgram.startTransaction("N3ApplySymbols");
        boolean ok = false;
        int rows = 0;
        try {
            for (String line : lines) {
                if (line.isEmpty() || line.startsWith("#") || line.startsWith("rva\t")) continue;
                String[] c = Arrays.copyOf(line.split("\t", -1), 7);
                for (int i = 0; i < 7; i++) if (c[i] == null) c[i] = ""; else c[i] = c[i].trim();
                rows++;
                String kind = c[1].toLowerCase();
                try {
                    if (kind.equals("struct_field")) { doStructField(c); continue; }
                    long r = parse(c[0]);
                    switch (kind) {
                        case "function": doFunction(c, r); break;
                        case "label": case "global": case "data": case "vtable": case "string_table": doLabel(c, r, kind); break;
                        default: fail.add(c[2] + ": unknown kind " + c[1]);
                    }
                } catch (Exception e) {
                    fail.add(c[2] + " @" + c[0] + ": " + e);
                }
            }
            ok = true;
        } finally {
            currentProgram.endTransaction(tx, ok && !dry);
        }
        rep.println("N3ApplySymbols: " + rows + " rows from " + args[0] + (dry ? "  (DRY RUN - rolled back)" : "  (committed)"));
        for (Map.Entry<String, Integer> e : cnt.entrySet()) rep.println("  " + e.getKey() + " = " + e.getValue());
        rep.println("warnings: " + warn.size());
        for (String w : warn) rep.println("  W " + w);
        rep.println("failures: " + fail.size());
        for (String f : fail) rep.println("  F " + f);
        rep.flush();
        if (repPath != null) rep.close();
    }
}
