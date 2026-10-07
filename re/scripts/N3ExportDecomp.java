// N3ExportDecomp - bulk-export decompiled C for a list of functions, so many agents can grep it in parallel
// instead of queueing on the Ghidra project lock. Run read-only against the D: copy (see D:\nioh3-re\README.md).
//
// Args (key=value, any order):
//   targets=<file>   one RVA per line (hex, 0x optional); anything after the first token and lines starting with # are ignored.
//                    Order matters: functions are exported in file order (put the hottest first).
//   out=<dir>        writes <dir>\<rva>.c per function (rva = function ENTRY, e.g. 0x5af410.c) and appends <dir>\index.tsv
//   timeout=30       decompiler timeout per function, seconds (a timeout/failure is recorded and the run continues)
//   threads=1        decompiler processes in parallel (each is a separate decompile.exe; keep this small)
//   max=0            stop after this many NEW functions (0 = all)
//   minutes=0        stop after this many minutes (0 = no limit); the run is resumable either way
//   strings=<file>   optional re\out\strings.tsv: fallback for string refs that Ghidra has not defined as string data
//   retry=0          1 = re-export functions whose index row says timeout/fail
//   force=0          1 = ignore the existing index and re-export everything (index.tsv is rewritten)
// Resume: rows already in index.tsv are skipped, so re-running the same command continues where it stopped.
// A target that is not a function entry is resolved to its containing function (.pdata first, like N3Query);
// several targets in one function are exported once.
//
// Graph mode (used to build the target list):
//   graph=<seedFile> depth=2 graphout=<file> [hub=100]
//   BFS over the call graph from the seeds (both directions) to <depth>. Writes one row per reached function:
//   rva  depth  n_callers  n_callees  callers(csv rvas)  callees(csv rvas)
//   Nodes with more than <hub> callers are not expanded towards their callers (they are generic helpers).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.StringDataInstance;
import ghidra.util.task.TaskMonitor;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class N3ExportDecomp extends GhidraScript {
    long base;
    byte[] pdata;
    Map<Long, String> fallbackStrings = new HashMap<>();
    Map<String, String> opt = new HashMap<>();

    String rva(Address a) { return "0x" + Long.toHexString(a.getOffset() - base); }
    Address addr(long r) { return currentProgram.getImageBase().add(r); }
    static long parse(String s) { s = s.trim().toLowerCase(); if (s.startsWith("0x")) s = s.substring(2); return Long.parseUnsignedLong(s, 16); }
    int u32(byte[] b, int o) { return (b[o] & 0xff) | (b[o+1] & 0xff) << 8 | (b[o+2] & 0xff) << 16 | (b[o+3] & 0xff) << 24; }
    String o(String k, String d) { return opt.getOrDefault(k, d); }
    int oi(String k, int d) { return Integer.parseInt(o(k, Integer.toString(d))); }

    void loadPdata() throws Exception {
        Memory mem = currentProgram.getMemory();
        byte[] h = new byte[0x400];
        mem.getBytes(currentProgram.getImageBase(), h);
        int pe = u32(h, 0x3c);
        int dir3 = pe + 24 + 112 + 8 * 3;
        long pr = u32(h, dir3) & 0xffffffffL;
        pdata = new byte[u32(h, dir3 + 4)];
        mem.getBytes(addr(pr), pdata);
    }

    long pdataStart(long r) throws Exception {
        if (pdata == null) loadPdata();
        int lo = 0, hi = pdata.length / 12 - 1, hit = -1;
        while (lo <= hi) {
            int mid = (lo + hi) >>> 1;
            long b = u32(pdata, mid * 12) & 0xffffffffL, e = u32(pdata, mid * 12 + 4) & 0xffffffffL;
            if (r < b) hi = mid - 1; else if (r >= e) lo = mid + 1; else { hit = mid; break; }
        }
        if (hit < 0) return -1;
        long begin = u32(pdata, hit * 12) & 0xffffffffL, unw = u32(pdata, hit * 12 + 8) & 0xffffffffL;
        Memory mem = currentProgram.getMemory();
        for (int guard = 0; guard < 32; guard++) {
            if ((unw & 1) != 0) { long rf = unw & ~1L; begin = mem.getInt(addr(rf)) & 0xffffffffL; unw = mem.getInt(addr(rf + 8)) & 0xffffffffL; continue; }
            int flags = (mem.getByte(addr(unw)) & 0xff) >> 3;
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
        if (ps >= 0) { Function f = fm.getFunctionAt(addr(ps)); if (f != null) return f; }
        Function f = fm.getFunctionContaining(addr(r));
        if (f != null) return f;
        if (ps >= 0) { new CreateFunctionCmd(addr(ps)).applyTo(currentProgram, monitor); return fm.getFunctionAt(addr(ps)); }
        // leaf functions have no .pdata entry and some were never discovered by analysis; the profiler reports their
        // 16-byte-aligned entry ("Nioh3+XXXX(leaf?)"), so create one there (in memory only; -readOnly + rolled back).
        if ((r & 0xf) == 0) {
            MemoryBlock mb = currentProgram.getMemory().getBlock(addr(r));
            if (mb != null && mb.isExecute()) {
                if (currentProgram.getListing().getInstructionAt(addr(r)) == null)
                    new ghidra.app.cmd.disassemble.DisassembleCommand(addr(r), null, true).applyTo(currentProgram, monitor);
                new CreateFunctionCmd(addr(r)).applyTo(currentProgram, monitor);
                return fm.getFunctionAt(addr(r));
            }
        }
        return null;
    }

    static String clean(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '\t') b.append("\\t"); else if (c == '\n') b.append("\\n"); else if (c == '\r') b.append("\\r");
            else if (c < 0x20) b.append(String.format("\\x%02x", (int) c)); else b.append(c);
        }
        return b.toString();
    }

    String stringAt(Address a) {
        Data d = currentProgram.getListing().getDataAt(a);
        if (d != null && d.hasStringValue()) {
            String s = StringDataInstance.getStringDataInstance(d).getStringValue();
            if (s != null) return s;
        }
        return fallbackStrings.get(a.getOffset() - base);
    }

    // Strings referenced from the function body (directly, or through one pointer slot).
    List<String> stringRefs(Function f, int max) {
        LinkedHashSet<String> out = new LinkedHashSet<>();
        Listing l = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();
        InstructionIterator it = l.getInstructions(f.getBody(), true);
        while (it.hasNext() && out.size() < max) {
            Instruction ins = it.next();
            for (Reference rf : ins.getReferencesFrom()) {
                Address to = rf.getToAddress();
                if (!to.isMemoryAddress()) continue;
                String s = stringAt(to);
                if (s == null) {
                    try { long p = mem.getLong(to); if (p > base && p < base + 0x10000000L) s = stringAt(addr(p - base)); } catch (Exception e) { }
                }
                if (s != null && s.length() >= 3) out.add(clean(s.length() > 120 ? s.substring(0, 120) + "..." : s));
            }
        }
        return new ArrayList<>(out);
    }

    // ------------------------------------------------------------------ graph mode
    void graph() throws Exception {
        int depth = oi("depth", 2), hub = oi("hub", 100);
        Map<Long, Integer> seen = new LinkedHashMap<>();
        Map<Long, Function> fn = new HashMap<>();
        ArrayDeque<Long> q = new ArrayDeque<>();
        for (String line : Files.readAllLines(Paths.get(o("graph", null)), StandardCharsets.UTF_8)) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            Function f = funcFor(parse(line.split("\\s+")[0]));
            if (f == null) continue;
            long e = f.getEntryPoint().getOffset() - base;
            if (seen.containsKey(e)) continue;
            seen.put(e, 0); fn.put(e, f); q.add(e);
        }
        try (PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(o("graphout", null)), StandardCharsets.UTF_8))) {
            w.println("rva\tdepth\tn_callers\tn_callees\tcallers\tcallees");
            while (!q.isEmpty()) {
                long e = q.poll();
                int d = seen.get(e);
                Function f = fn.get(e);
                Set<Function> cr = f.getCallingFunctions(TaskMonitor.DUMMY), ce = f.getCalledFunctions(TaskMonitor.DUMMY);
                StringBuilder a = new StringBuilder(), b = new StringBuilder();
                int ki = 0;
                for (Function c : cr) { if (ki++ < 300) a.append(rva(c.getEntryPoint())).append(','); }
                int ko = 0;
                for (Function c : ce) { if (c.isExternal() || c.isThunk() && c.getThunkedFunction(true) != null && c.getThunkedFunction(true).isExternal()) continue; if (ko++ < 300) b.append(rva(c.getEntryPoint())).append(','); }
                w.println("0x" + Long.toHexString(e) + "\t" + d + "\t" + cr.size() + "\t" + ce.size() + "\t" + a + "\t" + b);
                if (d >= depth) continue;
                List<Function> next = new ArrayList<>();
                if (cr.size() <= hub) next.addAll(cr);
                next.addAll(ce);
                for (Function c : next) {
                    if (c.isExternal()) continue;
                    long ce2 = c.getEntryPoint().getOffset() - base;
                    if (seen.containsKey(ce2)) continue;
                    seen.put(ce2, d + 1); fn.put(ce2, c); q.add(ce2);
                }
            }
        }
        println("graph: " + seen.size() + " functions");
    }

    // ------------------------------------------------------------------ export mode
    static final ThreadLocal<DecompInterface> TL = new ThreadLocal<>();
    final List<DecompInterface> allDi = Collections.synchronizedList(new ArrayList<>());

    DecompInterface di() {
        DecompInterface d = TL.get();
        if (d == null) {
            d = new DecompInterface();
            d.setOptions(new DecompileOptions());
            d.toggleCCode(true);
            d.toggleSyntaxTree(false);
            d.setSimplificationStyle("decompile");
            d.openProgram(currentProgram);
            TL.set(d);
            allDi.add(d);
        }
        return d;
    }

    @Override
    public void run() throws Exception {
        // analyzeHeadless.bat (cmd) splits "key=value" into two args, so accept both "key=value" and "key value"
        String[] av = getScriptArgs();
        for (int j = 0; j < av.length; j++) {
            String a = av[j];
            int i = a.indexOf('=');
            if (i > 0) opt.put(a.substring(0, i).toLowerCase(), a.substring(i + 1));
            else if (j + 1 < av.length) opt.put(a.toLowerCase(), av[++j]);
        }
        base = currentProgram.getImageBase().getOffset();
        int tx = currentProgram.startTransaction("export");   // in-memory function creation only; rolled back (and -readOnly anyway)
        try {
            if (opt.containsKey("graph")) { graph(); return; }
            export();
        } finally {
            currentProgram.endTransaction(tx, false);
            for (DecompInterface d : allDi) try { d.dispose(); } catch (Exception e) { }
        }
    }

    void export() throws Exception {
        File outDir = new File(o("out", "D:\\nioh3-re\\decomp"));
        outDir.mkdirs();
        int timeout = oi("timeout", 30), threads = Math.max(1, oi("threads", 1)), max = oi("max", 0), minutes = oi("minutes", 0);
        boolean retry = o("retry", "0").equals("1"), force = o("force", "0").equals("1");
        if (opt.containsKey("strings")) {
            for (String line : Files.readAllLines(Paths.get(o("strings", null)), StandardCharsets.UTF_8)) {
                String[] p = line.split("\t", 5);
                if (p.length < 5 || !p[0].startsWith("0x")) continue;
                try { fallbackStrings.put(parse(p[0]), p[4]); } catch (Exception e) { }
            }
        }
        File idx = new File(outDir, "index.tsv");
        Set<String> done = new HashSet<>();
        if (idx.exists() && !force) {
            for (String line : Files.readAllLines(idx.toPath(), StandardCharsets.UTF_8)) {
                String[] p = line.split("\t");
                if (p.length < 7 || p[0].equals("rva")) continue;
                if (retry && !p[6].equals("ok")) continue;
                done.add(p[0]);
            }
        }
        boolean header = force || !idx.exists() || idx.length() == 0;
        PrintWriter iw = new PrintWriter(new OutputStreamWriter(new FileOutputStream(idx, !force), StandardCharsets.UTF_8));
        if (header) iw.println("rva\tname\tsize\tn_callers\tn_callees\tlines\tstatus\tstrings");
        iw.flush();
        PrintWriter log = new PrintWriter(new OutputStreamWriter(new FileOutputStream(new File(outDir, "export_log.txt"), true), StandardCharsets.UTF_8), true);

        // resolve targets -> unique function entries, in file order
        LinkedHashMap<String, Function> todo = new LinkedHashMap<>();
        int nTargets = 0, unresolved = 0;
        for (String line : Files.readAllLines(Paths.get(o("targets", null)), StandardCharsets.UTF_8)) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            nTargets++;
            Function f = null;
            try { f = funcFor(parse(line.split("\\s+")[0])); } catch (Exception e) { }
            if (f == null) { unresolved++; log.println("unresolved target " + line); continue; }
            String r = rva(f.getEntryPoint());
            if (done.contains(r) || todo.containsKey(r)) continue;
            todo.put(r, f);
            if (max > 0 && todo.size() >= max) break;
        }
        long t0 = System.currentTimeMillis();
        log.println("=== " + new Date() + " targets=" + nTargets + " unresolved=" + unresolved + " alreadyDone=" + done.size()
                + " toExport=" + todo.size() + " threads=" + threads + " timeout=" + timeout + "s");
        println("export: " + todo.size() + " functions to export (" + done.size() + " already done)");

        ExecutorService pool = Executors.newFixedThreadPool(threads);
        Semaphore inflight = new Semaphore(threads * 4);
        AtomicInteger nDone = new AtomicInteger(), nFail = new AtomicInteger();
        final int total = todo.size();
        final String stamp = new java.text.SimpleDateFormat("yyyy-MM-dd HH:mm").format(new Date());
        String pj = "?";
        try { pj = getState().getProject().getProjectLocator().getProjectDir().getPath(); } catch (Exception e) { }
        final String proj = pj;
        for (Map.Entry<String, Function> en : todo.entrySet()) {
            if (monitor.isCancelled()) break;
            if (minutes > 0 && System.currentTimeMillis() - t0 > minutes * 60000L) { log.println("time budget reached, stopping (resumable)"); break; }
            final String r = en.getKey();
            final Function f = en.getValue();
            // header (single-threaded DB reads)
            Set<Function> cr = f.getCallingFunctions(TaskMonitor.DUMMY), ce = f.getCalledFunctions(TaskMonitor.DUMMY);
            StringBuilder h = new StringBuilder();
            h.append("// rva: ").append(r).append("   name: ").append(f.getName()).append("   size: ").append(f.getBody().getNumAddresses())
             .append(" bytes   entry: ").append(f.getEntryPoint()).append('\n');
            h.append("// signature: ").append(f.getSignature().getPrototypeString()).append('\n');
            StringBuilder s = new StringBuilder();
            int k = 0;
            for (Function c : cr) { if (k++ < 60) s.append(c.getName()).append('@').append(rva(c.getEntryPoint())).append(' '); }
            h.append("// callers(").append(cr.size()).append("): ").append(s).append(cr.size() > 60 ? "..." : "").append('\n');
            s.setLength(0); k = 0;
            for (Function c : ce) { if (k++ < 80) s.append(c.getName()).append('@').append(c.isExternal() ? "EXT" : rva(c.getEntryPoint())).append(' '); }
            h.append("// callees(").append(ce.size()).append("): ").append(s).append(ce.size() > 80 ? "..." : "").append('\n');
            List<String> strs = stringRefs(f, 12);
            for (String st : strs) h.append("// string: \"").append(st).append("\"\n");
            h.append("// exported ").append(stamp).append(" from ").append(proj).append(" (Ghidra names as of that copy; see re\\symbols\\symbols.tsv for newer names)\n\n");
            final String hdr = h.toString();
            final int ncr = cr.size(), nce = ce.size();
            final String strCol = String.join(" | ", strs.subList(0, Math.min(5, strs.size())));
            final long size = f.getBody().getNumAddresses();
            inflight.acquire();
            pool.submit(() -> { try { exportOne(outDir, iw, log, f, r, hdr, ncr, nce, strCol, size, timeout, nDone, nFail, total, t0); } finally { inflight.release(); } });
        }
        pool.shutdown();
        pool.awaitTermination(7, TimeUnit.DAYS);
        double el = (System.currentTimeMillis() - t0) / 1000.0;
        log.println(String.format("=== finished %s  exported=%d  failed=%d  elapsed=%.0fs", new Date(), nDone.get(), nFail.get(), el));
        println("export done: " + nDone.get() + " (" + nFail.get() + " failed) in " + el + " s");
        iw.close();
        log.close();
    }

    void exportOne(File outDir, PrintWriter iw, PrintWriter log, Function f, String r, String hdr, int ncr, int nce, String strCol, long size,
                   int timeout, AtomicInteger nDone, AtomicInteger nFail, int total, long t0) {
                String status = "ok", body;
                try {
                    DecompileResults res = di().decompileFunction(f, timeout, TaskMonitor.DUMMY);
                    if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) body = res.getDecompiledFunction().getC();
                    else {
                        String err = res == null ? "null result" : String.valueOf(res.getErrorMessage()).trim();
                        status = (res != null && res.isTimedOut()) || err.toLowerCase().contains("timeout") ? "timeout" : "fail";
                        body = "// DECOMPILE " + status.toUpperCase() + ": " + err.replace("\n", " ") + "\n// use: D:\\nioh3-re\\decomp_ro.cmd asm:" + r + ":200\n";
                    }
                } catch (Throwable t) { status = "fail"; body = "// DECOMPILE FAIL: " + t + "\n"; }
                try {
                    Files.write(new File(outDir, r + ".c").toPath(), (hdr + body).getBytes(StandardCharsets.UTF_8));
                } catch (IOException e) { status = "fail"; }
                int lines = body.split("\n", -1).length;
                synchronized (iw) {
                    iw.println(r + "\t" + f.getName() + "\t" + size + "\t" + ncr + "\t" + nce + "\t" + lines + "\t" + status + "\t" + strCol.replace("\t", " "));
                    iw.flush();
                }
                if (!status.equals("ok")) { nFail.incrementAndGet(); log.println(status + " " + r + " " + f.getName() + " size=" + size); }
                int n = nDone.incrementAndGet();
                if (n % 50 == 0 || n == total) {
                    double el = (System.currentTimeMillis() - t0) / 1000.0;
                    log.println(String.format("progress %d/%d  fail=%d  elapsed=%.0fs  rate=%.2f/s  eta=%.0fs", n, total, nFail.get(), el, n / el, (total - n) / (n / el)));
                }
    }
}
