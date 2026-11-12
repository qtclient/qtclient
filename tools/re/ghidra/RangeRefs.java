// Inspect an address range.
// Args: refs <lo> <hi>  -> "RREF <target> <from> <function-entry> <function-name>" for every
//                          reference into [lo, hi) (finds the code that reads a data table);
//       funcs <lo> <hi> -> "FUNC <entry> <size> <name>" for every function starting in [lo, hi).
//@category ReverseEngineering

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;

public class RangeRefs extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Address lo = toAddr(args[1]);
		Address hi = toAddr(args[2]);
		if (args[0].equals("funcs")) {
			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(new AddressSet(lo, hi.previous()), true);
			while (it.hasNext()) {
				Function f = it.next();
				println("FUNC " + f.getEntryPoint() + " " + f.getBody().getNumAddresses() + " " + f.getName(true));
			}
			return;
		}
		ReferenceManager rm = currentProgram.getReferenceManager();
		for (Address a = lo; a.compareTo(hi) < 0 && !monitor.isCancelled(); a = a.next()) {
			for (Reference r : rm.getReferencesTo(a)) {
				Function f = getFunctionContaining(r.getFromAddress());
				println("RREF " + a + " " + r.getFromAddress() + " "
					+ (f == null ? "none none" : f.getEntryPoint() + " " + f.getName(true)));
			}
		}
	}
}
