#!/usr/bin/env bash
# re_verify.sh — Agent B: 从反汇编指令独立分析函数，产出字节级字段映射 JSON
#
# 用法:
#   ./tools/re_verify.sh                    # 处理 re_task_list.txt 中所有函数
#   ./tools/re_verify.sh nicUniCmdBssInfoTagBasic  # 只处理指定函数
#
# 输出: tmp/re_results/agent_b/<func_name>.json

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TASK_LIST="$SCRIPT_DIR/re_task_list.txt"
EXPORT_DIR="$REPO_ROOT/tmp/ghidra_exports"
OUTPUT_DIR="$REPO_ROOT/tmp/re_results/agent_b"
PROGRESS_LOG="$OUTPUT_DIR/progress.log"

mkdir -p "$OUTPUT_DIR"

# 解析任务列表（跳过注释和空行）
declare -a FUNC_LABELS=()
declare -A DECOMP_FILES=()
declare -A DISASM_FILES=()

while IFS= read -r line; do
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ -z "${line//[[:space:]]/}" ]] && continue
    read -r label decomp disasm <<< "$line"
    FUNC_LABELS+=("$label")
    DECOMP_FILES["$label"]="$decomp"
    DISASM_FILES["$label"]="$disasm"
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
echo "=== Agent B (反汇编分析) — $TOTAL 个函数 ===" | tee -a "$PROGRESS_LOG"
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$PROGRESS_LOG"
echo "" | tee -a "$PROGRESS_LOG"

SUCCESS=0
FAILED=0

for i in "${!FUNC_LABELS[@]}"; do
    label="${FUNC_LABELS[$i]}"
    disasm_file="$EXPORT_DIR/${DISASM_FILES[$label]}"
    output_file="$OUTPUT_DIR/${label}.json"
    idx=$((i + 1))

    printf "[%d/%d] Analyzing %s... " "$idx" "$TOTAL" "$label"
    start_ts=$(date +%s)

    # 检查输入文件
    if [[ ! -f "$disasm_file" ]]; then
        echo "SKIP (disasm file not found: ${DISASM_FILES[$label]})"
        echo "[SKIP] $label: disasm file not found" >> "$PROGRESS_LOG"
        ((FAILED++)) || true
        continue
    fi

    # 如果已经存在有效输出，跳过（避免重复计费）
    if [[ -f "$output_file" ]] && python3 -c "import json,sys; json.load(open('$output_file'))" 2>/dev/null; then
        echo "CACHED (already exists)"
        echo "[CACHED] $label" >> "$PROGRESS_LOG"
        ((SUCCESS++)) || true
        continue
    fi

    disasm_content="$(cat "$disasm_file")"

    # 构建 prompt（写到临时文件避免命令行长度限制）
    prompt_file="$(mktemp /tmp/re_prompt_b_XXXXXX.txt)"
    trap 'rm -f "$prompt_file"' RETURN

    cat > "$prompt_file" << 'PROMPT_EOF'
你是一个 x86-64 逆向工程专家。分析以下 Ghidra 导出的反汇编指令。

目标：提取该函数向内存写入的所有 store 操作，推断它构建的数据结构布局。

对于每一个写入操作（MOV [reg+offset], value 等），输出：
- offset: 目标偏移（相对于 buffer 基址，十六进制字符串，如 "0x00"）
- size: 写入大小（1/2/4/8 字节，整数）
- name: 推断的字段名称（如果无法推断写 "unknown_XX"，XX 为十六进制偏移）
- value: 写入的值（立即数写具体值如 "0x0001"，寄存器写入写来源如 "from RDX"）
- evidence: 完整的汇编指令及其地址（原文引用，如 "0x14014c62a  MOV word ptr [RDI + 0x0], 0x0"）

输出格式为 JSON，不要输出任何其他内容，只输出 JSON：
{
  "function": "函数名",
  "address": "0x地址",
  "total_payload_size": 数字或null,
  "fields": [
    {
      "offset": "0x00",
      "size": 2,
      "name": "tag",
      "value": "0x0000",
      "evidence": "0x14014c62a  MOV word ptr [RDI + 0x0], 0x0"
    }
  ],
  "notes": "任何不确定的推断"
}

重要规则：
1. 只报告你能找到对应 MOV/STORE 指令的字段，不要推断不存在的字段
2. 区分立即数写入（常量，精确报告值）和寄存器写入（变量，描述来源寄存器）
3. 注意 LEA vs MOV 的区别 — LEA 是地址计算不是写入，不要报告 LEA
4. 如果某个写入的目标偏移不确定（间接寻址、通过计算得到的偏移），在 name 中标注 "indirect" 或 "computed"
5. 注意操作数大小前缀: byte ptr=1字节, word ptr=2字节, dword ptr=4字节, qword ptr=8字节
6. total_payload_size: 如果能从 sub/add RSP 或 alloc 调用推断 buffer 大小则填数字，否则填 null
7. 输出必须是合法 JSON，不包含注释

=== 反汇编 ===
PROMPT_EOF

    # 追加反汇编内容到 prompt 文件
    echo "$disasm_content" >> "$prompt_file"

    # 调用 codex exec，使用 gpt-5.2-codex（与 Agent A 的 5.3 不同，降低相关幻觉）
    codex_exit=0
    codex exec \
        --dangerously-bypass-approvals-and-sandbox \
        --ephemeral \
        -m gpt-5.2-codex \
        -o "$output_file" \
        - < "$prompt_file" 2>>"$PROGRESS_LOG" || codex_exit=$?

    rm -f "$prompt_file"
    trap - RETURN

    end_ts=$(date +%s)
    elapsed=$((end_ts - start_ts))

    if [[ $codex_exit -ne 0 ]]; then
        echo "FAILED (codex exit=$codex_exit, ${elapsed}s)"
        echo "[FAILED] $label: codex exit=$codex_exit" >> "$PROGRESS_LOG"
        ((FAILED++)) || true
        sleep 2
        continue
    fi

    # 验证输出是否为合法 JSON
    if [[ ! -f "$output_file" ]]; then
        echo "FAILED (no output file)"
        echo "[FAILED] $label: no output file generated" >> "$PROGRESS_LOG"
        ((FAILED++)) || true
        sleep 2
        continue
    fi

    if ! python3 -c "import json,sys; data=json.load(open('$output_file')); assert 'fields' in data" 2>/dev/null; then
        echo "INVALID JSON (${elapsed}s)"
        echo "[INVALID_JSON] $label: output not valid JSON with 'fields' key" >> "$PROGRESS_LOG"
        mv "$output_file" "${output_file%.json}_invalid.txt"
        ((FAILED++)) || true
        sleep 2
        continue
    fi

    field_count=$(python3 -c "import json; d=json.load(open('$output_file')); print(len(d['fields']))" 2>/dev/null || echo "?")
    echo "OK (${elapsed}s, ${field_count} fields)"
    echo "[OK] $label: ${field_count} fields, ${elapsed}s" >> "$PROGRESS_LOG"
    ((SUCCESS++)) || true

    # rate limit 保护
    [[ $idx -lt $TOTAL ]] && sleep 2
done

echo ""
echo "=== 完成 ===" | tee -a "$PROGRESS_LOG"
echo "成功: $SUCCESS / $TOTAL, 失败: $FAILED" | tee -a "$PROGRESS_LOG"
echo "输出目录: $OUTPUT_DIR"
