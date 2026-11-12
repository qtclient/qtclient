// Print code references to C strings as
// "SREF <string> <string-addr> <from-addr> <function-entry> <function-name>".
// Args: <text>... ; "prefix*" matches every string starting with prefix.
// Strings are found by byte search (pooled literals have no defined Data), then
// widened back to the preceding NUL so the reference target is the string start.
//@category ReverseEngineering

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;

import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import java.util.Set;

public class StringRefs extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Memory mem = currentProgram.getMemory();
		for (String arg : getScriptArgs()) {
			boolean prefix = arg.endsWith("*");
			String text = prefix ? arg.substring(0, arg.length() - 1) : arg;
			byte[] needle = text.getBytes(StandardCharsets.UTF_8);
			Set<Address> starts = new LinkedHashSet<>();
			Address at = mem.getMinAddress();
			while (at != null && !monitor.isCancelled()) {
				Address hit = mem.findBytes(at, needle, null, true, monitor);
				if (hit == null) break;
				Address start = hit;
				while (true) {
					Address prev = start.previous();
					if (prev == null || !mem.contains(prev) || mem.getByte(prev) == 0) break;
					start = prev;
				}
				if (!prefix || start.equals(hit)) starts.add(start);
				at = hit.next();
			}
			for (Address s : starts) {
				String value = readCString(mem, s);
				Reference[] refs = getReferencesTo(s);
				if (refs.length == 0) println("SREF " + quote(value) + " " + s + " none none none");
				for (Reference r : refs) {
					Function f = getFunctionContaining(r.getFromAddress());
					println("SREF " + quote(value) + " " + s + " " + r.getFromAddress() + " "
						+ (f == null ? "none none" : f.getEntryPoint() + " " + f.getName(true)));
				}
			}
		}
	}

	private static String readCString(Memory mem, Address a) throws Exception {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < 400; i++) {
			byte b = mem.getByte(a.add(i));
			if (b == 0) break;
			sb.append((char) (b & 0xff));
		}
		return sb.toString();
	}

	private static String quote(String s) {
		return "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n") + "\"";
	}
}
