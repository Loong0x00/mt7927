// Extract firmware/DMA related string xrefs and surrounding functions for mtkwecx.sys
//@author codex
//@category MT7927

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.RefType;

public class GhidraExtractFwFlow extends GhidraScript {

    private static class RefRow {
        String from;
        String fnName;
        String fnEntry;
        String ins;
        String refType;
    }

    private static class MatchRow {
        String strAddr;
        String strVal;
        List<RefRow> refs = new ArrayList<>();
    }

    private static class FnGraph {
        String name;
        Set<String> callers = new HashSet<>();
        Set<String> callees = new HashSet<>();
    }

    @Override
    protected void run() throws Exception {
        String outPath = "/tmp/mtkwecx_fw_flow_auto.md";
        if (getScriptArgs().length > 0) {
            outPath = getScriptArgs()[0];
        }

        String[] keywords = new String[] {
            "MT6639PreFirmwareDownloadInit",
            "MT6639WpdmaConfig",
            "MT6639InitTxRxRing",
            "MT6639ConfigIntMask",
            "HalDownloadPatchFirmware",
            "AsicConnac3xLoadRomPatch",
            "AsicConnac3xLoadFirmware",
            "AsicConnac3xWpdmaInitRing",
            "AsicConnac3xWfdmaWaitIdle",
            "P0T15:AP CMD",
            "P0T16:FWDL",
            "P0R0:FWDL",
            "HOST_INT_STA",
            "MCU_CMD",
            "WPDMA_GLO_CFG",
            "WFDMA"
        };

        Listing listing = currentProgram.getListing();
        List<MatchRow> matches = new ArrayList<>();
        Set<String> seenStrAddr = new HashSet<>();

        DataIterator dit = listing.getDefinedData(true);
        while (dit.hasNext()) {
            Data d = dit.next();
            if (monitor.isCancelled()) {
                return;
            }
            Object v = d.getValue();
            if (v == null) {
                continue;
            }

            String s = String.valueOf(v);
            if (s.length() < 4) {
                continue;
            }

            boolean hit = false;
            for (String kw : keywords) {
                if (s.contains(kw)) {
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                continue;
            }

            String saddr = d.getAddress().toString();
            if (seenStrAddr.contains(saddr)) {
                continue;
            }
            seenStrAddr.add(saddr);

            MatchRow mr = new MatchRow();
            mr.strAddr = saddr;
            mr.strVal = s;

            Reference[] refs = getReferencesTo(d.getAddress());
            for (Reference r : refs) {
                RefRow rr = new RefRow();
                Address from = r.getFromAddress();
                rr.from = from.toString();

                Function fn = getFunctionContaining(from);
                if (fn != null) {
                    rr.fnName = fn.getName();
                    rr.fnEntry = fn.getEntryPoint().toString();
                }
                else {
                    rr.fnName = "<no_function>";
                    rr.fnEntry = "";
                }

                Instruction ins = listing.getInstructionAt(from);
                rr.ins = ins != null ? ins.toString() : "";
                rr.refType = r.getReferenceType().toString();

                mr.refs.add(rr);
            }
            matches.add(mr);
        }

        Collections.sort(matches, Comparator.comparing(a -> a.strAddr));

        Map<String, FnGraph> fnMap = new HashMap<>();
        for (MatchRow mr : matches) {
            for (RefRow rr : mr.refs) {
                if (rr.fnEntry.isEmpty()) continue;
                FnGraph g = fnMap.get(rr.fnEntry);
                if (g == null) {
                    g = new FnGraph();
                    g.name = rr.fnName;
                    fnMap.put(rr.fnEntry, g);
                }
            }
        }

        for (Map.Entry<String, FnGraph> e : fnMap.entrySet()) {
            Address fEntry = toAddr(e.getKey());
            Function fn = getFunctionAt(fEntry);
            if (fn == null) continue;

            InstructionIterator iit = listing.getInstructions(fn.getBody(), true);
            while (iit.hasNext()) {
                Instruction ins = iit.next();
                if (ins.getFlowType().isCall()) {
                    Address[] flows = ins.getFlows();
                    if (flows != null) {
                        for (Address fa : flows) {
                            Function callee = getFunctionContaining(fa);
                            if (callee != null) {
                                e.getValue().callees.add(callee.getName() + "@" + callee.getEntryPoint());
                            }
                            else {
                                e.getValue().callees.add(fa.toString());
                            }
                        }
                    }
                }
            }

            Reference[] toRefs = getReferencesTo(fn.getEntryPoint());
            for (Reference r : toRefs) {
                if (!r.getReferenceType().isCall()) continue;
                Function caller = getFunctionContaining(r.getFromAddress());
                if (caller != null) {
                    e.getValue().callers.add(caller.getName() + "@" + caller.getEntryPoint());
                }
                else {
                    e.getValue().callers.add(r.getFromAddress().toString());
                }
            }
        }

        writeMarkdown(outPath, matches, fnMap);
        println("Wrote: " + outPath);
    }

    private void writeMarkdown(String outPath, List<MatchRow> matches, Map<String, FnGraph> fnMap)
            throws IOException {
        File out = new File(outPath);
        File parent = out.getParentFile();
        if (parent != null) parent.mkdirs();

        try (BufferedWriter bw = new BufferedWriter(new FileWriter(out))) {
            bw.write("# mtkwecx.sys firmware/DMA reverse notes (auto extract)\n\n");
            bw.write("Program: " + currentProgram.getName() + "\n\n");

            bw.write("## String Xrefs\n\n");
            for (MatchRow mr : matches) {
                bw.write("### " + mr.strAddr + " `" + esc(mr.strVal) + "`\n\n");
                if (mr.refs.isEmpty()) {
                    bw.write("- (no xrefs found)\n\n");
                    continue;
                }
                for (RefRow rr : mr.refs) {
                    bw.write("- from `" + rr.from + "` fn `" + esc(rr.fnName) + "` (" + rr.fnEntry + ") ref="
                            + esc(rr.refType) + " ins=`" + esc(rr.ins) + "`\n");
                }
                bw.write("\n");
            }

            bw.write("## Function Call Graph (functions seen in xrefs)\n\n");
            List<String> keys = new ArrayList<>(fnMap.keySet());
            Collections.sort(keys);
            for (String entry : keys) {
                FnGraph g = fnMap.get(entry);
                bw.write("### " + esc(g.name) + " @ " + entry + "\n\n");
                bw.write("Callers:\n");
                if (g.callers.isEmpty()) {
                    bw.write("- (none)\n");
                } else {
                    List<String> callers = new ArrayList<>(g.callers);
                    Collections.sort(callers);
                    for (String c : callers) bw.write("- " + esc(c) + "\n");
                }
                bw.write("Callees:\n");
                if (g.callees.isEmpty()) {
                    bw.write("- (none)\n");
                } else {
                    List<String> callees = new ArrayList<>(g.callees);
                    Collections.sort(callees);
                    for (String c : callees) bw.write("- " + esc(c) + "\n");
                }
                bw.write("\n");
            }
        }
    }

    private String esc(String s) {
        if (s == null) return "";
        return s.replace("`", "'").replace("\n", "\\n").replace("\r", "");
    }
}
