#!/usr/bin/env bash
# re_analyze.sh — Agent A: 从反编译 C 伪代码分析函数，产出字节级字段映射 JSON
#
# 用法:
#   ./tools/re_analyze.sh                    # 处理 re_task_list.txt 中所有函数
#   ./tools/re_analyze.sh nicUniCmdBssInfoTagBasic  # 只处理指定函数
#
# 输出: tmp/re_results/agent_a/<func_name>.json

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TASK_LIST="$SCRIPT_DIR/re_task_list.txt"
EXPORT_DIR="$REPO_ROOT/tmp/ghidra_exports"
OUTPUT_DIR="$REPO_ROOT/tmp/re_results/agent_a"
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
echo "=== Agent A (反编译分析) — $TOTAL 个函数 ===" | tee -a "$PROGRESS_LOG"
echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$PROGRESS_LOG"
echo "" | tee -a "$PROGRESS_LOG"

SUCCESS=0
FAILED=0

for i in "${!FUNC_LABELS[@]}"; do
    label="${FUNC_LABELS[$i]}"
    decomp_file="$EXPORT_DIR/${DECOMP_FILES[$label]}"
    output_file="$OUTPUT_DIR/${label}.json"
    idx=$((i + 1))

    printf "[%d/%d] Analyzing %s... " "$idx" "$TOTAL" "$label"
    start_ts=$(date +%s)

    # 检查输入文件
    if [[ ! -f "$decomp_file" ]]; then
        echo "SKIP (decomp file not found: ${DECOMP_FILES[$label]})"
        echo "[SKIP] $label: decomp file not found" >> "$PROGRESS_LOG"
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

    decomp_content="$(cat "$decomp_file")"

    # 构建 prompt（写到临时文件避免命令行长度限制）
    prompt_file="$(mktemp /tmp/re_prompt_a_XXXXXX.txt)"
    trap 'rm -f "$prompt_file"' RETURN

    cat > "$prompt_file" << 'PROMPT_EOF'
你是一个 Windows 驱动逆向工程专家。分析以下 Ghidra 反编译的 C 伪代码。

目标：提取该函数构建的 MCU 命令 payload 的字节级字段映射。

对于函数中写入 buffer/结构体的每一个字段，输出：
- offset: 相对于 payload 起始的字节偏移（十六进制字符串，如 "0x00"）
- size: 字段大小（字节数，整数）
- name: 字段名称（从变量名/上下文推断）
- value: 写入的值（常量则写具体值如 "0x0001"，变量则写来源描述如 "bss_index from param"）
- evidence: 反编译中对应的代码行（原文引用）

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
      "evidence": "*(ushort *)(buf + 0) = 0;"
    }
  ],
  "notes": "任何不确定的推断"
}

重要规则：
1. 只报告你能在代码中找到证据的字段
2. 如果某个偏移的含义不确定，在 notes 中标注，不要猜测字段名
3. 常量值必须精确，不要四舍五入
4. 如果函数调用了子函数来填充部分 buffer，在 fields 中用 "filled_by_subfunc" 作为 name，value 填子函数地址
5. total_payload_size: 如果能从代码中确定总大小则填数字，否则填 null
6. 输出必须是合法 JSON，不包含注释

=== 反编译代码 ===
PROMPT_EOF

    # 追加反编译内容到 prompt 文件
    echo "$decomp_content" >> "$prompt_file"

    # 调用 codex exec，从 stdin 读取 prompt（使用 config.toml 中配置的默认模型）
    codex_exit=0
    codex exec \
        --dangerously-bypass-approvals-and-sandbox \
        --ephemeral \
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
        # 保留输出文件以供调试
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
