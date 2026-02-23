// Dump selected function decompilation/disassembly for reverse engineering
//@author codex
//@category MT7927

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.scalar.Scalar;

public class GhidraDumpFuncs extends GhidraScript {

    @Override
    protected void run() throws Exception {
        if (getScriptArgs().length < 2) {
            printerr("Usage: GhidraDumpFuncs.java <out_md> <funcNameOrAddr> [funcNameOrAddr ...]");
            return;
        }

        String outPath = getScriptArgs()[0];
        List<String> targets = new ArrayList<>();
        for (int i = 1; i < getScriptArgs().length; i++) {
            targets.add(getScriptArgs()[i]);
        }

        File out = new File(outPath);
        if (out.getParentFile() != null) {
            out.getParentFile().mkdirs();
        }

        DecompInterface ifc = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        ifc.setOptions(options);
        ifc.openProgram(currentProgram);

        Listing listing = currentProgram.getListing();

        try (BufferedWriter bw = new BufferedWriter(new FileWriter(out))) {
            bw.write("# Ghidra Function Dump\n\n");
            bw.write("Program: " + currentProgram.getName() + "\n\n");

            for (String t : targets) {
                Function fn = resolveFunction(t);
                if (fn == null) {
                    bw.write("## " + t + "\n\n");
                    bw.write("- not found\n\n");
                    continue;
                }

                bw.write("## " + fn.getName() + " @ " + fn.getEntryPoint() + "\n\n");

                Set<Long> immSet = new HashSet<>();
                InstructionIterator it = listing.getInstructions(fn.getBody(), true);
                while (it.hasNext() && !monitor.isCancelled()) {
                    Instruction ins = it.next();
                    for (int i = 0; i < ins.getNumOperands(); i++) {
                        Object[] objs = ins.getOpObjects(i);
                        if (objs == null) continue;
                        for (Object o : objs) {
                            if (o instanceof Scalar) {
                                Scalar s = (Scalar) o;
                                long v = s.getUnsignedValue();
                                if (v >= 0x10000L) {
                                    immSet.add(v);
                                }
                            }
                        }
                    }
                }
                List<Long> imms = new ArrayList<>(immSet);
                Collections.sort(imms);
                bw.write("### Immediates (>=0x10000)\n\n");
                if (imms.isEmpty()) {
                    bw.write("- (none)\n\n");
                } else {
                    for (Long v : imms) {
                        bw.write("- 0x" + Long.toHexString(v) + "\n");
                    }
                    bw.write("\n");
                }

                bw.write("### Disassembly\n\n");
                bw.write("```asm\n");
                it = listing.getInstructions(fn.getBody(), true);
                while (it.hasNext() && !monitor.isCancelled()) {
                    Instruction ins = it.next();
                    bw.write(ins.getAddress() + ": " + ins + "\n");
                }
                bw.write("```\n\n");

                bw.write("### Decompiled C\n\n");
                DecompileResults res = ifc.decompileFunction(fn, 60, monitor);
                if (!res.decompileCompleted() || res.getDecompiledFunction() == null) {
                    bw.write("- decompile failed\n\n");
                } else {
                    bw.write("```c\n");
                    bw.write(res.getDecompiledFunction().getC());
                    bw.write("\n```\n\n");
                }
            }
        }

        println("Wrote: " + outPath);
    }

    private Function resolveFunction(String target) {
        Function fn = super.getFunction(target);
        if (fn != null) return fn;
        try {
            return getFunctionAt(toAddr(target));
        } catch (Exception e) {
            return null;
        }
    }
}
