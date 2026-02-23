#!/usr/bin/env bash
# re_diff.sh — 对比 Agent A (反编译) 和 Agent B (反汇编) 的 JSON 结果，产出共识报告
#
# 用法:
#   ./tools/re_diff.sh                    # 处理 re_task_list.txt 中所有函数
#   ./tools/re_diff.sh nicUniCmdBssInfoTagBasic  # 只处理指定函数
#
# 输出: tmp/re_results/consensus/<func_name>_consensus.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TASK_LIST="$SCRIPT_DIR/re_task_list.txt"
AGENT_A_DIR="$REPO_ROOT/tmp/re_results/agent_a"
AGENT_B_DIR="$REPO_ROOT/tmp/re_results/agent_b"
CONSENSUS_DIR="$REPO_ROOT/tmp/re_results/consensus"

mkdir -p "$CONSENSUS_DIR"

# 解析任务列表（跳过注释和空行）
declare -a FUNC_LABELS=()

while IFS= read -r line; do
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ -z "${line//[[:space:]]/}" ]] && continue
    read -r label _ <<< "$line"
    FUNC_LABELS+=("$label")
done < "$TASK_LIST"

# 如果指定了函数名，只处理该函数
FILTER="${1:-}"
if [[ -n "$FILTER" ]]; then
    found=0
    for label in "${FUNC_LABELS[@]}"; do
        [[ "$label" == "$FILTER" ]] && { found=1; break; }
    done
    if [[ $found -eq 0 ]]; then
        echo "ERROR: Function '$FILTER' not found in task list" >&2
        exit 1
    fi
    FUNC_LABELS=("$FILTER")
fi

TOTAL="${#FUNC_LABELS[@]}"
echo "=== Diff 对比 — $TOTAL 个函数 ==="

# Python diff 脚本（内联，避免额外文件依赖）
DIFF_PY=$(cat << 'PYEOF'
import json
import sys
from pathlib import Path

func = sys.argv[1]
file_a = sys.argv[2]
file_b = sys.argv[3]
output = sys.argv[4]

def load_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except Exception as e:
        return None

data_a = load_json(file_a)
data_b = load_json(file_b)

lines = []
lines.append(f"# {func} — Consensus Report")
lines.append("")
lines.append(f"**Agent A (反编译)**: `{Path(file_a).name}`  ")
lines.append(f"**Agent B (反汇编)**: `{Path(file_b).name}`  ")
lines.append(f"**生成时间**: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
lines.append("")

if data_a is None and data_b is None:
    lines.append("## 错误")
    lines.append("两个 Agent 的输出均不可用或不是有效 JSON。")
    Path(output).write_text("\n".join(lines))
    print("MISSING_BOTH")
    sys.exit(0)

if data_a is None:
    lines.append("## 警告")
    lines.append("Agent A 输出不可用，仅显示 Agent B 结果（未验证）。")
    lines.append("")
    lines.append("## Agent B 字段（未验证）")
    lines.append("")
    lines.append("| Offset | Size | Name | Value | Evidence |")
    lines.append("|--------|------|------|-------|----------|")
    for f in (data_b.get("fields") or []):
        ev = f.get("evidence", "").replace("|", "\\|").replace("\n", " ")[:80]
        lines.append(f"| {f.get('offset','?')} | {f.get('size','?')} | {f.get('name','?')} | {f.get('value','?')} | {ev} |")
    Path(output).write_text("\n".join(lines))
    print("A_MISSING")
    sys.exit(0)

if data_b is None:
    lines.append("## 警告")
    lines.append("Agent B 输出不可用，仅显示 Agent A 结果（未验证）。")
    lines.append("")
    lines.append("## Agent A 字段（未验证）")
    lines.append("")
    lines.append("| Offset | Size | Name | Value | Evidence |")
    lines.append("|--------|------|------|-------|----------|")
    for f in (data_a.get("fields") or []):
        ev = f.get("evidence", "").replace("|", "\\|").replace("\n", " ")[:80]
        lines.append(f"| {f.get('offset','?')} | {f.get('size','?')} | {f.get('name','?')} | {f.get('value','?')} | {ev} |")
    Path(output).write_text("\n".join(lines))
    print("B_MISSING")
    sys.exit(0)

# 两者都有数据，进行对比
func_addr = data_a.get("address") or data_b.get("address") or "unknown"
size_a = data_a.get("total_payload_size")
size_b = data_b.get("total_payload_size")

lines.append(f"**函数地址**: `{func_addr}`  ")
lines.append(f"**Payload 大小 (A/B)**: {size_a} / {size_b}")
if size_a is not None and size_b is not None and size_a != size_b:
    lines.append(f"  **CONFLICT: 大小不一致!**")
lines.append("")

# 按 offset 建立字段索引
def normalize_offset(o):
    """将各种格式的 offset 统一为整数"""
    if o is None:
        return None
    s = str(o).strip().lower()
    try:
        if s.startswith("0x"):
            return int(s, 16)
        return int(s)
    except ValueError:
        return None

fields_a = {}
for f in (data_a.get("fields") or []):
    off = normalize_offset(f.get("offset"))
    if off is not None:
        fields_a[off] = f

fields_b = {}
for f in (data_b.get("fields") or []):
    off = normalize_offset(f.get("offset"))
    if off is not None:
        fields_b[off] = f

all_offsets = sorted(set(list(fields_a.keys()) + list(fields_b.keys())))

confirmed = []
conflicts = []
only_a = []
only_b = []

for off in all_offsets:
    fa = fields_a.get(off)
    fb = fields_b.get(off)

    if fa is not None and fb is not None:
        # 两者都有这个 offset
        # 比较 size 和 value
        size_match = (str(fa.get("size","")) == str(fb.get("size","")))
        val_a = str(fa.get("value","")).lower().strip()
        val_b = str(fb.get("value","")).lower().strip()
        # 对于常量值，去掉前导零进行比较
        def canon_val(v):
            try:
                if v.startswith("0x"):
                    return hex(int(v, 16))
                return hex(int(v))
            except:
                return v
        val_match = (canon_val(val_a) == canon_val(val_b))

        if size_match and val_match:
            confirmed.append((off, fa, fb))
        else:
            conflicts.append((off, fa, fb, size_match, val_match))
    elif fa is not None:
        only_a.append((off, fa))
    else:
        only_b.append((off, fb))

# 输出确认字段
lines.append("## 确认字段 (Agent A + B 一致)")
lines.append("")
if confirmed:
    lines.append("| Offset | Size | Name | Value | Evidence (A) |")
    lines.append("|--------|------|------|-------|--------------|")
    for off, fa, fb in confirmed:
        off_hex = f"0x{off:02x}"
        ev = fa.get("evidence","").replace("|","\\|").replace("\n"," ")[:80]
        name = fa.get("name") or fb.get("name") or "?"
        lines.append(f"| {off_hex} | {fa.get('size','?')} | {name} | {fa.get('value','?')} | {ev} |")
else:
    lines.append("_无确认字段_")
lines.append("")

# 输出冲突字段
lines.append("## 冲突字段 (A 和 B 不一致)")
lines.append("")
if conflicts:
    for off, fa, fb, size_match, val_match in conflicts:
        off_hex = f"0x{off:02x}"
        lines.append(f"### offset {off_hex}")
        lines.append("")
        lines.append(f"| | Size | Name | Value |")
        lines.append(f"|---|------|------|-------|")
        lines.append(f"| Agent A | {fa.get('size','?')} | {fa.get('name','?')} | {fa.get('value','?')} |")
        lines.append(f"| Agent B | {fb.get('size','?')} | {fb.get('name','?')} | {fb.get('value','?')} |")
        issues = []
        if not size_match:
            issues.append("size 不一致")
        if not val_match:
            issues.append("value 不一致")
        lines.append(f"**冲突类型**: {', '.join(issues)}")
        lines.append(f"**Evidence A**: `{fa.get('evidence','').replace(chr(10),' ')[:100]}`")
        lines.append(f"**Evidence B**: `{fb.get('evidence','').replace(chr(10),' ')[:100]}`")
        lines.append("")
else:
    lines.append("_无冲突字段_")
    lines.append("")

# 仅 A 有的字段
lines.append("## 仅 Agent A 发现的字段")
lines.append("")
if only_a:
    lines.append("| Offset | Size | Name | Value | Evidence |")
    lines.append("|--------|------|------|-------|----------|")
    for off, fa in only_a:
        off_hex = f"0x{off:02x}"
        ev = fa.get("evidence","").replace("|","\\|").replace("\n"," ")[:80]
        lines.append(f"| {off_hex} | {fa.get('size','?')} | {fa.get('name','?')} | {fa.get('value','?')} | {ev} |")
else:
    lines.append("_无_")
lines.append("")

# 仅 B 有的字段
lines.append("## 仅 Agent B 发现的字段")
lines.append("")
if only_b:
    lines.append("| Offset | Size | Name | Value | Evidence |")
    lines.append("|--------|------|------|-------|----------|")
    for off, fb in only_b:
        off_hex = f"0x{off:02x}"
        ev = fb.get("evidence","").replace("|","\\|").replace("\n"," ")[:80]
        lines.append(f"| {off_hex} | {fb.get('size','?')} | {fb.get('name','?')} | {fb.get('value','?')} | {ev} |")
else:
    lines.append("_无_")
lines.append("")

# Notes
notes_a = data_a.get("notes","").strip()
notes_b = data_b.get("notes","").strip()
if notes_a or notes_b:
    lines.append("## 注释")
    lines.append("")
    if notes_a:
        lines.append(f"**Agent A**: {notes_a}")
        lines.append("")
    if notes_b:
        lines.append(f"**Agent B**: {notes_b}")
        lines.append("")

# 统计摘要
lines.append("## 统计摘要")
lines.append("")
lines.append(f"- 确认字段: **{len(confirmed)}**")
lines.append(f"- 冲突字段: **{len(conflicts)}**")
lines.append(f"- 仅 A 有: **{len(only_a)}**")
lines.append(f"- 仅 B 有: **{len(only_b)}**")
total_fields = len(confirmed) + len(conflicts) + len(only_a) + len(only_b)
if total_fields > 0:
    confidence = int(100 * len(confirmed) / total_fields)
    lines.append(f"- 置信度: **{confidence}%** ({len(confirmed)}/{total_fields})")
lines.append("")

Path(output).write_text("\n".join(lines))

status = "OK"
if conflicts:
    status = f"CONFLICTS({len(conflicts)})"
print(status)
PYEOF
)

SUCCESS=0
FAILED=0
MISSING=0

for i in "${!FUNC_LABELS[@]}"; do
    label="${FUNC_LABELS[$i]}"
    file_a="$AGENT_A_DIR/${label}.json"
    file_b="$AGENT_B_DIR/${label}.json"
    output_file="$CONSENSUS_DIR/${label}_consensus.md"
    idx=$((i + 1))

    printf "[%d/%d] Diff %s... " "$idx" "$TOTAL" "$label"

    result=$(python3 -c "$DIFF_PY" "$label" "$file_a" "$file_b" "$output_file" 2>&1) || {
        echo "PYTHON_ERROR: $result"
        ((FAILED++)) || true
        continue
    }

    echo "$result"

    case "$result" in
        OK|CONFLICTS*) ((SUCCESS++)) || true ;;
        MISSING_BOTH)  ((MISSING++)) || true ;;
        A_MISSING|B_MISSING) ((MISSING++)) || true ;;
        *) ((FAILED++)) || true ;;
    esac
done

echo ""
echo "=== Diff 完成 ==="
echo "处理: $SUCCESS / $TOTAL (missing: $MISSING, error: $FAILED)"
echo "报告目录: $CONSENSUS_DIR"
echo ""
echo "查看报告:"
for label in "${FUNC_LABELS[@]}"; do
    report="$CONSENSUS_DIR/${label}_consensus.md"
    [[ -f "$report" ]] && echo "  $report"
done
