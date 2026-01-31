#!/usr/bin/env bash

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

# ==============================================================================
# 主函數: 程式碼覆蓋率分析
# ==============================================================================
# 功能:
#   1. 建置 Coverage 版本
#   2. 清理舊的覆蓋率資料
#   3. 執行測試並收集覆蓋率
#   4. 過濾測試與第三方程式碼
#   5. 生成終端與 HTML 報告
#   6. 啟動 HTTP server 供瀏覽器查看
# ==============================================================================
main() {
	local coverage_dir="${BUILD_DIR}/Debug"
	local html_dir="${coverage_dir}/coverage-html"

	# 檢查必要工具
	log_info "檢查必要工具"
	check_command "$CMAKE"
	check_command "$CTEST"
	check_command "$LCOV"
	check_command "$GENHTML"

	# 建置 Coverage 版本
	log_info "建置 Coverage 版本"
	if ! "$CMAKE" --preset coverage; then
		die "CMake 配置失敗 (preset: coverage)"
	fi
	log_success "CMake 配置完成"

	if ! "$CMAKE" --build --preset coverage; then
		die "建置失敗 (preset: coverage)"
	fi
	log_success "建置完成"

	# 清理舊資料
	log_info "清理舊的覆蓋率資料"
	find "$coverage_dir" -name "*.gcda" -delete 2>/dev/null || true
	rm -f "${coverage_dir}/coverage.info" "${coverage_dir}/coverage_filtered.info"
	rm -rf "$html_dir"
	log_success "清理完成"

	# 執行測試
	log_info "執行測試並收集覆蓋率資料"
	if ! "$CTEST" --preset coverage --output-on-failure; then
		die "測試執行失敗"
	fi
	log_success "測試執行完成"

	# 收集覆蓋率
	log_info "收集覆蓋率資料"
	if ! "$LCOV" --capture \
		--directory "$coverage_dir" \
		--output-file "${coverage_dir}/coverage.info" \
		--rc branch_coverage=1 \
		--ignore-errors mismatch,inconsistent,negative,unused,count; then
		die "lcov capture 失敗"
	fi
	log_success "覆蓋率資料收集完成"

	# 過濾
	log_info "過濾測試與第三方程式碼"
	if ! "$LCOV" --remove "${coverage_dir}/coverage.info" \
		'*/tests/*' '*/_deps/*' '*/build/*' '/usr/*' \
		--output-file "${coverage_dir}/coverage_filtered.info" \
		--rc branch_coverage=1 \
		--ignore-errors mismatch,inconsistent,negative,unused,count; then
		die "lcov filter 失敗"
	fi
	log_success "過濾完成"

	# 顯示終端報告
	echo ""
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	echo -e "${C_BOLD}${C_GREEN}程式碼覆蓋率報告 (摘要)${C_RESET}"
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	"$LCOV" --list "${coverage_dir}/coverage_filtered.info" --ignore-errors unused
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	echo ""

	# 生成 HTML 報告
	log_info "生成 HTML 報告"
	if ! "$GENHTML" "${coverage_dir}/coverage_filtered.info" \
		--output-directory "$html_dir" \
		--rc branch_coverage=1 \
		--demangle-cpp \
		--title "HFT Trading Engine - Coverage Report" \
		--legend \
		--ignore-errors source,unused; then
		die "genhtml 生成失敗"
	fi
	log_success "HTML 報告生成完成"

	log_success "Coverage 分析完成"
	log_info "原始資料: ${coverage_dir}/coverage_filtered.info"
	log_info "HTML 報告: ${html_dir}/index.html"
	echo ""

	# 詢問是否開啟 HTML 報告
	echo -e "${C_YELLOW}是否在瀏覽器中查看 HTML 報告? [Y/n]${C_RESET}"
	read -r response

	case "${response,,}" in # 轉換為小寫
	n | no)
		log_info "已跳過。手動查看方式:"
		echo -e "  ${C_BLUE}cd ${html_dir} && python3 -m http.server 8000${C_RESET}"
		return 0
		;;
	*)
		# 預設為 Yes
		serve_html_report "$html_dir"
		;;
	esac
}

# ==============================================================================
# 輔助函數: 使用 Python HTTP Server 提供 HTML 報告
# ==============================================================================
serve_html_report() {
	local html_dir="$1"
	local port="${COVERAGE_HTTP_PORT:-8000}"

	# 檢查 Python 是否可用
	log_info "檢查 Python 環境"
	if ! command -v "$PYTHON" &>/dev/null; then
		log_error "找不到 $PYTHON,無法啟動 HTTP server"
		log_info "請手動開啟: file://${html_dir}/index.html"
		return 1
	fi

	# 檢查 port 是否被佔用
	check_command "$LSOF"
	if "$LSOF" -Pi :${port} -sTCP:LISTEN -t &>/dev/null; then
		log_warning "Port ${port} 已被佔用,嘗試使用隨機 port"
		port=0 # 讓系統自動分配
	fi

	log_info "啟動 HTTP Server..."
	echo -e "${C_BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RESET}"
	echo -e "${C_BOLD}Coverage HTML 報告已啟動${C_RESET}"
	echo ""
	echo -e "  URL: ${C_GREEN}http://localhost:${port}${C_RESET}"
	echo ""
	echo -e "${C_YELLOW}提示:${C_RESET}"
	echo "  - 按 Ctrl+C 停止 HTTP server"
	echo "  - 報告會自動在瀏覽器中打開"
	echo -e "${C_BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RESET}"
	echo ""

	# 切換到 HTML 目錄並啟動 server
	check_dir "$html_dir"
	cd "$html_dir" || die "無法進入目錄: $html_dir"

	# 在背景啟動 Python HTTP server
	"$PYTHON" -m http.server ${port} &>/dev/null &
	local server_pid=$!

	# 等待 server 啟動 (最多等 2 秒)
	local waited=0
	while [ $waited -lt 20 ]; do
		if "$LSOF" -Pi :${port} -sTCP:LISTEN -t &>/dev/null; then
			break
		fi
		sleep 0.1
		waited=$((waited + 1))
	done

	# 如果使用了隨機 port,獲取實際 port
	if [ "$port" = "0" ]; then
		port=$("$LSOF" -Pan -p $server_pid -i | grep LISTEN | awk '{print $9}' | cut -d: -f2)
	fi

	local url="http://localhost:${port}"

	# 在瀏覽器中打開
	if command -v xdg-open &>/dev/null; then
		xdg-open "$url" &>/dev/null
	elif command -v open &>/dev/null; then
		open "$url" &>/dev/null
	else
		log_warning "無法自動打開瀏覽器,請手動訪問: $url"
	fi

	# 等待用戶中斷 (Ctrl+C)
	trap "kill $server_pid 2>/dev/null; log_info 'HTTP server 已停止'; exit 0" INT TERM

	wait $server_pid
}

main "$@"
