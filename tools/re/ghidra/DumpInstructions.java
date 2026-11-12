// Dump every instruction of the functions containing the given addresses to
// <outdir>/<entry>.asm as "<addr>\t<instruction>\t<referenced addresses>".
// Args: <outdir> <address>...
//@category ReverseEngineering

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;

import java.io.File;
import java.io.PrintWriter;

public class DumpInstructions extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args[0]);
		dir.mkdirs();
		for (int i = 1; i < args.length; i++) {
			Function f = getFunctionContaining(toAddr(args[i]));
			if (f == null) {
				println("ASM missing " + args[i]);
				continue;
			}
			File out = new File(dir, f.getEntryPoint() + ".asm");
			try (PrintWriter w = new PrintWriter(out)) {
				InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
				while (it.hasNext()) {
					Instruction ins = it.next();
					StringBuilder refs = new StringBuilder();
					for (Reference r : ins.getReferencesFrom()) {
						refs.append(r.getToAddress()).append(' ');
					}
					w.println(ins.getAddress() + "\t" + ins + "\t" + refs.toString().trim());
				}
			}
			println("ASM " + f.getEntryPoint() + " -> " + out);
		}
	}
}
