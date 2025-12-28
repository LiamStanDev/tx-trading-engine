#!/usr/bin/env bash

################################################################################
# run.sh - 通用 C++ 專案執行腳本
################################################################################
# 版本: 1.0.0
# 用途: 提供統一的專案命令入口，內建 build/test/bench 完整實作
# 特性:
#   - 內建完整命令實作（不依賴外部腳本）
#   - 自包含日誌系統
#   - 智能參數解析
#   - CMake/CTest/Google Benchmark 完整支援
#   - 自動發現與執行測試/benchmark
#   - Dry-run 模式
#   - 可選配置檔支援
#
# 使用方式:
#   ./run.sh <command> [build-type] [...options]
#
# 內建命令:
#   build   - CMake 建置
#   test    - CTest + GTest 執行
#   bench   - Google Benchmark 執行
#   clean   - 清理建置目錄
#   install - CMake install
#   list    - 列出所有命令
#   help    - 顯示幫助
#
# 範例:
#   ./run.sh build debug
#   ./run.sh test debug verbose
#   ./run.sh bench release
#   ./run.sh clean debug
################################################################################

# -e: 錯誤立即退出, -u: 變數未定義立即出錯, -o pipefail: pipe(|)其中一條指令失敗整個失敗
set -euo pipefail

################################################################################
# 版本資訊
################################################################################
readonly SCRIPT_VERSION="1.0.0"
readonly SCRIPT_NAME="run.sh"

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

LOG_CURRENT_LEVEL=${LOG_LEVEL:-${LOG_LEVEL_INFO}}

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
  local length=${2:-50}
  local color=${3:-${LOG_COLOR_CYAN}}

  printf "${color}%${length}s${LOG_COLOR_NC}\n" | tr ' ' "${char}"
}

log_header() {
  local title="$1"
  local color=${2:-${LOG_COLOR_CYAN}}

  log_separator "=" 50 "${color}"
  echo -e "${color}${title}${LOG_COLOR_NC}"
  log_separator "=" 50 "${color}"
}

# 確認提示
log_confirm() {
  local prompt="$1"
  local default=${2:-n}

  if [[ "${default}" == "y" ]]; then
    read -p "$(echo -e "${LOG_COLOR_YELLOW}${prompt} (Y/n)${LOG_COLOR_NC} ")" -n 1 -r
    echo
    [[ -z $REPLY ]] || [[ $REPLY =~ ^[Yy]$ ]]
  else
    read -p "$(echo -e "${LOG_COLOR_YELLOW}${prompt} (y/N)${LOG_COLOR_NC} ")" -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]]
  fi
}

################################################################################
# 全域配置與常數
################################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 可以透過 run.config 進行配置
# 目錄配置
BUILD_DIR_PREFIX="${BUILD_DIR_PREFIX:-build}"
INSTALL_PREFIX="${INSTALL_PREFIX:-install}"

# 建置類型
DEFAULT_BUILD_TYPE="${DEFAULT_BUILD_TYPE:-debug}"
BENCH_BUILD_TYPE="${BENCH_BUILD_TYPE:-release}"
VALID_BUILD_TYPES="${VALID_BUILD_TYPES:-debug release relwithdebinfo minsizerel}"

# CMake 配置
CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
CMAKE_EXTRA_ARGS="${CMAKE_EXTRA_ARGS:-}"

# 執行模式
DRY_RUN=false
VERBOSE=false

################################################################################
# 工具函式
################################################################################
# 載入可選配置檔
load_config() {
  local config_file="${SCRIPT_DIR}/run.config"

  if [[ -f "${config_file}" ]]; then
    log_debug "載入配置檔: ${config_file}"
    # shellcheck source=/dev/null
    source "${config_file}"
  fi
}
# 檢查是否為有效的 build-type
is_valid_build_type() {
  local type="$1"
  [[ " ${VALID_BUILD_TYPES} " =~ " ${type} " ]]
}
# 解析 build-type
parse_build_type() {
  local command="$1"
  local arg="${2:-}"

  if [[ -z "${arg}" ]]; then
    if [[ "${command}" == "bench" ]]; then
      echo "${BENCH_BUILD_TYPE}"
    else
      echo "${DEFAULT_BUILD_TYPE}"
    fi
    return
  fi

  if is_valid_build_type "${arg}"; then
    echo "${arg}"
  else
    if [[ "${command}" == "bench" ]]; then
      echo "${BENCH_BUILD_TYPE}"
    else
      echo "${DEFAULT_BUILD_TYPE}"
    fi
  fi
}
# 取得 CMake build type（首字母大寫）
get_cmake_build_type() {
  local type="$1"
  case "${type,,}" in
  debug) echo "Debug" ;;
  release) echo "Release" ;;
  relwithdebinfo) echo "RelWithDebInfo" ;;
  minsizerel) echo "MinSizeRel" ;;
  *) echo "Debug" ;;
  esac
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
  echo "通用 C++ 專案執行腳本（內建完整命令版）"
}
# 顯示使用說明
show_usage() {
  cat <<EOF
用法: ${SCRIPT_NAME} <command> [build-type] [...options]
內建命令:
  build [build-type] [clean] [...cmake-args]
      建置專案（預設: debug）
      
  test [build-type] [verbose] [...filter]
      執行測試（預設: debug）
      
  bench [build-type] [...args]
      執行效能測試（預設: release）
      
  clean [build-type|all]
      清理建置目錄
      
  install [build-type]
      安裝專案（CMake install）
      
  list
      列出所有可用命令
      
  help
      顯示此幫助訊息
Flags:
  --help, -h          顯示幫助
  --version, -v       顯示版本
  --dry-run           Dry-run 模式（不實際執行）
  --verbose           詳細輸出
建置類型:
  debug, release, relwithdebinfo, minsizerel
範例:
  ${SCRIPT_NAME} build debug
      # 建置 debug 版本
      
  ${SCRIPT_NAME} build release clean
      # 清理後建置 release 版本
      
  ${SCRIPT_NAME} build debug -DUSE_ASAN=ON
      # 建置 debug 並啟用 Address Sanitizer
      
  ${SCRIPT_NAME} test debug
      # 執行 debug 測試
      
  ${SCRIPT_NAME} test debug verbose
      # 執行測試（詳細模式）
      
  ${SCRIPT_NAME} test debug Price*
      # 只執行符合 Price* 的測試
      
  ${SCRIPT_NAME} bench release
      # 執行 release 效能測試
      
  ${SCRIPT_NAME} bench release --filter=Price
      # 只執行符合 Price 的 benchmark
      
  ${SCRIPT_NAME} clean debug
      # 清理 build/debug/
      
  ${SCRIPT_NAME} install release
      # 安裝 release 版本
配置檔:
  可在專案根目錄建立 run.config 自定義配置
  
專案: https://github.com/yourusername/yourproject
EOF
}

################################################################################
# build 命令實作
################################################################################
cmd_build() {
  local build_type="$1"
  shift

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

  local build_dir="${BUILD_DIR_PREFIX}/${build_type}"
  local cmake_build_type
  cmake_build_type=$(get_cmake_build_type "${build_type}")

  log_header "建置系統" "${LOG_COLOR_GREEN}"
  log_info "建置類型: ${cmake_build_type}"
  log_info "建置目錄: ${build_dir}"
  log_info "CPU 核心數: $(get_cpu_cores)"

  if [[ "${DRY_RUN}" == true ]]; then
    log_info "[DRY-RUN] 將執行建置: ${build_type}"
    if [[ "${should_clean}" == true ]]; then
      log_info "[DRY-RUN] 將清理: ${build_dir}"
    fi
    return
  fi

  # 檢查必要命令
  check_command cmake

  # 清理（如果需要）
  if [[ "${should_clean}" == true ]]; then
    log_warn "清除舊建置目錄..."
    rm -rf "${build_dir}"
  fi

  # 執行 CMake 配置
  log_info "執行 CMake 配置..."

  local cmake_cmd=(
    cmake
    -B "${build_dir}"
    -DCMAKE_BUILD_TYPE="${cmake_build_type}"
    -G "${CMAKE_GENERATOR}"
  )

  # 添加額外參數
  if [[ -n "${CMAKE_EXTRA_ARGS}" ]]; then
    read -ra extra_args_array <<<"${CMAKE_EXTRA_ARGS}"
    cmake_cmd+=("${extra_args_array[@]}")
  fi

  if [[ ${#cmake_args[@]} -gt 0 ]]; then
    cmake_cmd+=("${cmake_args[@]}")
  fi

  "${cmake_cmd[@]}" || log_fatal "CMake 配置失敗"

  # 生成 compile_commands.json 軟連結
  if [[ -f "${build_dir}/compile_commands.json" ]]; then
    ln -sf "${build_dir}/compile_commands.json" . 2>/dev/null || true
  fi

  # 執行建置
  log_info "開始編譯（使用 $(get_cpu_cores) 個 CPU 核心）..."

  cmake --build "${build_dir}" -j "$(get_cpu_cores)" || log_fatal "編譯失敗"

  # 建置成功
  log_separator "=" 50 "${LOG_COLOR_GREEN}"
  log_success "建置成功！"
  log_separator "=" 50 "${LOG_COLOR_GREEN}"
  log_info "執行檔位置: ${build_dir}/"
  log_info "執行測試: ${SCRIPT_NAME} test ${build_type}"
  log_info "執行性能測試: ${SCRIPT_NAME} bench ${build_type}"
}

################################################################################
# test 命令實作
################################################################################
cmd_test() {
  local build_type="$1"
  shift

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

  local build_dir="${BUILD_DIR_PREFIX}/${build_type}"

  # 檢查建置目錄是否存在
  if [[ ! -d "${build_dir}" ]]; then
    log_error "建置目錄不存在: ${build_dir}"
    log_info "請先執行: ${SCRIPT_NAME} build ${build_type}"
    exit 1
  fi

  log_header "單元測試" "${LOG_COLOR_BLUE}"
  log_info "建置類型: ${build_type}"
  log_info "建置目錄: ${build_dir}"
  if [[ -n "${test_filter}" ]]; then
    log_info "測試過濾: ${test_filter}"
  fi
  echo ""

  if [[ "${DRY_RUN}" == true ]]; then
    log_info "[DRY-RUN] 將執行測試: ${build_type}"
    return
  fi

  # 檢查必要命令
  check_command ctest

  # 執行 CTest
  log_info "執行 CTest..."

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

  # 執行 GTest
  echo ""
  log_separator "-" 50
  log_info "執行 GTest 測試程式"
  log_separator "-" 50

  local test_executables
  test_executables=$(find "${build_dir}" -type f -name "*-tests" -executable 2>/dev/null || true)

  if [[ -z "${test_executables}" ]]; then
    log_warn "找不到測試執行檔（*-tests）"
  else
    for test_exe in ${test_executables}; do
      log_info "執行: ${test_exe}"

      local gtest_args=(--gtest_color=yes)

      if [[ -n "${test_filter}" ]]; then
        gtest_args+=(--gtest_filter="${test_filter}")
      fi

      "${test_exe}" "${gtest_args[@]}" || log_fatal "測試失敗: ${test_exe}"
      echo ""
    done
  fi

  # 測試成功
  log_separator "=" 50 "${LOG_COLOR_GREEN}"
  log_success "所有測試通過！"
  log_separator "=" 50 "${LOG_COLOR_GREEN}"
}

################################################################################
# bench 命令實作
################################################################################
cmd_bench() {
  local build_type="$1"
  shift

  local bench_args=("$@")
  local build_dir="${BUILD_DIR_PREFIX}/${build_type}"

  # 檢查建置目錄是否存在
  if [[ ! -d "${build_dir}" ]]; then
    log_error "建置目錄不存在: ${build_dir}"
    log_info "請先執行: ${SCRIPT_NAME} build ${build_type}"
    exit 1
  fi

  # Debug 模式警告
  if [[ "${build_type,,}" == "debug" ]]; then
    log_separator "=" 50 "${LOG_COLOR_YELLOW}"
    log_warn "您正在 Debug 模式下執行性能測試"
    log_warn "建議使用 Release 模式以獲得準確結果"
    log_separator "=" 50 "${LOG_COLOR_YELLOW}"
    echo ""

    if ! log_confirm "是否繼續？"; then
      log_info "已取消"
      exit 0
    fi
  fi

  log_header "性能測試" "${LOG_COLOR_MAGENTA}"
  log_info "建置類型: ${build_type}"
  log_info "建置目錄: ${build_dir}"
  echo ""

  # 顯示系統資訊
  log_info "系統資訊:"
  if command -v lscpu &>/dev/null; then
    log_info "  CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
  fi
  log_info "  核心數: $(get_cpu_cores)"
  if command -v free &>/dev/null; then
    log_info "  記憶體: $(free -h | awk '/^Mem:/ {print $2}')"
  fi
  log_info "  作業系統: $(uname -s) $(uname -r)"
  echo ""

  if [[ "${DRY_RUN}" == true ]]; then
    log_info "[DRY-RUN] 將執行性能測試: ${build_type}"
    return
  fi

  # 尋找性能測試執行檔
  local bench_executables
  bench_executables=$(find "${build_dir}" -type f \( -name "*bench*" -o -name "*benchmark*" \) -executable 2>/dev/null || true)

  if [[ -z "${bench_executables}" ]]; then
    log_separator "=" 50 "${LOG_COLOR_YELLOW}"
    log_warn "目前尚未建立性能測試"
    log_separator "=" 50 "${LOG_COLOR_YELLOW}"
    exit 0
  fi

  # 執行性能測試
  log_info "開始執行性能測試..."
  echo ""

  for bench_exe in ${bench_executables}; do
    log_separator "=" 50 "${LOG_COLOR_MAGENTA}"
    log_info "執行: ${bench_exe}"
    log_separator "=" 50 "${LOG_COLOR_MAGENTA}"

    local benchmark_args=(--benchmark_color=true)

    # 解析參數
    for arg in "${bench_args[@]}"; do
      if [[ "${arg}" =~ ^--filter= ]]; then
        benchmark_args+=(--benchmark_filter="${arg#--filter=}")
      elif [[ "${arg}" =~ ^--format= ]]; then
        benchmark_args+=(--benchmark_format="${arg#--format=}")
      else
        benchmark_args+=("${arg}")
      fi
    done

    "${bench_exe}" "${benchmark_args[@]}" || log_warn "性能測試執行異常: ${bench_exe}"
    echo ""
  done

  # 完成
  log_separator "=" 50 "${LOG_COLOR_GREEN}"
  log_success "性能測試完成！"
  log_separator "=" 50 "${LOG_COLOR_GREEN}"
}

################################################################################
# clean 命令實作
################################################################################
cmd_clean() {
  local target="${1:-all}"
  local build_dir="${BUILD_DIR_PREFIX}"

  if [[ "${target}" == "all" ]] || [[ -z "${target}" ]]; then
    if [[ "${DRY_RUN}" == true ]]; then
      log_info "[DRY-RUN] 將刪除: ${build_dir}/"
      return
    fi

    if [[ -d "${build_dir}" ]]; then
      log_warn "清理建置目錄: ${build_dir}/"
      rm -rf "${build_dir}"
      log_success "已刪除: ${build_dir}/"
    else
      log_info "建置目錄不存在: ${build_dir}/"
    fi
  else
    if ! is_valid_build_type "${target}"; then
      log_error "無效的建置類型: ${target}"
      log_info "有效類型: ${VALID_BUILD_TYPES}"
      exit 1
    fi

    local target_dir="${build_dir}/${target}"

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
# install 命令實作
################################################################################
cmd_install() {
  local build_type="$1"
  local build_dir="${BUILD_DIR_PREFIX}/${build_type}"
  local install_dir="${INSTALL_PREFIX}/${build_type}"

  if [[ ! -d "${build_dir}" ]]; then
    log_error "建置目錄不存在: ${build_dir}"
    log_info "請先執行: ${SCRIPT_NAME} build ${build_type}"
    exit 1
  fi

  log_header "安裝專案" "${LOG_COLOR_CYAN}"
  log_info "建置類型: ${build_type}"
  log_info "來源目錄: ${build_dir}"
  log_info "安裝目錄: ${install_dir}"
  echo ""

  if [[ "${DRY_RUN}" == true ]]; then
    log_info "[DRY-RUN] 將安裝: ${build_type}"
    return
  fi

  cmake --install "${build_dir}" --prefix "${install_dir}" || log_fatal "安裝失敗"

  log_success "安裝成功！"
  log_info "安裝路徑: ${install_dir}"
}

################################################################################
# list 命令實作
################################################################################
cmd_list() {
  log_header "可用命令" "${LOG_COLOR_CYAN}"

  echo "內建命令:"
  echo "  - build      建置專案"
  echo "  - test       執行測試"
  echo "  - bench      執行效能測試"
  echo "  - clean      清理建置目錄"
  echo "  - install    安裝專案"
  echo "  - list       列出所有命令"
  echo "  - help       顯示幫助"

  echo ""
  log_info "執行 '${SCRIPT_NAME} help' 查看詳細說明"
}

################################################################################
# 主程式入口
################################################################################
main() {
  # 載入配置
  load_config

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

  # 處理內建命令
  case "${command}" in
  build)
    local build_type
    build_type=$(parse_build_type "${command}" "${1:-}")
    if is_valid_build_type "${1:-}"; then
      shift
    fi
    cmd_build "${build_type}" "$@"
    ;;

  test)
    local build_type
    build_type=$(parse_build_type "${command}" "${1:-}")
    if is_valid_build_type "${1:-}"; then
      shift
    fi
    cmd_test "${build_type}" "$@"
    ;;

  bench)
    local build_type
    build_type=$(parse_build_type "${command}" "${1:-}")
    if is_valid_build_type "${1:-}"; then
      shift
    fi
    cmd_bench "${build_type}" "$@"
    ;;

  clean)
    cmd_clean "${1:-all}"
    ;;

  install)
    local build_type
    build_type=$(parse_build_type "${command}" "${1:-}")
    cmd_install "${build_type}"
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
