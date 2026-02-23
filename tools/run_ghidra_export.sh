#!/bin/bash
# run_ghidra_export.sh — Ghidra headless batch decompile/disassemble
# Usage: bash tools/run_ghidra_export.sh
# Exports all functions listed in BatchExport.java to tmp/ghidra_exports/

set -e

GHIDRA_HOME="/home/user/mt7927/ghidra_12.0.3_PUBLIC"
PROJECT_DIR="/home/user/mt7927/tmp/ghidra_project"
PROJECT_NAME="mt7927_re"
SCRIPT_DIR="/home/user/mt7927/tmp"
OUTPUT_DIR="/home/user/mt7927/tmp/ghidra_exports"
LOG_FILE="$OUTPUT_DIR/ghidra_log.txt"

mkdir -p "$OUTPUT_DIR"

echo "============================================================"
echo "  MT7927 Ghidra Headless Batch Export"
echo "  $(date)"
echo "  Project : $PROJECT_DIR/$PROJECT_NAME"
echo "  Script  : $SCRIPT_DIR/BatchExport.java"
echo "  Output  : $OUTPUT_DIR"
echo "============================================================"
echo ""

# Run Ghidra headless
# -process mtkwecx.sys  : operate on the already-imported program (no re-analysis)
# -noanalysis            : skip auto-analysis (program is already analysed)
# -postScript            : run our script after loading
"$GHIDRA_HOME/support/analyzeHeadless" \
    "$PROJECT_DIR" "$PROJECT_NAME" \
    -process "mtkwecx.sys" \
    -noanalysis \
    -scriptPath "$SCRIPT_DIR" \
    -postScript BatchExport.java \
    2>&1 | tee "$LOG_FILE"

GHIDRA_EXIT=${PIPESTATUS[0]}

echo ""
echo "============================================================"
echo "  Ghidra exit code: $GHIDRA_EXIT"
echo "  Log: $LOG_FILE"
echo "============================================================"

# Count exported files
DECOMP_COUNT=$(ls "$OUTPUT_DIR"/*_decomp.c   2>/dev/null | wc -l)
DISASM_COUNT=$(ls "$OUTPUT_DIR"/*_disasm.txt 2>/dev/null | wc -l)

echo ""
echo "Exported files:"
echo "  Decompile (.c)   : $DECOMP_COUNT"
echo "  Disassemble (.txt): $DISASM_COUNT"
echo ""

if [ -f "$OUTPUT_DIR/EXPORT_SUMMARY.txt" ]; then
    echo "=== Export Summary ==="
    cat "$OUTPUT_DIR/EXPORT_SUMMARY.txt"
fi

echo ""
echo "Done. Files in $OUTPUT_DIR:"
ls -lh "$OUTPUT_DIR"/*.c "$OUTPUT_DIR"/*.txt 2>/dev/null | awk '{print $5, $9}' | head -80

exit $GHIDRA_EXIT
