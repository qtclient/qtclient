// Decompile functions to <outdir>/<entry>.c.
// Args: <outdir> <address|function-name>...
//@category ReverseEngineering

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.File;
import java.io.PrintWriter;

public class DecompileAt extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args[0]);
		dir.mkdirs();
		DecompInterface di = new DecompInterface();
		di.openProgram(currentProgram);
		for (int i = 1; i < args.length; i++) {
			Function f = null;
			Address a = toAddr(args[i]);
			if (a != null) f = getFunctionContaining(a);
			if (f == null) f = getGlobalFunctions(args[i]).stream().findFirst().orElse(null);
			if (f == null) {
				println("DECOMP missing " + args[i]);
				continue;
			}
			DecompileResults res = di.decompileFunction(f, 300, monitor);
			File out = new File(dir, f.getEntryPoint() + ".c");
			try (PrintWriter w = new PrintWriter(out)) {
				w.println("// " + f.getName() + " @ " + f.getEntryPoint());
				w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// failed: " + res.getErrorMessage());
			}
			println("DECOMP " + f.getEntryPoint() + " " + f.getName() + " -> " + out);
		}
		di.dispose();
	}
}
