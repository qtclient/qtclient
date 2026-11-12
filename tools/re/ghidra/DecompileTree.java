// Decompile functions and their internal callees (BFS) to <outdir>/<entry>.c.
// Args: <outdir> <depth> <maxFunctions> <namesFile|-> <address>...
// namesFile: "0xADDR name" per line, applied as in-memory labels before decompiling.
//@category ReverseEngineering

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.Set;

public class DecompileTree extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args[0]);
		dir.mkdirs();
		int depth = Integer.parseInt(args[1]);
		int max = Integer.parseInt(args[2]);
		if (!args[3].equals("-")) {
			int tx = currentProgram.startTransaction("names");
			try (BufferedReader r = new BufferedReader(new FileReader(args[3]))) {
				String line;
				while ((line = r.readLine()) != null) {
					String[] p = line.trim().split("\\s+", 2);
					if (p.length < 2) continue;
					Function f = getFunctionAt(toAddr(p[0]));
					if (f == null) continue;
					try {
						f.setName(p[1].replaceAll("[^A-Za-z0-9_:~]", "_"), SourceType.USER_DEFINED);
					} catch (Exception e) {
						// duplicate names are fine to skip
					}
				}
			}
			currentProgram.endTransaction(tx, true);
		}
		DecompInterface di = new DecompInterface();
		di.openProgram(currentProgram);
		ArrayDeque<Object[]> q = new ArrayDeque<>();
		Set<Address> seen = new HashSet<>();
		for (int i = 4; i < args.length; i++) {
			// Headless sometimes packs "0xA 0xB …" into one argv — split on whitespace.
			for (String tok : args[i].trim().split("\\s+")) {
				if (tok.isEmpty()) continue;
				Address addr;
				try {
					addr = toAddr(tok);
				} catch (Exception e) {
					printerr("bad address: " + tok);
					continue;
				}
				if (addr == null) {
					printerr("null address: " + tok);
					continue;
				}
				Function f = getFunctionContaining(addr);
				if (f != null) q.add(new Object[] {f, 0});
				else printerr("no function at " + tok);
			}
		}
		int n = 0;
		while (!q.isEmpty() && n < max) {
			Object[] it = q.poll();
			Function f = (Function) it[0];
			int d = (Integer) it[1];
			if (!seen.add(f.getEntryPoint())) continue;
			File out = new File(dir, f.getEntryPoint() + ".c");
			if (!out.exists()) {
				DecompileResults res = di.decompileFunction(f, 300, monitor);
				try (PrintWriter w = new PrintWriter(out)) {
					w.println("// " + f.getName(true) + " @ " + f.getEntryPoint() + " depth " + d);
					w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// failed: " + res.getErrorMessage());
				}
			}
			n++;
			println("TREE " + d + " " + f.getEntryPoint() + " " + f.getName(true));
			if (d >= depth) continue;
			for (Function c : f.getCalledFunctions(monitor)) {
				if (c.isExternal() || c.isThunk()) continue;
				if (c.getEntryPoint().getOffset() >= 0x102000000L) continue;
				q.add(new Object[] {c, d + 1});
			}
		}
		di.dispose();
	}
}
