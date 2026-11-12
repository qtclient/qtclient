// Emulate a static initializer and print the mouse-binding tables it builds
// (input::TGameActionMapper rules: std::vector of 0x28-byte
// {int context; int button; int modifiers; int pad; std::vector<int> actions}).
// Imports are stubbed: operator new (bump heap), memcpy/memmove/memset,
// everything else returns 0. Output: "RULE <table> <context> <button> <modifiers> <actions...>".
// Args: <initializer-addr> <table-addr>...
//@category ReverseEngineering

import ghidra.app.emulator.EmulatorHelper;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.math.BigInteger;
import java.util.HashMap;
import java.util.Map;

public class DumpMouseBindings extends GhidraScript {
	private EmulatorHelper emu;
	private long heap = 0x600000000L;
	private final java.util.Set<String> seen = new java.util.HashSet<>();

	private long reg(String name) { return emu.readRegister(name).longValue(); }
	private void setReg(String name, long v) { emu.writeRegister(name, BigInteger.valueOf(v)); }
	private long read64(long a) {
		byte[] b = emu.readMemory(toAddr(a), 8);
		long v = 0;
		for (int i = 7; i >= 0; i--) v = (v << 8) | (b[i] & 0xff);
		return v;
	}
	private int read32(long a) {
		byte[] b = emu.readMemory(toAddr(a), 4);
		return (b[0] & 0xff) | (b[1] & 0xff) << 8 | (b[2] & 0xff) << 16 | (b[3] & 0xff) << 24;
	}

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		emu = new EmulatorHelper(currentProgram);
		Map<Long, String> stubs = new HashMap<>();
		FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
		while (it.hasNext()) {
			Function f = it.next();
			if (f.isThunk() || f.isExternal()) {
				Function target = f.isThunk() ? f.getThunkedFunction(true) : f;
				stubs.put(f.getEntryPoint().getOffset(), target.getName());
				emu.setBreakpoint(f.getEntryPoint());
			}
		}
		long sentinel = 0x7ffff0000000L;
		long stack = 0x7fff00100000L;
		setReg("RSP", stack - 8);
		emu.writeMemory(toAddr(stack - 8), le(sentinel));
		emu.setBreakpoint(toAddr(sentinel));
		Address entry = toAddr(args[0]);
		emu.writeRegister(emu.getPCRegister(), entry.getOffset());

		int steps = 0;
		while (!monitor.isCancelled()) {
			emu.run(monitor);
			long pc = emu.getExecutionAddress().getOffset();
			if (pc == sentinel) break;
			String name = stubs.get(pc);
			if (name == null) {
				println("STOP at " + Long.toHexString(pc) + ": " + emu.getLastError());
				return;
			}
			long rdi = reg("RDI"), rsi = reg("RSI"), rdx = reg("RDX");
			long ret = 0;
			if (seen.add(name)) println("IMPORT " + name);
			if (name.equals("_Znwm") || name.equals("_Znam") || name.startsWith("operator.new") || name.startsWith("operator_new")) {
				ret = heap;
				heap += (rdi + 15) & ~15L;
			} else if (name.contains("memcpy") || name.contains("memmove")) {
				if (rdx > 0) emu.writeMemory(toAddr(rdi), emu.readMemory(toAddr(rsi), (int) rdx));
				ret = rdi;
			} else if (name.contains("memset")) {
				byte[] b = new byte[(int) rdx];
				java.util.Arrays.fill(b, (byte) rsi);
				if (rdx > 0) emu.writeMemory(toAddr(rdi), b);
				ret = rdi;
			} else if (name.contains("fromUtf8")) {
				emu.writeMemory(toAddr(rdi), new byte[24]);
				ret = rdi;
			}
			setReg("RAX", ret);
			long rsp = reg("RSP");
			emu.writeRegister(emu.getPCRegister(), read64(rsp));
			setReg("RSP", rsp + 8);
			if (++steps > 200000) { println("too many import calls"); return; }
		}

		for (int i = 1; i < args.length; i++) {
			long table = Long.decode(args[i]);
			long begin = read64(table), end = read64(table + 8);
			for (long r = begin; r < end; r += 0x28) {
				StringBuilder sb = new StringBuilder("RULE " + args[i] + " " + read32(r) + " " + read32(r + 4) + " 0x"
					+ Integer.toHexString(read32(r + 8)));
				long ab = read64(r + 0x10), ae = read64(r + 0x18);
				for (long a = ab; a < ae; a += 4) sb.append(" ").append(read32(a));
				println(sb.toString());
			}
		}
	}

	private static byte[] le(long v) {
		byte[] b = new byte[8];
		for (int i = 0; i < 8; i++) b[i] = (byte) (v >>> (8 * i));
		return b;
	}
}
