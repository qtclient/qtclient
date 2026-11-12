// List SSE scalar float/double instructions reading [reg+disp] inside an address range.
// Args: <lo> <hi> <disp>... (hex). Prints "FLOAD <insnAddr> <fnEntry> <fnName> <insn>".
//@category tibia
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.OperandType;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class FindFloatLoads extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Address lo = toAddr(Long.decode(args[0]));
        Address hi = toAddr(Long.decode(args[1]));
        Set<Long> wanted = new HashSet<>();
        for (int i = 2; i < args.length; i++) wanted.add(Long.decode(args[i]));
        InstructionIterator it = currentProgram.getListing().getInstructions(lo, true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Instruction insn = it.next();
            if (insn.getAddress().compareTo(hi) > 0) break;
            String mn = insn.getMnemonicString().toUpperCase();
            if (!(mn.endsWith("SD") || mn.endsWith("SS") || mn.startsWith("CVT") || mn.startsWith("MOVQ")))
                continue;
            for (int op = 0; op < insn.getNumOperands(); op++) {
                if ((insn.getOperandType(op) & OperandType.DYNAMIC) == 0) continue;
                for (Object o : insn.getOpObjects(op)) {
                    if (o instanceof Scalar && wanted.contains(((Scalar) o).getUnsignedValue())) {
                        Function f = getFunctionContaining(insn.getAddress());
                        println("FLOAD " + insn.getAddress() + " "
                                + (f == null ? "-" : f.getEntryPoint() + " " + f.getName()) + " " + insn);
                    }
                }
            }
        }
    }
}
