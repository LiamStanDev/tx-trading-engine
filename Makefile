################################################################################
# Makefile for TX Trading Engine
# 使用 CMake Presets 進行建置管理
################################################################################

# 配置
PRESET_DEBUG := debug
PRESET_RELWITHDEBINFO := relwithdebinfo
PRESET_RELEASE := release
PRESET_COVERAGE := coverage

# 預設目標
DEFAULT_PRESET := $(PRESET_DEBUG)
TEST_PRESET := $(PRESET_RELWITHDEBINFO)
BENCH_PRESET := $(PRESET_RELWITHDEBINFO)
BENCH_RELEASE_PRESET := $(PRESET_RELEASE)

# 目錄
BUILD_DIR := build

# 工具
CMAKE := cmake
CTEST := ctest
LLVM_PROFDATA := llvm-profdata
LLVM_COV := llvm-cov
DOXYGEN := doxygen

# 檢測是否支援顏色
COLOR_RESET := \033[0m
COLOR_GREEN := \033[0;32m
COLOR_BLUE := \033[0;34m
COLOR_YELLOW := \033[1;33m

# Benchmark 參數
ARGS :=

################################################################################
# 主要目標
################################################################################

.PHONY: all
all: debug

.PHONY: help
help:
	@echo -e "$(COLOR_BLUE)TX Trading Engine - 可用命令$(COLOR_RESET)"
	@echo ""
	@echo -e "$(COLOR_GREEN)建置命令:$(COLOR_RESET)"
	@echo "  make [debug]         - Debug 建置 (預設)"
	@echo "  make relwithdebinfo  - RelWithDebInfo 建置 (測試+效能分析)"
	@echo "  make release         - Release 建置 (完全優化)"
	@echo ""
	@echo -e "$(COLOR_GREEN)測試命令:$(COLOR_RESET)"
	@echo "  make test            - 執行測試 (使用 relwithdebinfo)"
	@echo "  make bench           - 執行 benchmark (使用 relwithdebinfo)"
	@echo "  make bench-release   - 執行 benchmark (使用 release)"
	@echo "  make coverage        - 代碼覆蓋率分析"
	@echo ""
	@echo -e "$(COLOR_GREEN)清理命令:$(COLOR_RESET)"
	@echo "  make clean           - 清理所有建置目錄"
	@echo ""
	@echo -e "$(COLOR_YELLOW)範例:$(COLOR_RESET)"
	@echo "  make bench ARGS='--benchmark_filter=Price'"
	@echo "  make test VERBOSE=1"

################################################################################
# 建置目標
################################################################################

.PHONY: debug
debug:
	@echo -e "$(COLOR_BLUE)[建置] Debug 模式$(COLOR_RESET)"
	@$(CMAKE) --preset $(PRESET_DEBUG)
	@$(CMAKE) --build --preset $(PRESET_DEBUG)
	@echo -e "$(COLOR_GREEN)✓ Debug 建置完成$(COLOR_RESET)"

.PHONY: relwithdebinfo
relwithdebinfo:
	@echo -e "$(COLOR_BLUE)[建置] RelWithDebInfo 模式$(COLOR_RESET)"
	@$(CMAKE) --preset $(PRESET_RELWITHDEBINFO)
	@$(CMAKE) --build --preset $(PRESET_RELWITHDEBINFO)
	@echo -e "$(COLOR_GREEN)✓ RelWithDebInfo 建置完成$(COLOR_RESET)"

.PHONY: release
release:
	@echo -e "$(COLOR_BLUE)[建置] Release 模式$(COLOR_RESET)"
	@$(CMAKE) --preset $(PRESET_RELEASE)
	@$(CMAKE) --build --preset $(PRESET_RELEASE)
	@echo -e "$(COLOR_GREEN)✓ Release 建置完成$(COLOR_RESET)"

################################################################################
# 測試目標
################################################################################

.PHONY: test
test: relwithdebinfo
	@echo -e "$(COLOR_BLUE)[測試] 執行單元測試$(COLOR_RESET)"
	@$(CTEST) --preset $(TEST_PRESET) $(if $(VERBOSE),--verbose,)
	@echo -e "$(COLOR_GREEN)✓ 測試完成$(COLOR_RESET)"

.PHONY: bench
bench: relwithdebinfo
	@echo -e "$(COLOR_BLUE)[效能測試] 使用 RelWithDebInfo$(COLOR_RESET)"
	@find $(BUILD_DIR)/$(BENCH_PRESET) -type f \( -name "*bench*" -o -name "*benchmark*" \) -executable -exec {} $(ARGS) \;
	@echo -e "$(COLOR_GREEN)✓ Benchmark 完成$(COLOR_RESET)"

.PHONY: bench-release
bench-release: release
	@echo -e "$(COLOR_BLUE)[效能測試] 使用 Release$(COLOR_RESET)"
	@find $(BUILD_DIR)/$(BENCH_RELEASE_PRESET) -type f \( -name "*bench*" -o -name "*benchmark*" \) -executable -exec {} $(ARGS) \;
	@echo -e "$(COLOR_GREEN)✓ Benchmark 完成$(COLOR_RESET)"

################################################################################
# 覆蓋率目標
################################################################################

.PHONY: coverage
coverage:
	@echo -e "$(COLOR_BLUE)[覆蓋率] 建置 Coverage 版本$(COLOR_RESET)"
	@$(CMAKE) --preset $(PRESET_COVERAGE)
	@$(CMAKE) --build --preset $(PRESET_COVERAGE)
	@echo -e "$(COLOR_BLUE)[覆蓋率] 執行測試$(COLOR_RESET)"
	@cd $(BUILD_DIR)/$(PRESET_COVERAGE) && \
		LLVM_PROFILE_FILE="coverage-%p.profraw" ./tx-common/tests/tx-common-tests || true
	@echo -e "$(COLOR_BLUE)[覆蓋率] 合併 Profile 資料$(COLOR_RESET)"
	@$(LLVM_PROFDATA) merge -sparse \
		$(BUILD_DIR)/$(PRESET_COVERAGE)/coverage-*.profraw \
		-o $(BUILD_DIR)/$(PRESET_COVERAGE)/coverage.profdata
	@echo -e "$(COLOR_BLUE)[覆蓋率] 生成 HTML 報告$(COLOR_RESET)"
	@$(LLVM_COV) show \
		$(BUILD_DIR)/$(PRESET_COVERAGE)/tx-common/tests/tx-common-tests \
		-instr-profile=$(BUILD_DIR)/$(PRESET_COVERAGE)/coverage.profdata \
		-format=html \
		-output-dir=$(BUILD_DIR)/$(PRESET_COVERAGE)/coverage-html \
		-ignore-filename-regex='(_deps|/tests/|/bench/)' \
		-show-line-counts-or-regions \
		-show-instantiations=false
	@echo ""
	@echo -e "$(COLOR_BLUE)[覆蓋率] 文字摘要:$(COLOR_RESET)"
	@$(LLVM_COV) report \
		$(BUILD_DIR)/$(PRESET_COVERAGE)/tx-common/tests/tx-common-tests \
		-instr-profile=$(BUILD_DIR)/$(PRESET_COVERAGE)/coverage.profdata \
		-ignore-filename-regex='(_deps|/tests/|/bench/)'
	@echo ""
	@echo -e "$(COLOR_GREEN)✓ 覆蓋率報告已生成$(COLOR_RESET)"
	@echo -e "  HTML 報告: $(BUILD_DIR)/$(PRESET_COVERAGE)/coverage-html/index.html"
	@echo -e "  開啟方式: xdg-open $(BUILD_DIR)/$(PRESET_COVERAGE)/coverage-html/index.html"


################################################################################
# 清理目標
################################################################################

.PHONY: clean
clean:
	@echo -e "$(COLOR_YELLOW)[清理] 移除建置目錄$(COLOR_RESET)"
	@rm -rf $(BUILD_DIR)
	@echo -e "$(COLOR_GREEN)✓ 清理完成$(COLOR_RESET)"
