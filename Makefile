SCRIPTS_DIR := scripts
export PROJECT_ROOT := $(shell pwd)

# 初始化 (請更動 conan 或者 cmake 時候要重新執行)
.PHONY: setup
setup:
	@bash $(SCRIPTS_DIR)/setup.sh


# 建置
.PHONY: debug
debug:
	@bash $(SCRIPTS_DIR)/build.sh debug

.PHONY: relwithdebinfo
relwithdebinfo:
	@bash $(SCRIPTS_DIR)/build.sh relwithdebinfo

.PHONY: release
release:
	@bash $(SCRIPTS_DIR)/build.sh release

# 覆蓋率
.PHONY: coverage
coverage:
	@bash $(SCRIPTS_DIR)/coverage.sh

.PHONY: coverage-html
coverage-html:
	@bash $(SCRIPTS_DIR)/coverage-html.sh

# 分析工具
.PHONY: disasm
disasm:
	@bash $(SCRIPTS_DIR)/disasm.sh

.PHONY: perf
perf:
	@bash $(SCRIPTS_DIR)/perf.sh

# 格式化
.PHONY: format
format:
	@fd -e cpp -e hpp -E build -x clang-format -i

# 清理
.PHONY: clean
clean:
	@rm -rf build compile_commands.json
