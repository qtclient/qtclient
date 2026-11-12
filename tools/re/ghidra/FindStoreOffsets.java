// Rank functions by how many distinct [reg+disp] store displacements they write.
// Args: <store|load> <min-distinct> <disp>... (hex). Prints "STORES <count> <fnEntry> <fnName> <disps>".
//@category tibia
import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.OperandType;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class FindStoreOffsets extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        boolean load = args[0].equals("load");
        int min = Integer.parseInt(args[1]);
        Set<Long> wanted = new HashSet<>();
        for (int i = 2; i < args.length; i++) wanted.add(Long.decode(args[i]));
        Map<Function, Set<Long>> hits = new HashMap<>();
        Listing listing = currentProgram.getListing();
        InstructionIterator it = listing.getInstructions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Instruction insn = it.next();
            String mn = insn.getMnemonicString();
            if (!mn.startsWith("MOV")) continue;
            if (insn.getNumOperands() < 2) continue;
            int op = load ? 1 : 0;
            int t = insn.getOperandType(op);
            if ((t & OperandType.DYNAMIC) == 0) continue;
            for (Object o : insn.getOpObjects(op)) {
                if (o instanceof Scalar) {
                    long d = ((Scalar) o).getUnsignedValue();
                    if (wanted.contains(d)) {
                        Function f = getFunctionContaining(insn.getAddress());
                        if (f != null) hits.computeIfAbsent(f, k -> new TreeSet<>()).add(d);
                    }
                }
            }
        }
        List<Map.Entry<Function, Set<Long>>> list = new ArrayList<>(hits.entrySet());
        list.sort((a, b) -> b.getValue().size() - a.getValue().size());
        for (Map.Entry<Function, Set<Long>> e : list) {
            if (e.getValue().size() < min) break;
            StringBuilder sb = new StringBuilder();
            for (Long d : e.getValue()) sb.append(" 0x").append(Long.toHexString(d));
            println("STORES " + e.getValue().size() + " " + e.getKey().getEntryPoint() + " " + e.getKey().getName() + sb);
        }
    }
}
