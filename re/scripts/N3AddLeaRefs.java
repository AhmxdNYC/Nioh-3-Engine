// N3AddLeaRefs - cheap replacement for the (disabled, very slow) "x86 Constant Reference Analyzer".
// On x86-64 Ghidra only creates operand references for real memory accesses at disassembly time;
// LEA reg,[rip+X] (how MSVC loads string/table/global addresses) gets its reference from constant
// propagation. This walks every instruction and adds a DATA reference for
//   * LEA with a RIP-relative target inside the image
//   * 64-bit immediates (mov r64, imm64) that point inside the image (runtime dump => real addresses)
// Run once, writable (no -readOnly), -noanalysis.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;

public class N3AddLeaRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        long base = currentProgram.getImageBase().getOffset();
        long end = currentProgram.getMaxAddress().getOffset();
        ReferenceManager rm = currentProgram.getReferenceManager();
        AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        long n = 0, lea = 0, imm = 0;
        while (it.hasNext()) {
            Instruction ins = it.next();
            if ((++n & 0xfffff) == 0) println("instructions " + n + " lea " + lea + " imm " + imm);
            String m = ins.getMnemonicString();
            boolean isLea = m.equals("LEA");
            int nops = ins.getNumOperands();
            for (int op = 0; op < nops; op++) {
                if (ins.getOperandReferences(op).length > 0) continue;
                Object[] objs = ins.getOpObjects(op);
                boolean rip = false;
                for (Object o : objs) if (o instanceof ghidra.program.model.lang.Register && ((ghidra.program.model.lang.Register) o).getName().equals("RIP")) rip = true;
                if (rip) {
                    for (Object o : objs) if (o instanceof Scalar) {
                        long v = ins.getAddress().getOffset() + ins.getLength() + ((Scalar) o).getSignedValue();
                        if (v >= base + 0x1000 && v <= end) {
                            rm.addMemoryReference(ins.getAddress(), sp.getAddress(v), RefType.DATA, SourceType.ANALYSIS, op);
                            lea++;
                        }
                    }
                    continue;
                }
                for (Object o : objs) {
                    long v;
                    if (o instanceof Address) v = ((Address) o).getOffset();
                    else if (o instanceof Scalar) v = ((Scalar) o).getUnsignedValue();
                    else continue;
                    if (v < base + 0x1000 || v > end) continue;
                    if (!isLea && !(o instanceof Scalar && ((Scalar) o).bitLength() == 64)) continue;
                    Address to = sp.getAddress(v);
                    rm.addMemoryReference(ins.getAddress(), to, isLea ? RefType.DATA : RefType.DATA, SourceType.ANALYSIS, op);
                    if (isLea) lea++; else imm++;
                    break;
                }
            }
        }
        println("done: instructions " + n + " lea refs " + lea + " imm64 refs " + imm);
    }
}
