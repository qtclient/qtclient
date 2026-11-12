// For each "<label> <addr> <addr>..." line of an input file, print the
// functions referencing each address: "CALL <label> <callee> <caller-entry> <caller-name> <from>".
// Args: <input-file>
//@category ReverseEngineering

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.nio.file.Files;
import java.nio.file.Paths;

public class CallerMap extends GhidraScript {
	@Override
	protected void run() throws Exception {
		for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
			String[] parts = line.trim().split("\\s+");
			if (parts.length < 2) continue;
			for (int i = 1; i < parts.length; i++) {
				Address callee = toAddr(parts[i]);
				for (Reference r : getReferencesTo(callee)) {
					Function f = getFunctionContaining(r.getFromAddress());
					println("CALL " + parts[0] + " " + callee + " " + (f == null ? "none none" : f.getEntryPoint() + " " + f.getName()) + " " + r.getFromAddress());
				}
			}
		}
	}
}
