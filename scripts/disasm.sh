#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

# ==============================================================================
# 主函數: 互動式反組譯工具
# ==============================================================================
# 功能:
#   1. 使用 fzf 選擇目標執行檔
#   2. 使用 nm 列出符號,並用 fzf 選擇函數
#   3. 使用 objdump 反組譯特定函數 (或整個執行檔)
#   4. 使用 less 分頁顯示結果
# ==============================================================================
main() {
	# 檢查必要工具
	log_info "檢查必要工具"
	check_command "$FZF"
	check_command "$OBJDUMP"
	check_command "$NM"

	# 檢查 BUILD_DIR 存在
	check_dir "$BUILD_DIR"

	# 步驟 1: 選擇執行檔
	log_info "搜尋可執行檔"
	local target
	target=$(select_executable "選擇反組譯目標")

	log_success "已選擇: $target"

	# 步驟 2: 選擇函數
	log_info "提取符號表"
	local func
	func=$("$NM" $NM_FLAGS "$target" |
		grep -E ' [TtWw] ' |
		awk '{$1=$2=""; print substr($0,3)}' |
		"$FZF" $FZF_FLAGS --prompt="選擇函數> ") || {
		# 未選擇函數,顯示完整反組譯
		log_warning "未選擇特定函數,將顯示完整反組譯"
		log_info "執行 objdump: $target"

		"$OBJDUMP" $OBJDUMP_FLAGS "$target" | less -R
		return 0
	}

	# 步驟 3: 反組譯特定函數
	log_success "已選擇函數: $func"
	log_info "執行反組譯"

	"$OBJDUMP" $OBJDUMP_FLAGS "$target" |
		awk -v fname="$func" '
            index($0, "<"fname">:") {flag=1}
            flag {print}
            /^[0-9a-f]+[[:space:]]+</ && flag && !index($0, "<"fname">:") {exit}
        ' |
		less -R
}

main "$@"
