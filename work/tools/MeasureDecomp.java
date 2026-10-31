// Dumps per-function body and decompile quality of the existing Ghidra project as TSV.
import java.io.PrintWriter;
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;

public class MeasureDecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0], "UTF-8")) {
            w.println("entry\tmin\tmax\tbytes\tinsns\tok\tbaddata\tjumptable\tpcode_err\tmsg\tranges");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                AddressSetView body = f.getBody();
                int insns = 0;
                for (Instruction i : currentProgram.getListing().getInstructions(body, true)) insns++;
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                boolean ok = r != null && r.getDecompiledFunction() != null;
                String c = ok ? r.getDecompiledFunction().getC() : "";
                String msg = r == null || r.getErrorMessage() == null ? "" : r.getErrorMessage().replaceAll("\\s+", " ");
                w.println(f.getEntryPoint() + "\t" + body.getMinAddress() + "\t" + body.getMaxAddress() + "\t"
                    + body.getNumAddresses() + "\t" + insns + "\t" + ok + "\t"
                    + count(c, "halt_baddata") + "\t" + count(c, "UNRECOVERED_JUMPTABLE") + "\t"
                    + count(msg, "pcode error") + "\t" + msg + "\t" + body.toString().replaceAll("\\s+", ""));
            }
        }
    }
    private static int count(String s, String n) {
        int c = 0;
        for (int i = s.indexOf(n); i >= 0; i = s.indexOf(n, i + n.length())) c++;
        return c;
    }
}
