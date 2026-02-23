#@author codex
#@category MT7927
#@keybinding
#@menupath
#@toolbar

from ghidra.program.model.listing import Function
from ghidra.program.model.symbol import RefType
from java.lang import String

# Args:
# 1) output markdown path
out_path = None
args = getScriptArgs()
if args and len(args) > 0:
    out_path = args[0]
else:
    out_path = "/tmp/mtkwecx_fw_flow.md"

keywords = [
    "MT6639PreFirmwareDownloadInit",
    "MT6639WpdmaConfig",
    "MT6639InitTxRxRing",
    "MT6639ConfigIntMask",
    "HalDownloadPatchFirmware",
    "AsicConnac3xLoadRomPatch",
    "AsicConnac3xLoadFirmware",
    "P0T15:AP CMD",
    "P0T16:FWDL",
    "P0R0:FWDL",
    "HOST_INT_STA",
    "MCU_CMD",
    "WPDMA_GLO_CFG",
    "WFDMA",
]

listing = currentProgram.getListing()
mem = currentProgram.getMemory()

matches = []
visited = set()

it = listing.getDefinedData(True)
while it.hasNext() and not monitor.isCancelled():
    d = it.next()
    val = d.getValue()
    if val is None:
        continue
    try:
        s = str(val)
    except:
        continue
    if len(s) < 4:
        continue

    hit = False
    for kw in keywords:
        if kw in s:
            hit = True
            break
    if not hit:
        continue

    saddr = d.getAddress()
    key = str(saddr)
    if key in visited:
        continue
    visited.add(key)

    refs = getReferencesTo(saddr)
    if refs is None:
        refs = []

    ref_rows = []
    for r in refs:
        from_addr = r.getFromAddress()
        fn = getFunctionContaining(from_addr)
        fn_name = fn.getName() if fn else "<no_function>"
        entry = str(fn.getEntryPoint()) if fn else ""

        ins = listing.getInstructionAt(from_addr)
        ins_text = ""
        if ins:
            ins_text = ins.toString()

        ref_rows.append((str(from_addr), fn_name, entry, ins_text, str(r.getReferenceType())))

    matches.append((str(saddr), s, ref_rows))

# sort by address string for deterministic output
matches.sort(key=lambda x: x[0])

# collect interesting function set
func_map = {}
for m in matches:
    for rr in m[2]:
        fn_name = rr[1]
        fn_entry = rr[2]
        if fn_name == "<no_function>" or fn_entry == "":
            continue
        func_map[fn_entry] = fn_name

# helper: collect callees and callers for each function
call_graph = {}
for fentry, fname in func_map.items():
    faddr = toAddr(fentry)
    fn = getFunctionAt(faddr)
    if fn is None:
        continue

    callees = set()
    callers = set()

    body = fn.getBody()
    ins_it = listing.getInstructions(body, True)
    while ins_it.hasNext() and not monitor.isCancelled():
        ins = ins_it.next()
        ft = ins.getFlowType()
        if ft and ft.isCall():
            flows = ins.getFlows()
            if flows:
                for fa in flows:
                    cfn = getFunctionContaining(fa)
                    if cfn:
                        callees.add(cfn.getName() + "@" + str(cfn.getEntryPoint()))
                    else:
                        callees.add(str(fa))

    refs_to = getReferencesTo(fn.getEntryPoint())
    if refs_to:
        for r in refs_to:
            if r.getReferenceType().isCall():
                cfrom = r.getFromAddress()
                cfn = getFunctionContaining(cfrom)
                if cfn:
                    callers.add(cfn.getName() + "@" + str(cfn.getEntryPoint()))
                else:
                    callers.add(str(cfrom))

    call_graph[fentry] = (fname, sorted(list(callers)), sorted(list(callees)))

# write markdown output
f = open(out_path, "w")
f.write("# mtkwecx.sys firmware/DMA reverse notes (auto extract)\n\n")
f.write("Program: %s\n\n" % currentProgram.getName())

f.write("## String Xrefs\n\n")
for saddr, sval, refs in matches:
    f.write("### %s `%s`\n\n" % (saddr, sval.replace("`", "'")))
    if not refs:
        f.write("- (no xrefs found)\n\n")
        continue
    for rr in refs:
        f.write("- from `%s` fn `%s` (%s) ref=%s ins=`%s`\n" % (
            rr[0], rr[1], rr[2], rr[4], rr[3].replace("`", "'")))
    f.write("\n")

f.write("## Function Call Graph (for functions seen in xrefs)\n\n")
for fentry in sorted(call_graph.keys()):
    fname, callers, callees = call_graph[fentry]
    f.write("### %s @ %s\n\n" % (fname, fentry))
    f.write("Callers:\n")
    if callers:
        for c in callers[:40]:
            f.write("- %s\n" % c)
    else:
        f.write("- (none)\n")
    f.write("Callees:\n")
    if callees:
        for c in callees[:80]:
            f.write("- %s\n" % c)
    else:
        f.write("- (none)\n")
    f.write("\n")

f.close()

print("Wrote: %s" % out_path)
