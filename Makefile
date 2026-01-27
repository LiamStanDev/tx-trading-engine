################################################################################
# C++ Development Toolkit
################################################################################
# 本 Makefile 提供完整的 C++ 開發工具鏈，包含：
# - 建置系統 (CMake Presets)
# - 效能分析 (Perf, Flamegraph)
# - 記憶體分析 (Valgrind Cachegrind)
# - 程式碼覆蓋率 (GCC gcov/lcov)
# - 反組譯與檢視 (Objdump)
# - 測試與基準測試 (CTest, Google Benchmark)
################################################################################

################################################################################
# 工具配置 (Tool Configuration)
################################################################################

# --- 路徑與目錄 ---
BUILD_DIR    := build
PRESETS      := debug relwithdebinfo release coverage clang-debug clang-relwithdebinfo

# --- 核心建置工具 ---
CMAKE        := cmake
CTEST        := ctest

# --- 效能分析工具 ---
# perf: Linux 核心級效能分析器，用於 CPU profiling
PERF         := sudo perf

# objdump: 反組譯工具
#   -C: Demangle C++ 符號名稱
#   -d: 反組譯執行段
#   -S: 交錯顯示原始碼（需要 debug info）
#   --visualize-jumps=color: 用顏色標示跳轉路徑
#   --no-show-raw-insn: 隱藏機器碼位元組
OBJDUMP      := objdump -C -d -S --visualize-jumps=color --no-show-raw-insn

# nm: 符號表查看器
#   -C: Demangle C++ 符號
NM           := nm -C

# --- 程式碼覆蓋率工具 ---
GCOV         := gcov
LCOV         := lcov
GENHTML      := genhtml

# --- 記憶體分析工具 ---
VALGRIND     := valgrind

# --- 互動式選單 (fzf) ---
# fzf: 模糊搜尋工具，用於互動式選擇
FZF          := fzf --height=60% --layout=reverse --border --ansi --preview-window=right:60%

# --- 終端顏色定義 ---
C_RESET  := \033[0m
C_GREEN  := \033[0;32m
C_BLUE   := \033[0;34m
C_YELLOW := \033[1;33m
C_RED    := \033[0;31m
C_BOLD   := \033[1m

# --- 搜尋優化 ---
FIND_EXCLUDE := -not -path "*/CMakeFiles/*" \
                -not -path "*/_deps/*" \
                -not -path "*/.cpm_cache/*" \
                -not -name "*.py" \
                -not -name "*.sh" \
                -not -name "*.cmake" \
                -not -name "CMake*"

# --- 使用者參數 ---
# 透過 ARGS 變數傳遞額外參數給程式
# 範例: make bench ARGS="--benchmark_filter=Price"
ARGS :=

################################################################################
# 說明文件 (Help Documentation)
################################################################################

# 顯示所有可用命令的說明
# 使用方式: make help 或 make
.PHONY: help
help:
	@echo -e "$(C_BOLD)$(C_BLUE)C++ Development Toolkit$(C_RESET)"
	@echo "-------------------------------------------------------"
	@echo -e "$(C_YELLOW)Build (CMake Presets):$(C_RESET)"
	@echo "  make build          - 交互式選擇建置模式"
	@echo "  make <preset>       - 直接建置 (debug, release, relwithdebinfo)"
	@echo ""
	@echo -e "$(C_YELLOW)Performance Analysis:$(C_RESET)"
	@echo "  make disasm         - 反組譯特定函數"
	@echo ""
	@echo -e "$(C_YELLOW)Code Quality:$(C_RESET)"
	@echo "  make test           - 執行單元測試"
	@echo "  make bench          - 執行效能基準測試"
	@echo "  make coverage       - 程式碼覆蓋率報告（終端）"
	@echo "  make coverage-html  - 程式碼覆蓋率報告（HTML）"
	@echo ""
	@echo -e "$(C_YELLOW)Development Tools:$(C_RESET)"
	@echo "  make clean          - 清除建置目錄"
	@echo ""
	@echo -e "$(C_BLUE)參數範例:$(C_RESET)"
	@echo "  make bench ARGS=\"--benchmark_filter=Price\""

################################################################################
# 建置系統 (Build System)
################################################################################

# --- 建置函數定義 ---
# 參數: $1 = preset 名稱 (debug, release, relwithdebinfo, coverage)
# 功能: 使用 CMake Presets 執行完整建置流程
define build_preset
	@echo -e "$(C_BLUE)[Build] 模式: $(C_BOLD)$1$(C_RESET)"
	@$(CMAKE) --preset $1
	@$(CMAKE) --build --preset $1
	@echo -e "$(C_GREEN)✓ $1 建置完成$(C_RESET)"
endef

# --- Debug 建置 ---
# 用途: 開發階段除錯，包含完整符號資訊，無優化
# 使用場景: GDB 除錯、詳細錯誤追蹤
.PHONY: debug
debug:
	$(call build_preset,debug)

# --- RelWithDebInfo 建置 ---
# 用途: 兼具效能與除錯能力（Release + Debug Info）
# 使用場景: 效能分析、生產環境問題追蹤
# 建議: 效能分析時使用此模式（make profile, make mca 等）
.PHONY: relwithdebinfo
relwithdebinfo:
	$(call build_preset,relwithdebinfo)

# --- Release 建置 ---
# 用途: 生產環境，完全優化，無除錯資訊
# 使用場景: 最終發布、效能基準測試
.PHONY: release
release:
	$(call build_preset,release)

# --- 互動式建置選擇 ---
# 功能: 使用 fzf 互動式選擇建置模式
# 使用方式: make build
.PHONY: build
build:
	@PRESET=$$(echo "$(PRESETS)" | tr ' ' '\n' | $(FZF) --prompt="選擇建置預設值 (Preset)> "); \
	if [ -n "$$PRESET" ]; then $(MAKE) $$PRESET; fi

################################################################################
# 測試與品質保證 (Testing & Quality Assurance)
################################################################################

# --- 單元測試 ---
# 功能: 執行 CTest 測試套件
# 使用方式: make test
# 參數:
#   --output-on-failure: 只在測試失敗時顯示詳細輸出
# 流程:
#   1. 互動式選擇建置模式 (debug/release/relwithdebinfo)
#   2. 若建置目錄不存在，自動建置
#   3. 執行 ctest
.PHONY: test
test:
	@PRESET=$$(echo "$(PRESETS)" | tr ' ' '\n' | $(FZF) --prompt="選擇測試環境> "); \
	if [ -z "$$PRESET" ]; then exit 0; fi; \
	$(MAKE) $$PRESET; \
	echo -e "$(C_BLUE)[Test] 執行 $$PRESET 測試$(C_RESET)"; \
	$(CTEST) --test-dir $(BUILD_DIR)/$$PRESET --output-on-failure

# --- 效能基準測試 ---
# 功能: 執行 Google Benchmark 測試
# 使用方式: make bench ARGS="--benchmark_filter=Price"
# 建議: 使用 release 或 relwithdebinfo 模式以獲得可靠結果
# 流程:
#   1. 選擇建置模式（限定 relwithdebinfo/release）
#   2. 自動建置（若需要）
#   3. 互動式選擇 benchmark 執行檔
#   4. 執行並傳遞 ARGS 參數
.PHONY: bench
bench:
	@mkdir -p reports
	@PRESET=$$(printf "relwithdebinfo\nrelease" | $(FZF) --prompt="選擇環境 (Benchmark)> "); \
	if [ -z "$$PRESET" ]; then exit 0; fi; \
	$(MAKE) $$PRESET; \
	TARGET_DIR="$(BUILD_DIR)/$$PRESET"; \
	TARGET=$$(find $$TARGET_DIR -maxdepth 3 -type f -executable $(FIND_EXCLUDE) -name "*bench*" | $(FZF) --prompt="選擇測試程式> "); \
	if [ -z "$$TARGET" ]; then exit 0; fi; \
	echo -e "$(C_YELLOW)-------------------------------------------------------$(C_RESET)"; \
	echo -e "$(C_BOLD)HFT Benchmark + Perf 分析模式$(C_RESET)"; \
	echo -e " - 直接輸入模式 (支援 Regex)"; \
	echo -e "$(C_YELLOW)-------------------------------------------------------$(C_RESET)"; \
	read -p "Filter 關鍵字: " FINAL_FILTER; \
	SAFE_FILTER=$$(echo "$$FINAL_FILTER" | sed 's/[^a-zA-Z0-9]/_/g'); \
	[ -z "$$SAFE_FILTER" ] && SAFE_FILTER="all"; \
	TIMESTAMP=$$(date +%Y%m%d_%H%M%S); \
	OUT_FILE="reports/benchmark_$${SAFE_FILTER}_$${TIMESTAMP}.txt"; \
	echo -e "\n$(C_BLUE)[HFT-Bench] 執行檔: $$TARGET$(C_RESET)"; \
	echo -e "$(C_BLUE)[HFT-Bench] 記錄檔: $$OUT_FILE$(C_RESET)"; \
	echo -e "$(C_YELLOW)-------------------------------------------------------$(C_RESET)"; \
	perf stat -d -d -d \
		$$TARGET \
			--benchmark_filter="$$FINAL_FILTER" \
			--benchmark_format=console $(ARGS) 2>&1 \
		| tee $$OUT_FILE

# --- 程式碼覆蓋率分析（終端報告）---
# 功能: 生成程式碼覆蓋率報告並顯示在終端
# 使用方式: make coverage
# 技術: GCC gcov/lcov (需要編譯時加入 --coverage)
# 流程:
#   1. 使用 coverage preset 建置（啟用 instrumentation）
#   2. 清理舊的 .gcda 檔案
#   3. 執行所有測試並收集 .gcda 檔案
#   4. 使用 lcov 生成覆蓋率報告
#   5. 過濾掉測試與第三方程式碼
# 輸出: 終端顯示覆蓋率摘要
.PHONY: coverage
coverage:
	@echo -e "$(C_BLUE)[Coverage] 步驟 1/5: 建置 Coverage 版本$(C_RESET)"
	@$(CMAKE) --preset coverage
	@$(CMAKE) --build --preset coverage
	@echo -e "$(C_GREEN)✓ 建置完成$(C_RESET)"
	@echo ""
	@echo -e "$(C_BLUE)[Coverage] 步驟 2/5: 清理舊的覆蓋率資料$(C_RESET)"
	@find $(BUILD_DIR)/coverage -name "*.gcda" -delete 2>/dev/null || true
	@rm -f $(BUILD_DIR)/coverage/coverage.info $(BUILD_DIR)/coverage/coverage_filtered.info
	@echo -e "$(C_GREEN)✓ 清理完成$(C_RESET)"
	@echo ""
	@echo -e "$(C_BLUE)[Coverage] 步驟 3/5: 執行測試並收集覆蓋率資料$(C_RESET)"
	@$(CTEST) --test-dir $(BUILD_DIR)/coverage --output-on-failure
	@echo -e "$(C_GREEN)✓ 測試執行完成$(C_RESET)"
	@echo ""
	@echo -e "$(C_BLUE)[Coverage] 步驟 4/5: 收集覆蓋率資料$(C_RESET)"
	@$(LCOV) --capture --directory $(BUILD_DIR)/coverage \
		--output-file $(BUILD_DIR)/coverage/coverage.info \
		--rc branch_coverage=1 \
		--ignore-errors mismatch,inconsistent,negative,unused,count
	@echo -e "$(C_GREEN)✓ 資料收集完成$(C_RESET)"
	@echo ""
	@echo -e "$(C_BLUE)[Coverage] 步驟 5/5: 過濾與生成報告$(C_RESET)"
	@$(LCOV) --remove $(BUILD_DIR)/coverage/coverage.info \
		'*/tests/*' '*/_deps/*' '*/build/*' '/usr/*' \
		--output-file $(BUILD_DIR)/coverage/coverage_filtered.info \
		--rc branch_coverage=1 \
		--ignore-errors mismatch,inconsistent,negative,unused,count
	@echo -e "$(C_YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(C_RESET)"
	@echo -e "$(C_BOLD)$(C_GREEN)程式碼覆蓋率報告 (排除測試與 Benchmark)$(C_RESET)"
	@echo -e "$(C_YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(C_RESET)"
	@$(LCOV) --list $(BUILD_DIR)/coverage/coverage_filtered.info --ignore-errors unused
	@echo -e "$(C_YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(C_RESET)"
	@echo -e "$(C_GREEN)✓ Coverage 分析完成！$(C_RESET)"
	@echo -e "$(C_BLUE)提示: 原始資料位於 $(BUILD_DIR)/coverage/coverage_filtered.info$(C_RESET)"

# --- 程式碼覆蓋率分析（HTML 報告）---
# 功能: 生成詳細的 HTML 格式覆蓋率報告
# 使用方式: make coverage-html
# 依賴: 如果 coverage_filtered.info 不存在，會先執行 make coverage
# 輸出: build/coverage/html/index.html
# 用途: 查看每一行程式碼的覆蓋情況，包含分支覆蓋率
.PHONY: coverage-html
coverage-html:
	@if [ ! -f "$(BUILD_DIR)/coverage/coverage_filtered.info" ]; then \
		echo -e "$(C_YELLOW)找不到覆蓋率資料，正在執行 make coverage...$(C_RESET)"; \
		$(MAKE) coverage; \
	fi
	@echo -e "$(C_BLUE)[Coverage HTML] 生成 HTML 報告...$(C_RESET)"
	@$(GENHTML) $(BUILD_DIR)/coverage/coverage_filtered.info \
		--output-directory $(BUILD_DIR)/coverage/html \
		--rc branch_coverage=1 \
		--demangle-cpp \
		--title "Engine Coverage Report" \
		--legend \
		--ignore-errors source,unused
	@echo -e "$(C_GREEN)✓ HTML 報告已生成$(C_RESET)"
	@echo -e "$(C_BLUE)查看報告: file://$(shell pwd)/$(BUILD_DIR)/coverage/html/index.html$(C_RESET)"

################################################################################
# HFT 效能分析工具 (High-Frequency Trading Performance Profiling)
################################################################################

.PHONY: profile
profile:
	@TARGET=$$(find $(BUILD_DIR) -type f -executable -not -path "*/CMakeFiles/*" -not -path "*/_deps/*" | $(FZF) --prompt="選擇目標> "); \
	if [ -z "$$TARGET" ]; then exit 0; fi; \
	ACTION=$$(echo -e "stat\nrecord\nreport\ntop" | $(FZF) --prompt="Perf Action> "); \
	case $$ACTION in \
		"stat")   $(PERF) stat -d -d -d $$TARGET $(ARGS) ;; \
		"record") $(PERF) record -g --call-graph dwarf $$TARGET $(ARGS) ;; \
		"report") $(PERF) report --hierarchy -M intel ;; \
		"top")    $(PERF) top -M intel ;; \
	esac


################################################################################
# 開發工具 (Development Tools)
################################################################################

# --- 反組譯檢視 ---
# 功能: 查看函數的組合語言代碼
# 使用方式: make disasm
# 互動流程:
#   1. 選擇執行檔
#   2. 選擇函數（可搜尋其他函數）
#      - 若不選擇函數，顯示整個執行檔的反組譯
# 技術:
#   - nm -C: 列出所有符號並 demangle C++ 名稱
#   - grep ' [TtWw] ': 過濾函數符號（T/t: text, W/w: weak）
#   - awk 提取完整函數簽名（包含參數）
#   - objdump 反組譯並顯示彩色跳轉箭頭
# 使用場景:
#   - 驗證編譯器優化（如 inline, loop unrolling）
#   - 學習 C++ 語義對應的機器碼
#   - 檢查關鍵函數的指令序列
# 互動操作（在 less 中）:
#   - 空格: 下一頁
#   - /pattern: 搜尋
#   - q: 退出
.PHONY: disasm
disasm:
	@TARGET=$$(find $(BUILD_DIR) -type f -executable $(FIND_EXCLUDE) -not -name "*.bin" | $(FZF) --prompt="選擇反組譯目標> "); \
	if [ -z "$$TARGET" ]; then echo "取消選擇"; exit 0; fi; \
	FUNC=$$($(NM) $$TARGET | grep -E ' [TtWw] ' | awk '{$$1=$$2=""; print substr($$0,3)}' | $(FZF) --prompt="選擇函數> "); \
	if [ -z "$$FUNC" ]; then \
		echo -e "$(C_BLUE)[Disasm] 顯示完整反組譯: $$TARGET$(C_RESET)"; \
		$(OBJDUMP) $$TARGET | less -R; \
	else \
		echo -e "$(C_BLUE)[Disasm] 函數: $$FUNC$(C_RESET)"; \
		$(OBJDUMP) $$TARGET | awk -v fname="$$FUNC" 'index($$0, "<"fname">:") {flag=1} flag {print} /^[0-9a-f]+[[:space:]]+</ && flag && !index($$0, "<"fname">:") {exit}' | less -R; \
	fi

# --- 清除建置目錄 ---
# 功能: 刪除所有建置產物
# 使用方式: make clean
# 注意: 此操作不可逆，會刪除所有建置檔案、測試執行檔和覆蓋率數據
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@echo -e "$(C_GREEN)Build directory cleaned.$(C_RESET)"

################################################################################
# 預設目標
################################################################################

.DEFAULT_GOAL := help
