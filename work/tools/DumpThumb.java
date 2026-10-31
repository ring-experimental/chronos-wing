// Seeds Thumb functions at push-lr prologues in the raw client.bin, then dumps decompiled C.
import java.io.PrintWriter;
import java.math.BigInteger;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;

public class DumpThumb extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String out = getScriptArgs()[0];
        Register tmode = currentProgram.getRegister("TMode");
        MemoryBlock block = currentProgram.getMemory().getBlocks()[0];
        Address a = block.getStart();
        Address end = block.getEnd();
        int seeded = 0;
        currentProgram.getProgramContext().setValue(tmode, a, end, BigInteger.ONE);
        while (a.compareTo(end) < 0) {
            int hi = currentProgram.getMemory().getByte(a.add(1)) & 0xff;
            if (hi == 0xb5) {
                if (getInstructionAt(a) == null) {
                    new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
                }
                if (getFunctionAt(a) == null && getInstructionAt(a) != null) {
                    createFunction(a, null);
                }
                seeded++;
            }
            a = a.add(2);
        }
        println("seeded " + seeded);
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                if (r != null && r.getDecompiledFunction() != null) {
                    w.println("// @" + f.getEntryPoint());
                    w.println(r.getDecompiledFunction().getC());
                }
            }
        }
    }
}
