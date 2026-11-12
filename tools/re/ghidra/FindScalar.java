// Print every instruction with one of the given immediate values as
// "SCALAR <value> <instruction-addr> <function-entry> <function-name>".
// Args: <value>... (hex with 0x, or decimal)
//@category ReverseEngineering

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;

import java.util.HashSet;
import java.util.Set;

public class FindScalar extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Set<Long> values = new HashSet<>();
		for (String arg : getScriptArgs())
			values.add(arg.startsWith("0x") ? Long.parseUnsignedLong(arg.substring(2), 16) : Long.parseLong(arg));
		InstructionIterator it = currentProgram.getListing().getInstructions(true);
		while (it.hasNext() && !monitor.isCancelled()) {
			Instruction insn = it.next();
			for (int op = 0; op < insn.getNumOperands(); op++) {
				for (Object o : insn.getOpObjects(op)) {
					if (!(o instanceof Scalar)) continue;
					long v = ((Scalar) o).getUnsignedValue();
					if (!values.contains(v)) continue;
					Function f = getFunctionContaining(insn.getAddress());
					println("SCALAR 0x" + Long.toHexString(v) + " " + insn.getAddress() + " "
						+ (f == null ? "none none" : f.getEntryPoint() + " " + f.getName()) + " | " + insn);
				}
			}
		}
	}
}
