#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

# ==============================================================================
# 主函數: 專案初始化 (Conan 2.0)
# ==============================================================================
# 功能:
#   1. 安裝所有建置類型的 Conan 依賴 (Debug, RelWithDebInfo, Release)
#   2. 配置 Debug 模式的 compile_commands.json 供 LSP 使用
# ==============================================================================
main() {
	log_info "開始 Conan 2.0 專案初始化"

	# 檢查必要工具
	log_info "檢查必要工具"
	check_command "$CONAN"
	check_command "$CMAKE"

	# 檢查 Conan profile 是否存在
	log_info "檢查 Conan profile: ${CONAN_PROFILE}"
	check_file "$CONAN_PROFILE"

	# 安裝 Debug 依賴
	log_info "安裝 Debug 依賴"
	if ! "$CONAN" install . \
		-pr="${CONAN_PROFILE}" \
		-s build_type=Debug \
		--build=missing; then
		die "Conan Debug 安裝失敗"
	fi
	log_success "Debug 依賴安裝完成"

	# 安裝 RelWithDebInfo 依賴
	log_info "安裝 RelWithDebInfo 依賴"
	if ! "$CONAN" install . \
		-pr="${CONAN_PROFILE}" \
		-s build_type=RelWithDebInfo \
		--build=missing; then
		die "Conan RelWithDebInfo 安裝失敗"
	fi
	log_success "RelWithDebInfo 依賴安裝完成"

	# 安裝 Release 依賴
	log_info "安裝 Release 依賴"
	if ! "$CONAN" install . \
		-pr="${CONAN_PROFILE}" \
		-s build_type=Release \
		--build=missing; then
		die "Conan Release 安裝失敗"
	fi
	log_success "Release 依賴安裝完成"

	# 配置 LSP (使用 Debug 模式)
	log_info "配置 compile_commands.json (使用 Debug 模式,供 LSP 使用)"
	if ! "$CMAKE" --preset debug; then
		die "CMake 配置失敗 (preset: debug)"
	fi
	log_success "CMake Debug 配置完成"

	# 建立 compile_commands.json 符號連結
	local compile_commands="${BUILD_DIR}/Debug/compile_commands.json"
	check_file "$compile_commands"
	ln -sf "$compile_commands" "${PROJECT_ROOT}/compile_commands.json"
	log_success "LSP 符號連結建立完成"

	log_success "專案初始化完成！"
	echo ""
	log_info "下一步: 執行 './scripts/build.sh debug' 開始建置"
}

main "$@"
