#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

# ==============================================================================
# 主函數: 互動式 Perf 分析工具
# ==============================================================================
# 功能:
#   1. 使用 fzf 選擇目標執行檔
#   2. 使用 fzf 選擇分析模式:
#      - stat: 快速統計 (Cache/Branch/TLB Miss Rate, IPC)
#      - record: 記錄 Hot Path (perf.data)
#      - report: 查看 perf.data 的分析報告
# ==============================================================================

# ==============================================================================
# 模式: perf stat (快速統計)
# ==============================================================================
# 針對 HFT 場景最重要的事件:
#   - Cache Misses (L1/LLC)
#   - TLB Misses (虛擬記憶體轉換)
#   - Branch Misses (分支預測)
#   - IPC (Instructions Per Cycle)
# ==============================================================================
mode_stat() {
  local target="$1"
  local args="${2:-}"

  log_info "執行 perf stat"
  log_info "目標: $target"
  log_info "事件: Cache/TLB/Branch + IPC"

  echo ""
  echo -e "${C_YELLOW}提示: 如果需要傳遞參數給執行檔,請在選擇模式前先設定 PERF_ARGS 環境變數${C_RESET}"
  echo -e "${C_YELLOW}範例: PERF_ARGS=\"--config myconfig.json\" ./scripts/perf.sh${C_RESET}"
  echo ""

  # 執行 perf stat
  echo ""
  set +e # 暫時允許錯誤 (因為 perf stat 會返回被執行程式的 exit code)
  "$PERF" stat -d -d -d "$target" $args
  local exit_code=$?
  set -e

  if [ $exit_code -ne 0 ]; then
    log_warning "perf stat 完成，但目標程式返回非零退出碼 ($exit_code)"
  fi

  # 顯示效能判斷指南
  show_perf_guide

  log_success "perf stat 完成"
}

# ==============================================================================
# 模式: perf record (記錄 Hot Path)
# ==============================================================================
# 使用高頻率採樣 (9999 Hz) + DWARF call graph
# 輸出: perf.data
# ==============================================================================
mode_record() {
  local target="$1"
  local args="${2:-}"
  local output_file="${PERF_DATA_FILE:-perf.data}"

  log_info "執行 perf record"
  log_info "目標: $target"
  log_info "採樣頻率: 9999 Hz (DWARF call graph)"
  log_info "輸出檔案: $output_file"

  echo ""
  echo -e "${C_YELLOW}提示: 如果需要傳遞參數給執行檔,請設定 PERF_ARGS 環境變數${C_RESET}"
  echo ""

  # 執行 perf record
  if ! "$PERF" record $PERF_RECORD_FLAGS -o "$output_file" "$target" $args; then
    die "perf record 執行失敗"
  fi

  log_success "perf record 完成"
  log_info "分析檔案: $output_file"
  log_info "下一步: 執行 './scripts/perf.sh' 並選擇 'report' 模式查看結果"
}

# ==============================================================================
# 模式: perf report (查看 Hot Path)
# ==============================================================================
# 讀取 perf.data 並顯示 Hot Spot 報告
# ==============================================================================
mode_report() {
  local input_file="${PERF_DATA_FILE:-perf.data}"

  log_info "執行 perf report"
  check_file "$input_file"

  log_info "讀取檔案: $input_file"
  log_info "只顯示佔比 >= 1% 的函數"

  echo ""

  # 執行 perf report
  if ! "$PERF" report -i "$input_file" $PERF_REPORT_FLAGS | less -R; then
    die "perf report 執行失敗"
  fi

  log_success "perf report 完成"
}

# ==============================================================================
# 主流程
# ==============================================================================
main() {
  # 檢查必要工具
  log_info "檢查必要工具"
  check_command "$PERF"
  check_command "$FZF"

  # 步驟 1: 選擇模式
  local mode
  mode=$(echo -e "stat\nrecord\nreport" |
    "$FZF" $FZF_FLAGS --prompt="選擇 perf 分析模式> " \
      --preview='case {} in
        stat) echo "快速統計 Cache/TLB/Branch Miss Rate + IPC" ;;
        record) echo "記錄執行檔的 Hot Path (輸出: perf.data)" ;;
        report) echo "查看 perf.data 的 Hot Spot 分析" ;;
      esac') || die "已取消"

  log_success "已選擇模式: $mode"

  # 步驟 2: 根據模式執行
  case "$mode" in
  stat | record)
    # 需要選擇執行檔
    local target
    target=$(select_executable "選擇要分析的執行檔")
    log_success "已選擇執行檔: $target"

    # 取得執行參數 (來自環境變數)
    local args="${PERF_ARGS:-}"

    if [ "$mode" = "stat" ]; then
      mode_stat "$target" "$args"
    else
      mode_record "$target" "$args"
    fi
    ;;

  report)
    mode_report
    ;;

  *)
    die "未知模式: $mode"
    ;;
  esac
}

# ==============================================================================
# 顯示 Perf 效能判斷指南
# ==============================================================================
show_perf_guide() {
  cat <<'EOF'

[HFT 效能指標速查]
-------------------------------------------------------
1. IPC (指令/週期)       : [優秀] > 2.0   | [差勁] < 1.0
2. Branch Misses        : [優秀] < 0.5%  | [差勁] > 2.0%
3. L1-D Cache Misses    : [優秀] < 1.0%  | [差勁] > 5.0%
4. LLC (L3) Misses      : [優秀] < 0.1%  | [差勁] > 1.0%
5. Context Switches     : [理想] 0 次    | [差勁] > 10 次
6. Frontend Stalls      : [差勁] > 15%   | (瓶頸: Fetch/Decode)
7. Backend Stalls       : [差勁] > 30%   | (瓶頸: Memory/Core)
8. Hybrid CPU (P/E-core): E-core cycles 應 < 5% (否則請綁核)
-------------------------------------------------------

EOF
}

main "$@"
