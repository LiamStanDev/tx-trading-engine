#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

# ==============================================================================
# 主函數: 建置專案
# ==============================================================================
# 用法: build.sh <debug|relwithdebinfo|release>
# 功能:
#   1. 驗證 preset 參數
#   2. 執行 CMake 配置
#   3. 執行建置
#   4. 為 Debug 模式建立 LSP 符號連結
# ==============================================================================
main() {
  local preset="${1:-}"

  # 檢查必要工具
  log_info "檢查必要工具"
  check_command "$CONAN"
  check_command "$CMAKE"

  # 驗證參數
  if [ -z "$preset" ]; then
    die "用法: build.sh <debug|relwithdebinfo|release>"
  fi

  # 驗證 preset
  case "$preset" in
  debug | relwithdebinfo | release)
    log_info "使用 preset: $preset"
    ;;
  *)
    die "無效的 preset: $preset（有效值: debug, relwithdebinfo, release）"
    ;;
  esac

  # 配置階段
  log_info "執行 CMake 配置 (preset: $preset)"
  if ! "$CMAKE" --preset "$preset"; then
    die "CMake 配置失敗 (preset: $preset)"
  fi
  log_success "CMake 配置完成"

  # 建置階段
  log_info "執行建置 (preset: $preset)"
  if ! "$CMAKE" --build --preset "$preset"; then
    die "建置失敗 (preset: $preset)"
  fi
  log_success "建置完成"

  # 如果是 debug,更新 LSP 符號連結
  if [ "$preset" = "debug" ]; then
    log_info "建立 compile_commands.json 符號連結 (用於 LSP)"
    local compile_commands="${BUILD_DIR}/Debug/compile_commands.json"

    check_file "$compile_commands"
    ln -sf "$compile_commands" "${PROJECT_ROOT}/compile_commands.json"
    log_success "LSP 設定完成"
  fi

  log_success "完整建置流程結束: $preset"
}

main "$@"
