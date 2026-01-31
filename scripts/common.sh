#!/usr/bin/env bash

# ==============================================================================
# 環境變數配置
# ==============================================================================

# --- 路徑配置 ---
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SCRIPTS_DIR="${PROJECT_ROOT}/scripts"
BUILD_DIR="${PROJECT_ROOT}/build"
CONAN_PROFILE_DIR="${PROJECT_ROOT}/conan-profile"

# --- 工具路徑 ---
CMAKE="${CMAKE:-cmake}"
CTEST="${CTEST:-ctest}"
CONAN="${CONAN:-conan}"
LCOV="${LCOV:-lcov}"
GENHTML="${GENHTML:-genhtml}"
OBJDUMP="${OBJDUMP:-objdump}"
NM="${NM:-nm}"
FZF="${FZF:-fzf}"
PERF="${PERF:-perf}"
PYTHON="${PYTHON:-python3}"
LSOF="${LSOF:-lsof}"

# --- 工具參數 ---
OBJDUMP_FLAGS="-C -d -S --visualize-jumps=color --no-show-raw-insn"
NM_FLAGS="-C"
FZF_FLAGS="--height=60% --layout=reverse --border --ansi --preview-window=right:60%"

# --- Perf 參數 ---
# perf record 參數 (高頻率採樣)
PERF_RECORD_FLAGS="-F 9999 --call-graph dwarf"
# perf report 輸出設定
PERF_REPORT_FLAGS="--stdio --percent-limit 1"

# --- Conan 配置 ---
CONAN_PROFILE="${CONAN_PROFILE_DIR}/gcc.profile"

# ==============================================================================
# 共用函數庫
# ==============================================================================

# --- 顏色定義 ---
C_RESET='\033[0m'
C_RED='\033[0;31m'
C_GREEN='\033[0;32m'
C_YELLOW='\033[1;33m'
C_BLUE='\033[0;34m'
C_BOLD='\033[1m'

# --- Log 函數 ---
log_info() {
  echo -e "${C_BLUE}[INFO]${C_RESET} $*"
}
log_success() {
  echo -e "${C_GREEN}[SUCCESS]${C_RESET} $*"
}
log_warning() {
  echo -e "${C_YELLOW}[WARNING]${C_RESET} $*"
}
log_error() {
  echo -e "${C_RED}[ERROR]${C_RESET} $*" >&2
}

# --- 錯誤處理 ---
die() {
  log_error "$*"
  exit 1
}

# --- 檢查工具是否存在 ---
check_command() {
  local cmd="$1"
  if ! command -v "$cmd" &>/dev/null; then
    die "找不到命令: $cmd，請先安裝"
  fi
}

# --- 檢查檔案是否存在 ---
check_file() {
  local file="$1"
  if [ ! -f "$file" ]; then
    die "找不到檔案: $file"
  fi
}

# --- 檢查目錄是否存在 ---
check_dir() {
  local dir="$1"
  if [ ! -d "$dir" ]; then
    die "找不到目錄: $dir"
  fi
}

# --- 檢查執行檔是否存在且可執行 ---
check_executable() {
  local exe="$1"
  if [ ! -f "$exe" ]; then
    die "找不到執行檔: $exe"
  fi
  if [ ! -x "$exe" ]; then
    die "檔案不可執行: $exe"
  fi
}

# --- 使用 FZF 選擇 BUILD_DIR 中的執行檔 ---
# 參數: $1 = 提示訊息 (選填, 預設: "選擇目標執行檔")
select_executable() {
  local prompt="${1:-選擇目標執行檔}"

  check_command "$FZF"
  check_dir "$BUILD_DIR"

  local target
  target=$(find "$BUILD_DIR" -type f -executable \
    -not -path "*/CMakeFiles/*" \
    -not -path "*/_deps/*" \
    -not -path "*/.cpm_cache/*" \
    -not -name "*.cmake" \
    -not -name "CMake*" \
    -not -name "*.bin" 2>/dev/null |
    "$FZF" $FZF_FLAGS --prompt="${prompt}> ") || die "已取消選擇"

  check_executable "$target"
  echo "$target"
}
