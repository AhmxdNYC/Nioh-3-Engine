// Pre-analysis: print available analyzer options and turn off the expensive ones.
import ghidra.app.script.GhidraScript;
import java.util.Map;

public class N3PreAnalysis extends GhidraScript {
    @Override
    public void run() throws Exception {
        Map<String, String> opts = getCurrentAnalysisOptionsAndValues(currentProgram);
        for (Map.Entry<String, String> e : opts.entrySet()) {
            println("OPT " + e.getKey() + " = " + e.getValue());
        }
        String[] off = {
            "Decompiler Parameter ID",
            "x86 Constant Reference Analyzer",
            "Aggressive Instruction Finder",
            "Embedded Media",
            "Condense Filler Bytes",
            "Scalar Operand References",
        };
        for (String k : off) {
            if (opts.containsKey(k)) { setAnalysisOption(currentProgram, k, "false"); println("DISABLED " + k); }
            else println("NOT FOUND " + k);
        }
    }
}
