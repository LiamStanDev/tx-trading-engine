#!/usr/bin/env bash

################################################################################
# run.sh - C++ 專案執行腳本
################################################################################
# 版本: 3.0.0
# 用途: 提供統一的專案命令入口，基於 CMakePresets.json
# 特性:
#   - 完全基於 CMake Presets（無回退邏輯）
#   - 自包含日誌系統
#   - 簡潔的命令集：build, test, bench, coverage, doc, clean
#
# 使用方式:
#   ./run.sh <command> <preset> [...options]
#
# 可用命令:
#   build <preset>           - 建置專案
#   test <preset>            - 執行測試
#   bench <preset>           - 執行效能測試
#   coverage                 - 代碼覆蓋率分析
#   doc [build|clean|open]   - API 文件生成
#   clean [preset|all]       - 清理建置目錄
#   list                     - 列出所有命令
#   help                     - 顯示幫助
#
# 範例:
#   ./run.sh build gcc-debug
#   ./run.sh test clang-debug verbose
#   ./run.sh bench gcc-release --filter=Price
#   ./run.sh coverage
################################################################################

set -euo pipefail

################################################################################
# 版本與常數
################################################################################
readonly SCRIPT_VERSION="3.0.0"
readonly SCRIPT_NAME="run.sh"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PRESETS_FILE="${SCRIPT_DIR}/CMakePresets.json"

# 預設 preset
readonly DEFAULT_PRESET="gcc-debug"
readonly BENCH_PRESET="gcc-release"
readonly COVERAGE_PRESET="coverage"

# 文件目錄
readonly DOCS_DIR="docs"

# 執行模式
DRY_RUN=false
VERBOSE=false

################################################################################
# 日誌系統
################################################################################

# 顏色定義
readonly LOG_COLOR_RED='\033[0;31m'
readonly LOG_COLOR_GREEN='\033[0;32m'
readonly LOG_COLOR_YELLOW='\033[1;33m'
readonly LOG_COLOR_BLUE='\033[0;34m'
readonly LOG_COLOR_MAGENTA='\033[0;35m'
readonly LOG_COLOR_CYAN='\033[0;36m'
readonly LOG_COLOR_GRAY='\033[0;90m'
readonly LOG_COLOR_NC='\033[0m'

# 日誌級別
readonly LOG_LEVEL_DEBUG=0
readonly LOG_LEVEL_INFO=1
readonly LOG_LEVEL_WARN=2
readonly LOG_LEVEL_ERROR=3

LOG_CURRENT_LEVEL=${LOG_CURRENT_LEVEL:-${LOG_LEVEL_INFO}}

# 時間戳
log_timestamp() {
	date '+%Y-%m-%d %H:%M:%S'
}

# 核心日誌函式
_log() {
	local level=$1
	local color=$2
	local prefix=$3
	shift 3
	local message="$*"

	if [[ ${level} -lt ${LOG_CURRENT_LEVEL} ]]; then
		return
	fi

	if [[ -t 1 ]]; then
		echo -e "${color}[$(log_timestamp)] [${prefix}]${LOG_COLOR_NC} ${message}"
	else
		echo "[$(log_timestamp)] [${prefix}] ${message}"
	fi
}

# 公開 API
log_debug() {
	_log ${LOG_LEVEL_DEBUG} "${LOG_COLOR_GRAY}" "DEBUG" "$@"
}

log_info() {
	_log ${LOG_LEVEL_INFO} "${LOG_COLOR_BLUE}" "INFO " "$@"
}

log_success() {
	_log ${LOG_LEVEL_INFO} "${LOG_COLOR_GREEN}" "OK   " "$@"
}

log_warn() {
	_log ${LOG_LEVEL_WARN} "${LOG_COLOR_YELLOW}" "WARN " "$@"
}

log_error() {
	_log ${LOG_LEVEL_ERROR} "${LOG_COLOR_RED}" "ERROR" "$@" >&2
}

log_fatal() {
	log_error "$@"
	log_error "程式異常終止"
	exit 1
}

# 分隔線
log_separator() {
	local char=${1:-=}
	local length=${2:-80}
	local color=${3:-${LOG_COLOR_CYAN}}

	printf "${color}%${length}s${LOG_COLOR_NC}\n" | tr ' ' "${char}"
}

log_header() {
	local title="$1"
	local color=${2:-${LOG_COLOR_CYAN}}

	echo ""
	log_separator "=" 80 "${color}"
	echo -e "${color}${title}${LOG_COLOR_NC}"
	log_separator "=" 80 "${color}"
}

################################################################################
# 工具函式
################################################################################

# 檢查 CMakePresets.json 是否存在（必須）
check_presets_required() {
	if [[ ! -f "${PRESETS_FILE}" ]]; then
		log_fatal "CMakePresets.json 不存在，此工具僅支援 CMake Presets"
	fi
}

# 取得 preset 的建置目錄
get_build_dir() {
	local preset="$1"
	echo "${SCRIPT_DIR}/build/${preset}"
}

# 取得 CPU 核心數
get_cpu_cores() {
	if command -v nproc &>/dev/null; then
		nproc
	else
		echo "4"
	fi
}

# 檢查命令是否存在
check_command() {
	local cmd="$1"
	if ! command -v "${cmd}" &>/dev/null; then
		log_fatal "必要命令不存在: ${cmd}"
	fi
}

# 顯示版本資訊
show_version() {
	echo "${SCRIPT_NAME} v${SCRIPT_VERSION}"
	echo "C++ 專案執行腳本（CMake Presets 專用）"
}

# 顯示使用說明
show_usage() {
	cat <<EOF
用法: ${SCRIPT_NAME} <command> [options]

可用命令:
  build <preset> [clean]
      建置專案（使用 CMakePresets.json）
      preset: gcc-debug, gcc-release, clang-debug, clang-release, etc.

  test <preset> [verbose] [filter]
      執行測試（使用 CMakePresets.json）

  bench <preset> [...args]
      執行效能測試（預設: gcc-release）

  coverage
      代碼覆蓋率分析（使用 coverage preset）

  doc [build|clean|open]
      生成 API 文件（需要 doxygen，預設: build）

  clean [preset|all]
      清理建置目錄

  list
      列出所有可用命令

  help
      顯示此幫助訊息

全域選項:
  --help, -h          顯示幫助
  --version, -v       顯示版本
  --dry-run           Dry-run 模式（不實際執行）

範例:
  ${SCRIPT_NAME} build gcc-debug
      # 使用 gcc-debug preset 建置

  ${SCRIPT_NAME} build clang-release clean
      # 清理後使用 clang-release preset 建置

  ${SCRIPT_NAME} test gcc-debug verbose
      # 使用 gcc-debug preset 執行測試（詳細模式）

  ${SCRIPT_NAME} bench gcc-release --filter=Price
      # 使用 gcc-release preset 執行 benchmark（過濾 Price）

  ${SCRIPT_NAME} coverage
      # 生成代碼覆蓋率報告

  ${SCRIPT_NAME} doc build
      # 生成 API 文件

  ${SCRIPT_NAME} clean all
      # 清理所有建置目錄

注意:
  - 此工具僅支援 CMakePresets.json，不支援傳統 CMake 配置
  - 所有 preset 名稱必須在 CMakePresets.json 中定義
  - 代碼格式化和靜態分析已整合到 pre-commit hook
EOF
}

################################################################################
# build 命令實作
################################################################################
cmd_build() {
	local preset="${1:-${DEFAULT_PRESET}}"
	shift || true

	local should_clean=false
	local cmake_args=()

	# 解析參數
	for arg in "$@"; do
		if [[ "${arg}" == "clean" ]]; then
			should_clean=true
		else
			cmake_args+=("${arg}")
		fi
	done

	local build_dir
	build_dir=$(get_build_dir "${preset}")

	log_header "建置系統" "${LOG_COLOR_GREEN}"
	log_info "Preset: ${preset}"
	log_info "建置目錄: ${build_dir}"
	log_info "CPU 核心數: $(get_cpu_cores)"

	if [[ "${DRY_RUN}" == true ]]; then
		log_info "[DRY-RUN] 將執行建置: ${preset}"
		if [[ "${should_clean}" == true ]]; then
			log_info "[DRY-RUN] 將清理: ${build_dir}"
		fi
		return
	fi

	# 檢查必要命令
	check_command cmake

	# 清理（如果需要）
	if [[ "${should_clean}" == true ]]; then
		log_warn "清除舊建置目錄: ${build_dir}"
		rm -rf "${build_dir}"
	fi

	# 執行 CMake 配置
	log_info "執行 CMake 配置..."
	local configure_cmd=(cmake --preset "${preset}")

	if [[ ${#cmake_args[@]} -gt 0 ]]; then
		configure_cmd+=("${cmake_args[@]}")
	fi

	"${configure_cmd[@]}" || log_fatal "CMake 配置失敗（Preset: ${preset}）"

	# 執行建置
	log_info "開始編譯（使用 $(get_cpu_cores) 個 CPU 核心）..."
	cmake --build --preset "${preset}" || log_fatal "編譯失敗"

	# 建置成功
	log_separator "=" 80 "${LOG_COLOR_GREEN}"
	log_success "建置成功！"
	log_separator "=" 80 "${LOG_COLOR_GREEN}"
	log_info "建置目錄: ${build_dir}"
	log_info "執行測試: ${SCRIPT_NAME} test ${preset}"
	log_info "執行性能測試: ${SCRIPT_NAME} bench ${preset}"
	echo ""
}

################################################################################
# test 命令實作
################################################################################
cmd_test() {
	local preset="${1:-${DEFAULT_PRESET}}"
	shift || true

	local test_verbose=false
	local test_filter=""

	# 解析參數
	for arg in "$@"; do
		if [[ "${arg}" == "verbose" ]] || [[ "${arg}" == "-v" ]]; then
			test_verbose=true
		else
			test_filter="${arg}"
		fi
	done

	local build_dir
	build_dir=$(get_build_dir "${preset}")

	# 檢查建置目錄是否存在
	if [[ ! -d "${build_dir}" ]]; then
		log_error "建置目錄不存在: ${build_dir}"
		log_info "請先執行: ${SCRIPT_NAME} build ${preset}"
		exit 1
	fi

	log_header "單元測試" "${LOG_COLOR_BLUE}"
	log_info "Preset: ${preset}"
	log_info "建置目錄: ${build_dir}"
	if [[ -n "${test_filter}" ]]; then
		log_info "測試過濾: ${test_filter}"
	fi

	if [[ "${DRY_RUN}" == true ]]; then
		log_info "[DRY-RUN] 將執行測試: ${preset}"
		return
	fi

	# 檢查必要命令
	check_command ctest

	# 執行 CTest
	log_separator "-" 80
	log_info "執行 CTest..."
	log_separator "-" 80
	echo ""

	if [[ -z "${test_filter}" ]] && [[ "${test_verbose}" == false ]]; then
		# 使用 Preset（無自訂參數時）
		ctest --preset "${preset}" || log_warn "CTest 執行失敗"
	else
		# 傳統 CTest（有自訂參數時）
		local ctest_cmd=(
			ctest
			--test-dir "${build_dir}"
			--output-on-failure
		)

		if [[ "${test_verbose}" == true ]]; then
			ctest_cmd+=(--verbose)
		fi

		if [[ -n "${test_filter}" ]]; then
			ctest_cmd+=(-R "${test_filter}")
		fi

		"${ctest_cmd[@]}" || log_warn "CTest 執行失敗"
	fi

	# 測試成功
	echo ""
	log_separator "=" 80 "${LOG_COLOR_GREEN}"
	log_success "測試完成！"
	log_separator "=" 80 "${LOG_COLOR_GREEN}"
	echo ""
}

################################################################################
# bench 命令實作
################################################################################
cmd_bench() {
	local preset="${1:-${BENCH_PRESET}}"
	shift || true

	local bench_args=("$@")
	local build_dir
	build_dir=$(get_build_dir "${preset}")

	# 檢查建置目錄是否存在
	if [[ ! -d "${build_dir}" ]]; then
		log_error "建置目錄不存在: ${build_dir}"
		log_info "請先執行: ${SCRIPT_NAME} build ${preset}"
		exit 1
	fi

	log_header "性能測試" "${LOG_COLOR_MAGENTA}"
	log_info "Preset: ${preset}"
	log_info "建置目錄: ${build_dir}"

	# 顯示系統資訊
	log_separator "-" 80
	log_info "系統資訊:"
	if command -v lscpu &>/dev/null; then
		log_info "  CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
	fi
	log_info "  核心數: $(get_cpu_cores)"
	if command -v free &>/dev/null; then
		log_info "  記憶體: $(free -h | awk '/^Mem:/ {print $2}')"
	fi
	log_info "  作業系統: $(uname -s) $(uname -r)"
	log_separator "-" 80
	echo ""

	if [[ "${DRY_RUN}" == true ]]; then
		log_info "[DRY-RUN] 將執行性能測試: ${preset}"
		return
	fi

	# 尋找 benchmark 執行檔
	local bench_executables
	bench_executables=$(find "${build_dir}" -type f \
		\( -name "*bench*" -o -name "*benchmark*" \) \
		-executable 2>/dev/null || true)

	if [[ -z "${bench_executables}" ]]; then
		log_warn "在 ${build_dir} 中找不到 benchmark 執行檔"
		log_info "Benchmark 檔案命名規則: *bench* 或 *benchmark*"
		exit 0
	fi

	# 執行 benchmark
	log_info "開始執行性能測試..."
	echo ""

	for bench_exe in ${bench_executables}; do
		log_separator "=" 80 "${LOG_COLOR_MAGENTA}"
		log_info "執行: ${bench_exe}"
		log_separator "=" 80 "${LOG_COLOR_MAGENTA}"
		echo ""

		"${bench_exe}" --benchmark_color=true "${bench_args[@]}" ||
			log_warn "性能測試執行異常: ${bench_exe}"
		echo ""
	done

	# 完成
	log_separator "=" 80 "${LOG_COLOR_GREEN}"
	log_success "性能測試完成！"
	log_separator "=" 80 "${LOG_COLOR_GREEN}"
	echo ""
}

################################################################################
# coverage 命令實作
################################################################################
cmd_coverage() {
	local preset="${COVERAGE_PRESET}"
	local build_dir
	build_dir=$(get_build_dir "${preset}")

	log_header "代碼覆蓋率分析" "${LOG_COLOR_MAGENTA}"
	log_info "Preset: ${preset}"
	log_info "建置目錄: ${build_dir}"

	# 檢查必要工具
	if ! command -v lcov &>/dev/null; then
		log_fatal "lcov 未安裝，請先安裝：sudo apt install lcov"
	fi

	if [[ "${DRY_RUN}" == true ]]; then
		log_info "[DRY-RUN] 將執行覆蓋率分析"
		return
	fi

	# 清理舊的覆蓋率資料
	if [[ -d "${build_dir}" ]]; then
		log_info "清理舊的覆蓋率資料..."
		find "${build_dir}" -name "*.gcda" -delete 2>/dev/null || true
	fi

	# 建置 coverage 版本
	log_separator "-" 80
	log_info "建置 coverage 版本..."
	log_separator "-" 80
	echo ""

	check_command cmake

	cmake --preset "${preset}" || log_fatal "CMake 配置失敗"
	cmake --build --preset "${preset}" || log_fatal "編譯失敗"

	# 執行測試
	echo ""
	log_separator "-" 80
	log_info "執行測試..."
	log_separator "-" 80
	echo ""

	ctest --test-dir "${build_dir}" --output-on-failure ||
		log_warn "部分測試失敗（但會繼續生成覆蓋率報告）"

	# 生成覆蓋率報告
	echo ""
	log_separator "-" 80
	log_info "生成覆蓋率報告..."
	log_separator "-" 80
	echo ""

	# 捕獲覆蓋率
	log_info "捕獲覆蓋率資料..."
	lcov --capture \
		--directory "${build_dir}" \
		--output-file "${build_dir}/coverage.info" \
		--exclude '/usr/*' \
		--exclude '*/_deps/*' \
		--exclude '*/tests/*' \
		--exclude '*/bench/*' \
		--rc branch_coverage=1 \
		--ignore-errors mismatch,inconsistent,deprecated,corrupt,negative,empty \
		2>&1 | grep -v "^lcov: WARNING:" || log_fatal "覆蓋率捕獲失敗"

	# 生成 HTML 報告
	if command -v genhtml &>/dev/null; then
		log_info "生成 HTML 報告..."

		genhtml "${build_dir}/coverage.info" \
			--output-directory "${build_dir}/coverage-html" \
			--branch-coverage \
			--title "TX Trading Engine Coverage Report" \
			--legend || log_fatal "genhtml 失敗"

		echo ""
		log_separator "=" 80 "${LOG_COLOR_GREEN}"
		log_success "覆蓋率報告生成完成！"
		log_separator "=" 80 "${LOG_COLOR_GREEN}"

		# 顯示摘要
		echo ""
		log_info "覆蓋率摘要："
		lcov --summary "${build_dir}/coverage.info" \
			--rc lcov_branch_coverage=1 2>&1 |
			grep -E "lines\.\.\.\.\.\.|functions\.\.\.\.|branches\.\.\.\.\."

		echo ""
		log_info "詳細報告："
		log_info "  HTML: ${build_dir}/coverage-html/index.html"
		log_info "  開啟方式: xdg-open ${build_dir}/coverage-html/index.html"
		echo ""
	else
		log_warn "genhtml 未安裝，無法生成 HTML 報告"
		log_info "只生成了 LCOV 資料檔: ${build_dir}/coverage.info"
	fi
}

################################################################################
# doc 命令實作
################################################################################
cmd_doc() {
	local action="${1:-build}" # build / clean / open

	if [[ "${action}" != "build" ]] && [[ "${action}" != "clean" ]] && [[ "${action}" != "open" ]]; then
		log_error "無效的操作: ${action}"
		log_info "有效操作: build, clean, open"
		exit 1
	fi

	local docs_dir="${SCRIPT_DIR}/${DOCS_DIR}"
	local doxyfile="${SCRIPT_DIR}/Doxyfile"

	# Clean 操作
	if [[ "${action}" == "clean" ]]; then
		log_header "清理文件目錄" "${LOG_COLOR_YELLOW}"

		if [[ "${DRY_RUN}" == true ]]; then
			log_info "[DRY-RUN] 將刪除: ${docs_dir}/"
			return
		fi

		if [[ -d "${docs_dir}" ]]; then
			log_warn "刪除文件目錄: ${docs_dir}/"
			rm -rf "${docs_dir}"
			log_success "已刪除: ${docs_dir}/"
		else
			log_info "文件目錄不存在: ${docs_dir}/"
		fi
		return
	fi

	# Open 操作
	if [[ "${action}" == "open" ]]; then
		local html_index="${docs_dir}/html/index.html"

		if [[ ! -f "${html_index}" ]]; then
			log_error "找不到文件: ${html_index}"
			log_info "請先執行: ${SCRIPT_NAME} doc build"
			exit 1
		fi

		log_info "開啟文件: ${html_index}"

		if command -v xdg-open &>/dev/null; then
			xdg-open "${html_index}" &>/dev/null &
			log_success "已開啟文件（使用預設瀏覽器）"
		elif command -v open &>/dev/null; then
			open "${html_index}" &>/dev/null &
			log_success "已開啟文件（使用預設瀏覽器）"
		else
			log_warn "無法自動開啟，請手動開啟: ${html_index}"
		fi
		return
	fi

	# Build 操作
	log_header "生成 API 文件" "${LOG_COLOR_CYAN}"
	log_info "輸出目錄: ${docs_dir}"

	# 檢查 Doxygen 是否存在
	if ! command -v doxygen &>/dev/null; then
		log_fatal "doxygen 未安裝，請先安裝：sudo apt install doxygen graphviz"
	fi

	if [[ "${DRY_RUN}" == true ]]; then
		log_info "[DRY-RUN] 將生成 API 文件"
		return
	fi

	# 檢查 Doxyfile 是否存在
	if [[ ! -f "${doxyfile}" ]]; then
		log_warn "Doxyfile 不存在，正在生成預設配置..."

		# 生成預設 Doxyfile
		doxygen -g "${doxyfile}" &>/dev/null || log_fatal "生成 Doxyfile 失敗"

		# 自訂配置
		log_info "自訂 Doxyfile 配置..."

		sed -i.bak \
			-e "s|^PROJECT_NAME.*|PROJECT_NAME           = \"TX Trading Engine\"|" \
			-e "s|^OUTPUT_DIRECTORY.*|OUTPUT_DIRECTORY       = ${DOCS_DIR}|" \
			-e "s|^INPUT .*|INPUT                  = tx-common/include tx-common/src|" \
			-e "s|^RECURSIVE.*|RECURSIVE              = YES|" \
			-e "s|^EXTRACT_ALL.*|EXTRACT_ALL            = YES|" \
			-e "s|^EXTRACT_PRIVATE.*|EXTRACT_PRIVATE        = NO|" \
			-e "s|^EXTRACT_STATIC.*|EXTRACT_STATIC         = YES|" \
			-e "s|^GENERATE_LATEX.*|GENERATE_LATEX         = NO|" \
			-e "s|^HAVE_DOT.*|HAVE_DOT               = YES|" \
			-e "s|^UML_LOOK.*|UML_LOOK               = YES|" \
			-e "s|^CALL_GRAPH.*|CALL_GRAPH             = YES|" \
			-e "s|^CALLER_GRAPH.*|CALLER_GRAPH           = YES|" \
			"${doxyfile}"

		rm -f "${doxyfile}.bak"
		log_success "已生成 Doxyfile 配置檔"
	fi

	# 執行 Doxygen
	echo ""
	log_separator "-" 80
	log_info "執行 Doxygen..."
	log_separator "-" 80
	echo ""

	if doxygen "${doxyfile}"; then
		echo ""
		log_separator "=" 80 "${LOG_COLOR_GREEN}"
		log_success "API 文件生成完成！"
		log_separator "=" 80 "${LOG_COLOR_GREEN}"

		echo ""
		log_info "文件位置："
		log_info "  HTML: ${docs_dir}/html/index.html"
		echo ""
		log_info "開啟文件："
		log_info "  ${SCRIPT_NAME} doc open"
		log_info "  xdg-open ${docs_dir}/html/index.html"
		echo ""
	else
		log_fatal "Doxygen 執行失敗"
	fi
}

################################################################################
# clean 命令實作
################################################################################
cmd_clean() {
	local target="${1:-all}"
	local build_dir="${SCRIPT_DIR}/build"

	if [[ "${target}" == "all" ]] || [[ -z "${target}" ]]; then
		if [[ "${DRY_RUN}" == true ]]; then
			log_info "[DRY-RUN] 將刪除: ${build_dir}/"
			return
		fi

		if [[ -d "${build_dir}" ]]; then
			log_warn "清理所有建置目錄: ${build_dir}/"
			rm -rf "${build_dir}"
			log_success "已刪除: ${build_dir}/"
		else
			log_info "建置目錄不存在: ${build_dir}/"
		fi
	else
		local target_dir
		target_dir=$(get_build_dir "${target}")

		if [[ "${DRY_RUN}" == true ]]; then
			log_info "[DRY-RUN] 將刪除: ${target_dir}/"
			return
		fi

		if [[ -d "${target_dir}" ]]; then
			log_warn "清理建置目錄: ${target_dir}/"
			rm -rf "${target_dir}"
			log_success "已刪除: ${target_dir}/"
		else
			log_info "建置目錄不存在: ${target_dir}/"
		fi
	fi
}

################################################################################
# list 命令實作
################################################################################
cmd_list() {
	log_header "可用命令" "${LOG_COLOR_CYAN}"
	echo "核心命令:"
	echo "  - build      建置專案（使用 CMakePresets）"
	echo "  - test       執行測試（使用 CMakePresets）"
	echo "  - bench      執行效能測試"
	echo "  - coverage   代碼覆蓋率分析"
	echo "  - doc        生成 API 文件"
	echo "  - clean      清理建置目錄"
	echo ""
	echo "輔助命令:"
	echo "  - list       列出所有命令"
	echo "  - help       顯示幫助"
	echo ""
	log_info "執行 '${SCRIPT_NAME} help' 查看詳細說明"
	echo ""
}

################################################################################
# 主程式入口
################################################################################
main() {
	# 檢查 CMakePresets.json（必須）
	check_presets_required

	# 處理 flags
	while [[ $# -gt 0 ]]; do
		case "$1" in
		--help | -h)
			show_usage
			exit 0
			;;
		--version | -v)
			show_version
			exit 0
			;;
		--dry-run)
			DRY_RUN=true
			shift
			;;
		--verbose)
			VERBOSE=true
			LOG_CURRENT_LEVEL=${LOG_LEVEL_DEBUG}
			shift
			;;
		*)
			break
			;;
		esac
	done

	# 檢查命令
	local command="${1:-help}"

	if [[ -z "${command}" ]] || [[ "${command}" == "help" ]]; then
		show_usage
		exit 0
	fi

	if [[ "${command}" == "list" ]]; then
		cmd_list
		exit 0
	fi

	shift

	# 處理命令
	case "${command}" in
	build)
		cmd_build "$@"
		;;

	test)
		cmd_test "$@"
		;;

	bench)
		cmd_bench "$@"
		;;

	coverage)
		cmd_coverage
		;;

	doc)
		cmd_doc "$@"
		;;

	clean)
		cmd_clean "$@"
		;;

	*)
		log_error "未知命令: ${command}"
		echo ""
		log_info "執行 '${SCRIPT_NAME} list' 查看可用命令"
		log_info "執行 '${SCRIPT_NAME} help' 查看詳細說明"
		exit 1
		;;
	esac
}

# 執行主程式
main "$@"
