// Map tibia::protobuf::protocol::Game{server,client}Message* classes to their
// typeinfo, vtable, construction sites and constant-returning virtual slots.
// Output: one JSON object per class on stdout, prefixed with "MSG ".
//@category ReverseEngineering

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;

import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class TibiaMessageMap extends GhidraScript {
	private static final Pattern NAME =
		Pattern.compile("^N5tibia8protobuf8protocol\\d+(Game(?:server|client)Message\\w*)E$");
	private static final int SLOTS = 24;

	@Override
	protected void run() throws Exception {
		Listing listing = currentProgram.getListing();
		ReferenceManager refs = currentProgram.getReferenceManager();
		Memory mem = currentProgram.getMemory();
		FunctionManager fm = currentProgram.getFunctionManager();
		int ptr = currentProgram.getDefaultPointerSize();

		for (Data d : listing.getDefinedData(true)) {
			if (monitor.isCancelled()) break;
			Object v = d.getValue();
			if (!(v instanceof String)) continue;
			Matcher m = NAME.matcher((String) v);
			if (!m.matches()) continue;
			String cls = m.group(1);
			StringBuilder out = new StringBuilder();
			out.append("{\"class\":\"").append(cls).append("\",\"ts\":\"").append(d.getAddress()).append('"');
			List<String> vts = new ArrayList<>();
			Set<String> ctorFns = new TreeSet<>();
			List<String> slots = new ArrayList<>();
			for (Address tsRef : findPointers(d.getAddress())) {
				Address ti = tsRef.subtract(ptr);
				for (Address tiSlot : findPointers(ti)) {
					Address vt = tiSlot.add(ptr);
					if (getFunctionAt(tiSlot) != null) continue;
					vts.add(vt.toString());
					for (Reference r3 : refs.getReferencesTo(vt)) {
						Function f = fm.getFunctionContaining(r3.getFromAddress());
						if (f != null) ctorFns.add(f.getEntryPoint() + ":" + f.getName());
					}
					if (slots.isEmpty()) {
						for (int i = 0; i < SLOTS; i++) {
							long target;
							try {
								target = ptr == 8 ? mem.getLong(vt.add((long) i * ptr)) : mem.getInt(vt.add((long) i * ptr));
							} catch (Exception e) {
								break;
							}
							Address ta = vt.getNewAddress(target);
							Function f = fm.getFunctionAt(ta);
							slots.add(f == null ? "?" + ta : f.getEntryPoint() + constReturn(f));
						}
					}
				}
			}
			out.append(",\"vtables\":").append(jsonList(vts));
			out.append(",\"ctorFns\":").append(jsonList(new ArrayList<>(ctorFns)));
			out.append(",\"slots\":").append(jsonList(slots)).append('}');
			println("MSG " + out);
		}
	}

	private List<Address> findPointers(Address target) {
		Memory mem = currentProgram.getMemory();
		int ptr = currentProgram.getDefaultPointerSize();
		byte[] needle = new byte[ptr];
		long v = target.getOffset();
		for (int i = 0; i < ptr; i++) needle[i] = (byte) (v >>> (8 * i));
		List<Address> hits = new ArrayList<>();
		for (MemoryBlock b : mem.getBlocks()) {
			if (!b.isInitialized() || b.isExecute()) continue;
			Address a = b.getStart();
			while (a != null && a.compareTo(b.getEnd()) <= 0) {
				Address hit = mem.findBytes(a, b.getEnd(), needle, null, true, monitor);
				if (hit == null) break;
				if (hit.getOffset() % ptr == 0) hits.add(hit);
				a = hit.add(1);
			}
		}
		return hits;
	}

	private String constReturn(Function f) {
		Listing listing = currentProgram.getListing();
		InstructionIterator it = listing.getInstructions(f.getBody(), true);
		int n = 0;
		Long imm = null;
		while (it.hasNext()) {
			Instruction ins = it.next();
			if (++n > 4) return "";
			String mn = ins.getMnemonicString();
			if (mn.equals("MOV") && ins.getNumOperands() == 2 && ins.getScalar(1) != null) {
				String reg = ins.getDefaultOperandRepresentation(0);
				if (reg.equals("EAX") || reg.equals("AL") || reg.equals("RAX")) imm = ins.getScalar(1).getUnsignedValue();
			}
			if (mn.equals("RET")) return imm == null ? "" : "=" + imm;
		}
		return "";
	}

	private static String jsonList(List<String> xs) {
		StringBuilder b = new StringBuilder("[");
		for (int i = 0; i < xs.size(); i++) {
			if (i > 0) b.append(',');
			b.append('"').append(xs.get(i).replace("\"", "'")).append('"');
		}
		return b.append(']').toString();
	}
}
